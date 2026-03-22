#include <string.h>
#include "item.h"

/*
 * Purpose:
 *   Implements item instance initialization and weapon classification helpers.
 *
 * Functions:
 *   - item_init: fills one Item struct from explicit values.
 *   - item_type_is_weapon / item_is_weapon: weapon-category checks.
 */

// Initialize one item instance for world or inventory usage.
void item_init(Item* item, const char* name, char symbol, int x, int y, ItemType type, int stackable, int quantity)
{
    if(!item) return;
    item->entity.x = x;
    item->entity.y = y;
    item->entity.symbol = symbol;
    item->entity.color = RENDER_COLOR_DEFAULT;
    item->entity.blocks = 0;
    item->entity.layer = TILE_LAYER_UNIT;
    item->entity.hide_below = 0;

    strncpy(item->name, name, sizeof(item->name)-1);
    item->name[sizeof(item->name)-1] = '\0';
    item->stackable = stackable;
    item->quantity = quantity;
    item->type = type;
    item->power = (type == ITEM_TYPE_CONSUMABLE) ? 10 : 0;
    item->weapon_skill_type = WEAPON_SKILL_UNARMED;
    item->accuracy_bonus = 0;
    item->crit_bonus = 0;
    item->parry_bonus = 0;
    item->block_bonus = 0;
    item->can_parry = 0;
    item->damage_type_mask = DAMAGE_TYPE_NONE;
    item->attack_mode_mask = ATTACK_MODE_FLAG_NONE;
    item->reach_bonus = 0;
    item->armor_penetration = 0;
    item->stamina_cost_mod = 0;
    item->status_bleed_chance = 0;
    item->status_stun_chance = 0;
    item->status_slow_chance = 0;
}

// Return 1 when item type belongs to weapon categories.
int item_type_is_weapon(ItemType type)
{
    return type == ITEM_TYPE_WEAPON_MAIN_HAND ||
           type == ITEM_TYPE_WEAPON_OFF_HAND ||
           type == ITEM_TYPE_WEAPON_ONE_HANDED ||
           type == ITEM_TYPE_WEAPON_VERSATILE ||
           type == ITEM_TYPE_WEAPON_TWO_HANDED;
}

// Return 1 when item instance belongs to weapon categories.
int item_is_weapon(const Item* item)
{
    if(!item)
        return 0;
    return item_type_is_weapon(item->type);
}

