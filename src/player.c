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
#include <conio.h> // for _getch()
#include <stdlib.h> // optional, for exit()

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
    actor_ensure_base_attributes(&p->character.actor);

    p->character.actor.health = 20;
    p->character.actor.max_health = 20;
    p->character.actor.stamina = 12;
    p->character.actor.max_stamina = 12;
    p->character.actor.max_willpower = actor_derived_max_willpower(&p->character.actor);
    p->character.actor.willpower = p->character.actor.max_willpower;
    p->character.actor.mana = 8;
    p->character.actor.max_mana = 8;
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

    // Map symbol and blocking
    p->character.actor.entity.symbol = '@';
    p->character.actor.entity.color = RENDER_COLOR_LIGHT_CYAN;
    p->character.actor.entity.blocks = 1;

    // Default position
    p->character.actor.entity.x = 0;
    p->character.actor.entity.y = 0;

    // Player-specific fields
    p->level = 1;
    p->experience = 0;
    p->gold = 0;
    p->selected_attack_mode = ATTACK_MODE_PUNCH;
    target_lock_clear(p);
    inventory_init(&p->character);
    journal_init(p);

    // Give starter items: healing potion in inventory, starter clothing equipped, and belt pouch equipped.
    player_add_starter_template(&p->character, "Healing Potion");

    // Equip starter clothing directly
    Item tmp;
    item_init_from_template(&tmp, item_template_by_name("Linen Footwraps"), -1, -1);
    p->character.equipped_clothing_feet = tmp;

    item_init_from_template(&tmp, item_template_by_name("Linen Trousers"), -1, -1);
    p->character.equipped_clothing_legs = tmp;

    item_init_from_template(&tmp, item_template_by_name("Linen Shirt"), -1, -1);
    p->character.equipped_clothing_chest = tmp;

    item_init_from_template(&tmp, item_template_by_name("Linen Cloak"), -1, -1);
    p->character.equipped_clothing_shoulders = tmp;

    item_init_from_template(&tmp, item_template_by_name("Small Linen Pouch"), -1, -1);
    p->character.equipped_bag_beltpouch = tmp;

    p->character.beltpouch_count = 0;
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
        if(!is_blocked(x, y, 0) && !bestiary_creature_at(x, y))
        {
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
    char line[256];
    int key;

    if(!p)
        return;

    while(1)
    {
        const Character* c = &p->character;
        const Actor* a = &c->actor;
        CombatSummary summary = combat_summary_for_character(c, p->selected_attack_mode);
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        int line_i = 0;

        ui_overlay_draw_frame("Character Sheet");

        snprintf(line, sizeof(line), "%s  |  Level %d  XP %d  Gold %d", c->name, p->level, p->experience, p->gold);
        if(line_i < status_line) ui_overlay_draw_line(line_i++, line);

        if(line_i < status_line) ui_overlay_draw_line(line_i++, "");

        snprintf(line, sizeof(line), "Health: %d/%d    Stamina: %d/%d", a->health, a->max_health, a->stamina, a->max_stamina);
        if(line_i < status_line) ui_overlay_draw_line(line_i++, line);

        snprintf(line, sizeof(line), "Willpower: %d/%d  Mana: %d/%d", a->willpower, a->max_willpower, a->mana, a->max_mana);
        if(line_i < status_line) ui_overlay_draw_line(line_i++, line);

        snprintf(line, sizeof(line), "Armor: %d  Dodge: %d  Block: %d%%  Parry: %d%%", a->armor_rating, a->dodge, a->block, a->parry);
        if(line_i < status_line) ui_overlay_draw_line(line_i++, line);

        snprintf(line, sizeof(line), "STR %d CON %d END %d AGI %d DEX %d SPD %d", a->strength, a->constitution, a->endurance, a->agility, a->dexterity, a->speed);
        if(line_i < status_line) ui_overlay_draw_line(line_i++, line);

        snprintf(line, sizeof(line), "INT %d WIS %d RSV %d CMP %d CHA %d BEA %d", a->intellect, a->wisdom, a->resolve, a->composure, a->charisma, a->beauty);
        if(line_i < status_line) ui_overlay_draw_line(line_i++, line);

        if(line_i < status_line) ui_overlay_draw_line(line_i++, "");

        snprintf(line, sizeof(line), "Weapon: %s  Skill: %s %d", summary.weapon_name, weapon_skill_short_name(summary.skill_type), summary.skill_level);
        if(line_i < status_line) ui_overlay_draw_line(line_i++, line);

        snprintf(line, sizeof(line), "Hit: %d%%  Crit: %d%%  Parry: %d%%  Damage: %d", summary.hit_chance, summary.crit_chance, summary.parry_chance, summary.damage);
        if(line_i < status_line) ui_overlay_draw_line(line_i++, line);

        snprintf(line, sizeof(line), "Attack Mode: %s  Damage Type: %s", attack_mode_name(summary.attack_mode), damage_type_name(summary.active_damage_type));
        if(line_i < status_line) ui_overlay_draw_line(line_i++, line);

        if(line_i < status_line) ui_overlay_draw_line(line_i++, "");
        if(line_i < status_line) ui_overlay_draw_line(line_i++, "Weapon Skills");

        for(int i = 0; i < WEAPON_SKILL_COUNT && line_i < status_line; i += 2)
        {
            int left_level = actor_get_weapon_skill(a, (WeaponSkillType)i);
            int left_xp = actor_get_weapon_skill_xp(a, (WeaponSkillType)i);

            if(i + 1 < WEAPON_SKILL_COUNT)
            {
                int right_level = actor_get_weapon_skill(a, (WeaponSkillType)(i + 1));
                int right_xp = actor_get_weapon_skill_xp(a, (WeaponSkillType)(i + 1));

                snprintf(
                    line,
                    sizeof(line),
                    "%-17.17s L%-2d XP%-3d   %-17.17s L%-2d XP%-3d",
                    weapon_skill_name((WeaponSkillType)i),
                    left_level,
                    left_xp,
                    weapon_skill_name((WeaponSkillType)(i + 1)),
                    right_level,
                    right_xp
                );
            }
            else
            {
                snprintf(
                    line,
                    sizeof(line),
                    "%-17.17s L%-2d XP%-3d",
                    weapon_skill_name((WeaponSkillType)i),
                    left_level,
                    left_xp
                );
            }

            ui_overlay_draw_line(line_i++, line);
        }

        while(line_i < status_line)
            ui_overlay_draw_line(line_i++, "");

        ui_overlay_draw_line(status_line, "Esc/Q close | i inventory | c character | m log | j journal");
        ui_overlay_draw_global_hotkeys();

        key = read_input_key();
        if(key == 'q' || key == 'Q' || key == 27)
            break;

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

    char c = _getch();
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

