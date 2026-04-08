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

    memset(item, 0, sizeof(*item));
    object_init(&item->object);
    item->object.base.x = x;
    item->object.base.y = y;
    item->object.base.z = AREA_GROUND_Z;
    item->object.base.symbol = symbol;
    item->object.base.color = RENDER_COLOR_DEFAULT;
    item->object.base.blocks = 0;
    item->object.base.layer = TILE_LAYER_EFFECT;
    item->object.base.hide_below = 0;

    strncpy(item->name, name, sizeof(item->name)-1);
    item->name[sizeof(item->name)-1] = '\0';
    item->stackable = stackable;
    item->stack_max = stackable ? 99 : 1;
    item->quantity = quantity;
    item->type = type;
    item->power = (type == ITEM_TYPE_CONSUMABLE) ? 10 : 0;
    item->damage_min = -1;
    item->damage_max = -1;
    item->stab_damage_min = -1;
    item->stab_damage_max = -1;
    item->cut_damage_min = -1;
    item->cut_damage_max = -1;
    item->smash_damage_min = -1;
    item->smash_damage_max = -1;
    item->punch_damage_min = -1;
    item->punch_damage_max = -1;
    item->kick_damage_min = -1;
    item->kick_damage_max = -1;
    item->weapon_skill_type = WEAPON_SKILL_UNARMED;
    item->accuracy_bonus = 0;
    item->crit_bonus = 0;
    item->parry_bonus = 0;
    item->block_bonus = 0;
    item->can_parry = 0;
    item->damage_type_mask = DAMAGE_TYPE_NONE;
    item->attack_mode_mask = ATTACK_MODE_FLAG_NONE;
    item->two_hand_attack_mode_mask = ATTACK_MODE_FLAG_NONE;
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
    item->is_material = 0;
    item->material_type = MATERIAL_TYPE_NONE;
    item->material_state = MATERIAL_STATE_NONE;
}


int item_type_is_weapon(ItemType type)
{
    switch(type)
    {
        case ITEM_TYPE_WEAPON_MAIN_HAND:
        case ITEM_TYPE_WEAPON_OFF_HAND:
        case ITEM_TYPE_WEAPON_ONE_HANDED:
        case ITEM_TYPE_WEAPON_VERSATILE:
        case ITEM_TYPE_WEAPON_TWO_HANDED:
            return 1;
        default:
            return 0;
    }
}

int item_has_category(const Item* item, const char* target)
{
    if(!item || !target || !*target)
        return 0;

    for(int i = 0; i < 4; ++i)
    {
        if(item->categories[i][0] == '\0')
            continue;
        if(strcasecmp(item->categories[i], target) == 0)
            return 1;
    }

    return 0;
}

int item_is_tool(const Item* item)
{
    return item_has_category(item, "tool");
}

NonWeaponSkillType item_tool_non_weapon_skill(const Item* item)
{
    if(!item_is_tool(item))
        return NON_WEAPON_SKILL_COUNT;

    if(item_has_category(item, "lumberjacking") || item_has_category(item, "woodcutting"))
        return NON_WEAPON_SKILL_LUMBERJACKING;
    if(item_has_category(item, "mining"))
        return NON_WEAPON_SKILL_MINING;
    if(item_has_category(item, "skinning"))
        return NON_WEAPON_SKILL_SKINNING;
    if(item_has_category(item, "carpentry") || item_has_category(item, "woodworking") || item_has_category(item, "sawing"))
        return NON_WEAPON_SKILL_CARPENTRY;
    if(item_has_category(item, "fishing"))
        return NON_WEAPON_SKILL_FISHING;
    if(item_has_category(item, "herbalism"))
        return NON_WEAPON_SKILL_HERBALISM;

    return NON_WEAPON_SKILL_COUNT;
}

// Return 1 when item instance belongs to weapon categories (by category)
int item_is_weapon(const Item* item)
{
    if(!item)
        return 0;
    return item_has_category(item, "weapon") || item_type_is_weapon(item->type);
}

int item_is_ranged_weapon(const Item* item)
{
    if(!item || !item_is_weapon(item))
        return 0;
    return item->ranged_type != RANGED_WEAPON_NONE;
}

int item_is_material(const Item* item)
{
    if(!item)
        return 0;
    return item->type == ITEM_TYPE_MATERIAL ||
           item->is_material ||
           item_has_category(item, "material");
}

