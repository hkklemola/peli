
// All includes must come first
#include "item_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include "item.h"
#include "character.h"

static void item_template_set_defaults(ItemTemplate* tmpl);
static int finalize_item_template(ItemTemplate* tmpl);
static void trim_in_place(char* text);
static int equals_ignore_case(const char* left, const char* right);
static int parse_ranged_weapon_type(const char* value, RangedWeaponType* out);
void clear_item_templates(void) {
    item_template_count = 0;
    // Optionally free storage if needed
}

void free_item_template(ItemTemplate* tmpl) {
    if (!tmpl) return;
    if (tmpl->name) {
        free((void*)tmpl->name);
        tmpl->name = NULL;
    }
    // Reset other fields as needed
}

typedef struct {
    const char* name;
    int flag;
} ContainerFlagMapping;

static const ContainerFlagMapping container_flag_mappings[] = {
    { "ALL", CONTAINER_ACCEPTS_ALL },
    { "EQUIPMENT", CONTAINER_ACCEPTS_EQUIPMENT },
    { "CONSUMABLE", CONTAINER_ACCEPTS_CONSUMABLE },
    { "AMMO", CONTAINER_ACCEPTS_AMMO },
    { "QUEST", CONTAINER_ACCEPTS_QUEST },
    { "MISC", CONTAINER_ACCEPTS_MISC },
    // Add more as needed
};

static int parse_container_accepted_flags(const char* value, int* out_flags) {
    if (!value || !*value) return 0;
    if (strcmp(value, "0") == 0) { *out_flags = CONTAINER_ACCEPTS_ALL; return 1; }
    int flags = 0;
    char buf[128];
    strncpy(buf, value, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
    char* token = strtok(buf, "|,");
    while (token) {
        trim_in_place(token);
        int found = 0;
        for (size_t i = 0; i < sizeof(container_flag_mappings)/sizeof(container_flag_mappings[0]); ++i) {
            if (equals_ignore_case(token, container_flag_mappings[i].name)) {
                flags |= container_flag_mappings[i].flag;
                found = 1;
                break;
            }
        }
        if (!found) {
            // Try numeric fallback
            char* endptr = NULL;
            int num = (int)strtol(token, &endptr, 10);
            if (endptr && *endptr == '\0') {
                flags |= num;
            } else {
                printf("[ITEM LOAD] Unknown container_accepted_flag: '%s'\n", token);
                return 0;
            }
        }
        token = strtok(NULL, "|,");
    }
    *out_flags = flags;
    return 1;
}



// Parse symbolic or numeric slot_type for equipment/containers
static int parse_slot_type(const char* value, EquipmentSlotType* out)
{
    static const struct {
        const char* name;
        EquipmentSlotType type;
    } mappings[] = {
        { "EQUIP_SLOT_NONE", EQUIP_SLOT_NONE },
        { "EQUIP_SLOT_MAIN_HAND", EQUIP_SLOT_MAIN_HAND },
        { "EQUIP_SLOT_OFF_HAND", EQUIP_SLOT_OFF_HAND },
        { "EQUIP_SLOT_ARMOR_HEAD", EQUIP_SLOT_ARMOR_HEAD },
        { "EQUIP_SLOT_ARMOR_EYES", EQUIP_SLOT_ARMOR_EYES },
        { "EQUIP_SLOT_ARMOR_FACE", EQUIP_SLOT_ARMOR_FACE },
        { "EQUIP_SLOT_ARMOR_NECK", EQUIP_SLOT_ARMOR_NECK },
        { "EQUIP_SLOT_ARMOR_SHOULDERS", EQUIP_SLOT_ARMOR_SHOULDERS },
        { "EQUIP_SLOT_ARMOR_CHEST", EQUIP_SLOT_ARMOR_CHEST },
        { "EQUIP_SLOT_ARMOR_ARMS", EQUIP_SLOT_ARMOR_ARMS },
        { "EQUIP_SLOT_ARMOR_HANDS", EQUIP_SLOT_ARMOR_HANDS },
        { "EQUIP_SLOT_ARMOR_WAIST", EQUIP_SLOT_ARMOR_WAIST },
        { "EQUIP_SLOT_ARMOR_LEGS", EQUIP_SLOT_ARMOR_LEGS },
        { "EQUIP_SLOT_ARMOR_FEET", EQUIP_SLOT_ARMOR_FEET },
        { "EQUIP_SLOT_CLOTHING_HEAD", EQUIP_SLOT_CLOTHING_HEAD },
        { "EQUIP_SLOT_CLOTHING_EYES", EQUIP_SLOT_CLOTHING_EYES },
        { "EQUIP_SLOT_CLOTHING_FACE", EQUIP_SLOT_CLOTHING_FACE },
        { "EQUIP_SLOT_CLOTHING_NECK", EQUIP_SLOT_CLOTHING_NECK },
        { "EQUIP_SLOT_CLOTHING_SHOULDERS", EQUIP_SLOT_CLOTHING_SHOULDERS },
        { "EQUIP_SLOT_CLOTHING_CHEST", EQUIP_SLOT_CLOTHING_CHEST },
        { "EQUIP_SLOT_CLOTHING_ARMS", EQUIP_SLOT_CLOTHING_ARMS },
        { "EQUIP_SLOT_CLOTHING_HANDS", EQUIP_SLOT_CLOTHING_HANDS },
        { "EQUIP_SLOT_CLOTHING_WAIST", EQUIP_SLOT_CLOTHING_WAIST },
        { "EQUIP_SLOT_CLOTHING_LEGS", EQUIP_SLOT_CLOTHING_LEGS },
        { "EQUIP_SLOT_CLOTHING_FEET", EQUIP_SLOT_CLOTHING_FEET },
        { "EQUIP_SLOT_ACCESSORY_HEAD", EQUIP_SLOT_ACCESSORY_HEAD },
        { "EQUIP_SLOT_ACCESSORY_EYES", EQUIP_SLOT_ACCESSORY_EYES },
        { "EQUIP_SLOT_ACCESSORY_FACE", EQUIP_SLOT_ACCESSORY_FACE },
        { "EQUIP_SLOT_ACCESSORY_NECK", EQUIP_SLOT_ACCESSORY_NECK },
        { "EQUIP_SLOT_ACCESSORY_WRIST", EQUIP_SLOT_ACCESSORY_WRIST },
        { "EQUIP_SLOT_ACCESSORY_WRIST_RIGHT", EQUIP_SLOT_ACCESSORY_WRIST_RIGHT },
        { "EQUIP_SLOT_ACCESSORY_WRIST_LEFT", EQUIP_SLOT_ACCESSORY_WRIST_LEFT },
        { "EQUIP_SLOT_ACCESSORY_FINGER_RIGHT", EQUIP_SLOT_ACCESSORY_FINGER_RIGHT },
        { "EQUIP_SLOT_ACCESSORY_FINGER_LEFT", EQUIP_SLOT_ACCESSORY_FINGER_LEFT },
        { "EQUIP_SLOT_ACCESSORY_TRINKET", EQUIP_SLOT_ACCESSORY_TRINKET },
        { "EQUIP_SLOT_ACCESSORY_TRINKET_1", EQUIP_SLOT_ACCESSORY_TRINKET_1 },
        { "EQUIP_SLOT_ACCESSORY_TRINKET_2", EQUIP_SLOT_ACCESSORY_TRINKET_2 },
        { "EQUIP_SLOT_CONTAINER_BACKPACK", EQUIP_SLOT_CONTAINER_BACKPACK },
        { "EQUIP_SLOT_CONTAINER_POUCH", EQUIP_SLOT_CONTAINER_POUCH },
        { "EQUIP_SLOT_CONTAINER_QUIVER", EQUIP_SLOT_CONTAINER_QUIVER },
        // Add more as needed
    };

    // Try symbolic mapping
    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++) {
        if(equals_ignore_case(value, mappings[i].name)) {
            *out = mappings[i].type;
            return 1;
        }
    }
    // Try numeric
    char* endptr = NULL;
    long num = strtol(value, &endptr, 10);
    if(endptr && *endptr == '\0' && num >= EQUIP_SLOT_NONE && num < EQUIP_SLOT_COUNT) {
        *out = (EquipmentSlotType)num;
        return 1;
    }
    return 0;
}

// Add this macro at the top for quick debug logging (or use your preferred logging method)
#define ITEM_TEMPLATE_DEBUG 1
#if ITEM_TEMPLATE_DEBUG
#define ITEM_DEBUG_LOG(fmt, ...) printf("[ITEM LOAD] " fmt "\n", ##__VA_ARGS__)
#else
#define ITEM_DEBUG_LOG(fmt, ...)
#endif

/*
 * Purpose:
 *   Defines static item templates and template-to-instance conversion helpers.
 *
 * Functions:
 *   - item_template_by_name: fetches template by item name.
 *   - item_init_from_template: initializes one item from template values.
 */

const ItemTemplate* item_templates = NULL;
int item_template_count = 0;

static ItemTemplate* item_templates_storage = NULL;
static int item_templates_capacity = 0;
static char g_item_template_last_error[256];

const char* item_templates_last_error(void)
{
    return g_item_template_last_error;
}

static char* item_strdup(const char* text)
{
    size_t length;
    char* copy;

    if(!text)
        return NULL;

    length = strlen(text);
    copy = (char*)malloc(length + 1);
    if(!copy)
        return NULL;

    memcpy(copy, text, length + 1);
    return copy;
}

static void trim_in_place(char* text)
{
    char* start;
    char* end;

    if(!text)
        return;

    start = text;
    while(*start && isspace((unsigned char)*start))
        start++;

    if(start != text)
        memmove(text, start, strlen(start) + 1);

    end = text + strlen(text);
    while(end > text && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
}

static int equals_ignore_case(const char* left, const char* right)
{
    unsigned char lc;
    unsigned char rc;

    if(!left || !right)
        return 0;

    while(*left && *right)
    {
        lc = (unsigned char)tolower((unsigned char)*left);
        rc = (unsigned char)tolower((unsigned char)*right);
        if(lc != rc)
            return 0;
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static int parse_item_type(const char* value, ItemType* out)
{
    static const struct {
        const char* name;
        ItemType type;
    } mappings[] = {
        { "CONSUMABLE", ITEM_TYPE_CONSUMABLE },
        { "WEAPON_MAIN_HAND", ITEM_TYPE_WEAPON_MAIN_HAND },
        { "WEAPON_OFF_HAND", ITEM_TYPE_WEAPON_OFF_HAND },
        { "WEAPON_ONE_HANDED", ITEM_TYPE_WEAPON_ONE_HANDED },
        { "WEAPON_VERSATILE", ITEM_TYPE_WEAPON_VERSATILE },
        { "WEAPON_TWO_HANDED", ITEM_TYPE_WEAPON_TWO_HANDED },
        { "ARMOR_HEAD", ITEM_TYPE_ARMOR_HEAD },
        { "ARMOR_EYES", ITEM_TYPE_ARMOR_EYES },
        { "ARMOR_FACE", ITEM_TYPE_ARMOR_FACE },
        { "ARMOR_NECK", ITEM_TYPE_ARMOR_NECK },
        { "ARMOR_SHOULDERS", ITEM_TYPE_ARMOR_SHOULDERS },
        { "ARMOR_CLOAK", ITEM_TYPE_ARMOR_CLOAK },
        { "ARMOR_CHEST", ITEM_TYPE_ARMOR_CHEST },
        { "ARMOR_WAIST", ITEM_TYPE_ARMOR_WAIST },
        { "ARMOR_ARMS", ITEM_TYPE_ARMOR_ARMS },
        { "ARMOR_HANDS", ITEM_TYPE_ARMOR_HANDS },
        { "ARMOR_LEGS", ITEM_TYPE_ARMOR_LEGS },
        { "ARMOR_FEET", ITEM_TYPE_ARMOR_FEET },
        { "ARMOR_BOOTS", ITEM_TYPE_ARMOR_BOOTS },
        { "CLOTHING_HEAD", ITEM_TYPE_CLOTHING_HEAD },
        { "CLOTHING_EYES", ITEM_TYPE_CLOTHING_EYES },
        { "CLOTHING_FACE", ITEM_TYPE_CLOTHING_FACE },
        { "CLOTHING_NECK", ITEM_TYPE_CLOTHING_NECK },
        { "CLOTHING_SHOULDERS", ITEM_TYPE_CLOTHING_SHOULDERS },
        { "CLOTHING_CHEST", ITEM_TYPE_CLOTHING_CHEST },
        { "CLOTHING_ARMS", ITEM_TYPE_CLOTHING_ARMS },
        { "CLOTHING_HANDS", ITEM_TYPE_CLOTHING_HANDS },
        { "CLOTHING_WAIST", ITEM_TYPE_CLOTHING_WAIST },
        { "CLOTHING_LEGS", ITEM_TYPE_CLOTHING_LEGS },
        { "CLOTHING_FEET", ITEM_TYPE_CLOTHING_FEET },
        { "ACCESSORY_HEAD", ITEM_TYPE_ACCESSORY_HEAD },
        { "ACCESSORY_EYES", ITEM_TYPE_ACCESSORY_EYES },
        { "ACCESSORY_FACE", ITEM_TYPE_ACCESSORY_FACE },
        { "ACCESSORY_NECK", ITEM_TYPE_ACCESSORY_NECK },
        { "ACCESSORY_TRINKET", ITEM_TYPE_ACCESSORY_TRINKET },
        { "ACCESSORY_FINGER", ITEM_TYPE_ACCESSORY_FINGER },
        { "ACCESSORY_WRIST", ITEM_TYPE_ACCESSORY_WRIST },
        { "ACCESSORY_BRACELET", ITEM_TYPE_ACCESSORY_WRIST }, /* legacy alias */
        { "CONTAINER_BACKPACK", ITEM_TYPE_CONTAINER_BACKPACK },
        { "CONTAINER_POUCH", ITEM_TYPE_CONTAINER_POUCH },
        { "CONTAINER_QUIVER", ITEM_TYPE_CONTAINER_QUIVER },
        /* Legacy aliases for save/template compatibility */
        { "BAG_BACKPACK", ITEM_TYPE_CONTAINER_BACKPACK },
        { "BAG_BELTPOUCH", ITEM_TYPE_CONTAINER_POUCH },
            { "BAG_POUCH", ITEM_TYPE_CONTAINER_POUCH },
        { "KEY", ITEM_TYPE_KEY },
    };

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(value, mappings[i].name))
        {
            *out = mappings[i].type;
            return 1;
        }
    }

    return 0;
}

static int parse_weapon_skill_type(const char* value, WeaponSkillType* out)
{
    static const struct {
        const char* name;
        WeaponSkillType type;
    } mappings[] = {
        { "NONE", WEAPON_SKILL_UNARMED },
        { "UNARMED", WEAPON_SKILL_UNARMED },
        { "DAGGER", WEAPON_SKILL_DAGGER },
        { "SWORD", WEAPON_SKILL_SWORD },
        { "AXE", WEAPON_SKILL_AXE },
        { "MACE", WEAPON_SKILL_MACE },
        { "SPEAR", WEAPON_SKILL_SPEAR },
        { "STAFF", WEAPON_SKILL_STAFF },
        { "POLEARM", WEAPON_SKILL_POLEARM },
        { "THROWN", WEAPON_SKILL_THROWN },
        { "BOW", WEAPON_SKILL_BOW },
        { "CROSSBOW", WEAPON_SKILL_CROSSBOW },
    };

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(value, mappings[i].name))
        {
            *out = mappings[i].type;
            return 1;
        }
    }

    return 0;
}

static int parse_item_effect_type(const char* value, ItemEffectType* out)
{
    static const struct {
        const char* name;
        ItemEffectType type;
    } mappings[] = {
        { "HEAL", ITEM_EFFECT_HEAL },
        { "MAP_KNOWLEDGE", ITEM_EFFECT_MAP_KNOWLEDGE },
    };

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(value, mappings[i].name))
        {
            *out = mappings[i].type;
            return 1;
        }
    }

    return 0;
}

static int parse_ranged_weapon_type(const char* value, RangedWeaponType* out)
{
    static const struct {
        const char* name;
        RangedWeaponType type;
    } mappings[] = {
        { "NONE", RANGED_WEAPON_NONE },
        { "THROWN", RANGED_WEAPON_THROWN },
        { "BOW", RANGED_WEAPON_BOW },
        { "CROSSBOW", RANGED_WEAPON_CROSSBOW },
    };

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(value, mappings[i].name))
        {
            *out = mappings[i].type;
            return 1;
        }
    }

    return 0;
}

static int parse_damage_flag_token(const char* token, int* out_flag)
{
    if(equals_ignore_case(token, "NONE"))
        *out_flag = DAMAGE_TYPE_NONE;
    else if(equals_ignore_case(token, "PIERCING"))
        *out_flag = DAMAGE_TYPE_PIERCING;
    else if(equals_ignore_case(token, "SLASHING"))
        *out_flag = DAMAGE_TYPE_SLASHING;
    else if(equals_ignore_case(token, "CRUSHING"))
        *out_flag = DAMAGE_TYPE_CRUSHING;
    else
        return 0;

    return 1;
}

static int parse_attack_mode_flag_token(const char* token, int* out_flag)
{
    if(equals_ignore_case(token, "NONE"))
        *out_flag = ATTACK_MODE_FLAG_NONE;
    else if(equals_ignore_case(token, "PUNCH") || equals_ignore_case(token, "JAB"))
        *out_flag = ATTACK_MODE_FLAG_PUNCH;
    else if(equals_ignore_case(token, "KICK"))
        *out_flag = ATTACK_MODE_FLAG_KICK;
    else if(equals_ignore_case(token, "STAB") || equals_ignore_case(token, "PIERCE") || equals_ignore_case(token, "THRUST"))
        *out_flag = ATTACK_MODE_FLAG_STAB;
    else if(equals_ignore_case(token, "CUT"))
        *out_flag = ATTACK_MODE_FLAG_CUT;
    else if(equals_ignore_case(token, "SMASH"))
        *out_flag = ATTACK_MODE_FLAG_SMASH;
    else
        return 0;

    return 1;
}

static int parse_flag_list(const char* value, int (*parse_one)(const char*, int*), int* out_mask)
{
    char buffer[128];
    char* cursor;
    int mask = 0;

    if(!value || !out_mask)
        return 0;

    snprintf(buffer, sizeof(buffer), "%s", value);
    cursor = buffer;

    while(cursor && *cursor)
    {
        char* next = strchr(cursor, '|');
        int flag = 0;

        if(next)
            *next = '\0';

        trim_in_place(cursor);
        if(cursor[0] != '\0')
        {
            if(!parse_one(cursor, &flag))
                return 0;
            mask |= flag;
        }
        if(next)
            cursor = next + 1;
        else
            break;
    }
    *out_mask = mask;
    return 1;
}

static int append_item_template(const ItemTemplate* source)
{
    ItemTemplate* resized;

    if(item_template_count >= item_templates_capacity)
    {
        int new_capacity = item_templates_capacity > 0 ? item_templates_capacity * 2 : 16;
        resized = (ItemTemplate*)realloc(item_templates_storage, (size_t)new_capacity * sizeof(ItemTemplate));
        if(!resized)
            return 0;

        item_templates_storage = resized;
        item_templates_capacity = new_capacity;
    }

    item_templates_storage[item_template_count++] = *source;
    item_templates = item_templates_storage;
    return 1;
}

static void item_template_set_defaults(ItemTemplate* tmpl)
{
    if(!tmpl)
        return;

    memset(tmpl, 0, sizeof(*tmpl));
    tmpl->quantity = 1;
    tmpl->stack_max = 99;
    tmpl->hide_below = 0;
    tmpl->effect_type = ITEM_EFFECT_HEAL;
    tmpl->consumable_reusable = 0;
    tmpl->map_knowledge_count = 0;
    tmpl->ranged_type = RANGED_WEAPON_NONE;
    tmpl->ranged_range = 0;
    tmpl->ammo_item_name[0] = '\0';
    tmpl->ammo_per_shot = 0;
    tmpl->camp_placeable = 0;
    tmpl->throwable = 0;
    tmpl->container_capacity = 0;
    tmpl->container_accepted_flags = CONTAINER_ACCEPTS_ALL;
    tmpl->is_attachment_host = 0;
    tmpl->host_attachment_slots = 0;

    tmpl->is_ammo = 0;

    for(int i = 0; i < ITEM_TEMPLATE_MAX_MAP_LOCATIONS; i++)
    {
        tmpl->map_location_index[i] = -1;
        tmpl->map_location_knowledge[i] = 0;
    }
    for(int i = 0; i < 4; i++)
        tmpl->categories[i][0] = '\0';
}

static int finalize_item_template(ItemTemplate* tmpl)
{
    if(!tmpl->name || tmpl->name[0] == '\0' || tmpl->symbol == '\0' || tmpl->type == ITEM_TYPE_NONE)
        return 0;

    if(item_template_by_name(tmpl->name))
        return 0;

    if(!append_item_template(tmpl))
        return 0;

    tmpl->name = NULL;
    return 1;
}




int item_templates_load(const char* path)
{
    FILE* file;
    char line[256];
    char line_snapshot[256] = "";
    int line_number = 0;
    ItemTemplate current;
    int have_current = 0;
    int loaded = 0;

    g_item_template_last_error[0] = '\0';

    if(!path)
    {
        snprintf(g_item_template_last_error, sizeof(g_item_template_last_error), "item template path is null");
        return 0;
    }

    file = fopen(path, "r");
    if(!file)
    {
        snprintf(g_item_template_last_error, sizeof(g_item_template_last_error), "could not open %s", path);
        return 0;
    }

    item_template_set_defaults(&current);

    while(fgets(line, sizeof(line), file))
    {
        char* equals;

        line_number++;
        snprintf(line_snapshot, sizeof(line_snapshot), "%s", line);

        trim_in_place(line);
        if(line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        if(line[0] == '[')
        {
            if(have_current)
            {
                if(!finalize_item_template(&current))
                    goto fail;
                loaded++;
                item_template_set_defaults(&current);
            }

            have_current = equals_ignore_case(line, "[item]");
            continue;
        }

        if(!have_current)
            continue;

        equals = strchr(line, '=');
        if(!equals)
            goto fail;

        *equals = '\0';
        trim_in_place(line);
        trim_in_place(equals + 1);

        if(equals_ignore_case(line, "name"))
        {
            free((void*)current.name);
            current.name = item_strdup(equals + 1);
            if(!current.name)
                goto fail;
        }
        else if(equals_ignore_case(line, "symbol"))
        {
            current.symbol = (equals[1] != '\0') ? equals[1] : '\0';
        }
        else if(equals_ignore_case(line, "type"))
        {
            if(!parse_item_type(equals + 1, &current.type))
                goto fail;
        }
        else if(equals_ignore_case(line, "categories") || equals_ignore_case(line, "category"))
        {
            // Parse comma or pipe separated categories into current.categories
            int cat_idx = 0;
            char buf[128];
            strncpy(buf, equals + 1, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
            char* token = strtok(buf, ",|;");
            while(token && cat_idx < 4) {
                trim_in_place(token);
                strncpy(current.categories[cat_idx], token, sizeof(current.categories[cat_idx])-1);
                current.categories[cat_idx][sizeof(current.categories[cat_idx])-1] = '\0';
                cat_idx++;
                token = strtok(NULL, ",|;");
            }
            // Zero out any remaining slots
            for(; cat_idx < 4; ++cat_idx) {
                current.categories[cat_idx][0] = '\0';
            }
        }
        else if(equals_ignore_case(line, "stackable"))
            current.stackable = atoi(equals + 1);
        else if(equals_ignore_case(line, "stack_max"))
            current.stack_max = atoi(equals + 1);
        else if(equals_ignore_case(line, "quantity"))
            current.quantity = atoi(equals + 1);
        else if(equals_ignore_case(line, "power"))
            current.power = atoi(equals + 1);
        else if(equals_ignore_case(line, "weapon_skill"))
        {
            if(!parse_weapon_skill_type(equals + 1, &current.weapon_skill_type))
                goto fail;
        }
        else if(equals_ignore_case(line, "accuracy_bonus"))
            current.accuracy_bonus = atoi(equals + 1);
        else if(equals_ignore_case(line, "crit_bonus"))
            current.crit_bonus = atoi(equals + 1);
        else if(equals_ignore_case(line, "parry_bonus"))
            current.parry_bonus = atoi(equals + 1);
        else if(equals_ignore_case(line, "block_bonus"))
            current.block_bonus = atoi(equals + 1);
        else if(equals_ignore_case(line, "can_parry"))
            current.can_parry = atoi(equals + 1);
        else if(equals_ignore_case(line, "damage_types"))
        {
            if(!parse_flag_list(equals + 1, parse_damage_flag_token, &current.damage_type_mask))
                goto fail;
        }
        else if(equals_ignore_case(line, "attack_modes"))
        {
            if(!parse_flag_list(equals + 1, parse_attack_mode_flag_token, &current.attack_mode_mask))
                goto fail;
        }
        else if(equals_ignore_case(line, "reach_bonus"))
            current.reach_bonus = atoi(equals + 1);
        else if(equals_ignore_case(line, "armor_penetration"))
            current.armor_penetration = atoi(equals + 1);
        else if(equals_ignore_case(line, "stamina_cost_mod"))
            current.stamina_cost_mod = atoi(equals + 1);
        else if(equals_ignore_case(line, "status_bleed_chance"))
            current.status_bleed_chance = atoi(equals + 1);
        else if(equals_ignore_case(line, "status_stun_chance"))
            current.status_stun_chance = atoi(equals + 1);
        else if(equals_ignore_case(line, "status_slow_chance"))
            current.status_slow_chance = atoi(equals + 1);
        else if(equals_ignore_case(line, "camp_placeable"))
            current.camp_placeable = atoi(equals + 1) ? 1 : 0;
        else if(equals_ignore_case(line, "throwable"))
            current.throwable = atoi(equals + 1) ? 1 : 0;
        else if(equals_ignore_case(line, "ranged_type"))
        {
            if(!parse_ranged_weapon_type(equals + 1, &current.ranged_type))
                goto fail;
        }
        else if(equals_ignore_case(line, "ranged_range"))
            current.ranged_range = atoi(equals + 1);
        else if(equals_ignore_case(line, "ammo_item"))
            snprintf(current.ammo_item_name, sizeof(current.ammo_item_name), "%s", equals + 1);
        else if(equals_ignore_case(line, "ammo_per_shot"))
            current.ammo_per_shot = atoi(equals + 1);
        else if(equals_ignore_case(line, "hide_below"))
            current.hide_below = atoi(equals + 1) ? 1 : 0;
        else if(equals_ignore_case(line, "effect_type"))
        {
            if(!parse_item_effect_type(equals + 1, &current.effect_type))
                goto fail;
        }
        else if(equals_ignore_case(line, "consumable_reusable"))
            current.consumable_reusable = atoi(equals + 1) ? 1 : 0;
        else if(equals_ignore_case(line, "container_capacity"))
            current.container_capacity = atoi(equals + 1);
        else if(equals_ignore_case(line, "container_accepted_flags") || equals_ignore_case(line, "accepted_content"))
        {
            int flags = 0;
            if (!parse_container_accepted_flags(equals + 1, &flags))
                goto fail;
            current.container_accepted_flags = flags;
        }
        else if(equals_ignore_case(line, "is_attachment_host"))
            current.is_attachment_host = atoi(equals + 1) ? 1 : 0;
        else if(equals_ignore_case(line, "host_attachment_slots"))
            current.host_attachment_slots = atoi(equals + 1);
        else if(equals_ignore_case(line, "is_ammo"))
            current.is_ammo = atoi(equals + 1) ? 1 : 0;
        else if(equals_ignore_case(line, "is_container"))
            current.is_container = atoi(equals + 1) ? 1 : 0;
        else if(equals_ignore_case(line, "slot_type"))
        {
            if (!parse_slot_type(equals + 1, (EquipmentSlotType*)&current.slot_type))
                goto fail;
        }
        else if(equals_ignore_case(line, "map_knowledge_count"))
        {
            current.map_knowledge_count = atoi(equals + 1);
            if(current.map_knowledge_count < 0 || current.map_knowledge_count > ITEM_TEMPLATE_MAX_MAP_LOCATIONS)
                goto fail;
        }
        else
        {
            int map_i = -1;
            if(sscanf(line, "map_location_%d_index", &map_i) == 1)
            {
                if(map_i < 0 || map_i >= ITEM_TEMPLATE_MAX_MAP_LOCATIONS)
                    goto fail;
                current.map_location_index[map_i] = atoi(equals + 1);
            }
            else if(sscanf(line, "map_location_%d_knowledge", &map_i) == 1)
            {
                if(map_i < 0 || map_i >= ITEM_TEMPLATE_MAX_MAP_LOCATIONS)
                    goto fail;
                current.map_location_knowledge[map_i] = atoi(equals + 1);
            }
            else
                goto fail;
        }
    }

    if(have_current)
    {
        if(!finalize_item_template(&current))
            goto fail;
        loaded++;
    }

    fclose(file);
    return loaded > 0;

fail:
    if(g_item_template_last_error[0] == '\0')
    {
        trim_in_place(line_snapshot);
        if(line_snapshot[0] != '\0')
            snprintf(g_item_template_last_error,
                     sizeof(g_item_template_last_error),
                     "%s:%d near '%s'",
                     path,
                     line_number,
                     line_snapshot);
        else
            snprintf(g_item_template_last_error,
                     sizeof(g_item_template_last_error),
                     "%s:%d (parse failure)",
                     path,
                     line_number);
    }
    free_item_template(&current);
    fclose(file);
    return 0;
}

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
    if(!item) return;

    if(!tmpl)
    {
        item_init(item, "", '?', x, y, ITEM_TYPE_NONE, 0, 0);
        item->quantity = 0;
        return;
    }

    item_init(item, tmpl->name, tmpl->symbol, x, y, tmpl->type, tmpl->stackable, tmpl->quantity);
    item->stack_max = tmpl->stack_max > 0 ? tmpl->stack_max : (item->stackable ? 99 : 1);
    for(int i = 0; i < 4; ++i)
        snprintf(item->categories[i], sizeof(item->categories[i]), "%s", tmpl->categories[i]);
    item->power = tmpl->power;
    item->weapon_skill_type = tmpl->weapon_skill_type;
    item->accuracy_bonus = tmpl->accuracy_bonus;
    item->crit_bonus = tmpl->crit_bonus;
    item->parry_bonus = tmpl->parry_bonus;
    item->block_bonus = tmpl->block_bonus;
    item->can_parry = tmpl->can_parry;
    item->damage_type_mask = tmpl->damage_type_mask;
    item->attack_mode_mask = tmpl->attack_mode_mask;
    item->reach_bonus = tmpl->reach_bonus;
    item->armor_penetration = tmpl->armor_penetration;
    item->stamina_cost_mod = tmpl->stamina_cost_mod;
    item->status_bleed_chance = tmpl->status_bleed_chance;
    item->status_stun_chance = tmpl->status_stun_chance;
    item->status_slow_chance = tmpl->status_slow_chance;
    item->camp_placeable = tmpl->camp_placeable ? 1 : 0;
    item->throwable = tmpl->throwable ? 1 : 0;
    item->ranged_type = tmpl->ranged_type;
    item->ranged_range = tmpl->ranged_range;
    snprintf(item->ammo_item_name, sizeof(item->ammo_item_name), "%s", tmpl->ammo_item_name);
    item->ammo_per_shot = tmpl->ammo_per_shot;
    item->is_ammo = tmpl->is_ammo ? 1 : 0;
    item->slot_type = tmpl->slot_type;
    item->is_container = tmpl->is_container ? 1 : 0;
    item->container_capacity = tmpl->container_capacity;
    item->container_accepted_flags = tmpl->container_accepted_flags;
    item->object.base.hide_below = tmpl->hide_below ? 1 : 0;
}
// End of file: ensure no stray or duplicate code remains below this point. 