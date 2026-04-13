#include <stdio.h>
#include <stdlib.h>
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
    item->quality = ITEM_QUALITY_REGULAR;
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
    item->heat_state = ITEM_HEAT_NONE;
    item->heat_turns_remaining = 0;
    item->is_readable = (type == ITEM_TYPE_BOOK || type == ITEM_TYPE_SCROLL) ? 1 : 0;
    item->book_content_type = BOOK_CONTENT_NONE;
    item->book_flavor[0] = '\0';
    item->book_content[0] = '\0';
    item->recipe_unlock_id[0] = '\0';
    item->book_location_count = 0;
    for(int i = 0; i < ITEM_BOOK_MAX_LOCATIONS; i++)
    {
        item->book_location_index[i] = -1;
        item->book_location_knowledge[i] = 0;
        item->book_location_hint[i][0] = '\0';
    }
}

static int item_quality_modifier_percent(ItemQuality quality)
{
    switch(quality)
    {
        case ITEM_QUALITY_HORRIBLE:    return -20;
        case ITEM_QUALITY_CRUDE:       return -10;
        case ITEM_QUALITY_GOOD:        return 10;
        case ITEM_QUALITY_EXCEPTIONAL: return 20;
        case ITEM_QUALITY_MASTERWORK:  return 35;
        case ITEM_QUALITY_REGULAR:
        case ITEM_QUALITY_UNSPECIFIED:
        case ITEM_QUALITY_COUNT:
        default:
            return 0;
    }
}

static int item_quality_can_vary(const Item* item)
{
    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;
    if(item->stackable || item->is_ammo)
        return 0;
    if(item->name[0] == '\0' || strcmp(item->name, "None") == 0 || strcmp(item->name, "Gold Coins") == 0)
        return 0;
    return 1;
}

static int item_quality_scale_nonnegative(int value, int percent)
{
    int delta;

    if(value <= 0 || percent == 0)
        return value;

    delta = (abs(value) * abs(percent) + 99) / 100;
    if(delta < 1)
        delta = 1;

    value += (percent > 0) ? delta : -delta;
    if(value < 0)
        value = 0;
    return value;
}

static int item_quality_scale_bonus(int value, int percent)
{
    int delta;

    if(value == 0 || percent == 0)
        return value;

    delta = (abs(value) * abs(percent) + 99) / 100;
    if(delta < 1)
        delta = 1;

    return value + ((percent > 0) ? delta : -delta);
}

static int item_quality_scale_cost(int value, int percent)
{
    int delta;

    if(value == 0 || percent == 0)
        return value;

    delta = (abs(value) * abs(percent) + 99) / 100;
    if(delta < 1)
        delta = 1;

    return value + ((percent > 0) ? -delta : delta);
}

static void item_quality_normalize_range(int* min_value, int* max_value)
{
    int temp;

    if(!min_value || !max_value)
        return;
    if(*min_value < 0 || *max_value < 0)
        return;
    if(*min_value <= *max_value)
        return;

    temp = *min_value;
    *min_value = *max_value;
    *max_value = temp;
}

const char* item_quality_name(ItemQuality quality)
{
    switch(quality)
    {
        case ITEM_QUALITY_HORRIBLE:    return "horrible";
        case ITEM_QUALITY_CRUDE:       return "crude";
        case ITEM_QUALITY_GOOD:        return "good";
        case ITEM_QUALITY_EXCEPTIONAL: return "exceptional";
        case ITEM_QUALITY_MASTERWORK:  return "masterwork";
        case ITEM_QUALITY_REGULAR:
        case ITEM_QUALITY_UNSPECIFIED:
        case ITEM_QUALITY_COUNT:
        default:
            return "regular";
    }
}

static const char* item_quality_display_prefix(ItemQuality quality)
{
    switch(quality)
    {
        case ITEM_QUALITY_HORRIBLE:    return "Horrible Quality";
        case ITEM_QUALITY_CRUDE:       return "Crude Quality";
        case ITEM_QUALITY_GOOD:        return "Good Quality";
        case ITEM_QUALITY_EXCEPTIONAL: return "Exceptional Quality";
        case ITEM_QUALITY_MASTERWORK:  return "Masterwork Quality";
        case ITEM_QUALITY_REGULAR:
        case ITEM_QUALITY_UNSPECIFIED:
        case ITEM_QUALITY_COUNT:
        default:
            return "";
    }
}

ItemQuality item_quality_from_string(const char* text)
{
    if(!text || !text[0])
        return ITEM_QUALITY_REGULAR;
    if(strcasecmp(text, "horrible") == 0 ||
       strcasecmp(text, "horribly crafted") == 0 ||
       strcasecmp(text, "horrible quality") == 0)
        return ITEM_QUALITY_HORRIBLE;
    if(strcasecmp(text, "crude") == 0 ||
       strcasecmp(text, "crudely crafted") == 0 ||
       strcasecmp(text, "crude quality") == 0)
        return ITEM_QUALITY_CRUDE;
    if(strcasecmp(text, "good") == 0 || strcasecmp(text, "good quality") == 0)
        return ITEM_QUALITY_GOOD;
    if(strcasecmp(text, "exceptional") == 0 || strcasecmp(text, "exceptional quality") == 0)
        return ITEM_QUALITY_EXCEPTIONAL;
    if(strcasecmp(text, "masterwork") == 0 || strcasecmp(text, "masterwork quality") == 0)
        return ITEM_QUALITY_MASTERWORK;
    return ITEM_QUALITY_REGULAR;
}

ItemQuality item_roll_quality(const Item* item)
{
    int roll;

    if(!item_quality_can_vary(item))
        return ITEM_QUALITY_REGULAR;

    roll = rand() % 100;
    if(roll < 5)
        return ITEM_QUALITY_HORRIBLE;
    if(roll < 25)
        return ITEM_QUALITY_CRUDE;
    if(roll < 65)
        return ITEM_QUALITY_REGULAR;
    if(roll < 85)
        return ITEM_QUALITY_GOOD;
    if(roll < 95)
        return ITEM_QUALITY_EXCEPTIONAL;
    return ITEM_QUALITY_MASTERWORK;
}

void item_apply_quality(Item* item, ItemQuality quality)
{
    int percent;

    if(!item)
        return;

    if(quality == ITEM_QUALITY_UNSPECIFIED)
        quality = item_roll_quality(item);
    if(quality < ITEM_QUALITY_HORRIBLE || quality >= ITEM_QUALITY_COUNT)
        quality = ITEM_QUALITY_REGULAR;

    item->quality = quality;
    percent = item_quality_modifier_percent(quality);
    if(percent == 0)
        return;

    item->power = item_quality_scale_nonnegative(item->power, percent);
    item->damage_min = item_quality_scale_nonnegative(item->damage_min, percent);
    item->damage_max = item_quality_scale_nonnegative(item->damage_max, percent);
    item->stab_damage_min = item_quality_scale_nonnegative(item->stab_damage_min, percent);
    item->stab_damage_max = item_quality_scale_nonnegative(item->stab_damage_max, percent);
    item->cut_damage_min = item_quality_scale_nonnegative(item->cut_damage_min, percent);
    item->cut_damage_max = item_quality_scale_nonnegative(item->cut_damage_max, percent);
    item->smash_damage_min = item_quality_scale_nonnegative(item->smash_damage_min, percent);
    item->smash_damage_max = item_quality_scale_nonnegative(item->smash_damage_max, percent);
    item->punch_damage_min = item_quality_scale_nonnegative(item->punch_damage_min, percent);
    item->punch_damage_max = item_quality_scale_nonnegative(item->punch_damage_max, percent);
    item->kick_damage_min = item_quality_scale_nonnegative(item->kick_damage_min, percent);
    item->kick_damage_max = item_quality_scale_nonnegative(item->kick_damage_max, percent);
    item->accuracy_bonus = item_quality_scale_bonus(item->accuracy_bonus, percent);
    item->crit_bonus = item_quality_scale_bonus(item->crit_bonus, percent);
    item->parry_bonus = item_quality_scale_bonus(item->parry_bonus, percent);
    item->block_bonus = item_quality_scale_bonus(item->block_bonus, percent);
    item->reach_bonus = item_quality_scale_bonus(item->reach_bonus, percent);
    item->armor_penetration = item_quality_scale_nonnegative(item->armor_penetration, percent);
    item->stamina_cost_mod = item_quality_scale_cost(item->stamina_cost_mod, percent);
    item->status_bleed_chance = item_quality_scale_nonnegative(item->status_bleed_chance, percent);
    item->status_stun_chance = item_quality_scale_nonnegative(item->status_stun_chance, percent);
    item->status_slow_chance = item_quality_scale_nonnegative(item->status_slow_chance, percent);

    item_quality_normalize_range(&item->damage_min, &item->damage_max);
    item_quality_normalize_range(&item->stab_damage_min, &item->stab_damage_max);
    item_quality_normalize_range(&item->cut_damage_min, &item->cut_damage_max);
    item_quality_normalize_range(&item->smash_damage_min, &item->smash_damage_max);
    item_quality_normalize_range(&item->punch_damage_min, &item->punch_damage_max);
    item_quality_normalize_range(&item->kick_damage_min, &item->kick_damage_max);
}

void item_format_display_name(const Item* item, char* out, size_t out_size)
{
    const char* prefix;

    if(!out || out_size == 0)
        return;

    if(!item || item->name[0] == '\0')
    {
        snprintf(out, out_size, "%s", "");
        return;
    }

    prefix = item_quality_display_prefix(item->quality);
    if(prefix[0] == '\0')
        snprintf(out, out_size, "%s", item->name);
    else
        snprintf(out, out_size, "%s %s", prefix, item->name);
}

const char* item_display_name(const Item* item)
{
    static char buffer[96];

    item_format_display_name(item, buffer, sizeof(buffer));
    return buffer;
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
    if(item_has_category(item, "blacksmithing") || item_has_category(item, "smithing"))
        return NON_WEAPON_SKILL_BLACKSMITHING;
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

