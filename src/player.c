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
#include "target_lock.h"
#include "ui_overlay.h"
#include "crafting_compendium.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // optional, for exit()
#include <time.h>

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
#define STAMINA_RECOVERY_DELAY_AFTER_STAMINA_USE 3
#define STAMINA_WAIT_RECOVERY_RATE 1
#define STAMINA_REST_RECOVERY_RATE 2
#define STAMINA_SLEEP_RECOVERY_RATE 5
#define STAMINA_REST_TURNS 4
#define STAMINA_SLEEP_TURNS 8
#define PLAYER_EXHAUSTION_MAX 100
#define PLAYER_SKILL_MAX_LEVEL 99

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
    if(a->stamina > a->max_stamina) a->stamina = a->max_stamina;
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
    if(!p || cost <= 0)
        return;

    p->character.actor.stamina -= cost;
    if(p->character.actor.stamina < 0)
        p->character.actor.stamina = 0;

    p->stamina_recovery_delay = STAMINA_RECOVERY_DELAY_AFTER_STAMINA_USE;
    p->is_resting = 0;
    p->rest_turns_left = 0;
    p->is_sleeping = 0;
    p->sleep_turns_left = 0;
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
    int speed;
    int wits;
    int regen;

    if(!p)
        return 2;

    speed = actor_attr_clamp(p->character.actor.speed);
    wits = actor_attr_clamp(p->character.actor.wits);
    regen = 2 + (((speed - 20) + (wits - 20)) / 60);
    if(regen < 1)
        regen = 1;
    if(regen > 4)
        regen = 4;
    return regen;
}

static void player_recover_stamina(Player* p, int amount)
{
    if(!p || amount <= 0)
        return;

    p->character.actor.stamina += amount;
    if(p->character.actor.stamina > p->character.actor.max_stamina)
        p->character.actor.stamina = p->character.actor.max_stamina;
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

            player_recover_stamina(p, STAMINA_SLEEP_RECOVERY_RATE);
            ap_recovered = player_recover_action_points(p, ap_regen);
            p->sleep_turns_left--;

            if(ap_recovered > 0)
                log_add("You sleep and recover %d stamina and %d action points.", STAMINA_SLEEP_RECOVERY_RATE, ap_recovered);
            else
                log_add("You sleep and recover %d stamina.", STAMINA_SLEEP_RECOVERY_RATE);

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

            player_recover_stamina(p, STAMINA_REST_RECOVERY_RATE);
            ap_recovered = player_recover_action_points(p, ap_regen);
            p->rest_turns_left--;

            if(ap_recovered > 0)
                log_add("You rest and recover %d stamina and %d action points.", STAMINA_REST_RECOVERY_RATE, ap_recovered);
            else
                log_add("You rest and recover %d stamina.", STAMINA_REST_RECOVERY_RATE);

            if(p->rest_turns_left <= 0)
            {
                p->is_resting = 0;
                player_reduce_exhaustion(p, 1);
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

        player_recover_stamina(p, STAMINA_WAIT_RECOVERY_RATE);
        ap_recovered = player_recover_action_points(p, ap_regen);
        if(ap_recovered > 0)
            log_add("You recover %d stamina and %d action points from standing still.", STAMINA_WAIT_RECOVERY_RATE, ap_recovered);
        else
            log_add("You recover %d stamina from standing still.", STAMINA_WAIT_RECOVERY_RATE);
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
    if(!p)
        return 0;
    if(in_combat)
    {
        log_add("It's too dangerous to rest while enemies are nearby.");
        return 0;
    }
    p->is_resting = 1;
    p->rest_turns_left = STAMINA_REST_TURNS;
    p->is_sleeping = 0;
    p->sleep_turns_left = 0;
    p->stamina_recovery_delay = 0;
    log_add("You sit down to rest.");
    return 1;
}

int player_start_sleep(Player* p, int in_combat)
{
    if(!p)
        return 0;
    if(in_combat)
    {
        log_add("You cannot sleep in combat.");
        return 0;
    }
    p->is_sleeping = 1;
    p->sleep_turns_left = STAMINA_SLEEP_TURNS;
    p->is_resting = 0;
    p->rest_turns_left = 0;
    p->stamina_recovery_delay = 0;
    log_add("You lie down to sleep.");
    return 1;
}

int player_wait(Player* p, int in_combat)
{
    int ap_regen;
    int ap_recovered = 0;

    if(!p)
        return 0;
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
        player_recover_stamina(p, STAMINA_WAIT_RECOVERY_RATE);
        ap_recovered = player_recover_action_points(p, ap_regen);
        if(ap_recovered > 0)
            log_add("You stand still and recover %d stamina and %d action points.", STAMINA_WAIT_RECOVERY_RATE, ap_recovered);
        else
            log_add("You stand still and recover %d stamina.", STAMINA_WAIT_RECOVERY_RATE);
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
void player_create(Player* p, const char* name, const char* race_id)
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
    p->character.actor.armor_rating = 2;
    p->character.actor.dodge = 10;
    p->character.actor.block = 8;
    p->character.actor.parry = 6;

    player_init_recovery(p);

    // Map symbol and blocking
    p->character.actor.entity.symbol = '@';
    p->character.actor.entity.color = RENDER_COLOR_LIGHT_CYAN;
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
void player_show_character_sheet(const Player* p)
{
    int key;

    if(!p)
        return;

    // scroll_offset persists across redraws so the position is kept while
    // holding a key or switching overlays back to this sheet.
    int scroll_offset = 0;

    while(1)
    {
        const Character* c = &p->character;
        const Actor* a = &c->actor;
        const RaceTemplate* race = race_template_by_id(a->race_id);
        const char* race_name = race ? race->name : (a->race_id[0] ? a->race_id : "Unknown");
        CombatSummary summary = combat_summary_for_character(c, p->selected_attack_mode);
        char damage_text[32];
        int content_lines = ui_overlay_content_lines();
        int visible_rows = (content_lines > 2) ? (content_lines - 2) : 0;
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;

        // Collect all logical lines into a buffer first, then do one windowed render.
#define CS_MAX_LINES 64
        char lines[CS_MAX_LINES][256];
        int total_lines = 0;
#define CS_ADD(fmt, ...) do { \
    if(total_lines < CS_MAX_LINES) \
        snprintf(lines[total_lines++], 256, fmt, ##__VA_ARGS__); \
} while(0)

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
        CS_ADD("Armor: %d  Dodge: %d  Block: %d%%  Parry: %d%%", a->armor_rating, a->dodge, a->block, a->parry);
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
            CS_ADD("Range: %d  AP Cost: %d  Armor Pen: %d",
                   combat_profile_melee_range(&attack_profile),
                   combat_profile_attack_action_point_cost(&attack_profile),
                   attack_profile.armor_penetration);
            CS_ADD("Attack Mode: %s  Damage Type: %s", attack_mode_name(summary.attack_mode), damage_type_name(summary.active_damage_type));
            if(attack_profile.next_unlock_mode != ATTACK_MODE_NONE)
                CS_ADD("Next Unlock: %s at %s %d",
                       attack_mode_name(attack_profile.next_unlock_mode),
                       weapon_skill_short_name(summary.skill_type),
                       attack_profile.next_unlock_skill_level);
        }
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

#undef CS_ADD

        // Clamp scroll offset now that we know the total.
        {
            int max_scroll = total_lines - visible_rows;
            if(max_scroll < 0) max_scroll = 0;
            if(scroll_offset < 0) scroll_offset = 0;
            if(scroll_offset > max_scroll) scroll_offset = max_scroll;
        }

        ui_overlay_draw_frame("Character Sheet");

        // Render the visible window.
        for(int d = 0; d < visible_rows; d++)
        {
            int src = scroll_offset + d;
            ui_overlay_draw_line(d, src < total_lines ? lines[src] : "");
        }

        ui_overlay_draw_line(status_line, "↑↓/PgUp/PgDn scroll | Esc/Q close | i inventory | c character | l log | j journal");
        ui_overlay_draw_global_hotkeys();

#undef CS_MAX_LINES

        key = read_input_key();
        if(key == 'q' || key == 'Q' || key == 27)
            break;

        // Scroll keys — adjust offset and redraw without treating as a command.
        if(key == INPUT_KEY_UP || key == INPUT_KEY_DOWN ||
           key == INPUT_KEY_PGUP || key == INPUT_KEY_PGDN ||
           key == INPUT_KEY_HOME || key == INPUT_KEY_END)
        {
            int max_scroll = total_lines - visible_rows;
            if(max_scroll < 0) max_scroll = 0;
            if(key == INPUT_KEY_UP)        scroll_offset--;
            else if(key == INPUT_KEY_DOWN) scroll_offset++;
            else if(key == INPUT_KEY_PGUP) scroll_offset -= 5;
            else if(key == INPUT_KEY_PGDN) scroll_offset += 5;
            else if(key == INPUT_KEY_HOME) scroll_offset = 0;
            else if(key == INPUT_KEY_END)  scroll_offset = max_scroll;
            if(scroll_offset < 0) scroll_offset = 0;
            if(scroll_offset > max_scroll) scroll_offset = max_scroll;
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


