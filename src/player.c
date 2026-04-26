#include "entity.h"
#include "actor.h"
#include "character.h"
#include "combat.h"
#include "player.h"
#include "item.h"
#include "item_data.h"
#include "inventory.h"
#include "atlas.h"
#include "bestiary.h"
#include "journal.h"
#include "log.h"
#include "tile.h"
#include "tileset.h"
#include "map.h"
#include "movement.h"
#include "overlay_nav.h"
#include "collision.h"
#include "race.h"
#include "input.h"
#include "keybind_helpers.h"
#include "target_lock.h"
#include "ui_overlay.h"
#include "crafting_compendium.h"
#include "furniture.h"
#include "world_items.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // optional, for exit()
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/*
 * Purpose:
 *   Implements player creation, placement, and legacy direct-input movement helper.
 *
 * Functions:
 *   - player_add_starter_template: helper to add one starter item by template name.
 *   - player_create: initializes player stats, entity data, and starter loadout.
 *   - player_place/player_place_random: set explicit or random valid start position.
 *   - player_show_character_sheet: renders a full stat/skill overlay page.
 *   - player_handle_input: legacy immediate input movement path.
 */

// Add one starter item template into a character inventory.
static void player_add_starter_template(Character* c, const char* template_name)
{
    Item item;
    const ItemTemplate* tmpl = item_template_by_name(template_name);

    if(!c || !tmpl)
        return;

    item_init_from_template(&item, tmpl, -1, -1);
    inventory_add(c, &item);
}

// Recalculate all derived resource maximums from base attributes and clamp current values.
#define STAMINA_RECOVERY_DELAY_AFTER_STAMINA_USE 2
#define STAMINA_WAIT_RECOVERY_RATE 2
#define STAMINA_REST_RECOVERY_RATE 4
#define STAMINA_SLEEP_RECOVERY_RATE 6
#define STAMINA_REST_TURNS 4
#define STAMINA_SLEEP_TURNS 6
#define STAMINA_SLEEP_BED_BONUS_RATE 2
#define STAMINA_SLEEP_BED_BONUS_TURN_REDUCTION 1
#define EXHAUSTION_STAMINA_COST_GAIN_THRESHOLD 3
#define EXHAUSTION_STAMINA_COST_DIVISOR 6
#define EXHAUSTION_REST_RECOVERY_AMOUNT 2
#define REST_FORWARD_TURNS 10
#define REST_RECOVERY_BASE 1
#define REST_RECOVERY_FURNITURE 2
#define SLEEP_FORWARD_TURNS 20
#define SLEEP_RECOVERY_BARE_GROUND 2
#define SLEEP_RECOVERY_BARE_FLOOR_INSIDE 3
#define SLEEP_RECOVERY_BEDROLL_OUTSIDE 3
#define SLEEP_RECOVERY_BEDROLL_INSIDE 4
#define SLEEP_RECOVERY_REGULAR_BED_INSIDE 5
#define SLEEP_RECOVERY_LUXURY_BED_INSIDE 6
#define REST_TURN_INTERVAL_MS 2000
#define SLEEP_TURN_INTERVAL_MS 1000
#define PLAYER_EXHAUSTION_MAX 100
#define PLAYER_SKILL_MAX_LEVEL 99

static int player_is_sleeping_on_bed(const Player* p);

typedef enum CharacterSheetTab {
    CHARACTER_SHEET_TAB_SUMMARY = 0,
    CHARACTER_SHEET_TAB_BODY,
    CHARACTER_SHEET_TAB_SKILLS,
    CHARACTER_SHEET_TAB_COUNT,
} CharacterSheetTab;

static int text_contains_ignore_case(const char* haystack, const char* needle)
{
    size_t needle_len;

    if(!haystack || !needle || needle[0] == '\0')
        return 0;

    needle_len = strlen(needle);
    for(const char* it = haystack; *it; it++)
    {
        size_t i = 0;
        while(i < needle_len && it[i] && tolower((unsigned char)it[i]) == tolower((unsigned char)needle[i]))
            i++;
        if(i == needle_len)
            return 1;
    }

    return 0;
}

static int player_tile_is_inside(const Player* p)
{
    TileLayer layer;
    const Tile* tile;

    if(!p || !current_area)
        return 0;

    tile = map_top_visible_tile(current_area,
                                p->character.actor.entity.x,
                                p->character.actor.entity.y,
                                &layer);
    if(!tile)
        return 0;

    return layer == TILE_LAYER_FLOOR;
}

static int player_has_bedroll_at_tile(const Player* p)
{
    int x;
    int y;
    int z;
    int count;

    if(!p)
        return 0;

    x = p->character.actor.entity.x;
    y = p->character.actor.entity.y;
    z = p->character.actor.entity.z;
    count = world_item_count_at_3d(x, y, z);

    for(int i = 0; i < count; i++)
    {
        WorldItem* world_item = world_item_at_ordinal_3d(x, y, z, i);
        if(!world_item || !world_item->active)
            continue;
        if(text_contains_ignore_case(world_item->item.name, "bedroll"))
            return 1;
    }

    return 0;
}

static int player_is_on_luxury_bed(const Player* p)
{
    Furniture* furniture;

    if(!p || !current_area)
        return 0;

    furniture = furniture_at_3d(current_area,
                                p->character.actor.entity.x,
                                p->character.actor.entity.y,
                                p->character.actor.entity.z);
    if(!furniture || furniture->type != FURNITURE_BED)
        return 0;

    if(furniture->template_data)
    {
        if(text_contains_ignore_case(furniture->template_data->id, "luxury") ||
           text_contains_ignore_case(furniture->template_data->name, "luxury"))
            return 1;
    }

    return text_contains_ignore_case(furniture_display_name(furniture), "luxury");
}

static int player_is_on_suitable_rest_furniture(const Player* p)
{
    Furniture* furniture;
    FurnitureInteractionType interaction;

    if(!p || !current_area)
        return 0;

    furniture = furniture_at_3d(current_area,
                                p->character.actor.entity.x,
                                p->character.actor.entity.y,
                                p->character.actor.entity.z);
    if(!furniture)
        return 0;

    if(furniture->type == FURNITURE_BED || furniture->type == FURNITURE_CHAIR)
        return 1;

    interaction = furniture_interaction_type(furniture);
    return interaction == FURNITURE_INTERACTION_REST || interaction == FURNITURE_INTERACTION_SIT;
}

static int player_sleep_stamina_recovery_amount(const Player* p)
{
    int inside;
    int has_bedroll;
    int on_bed;

    inside = player_tile_is_inside(p);
    has_bedroll = player_has_bedroll_at_tile(p);
    on_bed = player_is_sleeping_on_bed(p);

    if(on_bed && inside)
    {
        if(player_is_on_luxury_bed(p))
            return SLEEP_RECOVERY_LUXURY_BED_INSIDE;
        return SLEEP_RECOVERY_REGULAR_BED_INSIDE;
    }

    if(has_bedroll && inside)
        return SLEEP_RECOVERY_BEDROLL_INSIDE;
    if(has_bedroll)
        return SLEEP_RECOVERY_BEDROLL_OUTSIDE;
    if(inside)
        return SLEEP_RECOVERY_BARE_FLOOR_INSIDE;
    return SLEEP_RECOVERY_BARE_GROUND;
}

static void player_wait_milliseconds(int ms)
{
    if(ms <= 0)
        return;

#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((unsigned int)(ms * 1000));
#endif
}

static void player_draw_rest_progress(const char* label, int current_turn, int total_turns, int allow_cancel)
{
    char bar_line[64];
    char turn_line[32];
    int bar_width = 20;
    int filled;
    int i;

    if(!label || total_turns <= 0)
        return;

    filled = (bar_width * current_turn) / total_turns;
    if(filled > bar_width)
        filled = bar_width;

    bar_line[0] = '[';
    for(i = 0; i < bar_width; i++)
        bar_line[i + 1] = (i < filled) ? '#' : '.';
    bar_line[bar_width + 1] = ']';
    bar_line[bar_width + 2] = '\0';

    snprintf(turn_line, sizeof(turn_line), "Turn %d / %d", current_turn, total_turns);

    ui_overlay_draw_frame(label);
    ui_overlay_draw_line(0, "");
    ui_overlay_draw_line(1, bar_line);
    ui_overlay_draw_line(2, turn_line);
    if(allow_cancel)
        ui_overlay_draw_line(3, "Press any key to cancel");
    fflush(stdout);
}

PlayerTimedActionResult player_run_timed_action(Player* p,
                                                int turns,
                                                int tick_interval_ms,
                                                const char* label,
                                                int allow_cancel,
                                                PlayerTimedActionTickFn tick_fn,
                                                void* user_data,
                                                int* turns_completed)
{
    PlayerTimedActionResult result = PLAYER_TIMED_ACTION_COMPLETED;

    if(turns_completed)
        *turns_completed = 0;

    if(!p || turns <= 0)
        return result;

    for(int i = 0; i < turns; i++)
    {
        int key;

        if(p->character.actor.health <= 0)
        {
            result = PLAYER_TIMED_ACTION_STOPPED;
            break;
        }

        player_draw_rest_progress(label, i + 1, turns, allow_cancel);
        creatures_take_turns(p);

        if(turns_completed)
            *turns_completed = i + 1;

        if(p->character.actor.health <= 0)
        {
            result = PLAYER_TIMED_ACTION_STOPPED;
            break;
        }

        if(tick_fn && !tick_fn(p, user_data))
        {
            result = PLAYER_TIMED_ACTION_STOPPED;
            break;
        }

        if(allow_cancel)
        {
            key = read_input_key_nonblocking();
            if(key >= 0)
            {
                result = PLAYER_TIMED_ACTION_CANCELED;
                break;
            }
        }

        if(i < (turns - 1))
            player_wait_milliseconds(tick_interval_ms);
    }

    ui_overlay_reset_cache();

    return result;
}

static void player_forward_time_turns(Player* p, int turns, int tick_interval_ms, const char* label)
{
    (void)player_run_timed_action(p, turns, tick_interval_ms, label, 0, NULL, NULL, NULL);
}

static int player_exhaustion_ap_regen_penalty(const Player* p)
{
    if(!p)
        return 0;

    if(p->exhaustion >= 75)
        return 2;
    if(p->exhaustion >= 35)
        return 1;
    return 0;
}

static int player_exhaustion_stamina_recovery_penalty(const Player* p)
{
    if(!p)
        return 0;

    if(p->exhaustion >= 80)
        return 2;
    if(p->exhaustion >= 45)
        return 1;
    return 0;
}

static int player_adjust_stamina_recovery_for_exhaustion(const Player* p, int base_amount, int soften_penalty)
{
    int penalty;
    int adjusted;

    if(base_amount <= 0)
        return 0;

    penalty = player_exhaustion_stamina_recovery_penalty(p);
    if(soften_penalty && penalty > 0)
        penalty--;

    adjusted = base_amount - penalty;
    if(adjusted < 1)
        adjusted = 1;
    return adjusted;
}

static int player_is_sleeping_on_bed(const Player* p)
{
    Furniture* furniture;

    if(!p || !current_area)
        return 0;

    furniture = furniture_at_3d(current_area,
                                p->character.actor.entity.x,
                                p->character.actor.entity.y,
                                p->character.actor.entity.z);
    return furniture && furniture->type == FURNITURE_BED;
}

static int player_weapon_skill_xp_required_for_level(int skill_level)
{
    if(skill_level < 0)
        skill_level = 0;
    return 8 + (skill_level * 4);
}

static int player_non_weapon_skill_xp_required_for_level(int skill_level)
{
    if(skill_level < 0)
        skill_level = 0;
    return 100 * (skill_level + 1);
}

static void player_format_skill_xp_progress(char out[16], int level, int current_xp, int required_xp)
{
    if(!out)
        return;

    if(level >= PLAYER_SKILL_MAX_LEVEL)
        snprintf(out, 16, "MAX");
    else
        snprintf(out, 16, "%d/%d", current_xp, required_xp);
}

static void player_timestamp_now(char out[JOURNAL_TIMESTAMP_LENGTH])
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    if(!out)
        return;

    if(!tm_info)
    {
        snprintf(out, JOURNAL_TIMESTAMP_LENGTH, "unknown-time");
        return;
    }

    strftime(out, JOURNAL_TIMESTAMP_LENGTH, "%Y-%m-%d %H:%M", tm_info);
}

void player_apply_derived_maximums(Player* p)
{
    Actor* a;

    if(!p)
        return;

    a = &p->character.actor;

    a->max_health = actor_derived_max_health(a);
    a->max_stamina = actor_derived_max_stamina(a);
    a->max_action_points = actor_derived_max_action_points(a);
    a->max_mana = actor_derived_max_mana(a);
    a->max_willpower = actor_derived_max_willpower(a);

    if(a->health > a->max_health) a->health = a->max_health;
    a->stamina = actor_clamp_stamina_value(a, a->stamina);
    if(a->action_points < 0) a->action_points = 0;
    if(a->action_points > a->max_action_points) a->action_points = a->max_action_points;
    if(a->mana > a->max_mana) a->mana = a->max_mana;
    if(a->willpower > a->max_willpower) a->willpower = a->max_willpower;
}

void player_init_recovery(Player* p)
{
    if(!p)
        return;

    p->stamina_recovery_delay = 0;
    p->is_resting = 0;
    p->rest_turns_left = 0;
    p->is_sleeping = 0;
    p->sleep_turns_left = 0;
    p->skip_action_point_regen_turn = 0;
    p->exhaustion = 0;
}

void player_add_exhaustion(Player* p, int amount)
{
    if(!p || amount <= 0)
        return;

    p->exhaustion += amount;
    if(p->exhaustion > PLAYER_EXHAUSTION_MAX)
        p->exhaustion = PLAYER_EXHAUSTION_MAX;
}

void player_reduce_exhaustion(Player* p, int amount)
{
    if(!p || amount <= 0)
        return;

    p->exhaustion -= amount;
    if(p->exhaustion < 0)
        p->exhaustion = 0;
}

void player_clear_exhaustion(Player* p)
{
    if(!p)
        return;
    p->exhaustion = 0;
}

int player_exhaustion_surcharge(const Player* p)
{
    if(!p)
        return 0;
    return p->exhaustion;
}

int player_try_push_through_exhaustion(Player* p)
{
    if(!p)
        return 0;
    if(p->character.actor.willpower < 1)
        return 0;

    p->character.actor.willpower -= 1;
    return 1;
}

void player_attack_animation_clear(Player* p)
{
    if(!p)
        return;

    memset(&p->attack_animation, 0, sizeof(p->attack_animation));
    p->attack_animation.type = ATTACK_ANIM_NONE;
}

void player_attack_animation_start(Player* p,
                                  AttackAnimationType type,
                                  int origin_x,
                                  int origin_y,
                                  int origin_z,
                                  int target_x,
                                  int target_y,
                                  int target_z,
                                  int frame_max)
{
    if(!p)
        return;

    if(frame_max < 1)
        frame_max = 1;

    p->attack_animation.active = 1;
    p->attack_animation.type = type;
    p->attack_animation.origin_x = origin_x;
    p->attack_animation.origin_y = origin_y;
    p->attack_animation.origin_z = origin_z;
    p->attack_animation.target_x = target_x;
    p->attack_animation.target_y = target_y;
    p->attack_animation.target_z = target_z;
    p->attack_animation.frame = 0;
    p->attack_animation.frame_max = frame_max;
}

void player_attack_animation_advance(Player* p)
{
    if(!p || !p->attack_animation.active)
        return;

    p->attack_animation.frame++;
    if(p->attack_animation.frame >= p->attack_animation.frame_max)
        player_attack_animation_clear(p);
}

int player_attack_animation_active(const Player* p)
{
    return p && p->attack_animation.active;
}

void player_apply_stamina_cost(Player* p, int cost)
{
    int exhaustion_gain;

    if(!p || cost <= 0)
        return;

    p->character.actor.stamina -= cost;
    p->character.actor.stamina = actor_clamp_stamina_value(&p->character.actor, p->character.actor.stamina);

    p->stamina_recovery_delay = STAMINA_RECOVERY_DELAY_AFTER_STAMINA_USE;
    p->is_resting = 0;
    p->rest_turns_left = 0;
    p->is_sleeping = 0;
    p->sleep_turns_left = 0;

    exhaustion_gain = 0;
    if(cost >= EXHAUSTION_STAMINA_COST_GAIN_THRESHOLD)
    {
        exhaustion_gain = cost / EXHAUSTION_STAMINA_COST_DIVISOR;
        if(exhaustion_gain < 1)
            exhaustion_gain = 1;
    }

    if(exhaustion_gain > 0)
        player_add_exhaustion(p, exhaustion_gain);
}

void player_apply_action_point_cost(Player* p, int cost)
{
    if(!p || cost <= 0)
        return;

    p->character.actor.action_points -= cost;
    if(p->character.actor.action_points < 0)
        p->character.actor.action_points = 0;

    p->is_resting = 0;
    p->rest_turns_left = 0;
    p->is_sleeping = 0;
    p->sleep_turns_left = 0;
}

int player_action_point_regen_per_turn(const Player* p)
{
    if(!p)
        return 0;

    return p->character.actor.max_action_points;
}

static void player_recover_stamina(Player* p, int amount)
{
    if(!p || amount <= 0)
        return;

    p->character.actor.stamina += amount;
    p->character.actor.stamina = actor_clamp_stamina_value(&p->character.actor, p->character.actor.stamina);
}

int player_recover_action_points(Player* p, int amount)
{
    int before;

    if(!p || amount <= 0)
        return 0;

    before = p->character.actor.action_points;
    p->character.actor.action_points += amount;
    if(p->character.actor.action_points > p->character.actor.max_action_points)
        p->character.actor.action_points = p->character.actor.max_action_points;

    return p->character.actor.action_points - before;
}

int player_recover_action_points_from_stamina(Player* p, int stamina_cost, int ap_gain)
{
    int recovered;

    if(!p)
        return 0;
    if(stamina_cost < 1)
        stamina_cost = 1;
    if(ap_gain < 1)
        ap_gain = 1;

    if(p->character.actor.action_points >= p->character.actor.max_action_points)
    {
        log_add("Your action points are already full.");
        return 0;
    }

    if(p->character.actor.stamina < stamina_cost)
    {
        log_add("You are too exhausted to recover action points.");
        return 0;
    }

    player_apply_stamina_cost(p, stamina_cost);
    recovered = player_recover_action_points(p, ap_gain);
    log_add("You steady yourself and recover %d action point%s.",
            recovered,
            recovered == 1 ? "" : "s");
    return recovered > 0;
}

void player_recover_tick(Player* p, int in_combat)
{
    int ap_regen;

    if(!p)
        return;

    ap_regen = player_action_point_regen_per_turn(p);

    if(in_combat)
    {
        if(actor_is_unconscious(&p->character.actor))
        {
            int was_unconscious = 1;

            player_recover_stamina(p, 1);
            if(was_unconscious && !actor_is_unconscious(&p->character.actor))
                log_add("You regain consciousness.");

            if(p->stamina_recovery_delay > 0)
                p->stamina_recovery_delay--;
            return;
        }

        // Interrupted if resting or sleeping
        if(p->is_resting || p->is_sleeping)
        {
            p->is_resting = 0;
            p->rest_turns_left = 0;
            p->is_sleeping = 0;
            p->sleep_turns_left = 0;
            log_add("Your recovery is interrupted by nearby danger.");
        }

        if(p->stamina_recovery_delay > 0)
            p->stamina_recovery_delay--;

        return;
    }

    if(p->is_sleeping)
    {
        if(p->sleep_turns_left > 0)
        {
            int ap_recovered;
            int sleep_rate;

            sleep_rate = STAMINA_SLEEP_RECOVERY_RATE;
            if(player_is_sleeping_on_bed(p))
                sleep_rate += STAMINA_SLEEP_BED_BONUS_RATE;
            sleep_rate = player_adjust_stamina_recovery_for_exhaustion(p, sleep_rate, 1);

            player_recover_stamina(p, sleep_rate);
            ap_recovered = player_recover_action_points(p, ap_regen);
            p->sleep_turns_left--;

            if(ap_recovered > 0)
                log_add("You sleep and recover %d stamina and %d action points.", sleep_rate, ap_recovered);
            else
                log_add("You sleep and recover %d stamina.", sleep_rate);

            if(p->sleep_turns_left <= 0)
            {
                p->is_sleeping = 0;
                p->sleep_turns_left = 0;
                p->character.actor.health = p->character.actor.max_health;
                p->character.actor.mana = p->character.actor.max_mana;
                p->character.actor.willpower = p->character.actor.max_willpower;
                player_clear_exhaustion(p);
                log_add("You wake up feeling refreshed and free of exhaustion.");
            }
        }

        return;
    }

    if(p->is_resting)
    {
        if(p->rest_turns_left > 0)
        {
            int ap_recovered;
            int rest_rate;

            rest_rate = player_adjust_stamina_recovery_for_exhaustion(p, STAMINA_REST_RECOVERY_RATE, 0);

            player_recover_stamina(p, rest_rate);
            ap_recovered = player_recover_action_points(p, ap_regen);
            p->rest_turns_left--;

            if(ap_recovered > 0)
                log_add("You rest and recover %d stamina and %d action points.", rest_rate, ap_recovered);
            else
                log_add("You rest and recover %d stamina.", rest_rate);

            if(p->rest_turns_left <= 0)
            {
                p->is_resting = 0;
                player_reduce_exhaustion(p, EXHAUSTION_REST_RECOVERY_AMOUNT);
                log_add("You finish resting.");
            }
        }

        return;
    }

    if(p->stamina_recovery_delay > 0)
    {
        p->stamina_recovery_delay--;
    }
    else if(p->character.actor.stamina < p->character.actor.max_stamina)
    {
        int ap_recovered;
        int wait_rate;

        wait_rate = player_adjust_stamina_recovery_for_exhaustion(p, STAMINA_WAIT_RECOVERY_RATE, 0);

        player_recover_stamina(p, wait_rate);
        ap_recovered = player_recover_action_points(p, ap_regen);
        if(ap_recovered > 0)
            log_add("You recover %d stamina and %d action points from standing still.", wait_rate, ap_recovered);
        else
            log_add("You recover %d stamina from standing still.", wait_rate);
        return;
    }

    {
        int ap_recovered = player_recover_action_points(p, ap_regen);
        if(ap_recovered > 0)
            log_add("You regain %d action points.", ap_recovered);
    }
}

int player_start_rest(Player* p, int in_combat)
{
    int recovery;

    if(!p)
        return 0;
    if(actor_is_unconscious(&p->character.actor))
    {
        log_add("You are unconscious.");
        return 0;
    }
    if(in_combat)
    {
        log_add("It's too dangerous to rest while enemies are nearby.");
        return 0;
    }

    recovery = player_is_on_suitable_rest_furniture(p) ? REST_RECOVERY_FURNITURE : REST_RECOVERY_BASE;

    p->is_resting = 0;
    p->rest_turns_left = 0;
    p->is_sleeping = 0;
    p->sleep_turns_left = 0;
    p->stamina_recovery_delay = 0;

    player_forward_time_turns(p, REST_FORWARD_TURNS, REST_TURN_INTERVAL_MS, "Resting...");
    p->character.actor.stamina = actor_clamp_stamina_value(&p->character.actor,
                                                            p->character.actor.stamina + recovery);
    log_add("You rest for %d turns and recover %d stamina.", REST_FORWARD_TURNS, recovery);
    return 1;
}

int player_start_sleep(Player* p, int in_combat)
{
    int recovery;

    if(!p)
        return 0;
    if(actor_is_unconscious(&p->character.actor))
    {
        log_add("You are unconscious.");
        return 0;
    }
    if(in_combat)
    {
        log_add("You cannot sleep in combat.");
        return 0;
    }

    recovery = player_sleep_stamina_recovery_amount(p);

    p->is_sleeping = 0;
    p->sleep_turns_left = 0;
    p->is_resting = 0;
    p->rest_turns_left = 0;
    p->stamina_recovery_delay = 0;

    player_forward_time_turns(p, SLEEP_FORWARD_TURNS, SLEEP_TURN_INTERVAL_MS, "Sleeping...");
    p->character.actor.stamina = actor_clamp_stamina_value(&p->character.actor,
                                                            p->character.actor.stamina + recovery);
    log_add("You sleep for %d turns and recover %d stamina.", SLEEP_FORWARD_TURNS, recovery);
    return 1;
}

int player_wait(Player* p, int in_combat)
{
    int ap_regen;
    int ap_recovered = 0;

    if(!p)
        return 0;
    if(actor_is_unconscious(&p->character.actor))
    {
        log_add("You are unconscious.");
        return 0;
    }
    if(in_combat)
    {
        log_add("You cannot recover while in combat.");
        return 0;
    }

    ap_regen = player_action_point_regen_per_turn(p);

    if(p->stamina_recovery_delay > 0)
    {
        p->stamina_recovery_delay--;
        ap_recovered = player_recover_action_points(p, ap_regen);
        if(ap_recovered > 0)
            log_add("You take a moment to catch your breath and recover %d action points.", ap_recovered);
        else
            log_add("You take a moment to catch your breath.");
        return 1;
    }

    if(p->character.actor.stamina < p->character.actor.max_stamina)
    {
        int wait_rate;

        wait_rate = player_adjust_stamina_recovery_for_exhaustion(p, STAMINA_WAIT_RECOVERY_RATE, 0);
        player_recover_stamina(p, wait_rate);
        ap_recovered = player_recover_action_points(p, ap_regen);
        if(ap_recovered > 0)
            log_add("You stand still and recover %d stamina and %d action points.", wait_rate, ap_recovered);
        else
            log_add("You stand still and recover %d stamina.", wait_rate);
        return 1;
    }

    ap_recovered = player_recover_action_points(p, ap_regen);
    if(ap_recovered > 0)
    {
        log_add("You steady yourself and recover %d action points.", ap_recovered);
        return 1;
    }

    log_add("You are already at full stamina and action points.");
    return 1;
}

// Initialize all player fields, stats, and starter gear.
void player_create(Player* p, const char* name, const char* race_id, const Actor* rolled_attributes)
{
    if(!p)
        return;

    const RaceTemplate* player_race;

    memset(p, 0, sizeof(*p));
    strcpy(p->character.name, name);

    // Base stats come from selected race template (falls back to human/default).
    player_race = race_template_by_id(race_id);
    if(!player_race)
        player_race = race_template_by_id("human");
    if(!player_race)
        player_race = race_default_template();

    if(player_race)
    {
        race_apply_base_attributes(&p->character.actor, player_race);
    }
    else
    {
        p->character.actor.strength = 20;
        p->character.actor.constitution = 20;
        p->character.actor.endurance = 20;
        p->character.actor.agility = 20;
        p->character.actor.dexterity = 20;
        p->character.actor.speed = 20;
        p->character.actor.intellect = 20;
        p->character.actor.wisdom = 20;
        p->character.actor.resolve = 20;
        p->character.actor.composure = 20;
        p->character.actor.charisma = 20;
        p->character.actor.beauty = 20;
        p->character.actor.perception = 20;
        p->character.actor.wits = 20;
        snprintf(p->character.actor.race_id, sizeof(p->character.actor.race_id), "%s", "human");
    }

    if(rolled_attributes)
    {
        p->character.actor.strength = rolled_attributes->strength;
        p->character.actor.constitution = rolled_attributes->constitution;
        p->character.actor.endurance = rolled_attributes->endurance;
        p->character.actor.agility = rolled_attributes->agility;
        p->character.actor.dexterity = rolled_attributes->dexterity;
        p->character.actor.speed = rolled_attributes->speed;
        p->character.actor.intellect = rolled_attributes->intellect;
        p->character.actor.wisdom = rolled_attributes->wisdom;
        p->character.actor.resolve = rolled_attributes->resolve;
        p->character.actor.composure = rolled_attributes->composure;
        p->character.actor.charisma = rolled_attributes->charisma;
        p->character.actor.beauty = rolled_attributes->beauty;
        p->character.actor.perception = rolled_attributes->perception;
        p->character.actor.wits = rolled_attributes->wits;
        if(rolled_attributes->race_id[0] != '\0')
            snprintf(p->character.actor.race_id, sizeof(p->character.actor.race_id), "%s", rolled_attributes->race_id);
    }

    actor_ensure_base_attributes(&p->character.actor);

    player_apply_derived_maximums(p);
    p->character.actor.health = p->character.actor.max_health;
    p->character.actor.stamina = p->character.actor.max_stamina;
    p->character.actor.action_points = p->character.actor.max_action_points;
    p->character.actor.willpower = p->character.actor.max_willpower;
    p->character.actor.mana = p->character.actor.max_mana;
    for(int i = 0; i < WEAPON_SKILL_COUNT; ++i)
    {
        p->character.actor.weapon_skill[i] = 0;
        p->character.actor.weapon_skill_xp[i] = 0;
    }
    for(int i = 0; i < NON_WEAPON_SKILL_COUNT; ++i)
    {
        p->character.actor.non_weapon_skill[i] = 0;
        p->character.actor.non_weapon_skill_xp[i] = 0;
    }
    p->character.actor.armour_rating = 0;
    p->character.actor.hard_damage_reduction = 0;
    p->character.actor.soft_damage_reduction = 0;
    p->character.actor.dodge = 10;
    p->character.actor.block = 8;
    p->character.actor.parry = 6;
    actor_body_set_layout(&p->character.actor, ACTOR_BODY_LAYOUT_HUMANOID);
    (void)actor_body_distribute_health(&p->character.actor,
                                       p->character.actor.health,
                                       p->character.actor.max_health);

    player_init_recovery(p);

    // Map symbol and blocking
    p->character.actor.entity.symbol = '@';
    {
        const RaceTemplate* selected_race = race_template_by_id(p->character.actor.race_id);
        p->character.actor.entity.color = selected_race ? selected_race->glyph_color : RENDER_COLOR_LIGHT_CYAN;
    }
    p->character.actor.entity.blocks = 1;
    p->character.actor.entity.layer = TILE_LAYER_EFFECT;
    p->character.actor.entity.hide_below = 0;

    // Default position
    p->character.actor.entity.x = 0;
    p->character.actor.entity.y = 0;
    p->character.actor.entity.z = AREA_GROUND_Z;

    // Player-specific fields
    p->level = 1;
    p->experience = 0;
    p->gold = 0;
    p->selected_attack_mode = ATTACK_MODE_PUNCH;
    p->character.versatile_grip_mode = WEAPON_GRIP_ONE_HANDED;
    p->dragged_world_item_index = -1;
    target_lock_clear(p);

    (void)inventory_init(&p->character); // Return value ignored; add error handling if needed
    character_clear_recipe_unlocks(&p->character);
    crafting_compendium_init();

    // Give player a traveler's backpack in the backpack slot
    Item backpack = {0};
    item_init_from_template(&backpack, item_template_by_name("Traveler's Backpack"), -1, -1);
    for (int i = 0; i < p->character.equipment_slot_count; ++i) {
        if (p->character.equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_BACKPACK) {
            if(backpack.type != ITEM_TYPE_NONE)
                p->character.equipment_slots[i].item = backpack;
            break;
        }
    }
    update_dynamic_container_slots(&p->character);

    journal_init(p);
    p->playtime_seconds = 0ULL;
    player_timestamp_now(p->created_timestamp);
    snprintf(p->last_saved_timestamp, sizeof(p->last_saved_timestamp), "%s", p->created_timestamp);
    p->godmode = 0;
    p->travelling = 0;
    player_attack_animation_clear(p);

    // Give starter items: healing potion in inventory, starter clothing equipped
    player_add_starter_template(&p->character, "Healing Potion");
    player_add_starter_template(&p->character, "Surveyor's Atlas Page");
    player_add_starter_template(&p->character, "Bedroll");

    // Equip starter clothing directly using equipment_slots
    Item tmp = {0};
    item_init_from_template(&tmp, item_template_by_name("Linen Footwraps"), -1, -1);
    if(tmp.type != ITEM_TYPE_NONE)
        p->character.equipment_slots[EQUIP_SLOT_CLOTHING_FEET].item = tmp;
    item_init_from_template(&tmp, item_template_by_name("Linen Trousers"), -1, -1);
    if(tmp.type != ITEM_TYPE_NONE)
        p->character.equipment_slots[EQUIP_SLOT_CLOTHING_LEGS].item = tmp;
    item_init_from_template(&tmp, item_template_by_name("Linen Shirt"), -1, -1);
    if(tmp.type != ITEM_TYPE_NONE)
        p->character.equipment_slots[EQUIP_SLOT_CLOTHING_CHEST].item = tmp;
    item_init_from_template(&tmp, item_template_by_name("Linen Cloak"), -1, -1);
    if(tmp.type != ITEM_TYPE_NONE)
        p->character.equipment_slots[EQUIP_SLOT_CLOTHING_SHOULDERS].item = tmp;

}



// Set player position to explicit map coordinates.
void player_place(Player* p, int x, int y)
{
    p->character.actor.entity.x = x;
    p->character.actor.entity.y = y;
}

// Place player on a random free tile.
int player_place_random(Player* p)
{
    int area_width;
    int area_height;
    int attempts;

    if(!current_area)
        return 0;

    area_width = current_area->width;
    area_height = current_area->height;
    attempts = area_width * area_height;
    if(attempts < 200)
        attempts = 200;

    while(attempts--)
    {
        int x = rand() % area_width;
        int y = rand() % area_height;

        // Check if tile is free
        if(!is_blocked_3d(x, y, AREA_GROUND_Z, 0) && !bestiary_creature_at_3d(x, y, AREA_GROUND_Z))
        {
            p->character.actor.entity.z = AREA_GROUND_Z;
            player_place(p, x, y);
            return 1; // success
        }
    }

    log_add("Failed to place player on a free tile!");
    return 0; // failed
}

// Show complete character stats and weapon skills in an overlay.
static void player_character_sheet_add_line(char lines[][256],
                                            int* total_lines,
                                            const char* fmt,
                                            ...)
{
    va_list args;

    if(!lines || !total_lines || !fmt || *total_lines >= 64)
        return;

    va_start(args, fmt);
    vsnprintf(lines[*total_lines], 256, fmt, args);
    va_end(args);
    (*total_lines)++;
}

static void player_character_sheet_build_body_lines(const Player* p,
                                                    char lines[][256],
                                                    int* total_lines)
{
    const Actor* a = &p->character.actor;

    player_character_sheet_add_line(lines, total_lines, "%s  |  Level %d  XP %d  Gold %d",
                                    p->character.name, p->level, p->experience, p->gold);
    player_character_sheet_add_line(lines, total_lines, "%s", "");
    player_character_sheet_add_line(lines, total_lines, "Body / Paperdoll");
    player_character_sheet_add_line(lines, total_lines, "Layout: %s", actor_body_layout_name(a->body_layout));
    player_character_sheet_add_line(lines, total_lines, "Body HP Total: %d/%d",
                                    actor_body_total_health(a),
                                    actor_body_total_max_health(a));
    player_character_sheet_add_line(lines, total_lines, "%s", "");
    player_character_sheet_add_line(lines, total_lines, "%-12.12s %9.9s %4.4s %4.4s", "Body Part", "HP", "HR", "SR");
    player_character_sheet_add_line(lines, total_lines, "%s", "--------------------------------------");

    for(int part = 0; part < ACTOR_BODY_PART_COUNT; ++part)
    {
        if(!actor_body_part_is_active(a, (ActorBodyPart)part))
            continue;

        player_character_sheet_add_line(lines, total_lines,
                                        "%-12.12s %4d/%-4d %4d %4d",
                                        actor_body_part_name((ActorBodyPart)part),
                                        a->body_part_health[part],
                                        a->body_part_max_health[part],
                                        a->body_part_hard_damage_reduction[part],
                                        a->body_part_soft_damage_reduction[part]);
    }
}

void player_show_character_sheet(const Player* p)
{
    int key;
    CharacterSheetTab tab = CHARACTER_SHEET_TAB_SUMMARY;

    if(!p)
        return;

    // scroll_offset persists across redraws so the position is kept while
    // holding a key or switching overlays back to this sheet.
    int scroll_offset_summary = 0;
    int scroll_offset_body = 0;
    int scroll_offset_skills = 0;

    while(1)
    {
        const Character* c = &p->character;
        const Actor* a = &c->actor;
        const RaceTemplate* race = race_template_by_id(a->race_id);
        const char* race_name = race ? race->name : (a->race_id[0] ? a->race_id : "Unknown");
        CombatSummary summary = combat_summary_for_character(c, p->selected_attack_mode);
        char damage_text[32];
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        int visible_rows = (status_line > 0) ? (status_line - 1) : 0;

        int* scroll_offset = (tab == CHARACTER_SHEET_TAB_BODY)
            ? &scroll_offset_body
            : (tab == CHARACTER_SHEET_TAB_SKILLS)
                ? &scroll_offset_skills
                : &scroll_offset_summary;

        // Collect all logical lines into a buffer first, then do one windowed render.
#define CS_MAX_LINES 64
        char lines[CS_MAX_LINES][256];
        int total_lines = 0;
#define CS_ADD(fmt, ...) do { \
    if(total_lines < CS_MAX_LINES) \
        snprintf(lines[total_lines++], 256, fmt, ##__VA_ARGS__); \
} while(0)

        if(tab == CHARACTER_SHEET_TAB_BODY)
        {
            player_character_sheet_build_body_lines(p, lines, &total_lines);
        }
        else if(tab == CHARACTER_SHEET_TAB_SKILLS)
        {
            CS_ADD("Skills");
            CS_ADD("%s", "");

            CS_ADD("Weapon Skills");
            {
                static const WeaponSkillType ordered_weapon_skills[] = {
                    WEAPON_SKILL_AXE,
                    WEAPON_SKILL_AXE_2H,
                    WEAPON_SKILL_BOW,
                    WEAPON_SKILL_CROSSBOW,
                    WEAPON_SKILL_DAGGER,
                    WEAPON_SKILL_MACE,
                    WEAPON_SKILL_MACE_2H,
                    WEAPON_SKILL_POLEARM,
                    WEAPON_SKILL_SPEAR,
                    WEAPON_SKILL_SPEAR_2H,
                    WEAPON_SKILL_STAFF,
                    WEAPON_SKILL_SWORD,
                    WEAPON_SKILL_SWORD_2H,
                    WEAPON_SKILL_THROWN,
                    WEAPON_SKILL_UNARMED,
                };

                for(int i = 0; i < (int)(sizeof(ordered_weapon_skills) / sizeof(ordered_weapon_skills[0])); i += 2)
                {
                    WeaponSkillType left_skill = ordered_weapon_skills[i];
                    int left_level = actor_get_weapon_skill(a, left_skill);
                    int left_xp = actor_get_weapon_skill_xp(a, left_skill);
                    int left_required_xp = player_weapon_skill_xp_required_for_level(left_level);
                    char left_progress[16];

                    player_format_skill_xp_progress(left_progress, left_level, left_xp, left_required_xp);

                    if(i + 1 < (int)(sizeof(ordered_weapon_skills) / sizeof(ordered_weapon_skills[0])))
                    {
                        WeaponSkillType right_skill = ordered_weapon_skills[i + 1];
                        int right_level = actor_get_weapon_skill(a, right_skill);
                        int right_xp = actor_get_weapon_skill_xp(a, right_skill);
                        int right_required_xp = player_weapon_skill_xp_required_for_level(right_level);
                        char right_progress[16];

                        player_format_skill_xp_progress(right_progress, right_level, right_xp, right_required_xp);
                        CS_ADD("%-17.17s L%-2d XP %-7.7s   %-17.17s L%-2d XP %-7.7s",
                            weapon_skill_name(left_skill), left_level, left_progress,
                            weapon_skill_name(right_skill), right_level, right_progress);
                    }
                    else
                    {
                        CS_ADD("%-17.17s L%-2d XP %-7.7s",
                            weapon_skill_name(left_skill), left_level, left_progress);
                    }
                }
            }

            CS_ADD("%s", "");
            CS_ADD("Non-Weapon Skills");
            {
                static const NonWeaponSkillType ordered_non_weapon_skills[] = {
                    NON_WEAPON_SKILL_ALCHEMY,
                    NON_WEAPON_SKILL_ANIMAL_HANDLING,
                    NON_WEAPON_SKILL_BLACKSMITHING,
                    NON_WEAPON_SKILL_CARPENTRY,
                    NON_WEAPON_SKILL_COOKING,
                    NON_WEAPON_SKILL_FISHING,
                    NON_WEAPON_SKILL_HERBALISM,
                    NON_WEAPON_SKILL_LEATHERWORKING,
                    NON_WEAPON_SKILL_LUMBERJACKING,
                    NON_WEAPON_SKILL_MINING,
                    NON_WEAPON_SKILL_SKINNING,
                    NON_WEAPON_SKILL_SMELTING,
                    NON_WEAPON_SKILL_TAILORING,
                    NON_WEAPON_SKILL_TANNING,
                };

                for(int i = 0; i < (int)(sizeof(ordered_non_weapon_skills) / sizeof(ordered_non_weapon_skills[0])); i += 2)
                {
                    NonWeaponSkillType left_skill = ordered_non_weapon_skills[i];
                    int left_level = actor_get_non_weapon_skill(a, left_skill);
                    int left_xp = actor_get_non_weapon_skill_xp(a, left_skill);
                    int left_required_xp = player_non_weapon_skill_xp_required_for_level(left_level);
                    char left_progress[16];

                    player_format_skill_xp_progress(left_progress, left_level, left_xp, left_required_xp);

                    if(i + 1 < (int)(sizeof(ordered_non_weapon_skills) / sizeof(ordered_non_weapon_skills[0])))
                    {
                        NonWeaponSkillType right_skill = ordered_non_weapon_skills[i + 1];
                        int right_level = actor_get_non_weapon_skill(a, right_skill);
                        int right_xp = actor_get_non_weapon_skill_xp(a, right_skill);
                        int right_required_xp = player_non_weapon_skill_xp_required_for_level(right_level);
                        char right_progress[16];

                        player_format_skill_xp_progress(right_progress, right_level, right_xp, right_required_xp);
                        CS_ADD("%-17.17s L%-2d XP %-7.7s   %-17.17s L%-2d XP %-7.7s",
                            non_weapon_skill_name(left_skill), left_level, left_progress,
                            non_weapon_skill_name(right_skill), right_level, right_progress);
                    }
                    else
                    {
                        CS_ADD("%-17.17s L%-2d XP %-7.7s",
                            non_weapon_skill_name(left_skill), left_level, left_progress);
                    }
                }
            }
        }
        else
        {
            CS_ADD("%s  |  Level %d  XP %d  Gold %d", c->name, p->level, p->experience, p->gold);
            CS_ADD("%s", "");
            CS_ADD("Health: %d/%d    Stamina: %d/%d    AP: %d/%d",
                   a->health,
                   a->max_health,
                   a->stamina,
                   a->max_stamina,
                   a->action_points,
                   a->max_action_points);
            CS_ADD("Willpower: %d/%d  Mana: %d/%d", a->willpower, a->max_willpower, a->mana, a->max_mana);
            CS_ADD("Race: %s", race_name);
            CS_ADD("Exhaustion: %d", p->exhaustion);
            CS_ADD("Hard DR: %d  Soft DR: %d  Dodge: %d  Block: %d%%  Parry: %d%%",
                   a->hard_damage_reduction,
                   a->soft_damage_reduction,
                   a->dodge,
                   a->block,
                   a->parry);
            CS_ADD("STR %d CON %d END %d AGI %d DEX %d SPD %d", a->strength, a->constitution, a->endurance, a->agility, a->dexterity, a->speed);
            CS_ADD("INT %d WIS %d RSV %d CMP %d CHA %d", a->intellect, a->wisdom, a->resolve, a->composure, a->charisma);
            CS_ADD("BEA %d PER %d WIT %d", a->beauty, a->perception, a->wits);
            CS_ADD("%s", "");
            if(summary.damage_min == summary.damage_max)
                snprintf(damage_text, sizeof(damage_text), "%d", summary.damage_min);
            else
                snprintf(damage_text, sizeof(damage_text), "%d-%d", summary.damage_min, summary.damage_max);

            CS_ADD("Weapon: %s  Skill: %s %d", summary.weapon_name, weapon_skill_short_name(summary.skill_type), summary.skill_level);
            CS_ADD("Hit: %d%%  Crit: %d%%  Parry: %d%%  Damage: %s", summary.hit_chance, summary.crit_chance, summary.parry_chance, damage_text);
            {
                CombatProfile attack_profile = combat_profile_for_character_attack(c, p->selected_attack_mode);
                CS_ADD("Range: %d  AP Cost: %d  Armour Pen: %d",
                       combat_profile_melee_range(&attack_profile),
                       combat_profile_attack_action_point_cost(&attack_profile),
                       attack_profile.armour_penetration);
                CS_ADD("Attack Mode: %s  Damage Type: %s", attack_mode_name(summary.attack_mode), damage_type_name(summary.active_damage_type));
                if(attack_profile.next_unlock_mode != ATTACK_MODE_NONE)
                    CS_ADD("Next Unlock: %s at %s %d",
                           attack_mode_name(attack_profile.next_unlock_mode),
                           weapon_skill_short_name(summary.skill_type),
                           attack_profile.next_unlock_skill_level);
            }
        }

#undef CS_ADD

        // Clamp scroll offset now that we know the total.
        {
            int max_scroll = total_lines - visible_rows;
            if(max_scroll < 0) max_scroll = 0;
            if(*scroll_offset < 0) *scroll_offset = 0;
            if(*scroll_offset > max_scroll) *scroll_offset = max_scroll;
        }

        ui_overlay_draw_frame("Character Sheet");

        {
            char tab_line[160];
            snprintf(tab_line, sizeof(tab_line),
                     "Tabs: %c1.Summary  %c2.Body  %c3.Skills",
                     (tab == CHARACTER_SHEET_TAB_SUMMARY) ? '*' : ' ',
                     (tab == CHARACTER_SHEET_TAB_BODY) ? '*' : ' ',
                     (tab == CHARACTER_SHEET_TAB_SKILLS) ? '*' : ' ');
            ui_overlay_draw_line(0, tab_line);
        }

        // Render the visible window.
        for(int d = 0; d < visible_rows; d++)
        {
            int src = *scroll_offset + d;
            ui_overlay_draw_line(d + 1, src < total_lines ? lines[src] : "");
        }

        ui_overlay_draw_line(status_line, "A/D or <-/-> tabs | 1 summary | 2 body | 3 skills | ↑↓/PgUp/PgDn scroll | Esc/Q close | i inventory | u character | l log | j journal");
        ui_overlay_draw_global_hotkeys();

#undef CS_MAX_LINES

        key = read_input_key();
        if(key == 'q' || key == 'Q' || key == 27)
            break;

        if(key == '1')
        {
            tab = CHARACTER_SHEET_TAB_SUMMARY;
            continue;
        }
        if(key == '2')
        {
            tab = CHARACTER_SHEET_TAB_BODY;
            continue;
        }
        if(key == '3')
        {
            tab = CHARACTER_SHEET_TAB_SKILLS;
            continue;
        }
        if(key == 'a' || key == 'A' || key == INPUT_KEY_LEFT)
        {
            tab = (CharacterSheetTab)((tab + CHARACTER_SHEET_TAB_COUNT - 1) % CHARACTER_SHEET_TAB_COUNT);
            continue;
        }
        if(key == 'd' || key == 'D' || key == INPUT_KEY_RIGHT)
        {
            tab = (CharacterSheetTab)((tab + 1) % CHARACTER_SHEET_TAB_COUNT);
            continue;
        }

        // Scroll keys — adjust offset and redraw without treating as a command.
        if(key == INPUT_KEY_UP || key == INPUT_KEY_DOWN ||
           key == INPUT_KEY_PGUP || key == INPUT_KEY_PGDN ||
           key == INPUT_KEY_HOME || key == INPUT_KEY_END)
        {
            int max_scroll = total_lines - visible_rows;
            if(max_scroll < 0) max_scroll = 0;
            if(key == INPUT_KEY_UP)        (*scroll_offset)--;
            else if(key == INPUT_KEY_DOWN) (*scroll_offset)++;
            else if(key == INPUT_KEY_PGUP) (*scroll_offset) -= 5;
            else if(key == INPUT_KEY_PGDN) (*scroll_offset) += 5;
            else if(key == INPUT_KEY_HOME) *scroll_offset = 0;
            else if(key == INPUT_KEY_END)  *scroll_offset = max_scroll;
            if(*scroll_offset < 0) *scroll_offset = 0;
            if(*scroll_offset > max_scroll) *scroll_offset = max_scroll;
            continue;
        }

        {
            OverlayType next_overlay;
            if(overlay_type_from_key(key, &next_overlay) && next_overlay != OVERLAY_TYPE_CHARACTER)
            {
                overlay_request(next_overlay);
                break;
            }
        }
    }
}

// Legacy direct-input movement handler (kept for compatibility).
void player_handle_input()
{
    int nx = player.character.actor.entity.x;
    int ny = player.character.actor.entity.y;

    int c = read_input_key();
    switch(c)
    {
        case 'w': ny--; break;
        case 's': ny++; break;
        case 'a': nx--; break;
        case 'd': nx++; break;
        case 'q': exit(0); break;
    }

    if(is_blocked(nx, ny, 0))
        return;

    player.character.actor.entity.x = nx;
    player.character.actor.entity.y = ny;
}


