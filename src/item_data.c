#include <stddef.h>
#include <string.h>
#include "item_data.h"
#include "item.h"

/*
 * Purpose:
 *   Defines static item templates and template-to-instance conversion helpers.
 *
 * Functions:
 *   - item_template_by_name: fetches template by item name.
 *   - item_init_from_template: initializes one item from template values.
 */

const ItemTemplate item_templates[] = {
    {
        .name = "Healing Potion",
        .symbol = '!',
        .type = ITEM_TYPE_CONSUMABLE,
        .stackable = 1,
        .quantity = 1,
        .power = 10,
    },
    {
        .name = "Rusty Dagger",
        .symbol = '/',
        .type = ITEM_TYPE_WEAPON_ONE_HANDED,
        .stackable = 0,
        .quantity = 1,
        .power = 2,
        .weapon_skill_type = WEAPON_SKILL_DAGGER,
        .accuracy_bonus = 6,
        .crit_bonus = 8,
        .parry_bonus = 2,
        .can_parry = 1,
        .damage_type_mask = DAMAGE_TYPE_PIERCING,
        .attack_mode_mask = ATTACK_MODE_FLAG_STAB,
    },
    {
        .name = "Rusty Sword",
        .symbol = '/',
        .type = ITEM_TYPE_WEAPON_ONE_HANDED,
        .stackable = 0,
        .quantity = 1,
        .power = 3,
        .weapon_skill_type = WEAPON_SKILL_SWORD,
        .accuracy_bonus = 4,
        .crit_bonus = 4,
        .parry_bonus = 5,
        .can_parry = 1,
        .damage_type_mask = DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_SLASHING,
        .attack_mode_mask = ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_CUT,
    },
    {
        .name = "Hatchet",
        .symbol = '/',
        .type = ITEM_TYPE_WEAPON_MAIN_HAND,
        .stackable = 0,
        .quantity = 1,
        .power = 4,
        .weapon_skill_type = WEAPON_SKILL_AXE,
        .accuracy_bonus = 1,
        .crit_bonus = 5,
        .parry_bonus = 1,
        .can_parry = 0,
        .damage_type_mask = DAMAGE_TYPE_SLASHING,
        .attack_mode_mask = ATTACK_MODE_FLAG_CUT,
    },
    {
        .name = "Iron Mace",
        .symbol = '/',
        .type = ITEM_TYPE_WEAPON_MAIN_HAND,
        .stackable = 0,
        .quantity = 1,
        .power = 5,
        .weapon_skill_type = WEAPON_SKILL_MACE,
        .accuracy_bonus = 0,
        .crit_bonus = 2,
        .parry_bonus = 0,
        .can_parry = 0,
        .damage_type_mask = DAMAGE_TYPE_CRUSHING,
        .attack_mode_mask = ATTACK_MODE_FLAG_SMASH,
    },
    {
        .name = "Hunting Spear",
        .symbol = '/',
        .type = ITEM_TYPE_WEAPON_VERSATILE,
        .stackable = 0,
        .quantity = 1,
        .power = 5,
        .weapon_skill_type = WEAPON_SKILL_SPEAR,
        .accuracy_bonus = 3,
        .crit_bonus = 3,
        .parry_bonus = 2,
        .can_parry = 1,
        .damage_type_mask = DAMAGE_TYPE_PIERCING,
        .attack_mode_mask = ATTACK_MODE_FLAG_STAB,
    },
    {
        .name = "Quarterstaff",
        .symbol = '/',
        .type = ITEM_TYPE_WEAPON_VERSATILE,
        .stackable = 0,
        .quantity = 1,
        .power = 4,
        .weapon_skill_type = WEAPON_SKILL_STAFF,
        .accuracy_bonus = 2,
        .crit_bonus = 2,
        .parry_bonus = 6,
        .can_parry = 1,
        .damage_type_mask = DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_CRUSHING,
        .attack_mode_mask = ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_SMASH,
    },
    {
        .name = "Halberd",
        .symbol = '/',
        .type = ITEM_TYPE_WEAPON_VERSATILE,
        .stackable = 0,
        .quantity = 1,
        .power = 6,
        .weapon_skill_type = WEAPON_SKILL_POLEARM,
        .accuracy_bonus = 1,
        .crit_bonus = 4,
        .parry_bonus = 3,
        .can_parry = 1,
        .damage_type_mask = DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_CRUSHING,
        .attack_mode_mask = ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_SMASH,
    },
    {
        .name = "Leather Armor",
        .symbol = '[',
        .type = ITEM_TYPE_ARMOR_CHEST,
        .stackable = 0,
        .quantity = 1,
        .power = 2,
    },
    {
        .name = "Linen Footwraps",
        .symbol = ',',
        .type = ITEM_TYPE_CLOTHING_FEET,
        .stackable = 0,
        .quantity = 1,
        .power = 1,
    },
    {
        .name = "Linen Trousers",
        .symbol = '}',
        .type = ITEM_TYPE_CLOTHING_LEGS,
        .stackable = 0,
        .quantity = 1,
        .power = 1,
    },
    {
        .name = "Linen Shirt",
        .symbol = '}',
        .type = ITEM_TYPE_CLOTHING_CHEST,
        .stackable = 0,
        .quantity = 1,
        .power = 1,
    },
    {
        .name = "Linen Cloak",
        .symbol = '~',
        .type = ITEM_TYPE_CLOTHING_SHOULDERS,
        .stackable = 0,
        .quantity = 1,
        .power = 1,
    },
    {
        .name = "Small Linen Pouch",
        .symbol = 'p',
        .type = ITEM_TYPE_BAG_BELTPOUCH,
        .stackable = 0,
        .quantity = 1,
        .power = 0,
    },
};

const int item_template_count = sizeof(item_templates) / sizeof(ItemTemplate);

// Find item template by exact name, or return NULL.
const ItemTemplate* item_template_by_name(const char* name)
{
    if(!name) return NULL;
    for(int i = 0; i < item_template_count; i++)
    {
        if(strcmp(item_templates[i].name, name) == 0)
            return &item_templates[i];
    }
    return NULL;
}

// Initialize item instance using data from a template entry.
void item_init_from_template(Item* item, const ItemTemplate* tmpl, int x, int y)
{
    if(!item || !tmpl) return;
    item_init(item, tmpl->name, tmpl->symbol, x, y, tmpl->type, tmpl->stackable, tmpl->quantity);
    item->power = tmpl->power;
    item->weapon_skill_type = tmpl->weapon_skill_type;
    item->accuracy_bonus = tmpl->accuracy_bonus;
    item->crit_bonus = tmpl->crit_bonus;
    item->parry_bonus = tmpl->parry_bonus;
    item->can_parry = tmpl->can_parry;
    item->damage_type_mask = tmpl->damage_type_mask;
    item->attack_mode_mask = tmpl->attack_mode_mask;
}
