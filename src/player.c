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
#include "input.h"
#include "target_lock.h"
#include "ui_overlay.h"

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
#define PLAYER_OVERLAND_EXHAUSTION_MAX 100

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

    a->max_health   = actor_derived_max_health(a);
    a->max_stamina  = actor_derived_max_stamina(a);
    a->max_mana     = actor_derived_max_mana(a);
    a->max_willpower = actor_derived_max_willpower(a);

    if(a->health   > a->max_health)   a->health   = a->max_health;
    if(a->stamina  > a->max_stamina)  a->stamina  = a->max_stamina;
    if(a->mana     > a->max_mana)     a->mana     = a->max_mana;
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
    p->overland_exhaustion = 0;
}

void player_add_overland_exhaustion(Player* p, int amount)
{
    if(!p || amount <= 0)
        return;

    p->overland_exhaustion += amount;
    if(p->overland_exhaustion > PLAYER_OVERLAND_EXHAUSTION_MAX)
        p->overland_exhaustion = PLAYER_OVERLAND_EXHAUSTION_MAX;
}

void player_reduce_overland_exhaustion(Player* p, int amount)
{
    if(!p || amount <= 0)
        return;

    p->overland_exhaustion -= amount;
    if(p->overland_exhaustion < 0)
        p->overland_exhaustion = 0;
}

void player_clear_overland_exhaustion(Player* p)
{
    if(!p)
        return;
    p->overland_exhaustion = 0;
}

int player_overland_exhaustion_surcharge(const Player* p)
{
    if(!p)
        return 0;
    return p->overland_exhaustion;
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

static void player_recover_stamina(Player* p, int amount)
{
    if(!p || amount <= 0)
        return;

    p->character.actor.stamina += amount;
    if(p->character.actor.stamina > p->character.actor.max_stamina)
        p->character.actor.stamina = p->character.actor.max_stamina;
}

void player_recover_tick(Player* p, int in_combat)
{
    if(!p)
        return;

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
            player_recover_stamina(p, STAMINA_SLEEP_RECOVERY_RATE);
            p->sleep_turns_left--;
            log_add("You sleep and recover %d stamina.", STAMINA_SLEEP_RECOVERY_RATE);

            if(p->sleep_turns_left <= 0)
            {
                p->is_sleeping = 0;
                p->sleep_turns_left = 0;
                p->character.actor.health = p->character.actor.max_health;
                p->character.actor.mana = p->character.actor.max_mana;
                p->character.actor.willpower = p->character.actor.max_willpower;
                player_clear_overland_exhaustion(p);
                log_add("You wake up feeling refreshed.");
            }
        }

        return;
    }

    if(p->is_resting)
    {
        if(p->rest_turns_left > 0)
        {
            player_recover_stamina(p, STAMINA_REST_RECOVERY_RATE);
            p->rest_turns_left--;
            log_add("You rest and recover %d stamina.", STAMINA_REST_RECOVERY_RATE);

            if(p->rest_turns_left <= 0)
            {
                p->is_resting = 0;
                player_reduce_overland_exhaustion(p, 1);
                log_add("You finish resting.");
            }
        }

        return;
    }

    if(p->stamina_recovery_delay > 0)
    {
        p->stamina_recovery_delay--;
        return;
    }

    if(p->character.actor.stamina < p->character.actor.max_stamina)
    {
        player_recover_stamina(p, STAMINA_WAIT_RECOVERY_RATE);
        log_add("You recover %d stamina from standing still.", STAMINA_WAIT_RECOVERY_RATE);
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
    if(!p)
        return 0;
    if(in_combat)
    {
        log_add("You cannot recover while in combat.");
        return 0;
    }

    if(p->stamina_recovery_delay > 0)
    {
        p->stamina_recovery_delay--;
        log_add("You take a moment to catch your breath.");
        return 1;
    }

    if(p->character.actor.stamina < p->character.actor.max_stamina)
    {
        player_recover_stamina(p, STAMINA_WAIT_RECOVERY_RATE);
        log_add("You stand still and recover %d stamina.", STAMINA_WAIT_RECOVERY_RATE);
        return 1;
    }

    log_add("You are already at full stamina.");
    return 1;
}

// Initialize all player fields, stats, and starter gear.
void player_create(Player* p, const char* name)
{
    if(!p)
        return;

    memset(p, 0, sizeof(*p));
    strcpy(p->character.name, name);

    // Base stats
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
    actor_ensure_base_attributes(&p->character.actor);

    player_apply_derived_maximums(p);
    p->character.actor.health = p->character.actor.max_health;
    p->character.actor.stamina = p->character.actor.max_stamina;
    p->character.actor.willpower = p->character.actor.max_willpower;
    p->character.actor.mana = p->character.actor.max_mana;
    p->character.actor.weapon_skill[WEAPON_SKILL_UNARMED] = 4;
    p->character.actor.weapon_skill[WEAPON_SKILL_DAGGER] = 4;
    p->character.actor.weapon_skill[WEAPON_SKILL_SWORD] = 5;
    p->character.actor.weapon_skill[WEAPON_SKILL_AXE] = 3;
    p->character.actor.weapon_skill[WEAPON_SKILL_MACE] = 3;
    p->character.actor.weapon_skill[WEAPON_SKILL_SPEAR] = 4;
    p->character.actor.weapon_skill[WEAPON_SKILL_STAFF] = 2;
    p->character.actor.weapon_skill[WEAPON_SKILL_POLEARM] = 2;
    p->character.actor.armor_rating = 2;
    p->character.actor.dodge = 10;
    p->character.actor.block = 8;
    p->character.actor.parry = 6;

    player_init_recovery(p);

    // Map symbol and blocking
    p->character.actor.entity.symbol = '@';
    p->character.actor.entity.color = RENDER_COLOR_LIGHT_CYAN;
    p->character.actor.entity.blocks = 1;
    p->character.actor.entity.layer = TILE_LAYER_UNIT;
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
    target_lock_clear(p);
    (void)inventory_init(&p->character); // Return value ignored; add error handling if needed
    journal_init(p);
    p->playtime_seconds = 0ULL;
    player_timestamp_now(p->created_timestamp);
    snprintf(p->last_saved_timestamp, sizeof(p->last_saved_timestamp), "%s", p->created_timestamp);
    p->godmode = 0;
    p->travelling = 0;
    player_attack_animation_clear(p);

    // Give starter items: healing potion in inventory, starter clothing equipped, and belt pouch equipped.
    player_add_starter_template(&p->character, "Healing Potion");
    player_add_starter_template(&p->character, "Surveyor's Atlas Page");
    player_add_starter_template(&p->character, "Bedroll");

    // Equip starter clothing directly using equipment_slots
    Item tmp;
    item_init_from_template(&tmp, item_template_by_name("Linen Footwraps"), -1, -1);

    p->character.equipment_slots[EQUIP_SLOT_CLOTHING_FEET].item = tmp;

    item_init_from_template(&tmp, item_template_by_name("Linen Trousers"), -1, -1);
    p->character.equipment_slots[EQUIP_SLOT_CLOTHING_LEGS].item = tmp;

    item_init_from_template(&tmp, item_template_by_name("Linen Shirt"), -1, -1);
    p->character.equipment_slots[EQUIP_SLOT_CLOTHING_CHEST].item = tmp;

    item_init_from_template(&tmp, item_template_by_name("Linen Cloak"), -1, -1);
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
        CombatSummary summary = combat_summary_for_character(c, p->selected_attack_mode);
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
        CS_ADD("Health: %d/%d    Stamina: %d/%d", a->health, a->max_health, a->stamina, a->max_stamina);
        CS_ADD("Willpower: %d/%d  Mana: %d/%d", a->willpower, a->max_willpower, a->mana, a->max_mana);
        CS_ADD("Overland Exhaustion: %d", p->overland_exhaustion);
        CS_ADD("Armor: %d  Dodge: %d  Block: %d%%  Parry: %d%%", a->armor_rating, a->dodge, a->block, a->parry);
        CS_ADD("STR %d CON %d END %d AGI %d DEX %d SPD %d", a->strength, a->constitution, a->endurance, a->agility, a->dexterity, a->speed);
        CS_ADD("INT %d WIS %d RSV %d CMP %d CHA %d", a->intellect, a->wisdom, a->resolve, a->composure, a->charisma);
        CS_ADD("BEA %d PER %d WIT %d", a->beauty, a->perception, a->wits);
        CS_ADD("%s", "");
        CS_ADD("Weapon: %s  Skill: %s %d", summary.weapon_name, weapon_skill_short_name(summary.skill_type), summary.skill_level);
        CS_ADD("Hit: %d%%  Crit: %d%%  Parry: %d%%  Damage: %d", summary.hit_chance, summary.crit_chance, summary.parry_chance, summary.damage);
        {
            CombatProfile attack_profile = combat_profile_for_character_attack(c, p->selected_attack_mode);
            CS_ADD("Range: %d  Swing Cost: %d  Armor Pen: %d", combat_profile_melee_range(&attack_profile), combat_profile_attack_stamina_cost(&attack_profile), attack_profile.armor_penetration);
        }
        CS_ADD("Attack Mode: %s  Damage Type: %s", attack_mode_name(summary.attack_mode), damage_type_name(summary.active_damage_type));
        CS_ADD("%s", "");
        CS_ADD("Weapon Skills");

        for(int i = 0; i < WEAPON_SKILL_COUNT; i += 2)
        {
            int left_level = actor_get_weapon_skill(a, (WeaponSkillType)i);
            int left_xp = actor_get_weapon_skill_xp(a, (WeaponSkillType)i);

            if(i + 1 < WEAPON_SKILL_COUNT)
            {
                int right_level = actor_get_weapon_skill(a, (WeaponSkillType)(i + 1));
                int right_xp = actor_get_weapon_skill_xp(a, (WeaponSkillType)(i + 1));
                CS_ADD("%-17.17s L%-2d XP%-3d   %-17.17s L%-2d XP%-3d",
                    weapon_skill_name((WeaponSkillType)i), left_level, left_xp,
                    weapon_skill_name((WeaponSkillType)(i + 1)), right_level, right_xp);
            }
            else
            {
                CS_ADD("%-17.17s L%-2d XP%-3d",
                    weapon_skill_name((WeaponSkillType)i), left_level, left_xp);
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

        ui_overlay_draw_line(status_line, "\u2191\u2193/PgUp/PgDn scroll | Esc/Q close | i inventory | c character | m log | j journal");
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


