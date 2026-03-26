#include <string.h>
#include "item.h"
#include "map.h"

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
    item->entity.z = AREA_GROUND_Z;
    item->entity.symbol = symbol;
    item->entity.color = RENDER_COLOR_DEFAULT;
    item->entity.blocks = 0;
    item->entity.layer = TILE_LAYER_UNIT;
    item->entity.hide_below = 0;

    strncpy(item->name, name, sizeof(item->name)-1);
    item->name[sizeof(item->name)-1] = '\0';
    item->stackable = stackable;
    item->stack_max = stackable ? 99 : 1;
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
    item->camp_placeable = 0;
    item->throwable = 0;
    item->ranged_type = RANGED_WEAPON_NONE;
    item->ranged_range = 0;
    item->ammo_item_name[0] = '\0';
    item->ammo_per_shot = 0;
}


// Return 1 if any category is "weapon" (case-insensitive)
static int item_categories_include(const char categories[4][24], const char* target)
{
    for(int i = 0; i < 4; ++i) {
        if(categories[i][0] == '\0') continue;
        if(strcasecmp(categories[i], target) == 0) return 1;
    }
    return 0;
}

// Return 1 when item instance belongs to weapon categories (by category)
int item_is_weapon(const Item* item)
{
    if(!item)
        return 0;
    return item_categories_include(item->categories, "weapon");
}

int item_is_ranged_weapon(const Item* item)
{
    if(!item || !item_is_weapon(item))
        return 0;
    return item->ranged_type != RANGED_WEAPON_NONE;
}

