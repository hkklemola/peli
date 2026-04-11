#include "savegame.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "atlas.h"
#include "bestiary.h"
#include "combat.h"
#include "inventory.h"
#include "item_data.h"
#include "log.h"
#include "race.h"
#include "target_lock.h"
#include "world_map.h"
#include "world_items.h"

#define SAVE_EQUIP_SLOT_COUNT MAX_EQUIPMENT_SLOTS
#define SAVEGAME_VERSION 18

static void savegame_timestamp_now(char out[JOURNAL_TIMESTAMP_LENGTH])
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

void savegame_build_slot_path(int slot_index, char* out_path, size_t out_size)
{
    if(!out_path || out_size == 0)
        return;

    if(slot_index < 1)
        slot_index = 1;
    if(slot_index > SAVEGAME_SLOT_COUNT)
        slot_index = SAVEGAME_SLOT_COUNT;

    snprintf(out_path, out_size, "savegame_slot_%d.ini", slot_index);
}

void savegame_resolve_slot_path(int slot_index, char* out_path, size_t out_size)
{
    if(!out_path || out_size == 0)
        return;

    savegame_build_slot_path(slot_index, out_path, out_size);
    if(slot_index == 1 && !savegame_exists(out_path) && savegame_exists(SAVEGAME_FILE))
        snprintf(out_path, out_size, "%s", SAVEGAME_FILE);
}

static void savegame_slot_info_defaults(int slot_index, SavegameSlotInfo* out_info)
{
    if(!out_info)
        return;

    memset(out_info, 0, sizeof(*out_info));
    out_info->slot_index = slot_index;
    out_info->level = 1;
    snprintf(out_info->player_name, sizeof(out_info->player_name), "Unknown");
    snprintf(out_info->area_name, sizeof(out_info->area_name), "Unknown");
    out_info->created_timestamp[0] = '\0';
    out_info->last_saved_timestamp[0] = '\0';
}

static char* savegame_trim_whitespace(char* text)
{
    char* end;

    if(!text)
        return text;

    while(*text && isspace((unsigned char)*text))
        text++;

    if(*text == '\0')
        return text;

    end = text + strlen(text) - 1;
    while(end > text && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }

    return text;
}

static void savegame_clamp_player_position(Player* player)
{
    if(!player)
        return;

    if(player->character.actor.entity.z < AREA_MIN_Z)
        player->character.actor.entity.z = AREA_MIN_Z;
    if(player->character.actor.entity.z > AREA_MAX_Z)
        player->character.actor.entity.z = AREA_MAX_Z;

    if(!current_area || current_area->width <= 0 || current_area->height <= 0)
        return;

    if(player->character.actor.entity.x < 0)
        player->character.actor.entity.x = 0;
    if(player->character.actor.entity.x >= current_area->width)
        player->character.actor.entity.x = current_area->width - 1;

    if(player->character.actor.entity.y < 0)
        player->character.actor.entity.y = 0;
    if(player->character.actor.entity.y >= current_area->height)
        player->character.actor.entity.y = current_area->height - 1;
}

static int savegame_key_matches_one_index(const char* key, const char* format, int* out_index)
{
    int index;
    char expected[64];

    if(!key || !format || !out_index)
        return 0;

    if(sscanf(key, format, &index) != 1)
        return 0;

    snprintf(expected, sizeof(expected), format, index);
    if(strcmp(key, expected) != 0)
        return 0;

    *out_index = index;
    return 1;
}

static int savegame_parse_non_weapon_skill_key(const char* key, NonWeaponSkillType* out_skill, int* out_is_xp)
{
    if(!key || !out_skill || !out_is_xp)
        return 0;

    for(int i = 0; i < NON_WEAPON_SKILL_COUNT; i++)
    {
        char level_key[64];
        char xp_key[64];
        const char* stem = non_weapon_skill_save_key((NonWeaponSkillType)i);

        snprintf(level_key, sizeof(level_key), "%s_skill", stem);
        snprintf(xp_key, sizeof(xp_key), "%s_skill_xp", stem);

        if(strcmp(key, level_key) == 0)
        {
            *out_skill = (NonWeaponSkillType)i;
            *out_is_xp = 0;
            return 1;
        }

        if(strcmp(key, xp_key) == 0)
        {
            *out_skill = (NonWeaponSkillType)i;
            *out_is_xp = 1;
            return 1;
        }
    }

    if(strcmp(key, "husbandry_skill") == 0)
    {
        *out_skill = NON_WEAPON_SKILL_ANIMAL_HANDLING;
        *out_is_xp = 0;
        return 1;
    }

    if(strcmp(key, "husbandry_skill_xp") == 0)
    {
        *out_skill = NON_WEAPON_SKILL_ANIMAL_HANDLING;
        *out_is_xp = 1;
        return 1;
    }

    if(strcmp(key, "woodcutting_skill") == 0)
    {
        *out_skill = NON_WEAPON_SKILL_LUMBERJACKING;
        *out_is_xp = 0;
        return 1;
    }

    if(strcmp(key, "woodcutting_skill_xp") == 0)
    {
        *out_skill = NON_WEAPON_SKILL_LUMBERJACKING;
        *out_is_xp = 1;
        return 1;
    }

    return 0;
}

static int savegame_key_matches_two_indices(const char* key,
                                            const char* format,
                                            int* out_first,
                                            int* out_second)
{
    int first;
    int second;
    char expected[64];

    if(!key || !format || !out_first || !out_second)
        return 0;

    if(sscanf(key, format, &first, &second) != 2)
        return 0;

    snprintf(expected, sizeof(expected), format, first, second);
    if(strcmp(key, expected) != 0)
        return 0;

    *out_first = first;
    *out_second = second;
    return 1;
}

int savegame_slot_exists(int slot_index)
{
    char resolved[SAVEGAME_SLOT_PATH_LENGTH];

    if(slot_index < 1 || slot_index > SAVEGAME_SLOT_COUNT)
        return 0;

    savegame_resolve_slot_path(slot_index, resolved, sizeof(resolved));
    return savegame_exists(resolved);
}

int savegame_read_slot_info(int slot_index, SavegameSlotInfo* out_info)
{
    char path[SAVEGAME_SLOT_PATH_LENGTH];
    FILE* file;
    char line[256];

    if(!out_info)
        return 0;

    if(slot_index < 1 || slot_index > SAVEGAME_SLOT_COUNT)
    {
        savegame_slot_info_defaults(slot_index, out_info);
        return 0;
    }

    savegame_slot_info_defaults(slot_index, out_info);
    savegame_resolve_slot_path(slot_index, path, sizeof(path));
    if(!savegame_exists(path))
        return 0;

    file = fopen(path, "r");
    if(!file)
        return 0;

    out_info->occupied = 1;
    out_info->from_legacy_file = (slot_index == 1 && strcmp(path, SAVEGAME_FILE) == 0);

    while(fgets(line, sizeof(line), file))
    {
        char* eq;
        char* key;
        char* value;

        key = savegame_trim_whitespace(line);
        if(key[0] == '\0' || key[0] == '#' || key[0] == ';' || key[0] == '[')
            continue;

        eq = strchr(key, '=');
        if(!eq)
            continue;

        *eq = '\0';
        value = savegame_trim_whitespace(eq + 1);
        value[strcspn(value, "\r\n")] = '\0';
        key = savegame_trim_whitespace(key);

        if(strcmp(key, "player_name") == 0)
            snprintf(out_info->player_name, sizeof(out_info->player_name), "%s", value[0] ? value : "Unknown");
        else if(strcmp(key, "level") == 0)
            out_info->level = atoi(value);
        else if(strcmp(key, "area_name") == 0)
            snprintf(out_info->area_name, sizeof(out_info->area_name), "%s", value[0] ? value : "Unknown");
        else if(strcmp(key, "playtime_seconds") == 0)
            out_info->playtime_seconds = strtoull(value, NULL, 10);
        else if(strcmp(key, "created_timestamp") == 0)
            snprintf(out_info->created_timestamp, sizeof(out_info->created_timestamp), "%s", value);
        else if(strcmp(key, "last_saved_timestamp") == 0)
            snprintf(out_info->last_saved_timestamp, sizeof(out_info->last_saved_timestamp), "%s", value);
    }

    fclose(file);
    return 1;
}

int savegame_list_slots(SavegameSlotInfo* out_infos, int max_slots)
{
    int occupied_count = 0;

    if(!out_infos || max_slots <= 0)
        return 0;

    for(int i = 0; i < max_slots && i < SAVEGAME_SLOT_COUNT; i++)
    {
        int slot_index = i + 1;
        if(savegame_read_slot_info(slot_index, &out_infos[i]))
            occupied_count++;
    }

    return occupied_count;
}

int savegame_delete_slot(int slot_index)
{
    char path[SAVEGAME_SLOT_PATH_LENGTH];
    savegame_resolve_slot_path(slot_index, path, sizeof(path));
    return remove(path) == 0 ? 1 : 0;
}

/**
 * @brief Populate an array of 28 pointers to all equipped item slots in order.
 *        Array order: weapons (2), armor (9), clothing (8), accessories (7).
 * @param c The character to collect equipment from.
 * @param slots Array of 28 Item* pointers (must be pre-allocated).
 * @note Attached containers are persisted separately via container_N_* keys.
 */
static void collect_equipment_slots(Character* c, Item** slots)
{
    // Use the equipment_slots array directly
    for(int i = 0; i < EQUIP_SLOT_COUNT; ++i) {
        slots[i] = &c->equipment_slots[i].item;
    }
}

/**
 * @brief Clear an item slot, setting it to ITEM_TYPE_NONE with default values.
 * @param item Pointer to the Item to clear.
 */
static void clear_item(Item* item)
{
    item_init(item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
}

static int restore_generated_item(Item* item, const char* item_name, ItemQuality quality, int quantity)
{
    if(!item || !item_name || item_name[0] == '\0')
        return 0;

    if(quantity < 1)
        quantity = 1;

    if(strcmp(item_name, "Arrow") == 0 || strcmp(item_name, "Bolt") == 0)
    {
        item_init(item, item_name, ',', -1, -1, ITEM_TYPE_CONSUMABLE, 1, quantity);
        item->stack_max = 99;
        item->is_ammo = 1;
        item->damage_type_mask = DAMAGE_TYPE_PIERCING;
        item->object.base.color = RENDER_COLOR_LIGHT_YELLOW;
        item_apply_quality(item, quality);
        return 1;
    }

    if(strcmp(item_name, "Gold Coins") == 0)
    {
        item_init(item, item_name, '$', -1, -1, ITEM_TYPE_KEY, 1, quantity);
        item->stack_max = 999;
        item->object.base.color = RENDER_COLOR_LIGHT_YELLOW;
        item_apply_quality(item, quality);
        return 1;
    }

    return 0;
}

static int restore_item_from_saved_name(Item* item, const char* item_name, ItemQuality quality, int quantity)
{
    const ItemTemplate* tmpl;
    const char* resolved_name = item_name;

    if(!item)
        return 0;

    if(!item_name || item_name[0] == '\0' || strcmp(item_name, "None") == 0)
    {
        clear_item(item);
        return 1;
    }

    if(strcmp(item_name, "Lumber") == 0)
        resolved_name = "Log";
    else if(strcmp(item_name, "Oak Lumber") == 0)
        resolved_name = "Oak Log";
    else if(strcmp(item_name, "Spruce Lumber") == 0)
        resolved_name = "Spruce Log";
    else if(strcmp(item_name, "Pine Lumber") == 0)
        resolved_name = "Pine Log";
    else if(strcmp(item_name, "Birch Lumber") == 0)
        resolved_name = "Birch Log";
    else if(strcmp(item_name, "Yew Lumber") == 0)
        resolved_name = "Yew Log";
    else if(strcmp(item_name, "Maple Lumber") == 0)
        resolved_name = "Maple Log";

    if(quantity < 1)
        quantity = 1;

    tmpl = item_template_by_name(resolved_name);
    if(tmpl)
    {
        item_init_from_template_with_quality(item, tmpl, -1, -1, quality);
        item->quantity = quantity;
        return 1;
    }

    if(restore_generated_item(item, item_name, quality, quantity))
        return 1;

    clear_item(item);
    return 0;
}

/**
 * @brief Serialize an item to a key=value line in INI format.
 *        Format: key=ItemName|quality|quantity (e.g., "right_hand=Iron Sword|good|1").
 * @param file The FILE* to write to (should be opened for writing).
 * @param key The INI key name for this equipment slot.
 * @param item The Item to serialize (NULL or ITEM_TYPE_NONE becomes "None|regular|0").
 */
static void save_item(FILE* file, const char* key, const Item* item)
{
    const char* name = (!item || item->type == ITEM_TYPE_NONE) ? "None" : item->name;
    const char* quality = (!item || item->type == ITEM_TYPE_NONE) ? "regular" : item_quality_name(item->quality);
    int quantity = (!item || item->type == ITEM_TYPE_NONE) ? 0 : ((item->quantity > 0) ? item->quantity : 1);
    fprintf(file, "%s=%s|%s|%d\n", key, name, quality, quantity);
}

/**
 * @brief Deserialize an item from INI value format (ItemName|quality|quantity).
 *        Also accepts legacy ItemName|quantity entries and treats them as regular quality.
 * @param item Pointer to the Item to populate (will be overwritten).
 * @param value The de-serialized value string.
 * @note If template lookup fails or value is "None", item is cleared to ITEM_TYPE_NONE.
 */
static void load_item_value(Item* item, const char* value)
{
    char buffer[128];
    char* quantity_sep;
    char* quality_sep;
    int quantity;
    ItemQuality quality = ITEM_QUALITY_REGULAR;

    if(!item || !value)
        return;

    snprintf(buffer, sizeof(buffer), "%s", value);
    quantity_sep = strrchr(buffer, '|');
    if(!quantity_sep)
    {
        clear_item(item);
        return;
    }

    *quantity_sep = '\0';
    quantity = atoi(quantity_sep + 1);
    quality_sep = strrchr(buffer, '|');
    if(quality_sep)
    {
        *quality_sep = '\0';
        quality = item_quality_from_string(quality_sep + 1);
    }

    (void)restore_item_from_saved_name(item, buffer, quality, quantity);
}

static void sanitize_save_line(char* out, size_t out_size, const char* in)
{
    size_t w = 0;

    if(!out || out_size == 0)
        return;

    out[0] = '\0';
    if(!in)
        return;

    while(in[w] && w < out_size - 1)
    {
        char ch = in[w];
        out[w] = (ch == '\n' || ch == '\r') ? ' ' : ch;
        w++;
    }

    out[w] = '\0';
}

int savegame_exists(const char* path)
{
    FILE* file = fopen(path, "r");
    if(!file)
        return 0;
    fclose(file);
    return 1;
}

int savegame_save(const char* path, const Player* player)
{
    FILE* file;
    Item* equip_slots[SAVE_EQUIP_SLOT_COUNT];
    int overworld_x = 0;
    int overworld_y = 0;
    int road_count = 0;
    char last_saved_ts[JOURNAL_TIMESTAMP_LENGTH];

    if(!path || !player || !current_area)
        return 0;

    file = fopen(path, "w");
    if(!file)
        return 0;

    savegame_timestamp_now(last_saved_ts);

    fprintf(file, "save_version=%d\n", SAVEGAME_VERSION);
    fprintf(file, "created_timestamp=%s\n", player->created_timestamp[0] ? player->created_timestamp : last_saved_ts);
    fprintf(file, "last_saved_timestamp=%s\n", last_saved_ts);
    fprintf(file, "playtime_seconds=%llu\n", player->playtime_seconds);
    fprintf(file, "player_name=%s\n", player->character.name);
    fprintf(file, "player_race_id=%s\n", player->character.actor.race_id);
    fprintf(file, "area_name=%s\n", current_area->name);
    fprintf(file, "player_x=%d\n", player->character.actor.entity.x);
    fprintf(file, "player_y=%d\n", player->character.actor.entity.y);
    fprintf(file, "player_z=%d\n", player->character.actor.entity.z);
    fprintf(file, "target_lock_active=%d\n", player->target_lock.active);
    fprintf(file, "target_lock_kind=%d\n", player->target_lock.kind);
    fprintf(file, "target_lock_slot=%d\n", player->target_lock.slot_index);
    fprintf(file, "target_lock_z=%d\n", player->target_lock.z);
    fprintf(file, "target_lock_area=%s\n", player->target_lock.area_name);
    fprintf(file, "health=%d\n", player->character.actor.health);
    fprintf(file, "max_health=%d\n", player->character.actor.max_health);
    fprintf(file, "stamina=%d\n", player->character.actor.stamina);
    fprintf(file, "max_stamina=%d\n", player->character.actor.max_stamina);
    fprintf(file, "action_points=%d\n", player->character.actor.action_points);
    fprintf(file, "max_action_points=%d\n", player->character.actor.max_action_points);
    fprintf(file, "exhaustion=%d\n", player->exhaustion);
    fprintf(file, "travelling=%d\n", player->travelling ? 1 : 0);
    fprintf(file, "strength=%d\n", player->character.actor.strength);
    fprintf(file, "constitution=%d\n", player->character.actor.constitution);
    fprintf(file, "endurance=%d\n", player->character.actor.endurance);
    fprintf(file, "agility=%d\n", player->character.actor.agility);
    fprintf(file, "dexterity=%d\n", player->character.actor.dexterity);
    fprintf(file, "speed=%d\n", player->character.actor.speed);
    fprintf(file, "intellect=%d\n", player->character.actor.intellect);
    fprintf(file, "wisdom=%d\n", player->character.actor.wisdom);
    fprintf(file, "resolve=%d\n", player->character.actor.resolve);
    fprintf(file, "composure=%d\n", player->character.actor.composure);
    fprintf(file, "charisma=%d\n", player->character.actor.charisma);
    fprintf(file, "beauty=%d\n", player->character.actor.beauty);
    fprintf(file, "perception=%d\n", player->character.actor.perception);
    fprintf(file, "wits=%d\n", player->character.actor.wits);
    fprintf(file, "willpower=%d\n", player->character.actor.willpower);
    fprintf(file, "max_willpower=%d\n", player->character.actor.max_willpower);
    fprintf(file, "mana=%d\n", player->character.actor.mana);
    fprintf(file, "max_mana=%d\n", player->character.actor.max_mana);
    fprintf(file, "armor_rating=%d\n", player->character.actor.armor_rating);
    fprintf(file, "dodge=%d\n", player->character.actor.dodge);
    fprintf(file, "block=%d\n", player->character.actor.block);
    fprintf(file, "parry=%d\n", player->character.actor.parry);
    fprintf(file, "experience=%d\n", player->experience);
    fprintf(file, "level=%d\n", player->level);
    fprintf(file, "gold=%d\n", player->gold);
    fprintf(file, "selected_attack_mode=%d\n", (int)player->selected_attack_mode);
    fprintf(file, "versatile_grip_mode=%d\n", (int)player->character.versatile_grip_mode);

    if(world_map_get_overworld_position(&overworld_x, &overworld_y))
    {
        fprintf(file, "overworld_x=%d\n", overworld_x);
        fprintf(file, "overworld_y=%d\n", overworld_y);
    }

    for(int y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        for(int x = 0; x < WORLD_MAP_WIDTH; x++)
        {
            if(world_map_get_road_tier(x, y) > WORLD_MAP_ROAD_TIER_NONE)
                road_count++;
        }
    }

    fprintf(file, "road_tile_count=%d\n", road_count);
    if(road_count > 0)
    {
        int road_index = 0;
        for(int y = 0; y < WORLD_MAP_HEIGHT; y++)
        {
            for(int x = 0; x < WORLD_MAP_WIDTH; x++)
            {
                int road_tier = world_map_get_road_tier(x, y);
                if(road_tier <= WORLD_MAP_ROAD_TIER_NONE)
                    continue;

                fprintf(file, "road_tile_%d=%d|%d|%d\n", road_index, x, y, road_tier);
                road_index++;
            }
        }
    }

    for(int i = 0; i < WEAPON_SKILL_COUNT; i++)
    {
        fprintf(file, "weapon_skill_%d=%d\n", i, player->character.actor.weapon_skill[i]);
        fprintf(file, "weapon_skill_xp_%d=%d\n", i, player->character.actor.weapon_skill_xp[i]);
    }
    for(int i = 0; i < NON_WEAPON_SKILL_COUNT; i++)
    {
        const NonWeaponSkillType skill_type = (NonWeaponSkillType)i;
        const char* skill_key = non_weapon_skill_save_key(skill_type);

        fprintf(file, "%s_skill=%d\n", skill_key, actor_get_non_weapon_skill(&player->character.actor, skill_type));
        fprintf(file, "%s_skill_xp=%d\n", skill_key, actor_get_non_weapon_skill_xp(&player->character.actor, skill_type));
    }


    // Save all equipment/inventory slots in unified slot-based system
    for(int i = 0; i < player->character.equipment_slot_count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "equip_%d", i);
        // Save slot type for clarity and future-proofing
        fprintf(file, "%s_type=%d\n", key, player->character.equipment_slots[i].slot_type);
        save_item(file, key, &player->character.equipment_slots[i].item);
    }



    for(int i = 0; i < MAX_CREATURES; i++)
    {
        char key[32];
        if(!creatures[i].alive || !creatures[i].template)
            continue;
        snprintf(key, sizeof(key), "creature_%d", i);
        fprintf(file, "%s=%s|%d|%d|%d|%d|%d|%d|%d\n",
                key,
                creatures[i].template->name,
                creatures[i].actor.entity.x,
                creatures[i].actor.entity.y,
                creatures[i].actor.entity.z,
                creatures[i].actor.health,
                creatures[i].alive,
                creatures[i].disposition,
                (int)creatures[i].taming_stage);
    }

    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        char key[32];
        if(!world_items[i].active)
            continue;
        snprintf(key, sizeof(key), "world_item_%d", i);
        fprintf(file, "%s=%s|%s|%s|%d|%d|%d|%d\n",
                key,
                world_items[i].area_name,
                world_items[i].item.name,
                item_quality_name(world_items[i].item.quality),
                world_items[i].item.object.base.x,
                world_items[i].item.object.base.y,
                world_items[i].item.object.base.z,
                world_items[i].item.quantity);
    }

    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        char key[48];
        const WorldContainer* wc = &world_containers[i];

        if(!wc->active)
            continue;

        fprintf(file,
                "world_container_%d=%s|%d|%d|%d|%s|%d\n",
                i,
                wc->area_name,
                wc->x,
                wc->y,
                wc->z,
                wc->label,
                wc->item_count);

        for(int j = 0; j < wc->item_count && j < WORLD_CONTAINER_CAPACITY; j++)
        {
            snprintf(key, sizeof(key), "world_container_%d_item_%d", i, j);
            save_item(file, key, &wc->items[j]);
        }
    }

    for(int i = 0; i < MAX_WORLD_CORPSES; i++)
    {
        const WorldCorpse* corpse = &world_corpses[i];

        if(!corpse->active)
            continue;

        fprintf(file,
                "world_corpse_%d=%d|%s|%s|%d|%d|%d|%d|%d|%d\n",
                i,
                (int)corpse->type,
                corpse->area_name,
                corpse->source_name,
                corpse->x,
                corpse->y,
                corpse->z,
                corpse->world_container_index,
                corpse->skinned ? 1 : 0,
                corpse->butchered ? 1 : 0);

        for(int j = 0; j < corpse->skinning_loot_count && j < MAX_WORLD_CORPSE_LOOT_ENTRIES; j++)
        {
            fprintf(file,
                    "world_corpse_%d_skin_%d=%s|%d|%d|%d\n",
                    i,
                    j,
                    corpse->skinning_loot[j].item_name,
                    corpse->skinning_loot[j].chance_percent,
                    corpse->skinning_loot[j].min_quantity,
                    corpse->skinning_loot[j].max_quantity);
        }

        for(int j = 0; j < corpse->butchering_loot_count && j < MAX_WORLD_CORPSE_LOOT_ENTRIES; j++)
        {
            fprintf(file,
                    "world_corpse_%d_butcher_%d=%s|%d|%d|%d\n",
                    i,
                    j,
                    corpse->butchering_loot[j].item_name,
                    corpse->butchering_loot[j].chance_percent,
                    corpse->butchering_loot[j].min_quantity,
                    corpse->butchering_loot[j].max_quantity);
        }
    }

    for(int area_i = 0; area_i < MAX_AREAS; area_i++)
    {
        int mutation_write_index = 0;
        int tree_write_index = 0;
        int discovered_write_index = 0;
        const AtlasLocationInfo* info = atlas_get_location_info(area_i);

        fprintf(file, "location_knowledge_%d=%d\n", area_i, (int)atlas_get_knowledge(area_i));
        fprintf(file, "location_ts_aware_%d=%s\n", area_i, (info && info->first_aware_ts[0]) ? info->first_aware_ts : "");
        fprintf(file, "location_ts_located_%d=%s\n", area_i, (info && info->first_located_ts[0]) ? info->first_located_ts : "");
        fprintf(file, "location_ts_scouted_%d=%s\n", area_i, (info && info->first_scouted_ts[0]) ? info->first_scouted_ts : "");
        fprintf(file, "location_ts_first_visit_%d=%s\n", area_i, (info && info->first_visit_ts[0]) ? info->first_visit_ts : "");
        fprintf(file, "location_ts_latest_visit_%d=%s\n", area_i, (info && info->latest_visit_ts[0]) ? info->latest_visit_ts : "");

        if(info)
        {
            fprintf(file, "location_hint_count_%d=%d\n", area_i, info->hint_count);
            for(int hint_i = 0; hint_i < info->hint_count && hint_i < ATLAS_LOCATION_HINT_MAX; hint_i++)
            {
                char safe_hint[ATLAS_LOCATION_HINT_LENGTH];
                sanitize_save_line(safe_hint, sizeof(safe_hint), info->hints[hint_i]);
                fprintf(file, "location_hint_%d_%d=%s\n", area_i, hint_i, safe_hint);
            }
        }

        for(int tree_i = 0; tree_i < MAX_AREA_TREE_STATES; tree_i++)
        {
            const TreeDurabilityState* tree_state = &atlas[area_i].tree_states[tree_i];
            int species_raw;

            if(!tree_state->active)
                continue;

            species_raw = (tree_state->species > TREE_SPECIES_NONE && tree_state->species < TREE_SPECIES_COUNT)
                ? (int)tree_state->species
                : (int)TREE_SPECIES_OAK;

            fprintf(file, "tree_state_%d_%d=%d|%d|%d|%d|%d\n",
                    area_i,
                    tree_write_index,
                    tree_state->x,
                    tree_state->y,
                    tree_state->z,
                    tree_state->structure_points,
                    species_raw);
            tree_write_index++;
        }

        for(int mut_i = 0; mut_i < MAX_AREA_TILE_MUTATIONS; mut_i++)
        {
            const TileMutation* mutation = &atlas[area_i].tile_mutations[mut_i];
            if(!mutation->active)
                continue;

                fprintf(file, "tile_mutation_%d_%d=%d|%d|%d|%d|%d\n",
                    area_i,
                    mutation_write_index,
                    mutation->x,
                    mutation->y,
                    mutation->z,
                    (int)mutation->state,
                    (int)mutation->layer);
            mutation_write_index++;
        }

        for(int y = 0; y < atlas[area_i].height; y++)
        {
            for(int x = 0; x < atlas[area_i].width; x++)
            {
                if(!map_is_tile_discovered(&atlas[area_i], x, y))
                    continue;

                fprintf(file,
                        "area_discovered_%d_%d=%d|%d\n",
                        area_i,
                        discovered_write_index,
                        x,
                        y);
                discovered_write_index++;
            }
        }

        fprintf(file, "area_discovered_count_%d=%d\n", area_i, discovered_write_index);

        for(int furn_i = 0; furn_i < atlas[area_i].furniture_count; furn_i++)
        {
            const Furniture* furn = &atlas[area_i].furniture[furn_i];
            if(!furn || furn->type == FURNITURE_NONE)
                continue;
            if(!furn->is_open && furn->fuel_units <= 0 && !furn->is_ignited)
                continue;

            fprintf(file,
                    "furniture_state_%d_%d=%d|%d|%d|%d|%d|%d|%d\n",
                    area_i,
                    furn_i,
                    furn->base.base.x,
                    furn->base.base.y,
                    furn->base.base.z,
                    (int)furn->type,
                    furn->fuel_units,
                    furn->is_ignited ? 1 : 0,
                    furn->is_open ? 1 : 0);
        }
    }

    fprintf(file, "journal_count=%d\n", player->journal_count);
    for(int i = 0; i < JOURNAL_MAX_ENTRIES; i++)
    {
        char key[32];
        char ts_key[32];
        char safe_entry[JOURNAL_ENTRY_LENGTH];
        char safe_ts[JOURNAL_TIMESTAMP_LENGTH];

        snprintf(key, sizeof(key), "journal_%d", i);
        snprintf(ts_key, sizeof(ts_key), "journal_ts_%d", i);
        sanitize_save_line(safe_entry, sizeof(safe_entry), player->journal_entries[i]);
        sanitize_save_line(safe_ts, sizeof(safe_ts), player->journal_timestamps[i]);
        fprintf(file, "%s=%s\n", key, safe_entry);
        fprintf(file, "%s=%s\n", ts_key, safe_ts);
    }

    fclose(file);
    return 1;
}

int savegame_load(const char* path, Player* player)
{
    FILE* file;
    char line[256];
    Item* equip_slots[SAVE_EQUIP_SLOT_COUNT];
    int has_overworld_x = 0;
    int has_overworld_y = 0;
    int overworld_x = 0;
    int overworld_y = 0;
    int save_version = 1;

    if(!path || !player)
        return 0;

    file = fopen(path, "r");
    if(!file)
        return 0;

    (void)inventory_init(&player->character); // Return value ignored; add error handling if needed
    player->character.equipment_slot_count = MAX_EQUIPMENT_SLOTS;
    player->character.versatile_grip_mode = WEAPON_GRIP_ONE_HANDED;
    player->exhaustion = 0;
    player->travelling = 0;
    player->dragged_world_item_index = -1;
    actor_ensure_base_attributes(&player->character.actor);
    bestiary_init();
    world_items_init();
    for(int area_i = 0; area_i < MAX_AREAS; area_i++)
    {
        map_clear_discovery(&atlas[area_i]);
        atlas_clear_tile_mutations(&atlas[area_i]);

        for(int furn_i = 0; furn_i < atlas[area_i].furniture_count; furn_i++)
        {
            Furniture* furn = &atlas[area_i].furniture[furn_i];
            if(!furn || furn->type == FURNITURE_NONE)
                continue;

            furn->is_open = 0;
            furn->is_ignited = 0;
            furn->fuel_units = 0;
            furniture_refresh(furn);
        }
    }
    collect_equipment_slots(&player->character, equip_slots);

    for(int i = 0; i < WEAPON_SKILL_COUNT; i++)
    {
        player->character.actor.weapon_skill[i] = 0;
        player->character.actor.weapon_skill_xp[i] = 0;
    }
    for(int i = 0; i < NON_WEAPON_SKILL_COUNT; i++)
    {
        player->character.actor.non_weapon_skill[i] = 0;
        player->character.actor.non_weapon_skill_xp[i] = 0;
    }

    while(fgets(line, sizeof(line), file))
    {
        char* equals = strchr(line, '=');
        char* key;
        char* value;
        int index;
        int index2;
        NonWeaponSkillType non_weapon_skill = NON_WEAPON_SKILL_ANIMAL_HANDLING;
        int non_weapon_is_xp = 0;
        int handled_non_weapon_skill;

        if(!equals)
            continue;

        *equals = '\0';
        key = line;
        value = equals + 1;
        value[strcspn(value, "\r\n")] = '\0';
        handled_non_weapon_skill = savegame_parse_non_weapon_skill_key(key, &non_weapon_skill, &non_weapon_is_xp);

        if(strcmp(key, "save_version") == 0)
            save_version = atoi(value);
        else if(strcmp(key, "created_timestamp") == 0)
            snprintf(player->created_timestamp, sizeof(player->created_timestamp), "%s", value);
        else if(strcmp(key, "last_saved_timestamp") == 0)
            snprintf(player->last_saved_timestamp, sizeof(player->last_saved_timestamp), "%s", value);
        else if(strcmp(key, "playtime_seconds") == 0)
            player->playtime_seconds = strtoull(value, NULL, 10);
        else if(strcmp(key, "player_name") == 0)
            snprintf(player->character.name, sizeof(player->character.name), "%s", value);
        else if(strcmp(key, "player_race_id") == 0)
            snprintf(player->character.actor.race_id, sizeof(player->character.actor.race_id), "%s", value);
        else if(strcmp(key, "area_name") == 0)
        {
            int area_index = atlas_find_location(value);
            if(area_index >= 0)
                atlas_travel(area_index);
        }
        else if(strcmp(key, "player_x") == 0)
            player->character.actor.entity.x = atoi(value);
        else if(strcmp(key, "player_y") == 0)
            player->character.actor.entity.y = atoi(value);
        else if(strcmp(key, "player_z") == 0)
            player->character.actor.entity.z = atoi(value);
        else if(strcmp(key, "target_lock_active") == 0)
            player->target_lock.active = atoi(value) ? 1 : 0;
        else if(strcmp(key, "target_lock_kind") == 0)
            player->target_lock.kind = (TargetLockKind)atoi(value);
        else if(strcmp(key, "target_lock_slot") == 0)
            player->target_lock.slot_index = atoi(value);
        else if(strcmp(key, "target_lock_z") == 0)
            player->target_lock.z = atoi(value);
        else if(strcmp(key, "target_lock_area") == 0)
            snprintf(player->target_lock.area_name, sizeof(player->target_lock.area_name), "%s", value);
        else if(strcmp(key, "health") == 0)
            player->character.actor.health = atoi(value);
        else if(strcmp(key, "max_health") == 0)
            player->character.actor.max_health = atoi(value);
        else if(strcmp(key, "stamina") == 0)
            player->character.actor.stamina = atoi(value);
        else if(strcmp(key, "max_stamina") == 0)
            player->character.actor.max_stamina = atoi(value);
        else if(strcmp(key, "action_points") == 0)
            player->character.actor.action_points = atoi(value);
        else if(strcmp(key, "max_action_points") == 0)
            player->character.actor.max_action_points = atoi(value);
        else if(strcmp(key, "exhaustion") == 0 || strcmp(key, "overland_exhaustion") == 0)
            player->exhaustion = atoi(value);
        else if(strcmp(key, "travelling") == 0)
            player->travelling = atoi(value) ? 1 : 0;
        else if(strcmp(key, "strength") == 0)
            player->character.actor.strength = atoi(value);
        else if(strcmp(key, "constitution") == 0)
            player->character.actor.constitution = atoi(value);
        else if(strcmp(key, "endurance") == 0)
            player->character.actor.endurance = atoi(value);
        else if(strcmp(key, "agility") == 0)
            player->character.actor.agility = atoi(value);
        else if(strcmp(key, "dexterity") == 0)
            player->character.actor.dexterity = atoi(value);
        else if(strcmp(key, "speed") == 0)
            player->character.actor.speed = atoi(value);
        else if(strcmp(key, "intellect") == 0)
            player->character.actor.intellect = atoi(value);
        else if(strcmp(key, "wisdom") == 0)
            player->character.actor.wisdom = atoi(value);
        else if(strcmp(key, "resolve") == 0)
            player->character.actor.resolve = atoi(value);
        else if(strcmp(key, "composure") == 0)
            player->character.actor.composure = atoi(value);
        else if(strcmp(key, "charisma") == 0)
            player->character.actor.charisma = atoi(value);
        else if(strcmp(key, "beauty") == 0)
            player->character.actor.beauty = atoi(value);
        else if(strcmp(key, "perception") == 0)
            player->character.actor.perception = atoi(value);
        else if(strcmp(key, "wits") == 0)
            player->character.actor.wits = atoi(value);
        else if(strcmp(key, "willpower") == 0)
            player->character.actor.willpower = atoi(value);
        else if(strcmp(key, "max_willpower") == 0)
            player->character.actor.max_willpower = atoi(value);
        else if(strcmp(key, "mana") == 0)
            player->character.actor.mana = atoi(value);
        else if(strcmp(key, "max_mana") == 0)
            player->character.actor.max_mana = atoi(value);
        else if(strcmp(key, "armor_rating") == 0)
            player->character.actor.armor_rating = atoi(value);
        else if(strcmp(key, "dodge") == 0)
            player->character.actor.dodge = atoi(value);
        else if(strcmp(key, "block") == 0)
            player->character.actor.block = atoi(value);
        else if(strcmp(key, "parry") == 0)
            player->character.actor.parry = atoi(value);
        else if(strcmp(key, "experience") == 0)
            player->experience = atoi(value);
        else if(strcmp(key, "level") == 0)
            player->level = atoi(value);
        else if(strcmp(key, "gold") == 0)
            player->gold = atoi(value);
        else if(strcmp(key, "selected_attack_mode") == 0)
            player->selected_attack_mode = (AttackMode)atoi(value);
        else if(strcmp(key, "versatile_grip_mode") == 0)
            player->character.versatile_grip_mode = (WeaponGripMode)atoi(value);
        else if(strcmp(key, "overworld_x") == 0)
        {
            overworld_x = atoi(value);
            has_overworld_x = 1;
        }
        else if(strcmp(key, "overworld_y") == 0)
        {
            overworld_y = atoi(value);
            has_overworld_y = 1;
        }
        else if(strcmp(key, "road_tile_count") == 0)
        {
            (void)atoi(value);
        }
        else if(strcmp(key, "journal_count") == 0)
        {
            player->journal_count = atoi(value);
            if(player->journal_count < 0) player->journal_count = 0;
            if(player->journal_count > JOURNAL_MAX_ENTRIES) player->journal_count = JOURNAL_MAX_ENTRIES;
        }
        else if(sscanf(key, "weapon_skill_%d", &index) == 1 && index >= 0 && index < WEAPON_SKILL_COUNT)
            player->character.actor.weapon_skill[index] = atoi(value);
        else if(sscanf(key, "weapon_skill_xp_%d", &index) == 1 && index >= 0 && index < WEAPON_SKILL_COUNT)
            player->character.actor.weapon_skill_xp[index] = atoi(value);
        else if(handled_non_weapon_skill)
        {
            if(non_weapon_is_xp)
                player->character.actor.non_weapon_skill_xp[non_weapon_skill] = atoi(value);
            else
                player->character.actor.non_weapon_skill[non_weapon_skill] = atoi(value);
        }
        else if(savegame_key_matches_one_index(key, "equip_%d_type", &index) && index >= 0 && index < MAX_EQUIPMENT_SLOTS)
            player->character.equipment_slots[index].slot_type = (EquipmentSlotType)atoi(value);
        else if(savegame_key_matches_one_index(key, "equip_%d", &index) && index >= 0 && index < MAX_EQUIPMENT_SLOTS)
            load_item_value(&player->character.equipment_slots[index].item, value);

        else if(sscanf(key, "location_knowledge_%d", &index) == 1 && index >= 0 && index < MAX_AREAS)
        {
            int raw_knowledge = atoi(value);

            // Save schema v1 used 0..3 where 3 meant VISITED.
            if(save_version < 2 && raw_knowledge >= LOCATION_KNOWLEDGE_SCOUTED)
                raw_knowledge++;

            atlas_set_knowledge(index, (LocationKnowledge)raw_knowledge);
        }
        else if(sscanf(key, "location_ts_aware_%d", &index) == 1 && index >= 0 && index < MAX_AREAS)
            atlas_set_location_timestamp_aware(index, value);
        else if(sscanf(key, "location_ts_located_%d", &index) == 1 && index >= 0 && index < MAX_AREAS)
            atlas_set_location_timestamp_located(index, value);
        else if(sscanf(key, "location_ts_scouted_%d", &index) == 1 && index >= 0 && index < MAX_AREAS)
            atlas_set_location_timestamp_scouted(index, value);
        else if(sscanf(key, "location_ts_first_visit_%d", &index) == 1 && index >= 0 && index < MAX_AREAS)
            atlas_set_location_timestamp_first_visit(index, value);
        else if(sscanf(key, "location_ts_latest_visit_%d", &index) == 1 && index >= 0 && index < MAX_AREAS)
            atlas_set_location_timestamp_latest_visit(index, value);
        else if(sscanf(key, "location_hint_count_%d", &index) == 1 && index >= 0 && index < MAX_AREAS)
        {
            (void)atoi(value);
            atlas_clear_location_hints(index);
        }
        else
        {
            int hint_area = -1;
            int hint_index = -1;
            if(sscanf(key, "location_hint_%d_%d", &hint_area, &hint_index) == 2 &&
               hint_area >= 0 && hint_area < MAX_AREAS &&
               hint_index >= 0 && hint_index < ATLAS_LOCATION_HINT_MAX)
            {
                atlas_add_location_hint(hint_area, value);
            }
            else if(sscanf(key, "road_tile_%d", &index) == 1)
            {
                int x;
                int y;
                int road_tier;

                if(sscanf(value, "%d|%d|%d", &x, &y, &road_tier) == 3)
                    world_map_set_road_tier(x, y, road_tier);
            }
            else if(sscanf(key, "creature_%d", &index) == 1 && index >= 0 && index < MAX_CREATURES)
            {
                char template_name[64];
                int x;
                int y;
                int z;
                int health;
                int alive;
                int disposition;
                int taming_stage_raw;
                int matched;
                CreatureTemplate* tmpl;
                Creature* creature;

                matched = sscanf(value, "%63[^|]|%d|%d|%d|%d|%d|%d|%d", template_name, &x, &y, &z, &health, &alive, &disposition, &taming_stage_raw);
                if(matched != 8)
                {
                    /* Try v6 (no disposition/taming fields) */
                    disposition = -999;
                    taming_stage_raw = 0;
                    if(sscanf(value, "%63[^|]|%d|%d|%d|%d|%d", template_name, &x, &y, &z, &health, &alive) == 6)
                        matched = 6;
                    else if(sscanf(value, "%63[^|]|%d|%d|%d|%d", template_name, &x, &y, &health, &alive) == 5)
                    {
                        matched = 5;
                        z = 0;
                    }
                    else
                        matched = 0;
                }

                if(matched == 8 || matched == 6 || matched == 5)
                {
                    tmpl = bestiary_template_by_name(template_name);
                    if(tmpl)
                    {
                        creature = &creatures[index];
                        creature->alive = alive;
                        creature->template = tmpl;
                        creature->actor = tmpl->actor;
                        actor_ensure_base_attributes(&creature->actor);
                        creature->actor.entity.x = x;
                        creature->actor.entity.y = y;
                        creature->actor.entity.z = z;
                        creature->actor.entity.symbol = tmpl->symbol;
                        creature->actor.entity.color = tmpl->color;
                        creature->actor.entity.blocks = 1;
                        creature->actor.health = health;
                        creature->move_state = CREATURE_STATE_WANDER;
                        creature->state_turns = 0;
                        creature->move_dx = 0;
                        creature->move_dy = 0;
                        /* Seed disposition from save data; fall back to template baseline for old saves. */
                        creature->disposition = (disposition >= -100 && disposition <= 100)
                            ? disposition
                            : tmpl->base_disposition;
                        creature->taming_stage = (CreatureTamingStage)taming_stage_raw;
                    }
                }
            }
            else if(sscanf(key, "world_item_%d", &index) == 1 && index >= 0 && index < MAX_WORLD_ITEMS)
            {
                char area_name[32];
                char item_name[64];
                char quality_name[24];
                int x;
                int y;
                int z;
                int quantity;
                int matched;
                ItemQuality quality = ITEM_QUALITY_REGULAR;

                matched = sscanf(value,
                                 "%31[^|]|%63[^|]|%23[^|]|%d|%d|%d|%d",
                                 area_name,
                                 item_name,
                                 quality_name,
                                 &x,
                                 &y,
                                 &z,
                                 &quantity);
                if(matched == 7)
                {
                    quality = item_quality_from_string(quality_name);
                }
                else
                {
                    matched = sscanf(value, "%31[^|]|%63[^|]|%d|%d|%d|%d", area_name, item_name, &x, &y, &z, &quantity);
                    if(matched != 6)
                    {
                        if(sscanf(value, "%31[^|]|%63[^|]|%d|%d|%d", area_name, item_name, &x, &y, &quantity) != 5)
                            matched = 0;
                        else
                        {
                            matched = 5;
                            z = 0;
                        }
                    }
                }

                if(matched == 7 || matched == 6 || matched == 5)
                {
                    if(restore_item_from_saved_name(&world_items[index].item, item_name, quality, quantity) &&
                       world_items[index].item.type != ITEM_TYPE_NONE)
                    {
                        world_items[index].active = 1;
                        snprintf(world_items[index].area_name, sizeof(world_items[index].area_name), "%s", area_name);
                        world_items[index].item.object.base.x = x;
                        world_items[index].item.object.base.y = y;
                        world_items[index].item.object.base.z = z;
                    }
                }
            }
            else if(savegame_key_matches_two_indices(key, "world_container_%d_item_%d", &index, &index2) &&
                    index >= 0 && index < MAX_WORLD_CONTAINERS &&
                    index2 >= 0 && index2 < WORLD_CONTAINER_CAPACITY)
            {
                load_item_value(&world_containers[index].items[index2], value);
                if(world_containers[index].items[index2].type != ITEM_TYPE_NONE)
                {
                    if(index2 + 1 > world_containers[index].item_count)
                        world_containers[index].item_count = index2 + 1;
                }
            }
            else if(savegame_key_matches_one_index(key, "world_container_%d", &index) && index >= 0 && index < MAX_WORLD_CONTAINERS)
            {
                char area_name[32];
                char label[48];
                int x;
                int y;
                int z;
                int item_count;

                if(sscanf(value, "%31[^|]|%d|%d|%d|%47[^|]|%d", area_name, &x, &y, &z, label, &item_count) >= 5)
                {
                    world_containers[index].active = 1;
                    snprintf(world_containers[index].area_name, sizeof(world_containers[index].area_name), "%s", area_name);
                    world_containers[index].x = x;
                    world_containers[index].y = y;
                    world_containers[index].z = z;
                    snprintf(world_containers[index].label, sizeof(world_containers[index].label), "%s", label);
                    world_containers[index].item_count = 0;

                    if(sscanf(value, "%31[^|]|%d|%d|%d|%47[^|]|%d", area_name, &x, &y, &z, label, &item_count) == 6)
                    {
                        if(item_count < 0)
                            item_count = 0;
                        if(item_count > WORLD_CONTAINER_CAPACITY)
                            item_count = WORLD_CONTAINER_CAPACITY;
                        world_containers[index].item_count = item_count;
                    }
                }
            }
            else if(savegame_key_matches_two_indices(key, "world_corpse_%d_skin_%d", &index, &index2) &&
                    index >= 0 && index < MAX_WORLD_CORPSES &&
                    index2 >= 0 && index2 < MAX_WORLD_CORPSE_LOOT_ENTRIES)
            {
                WorldCorpseLootEntry* entry = &world_corpses[index].skinning_loot[index2];

                if(sscanf(value, "%31[^|]|%d|%d|%d",
                          entry->item_name,
                          &entry->chance_percent,
                          &entry->min_quantity,
                          &entry->max_quantity) == 4)
                {
                    if(index2 + 1 > world_corpses[index].skinning_loot_count)
                        world_corpses[index].skinning_loot_count = index2 + 1;
                }
            }
            else if(savegame_key_matches_two_indices(key, "world_corpse_%d_butcher_%d", &index, &index2) &&
                    index >= 0 && index < MAX_WORLD_CORPSES &&
                    index2 >= 0 && index2 < MAX_WORLD_CORPSE_LOOT_ENTRIES)
            {
                WorldCorpseLootEntry* entry = &world_corpses[index].butchering_loot[index2];

                if(sscanf(value, "%31[^|]|%d|%d|%d",
                          entry->item_name,
                          &entry->chance_percent,
                          &entry->min_quantity,
                          &entry->max_quantity) == 4)
                {
                    if(index2 + 1 > world_corpses[index].butchering_loot_count)
                        world_corpses[index].butchering_loot_count = index2 + 1;
                }
            }
            else if(savegame_key_matches_one_index(key, "world_corpse_%d", &index) && index >= 0 && index < MAX_WORLD_CORPSES)
            {
                char area_name[32];
                char source_name[32];
                int type_raw = (int)WORLD_CORPSE_CREATURE;
                int x = 0;
                int y = 0;
                int z = AREA_GROUND_Z;
                int container_index = -1;
                int skinned = 0;
                int butchered = 0;
                int matched;

                matched = sscanf(value,
                                 "%d|%31[^|]|%31[^|]|%d|%d|%d|%d|%d|%d",
                                 &type_raw,
                                 area_name,
                                 source_name,
                                 &x,
                                 &y,
                                 &z,
                                 &container_index,
                                 &skinned,
                                 &butchered);
                if(matched >= 6)
                {
                    if(matched < 7)
                        container_index = -1;
                    if(matched < 8)
                        skinned = 0;
                    if(matched < 9)
                        butchered = 0;

                    world_corpses[index].active = 1;
                    world_corpses[index].type = (WorldCorpseType)type_raw;
                    snprintf(world_corpses[index].area_name, sizeof(world_corpses[index].area_name), "%s", area_name);
                    snprintf(world_corpses[index].source_name, sizeof(world_corpses[index].source_name), "%s", source_name);
                    world_corpses[index].x = x;
                    world_corpses[index].y = y;
                    world_corpses[index].z = z;
                    world_corpses[index].world_container_index = container_index;
                    world_corpses[index].skinned = skinned ? 1 : 0;
                    world_corpses[index].butchered = butchered ? 1 : 0;
                    world_corpses[index].skinning_loot_count = 0;
                    world_corpses[index].butchering_loot_count = 0;
                    world_corpse_refresh_label(&world_corpses[index]);
                }
            }
            else if(savegame_key_matches_two_indices(key, "furniture_state_%d_%d", &index, &index2) && index >= 0 && index < MAX_AREAS)
            {
                int x;
                int y;
                int z;
                int type_raw = 0;
                int fuel_units = 0;
                int ignited = 0;
                int is_open = 0;
                Furniture* furn = NULL;
                int parsed = sscanf(value, "%d|%d|%d|%d|%d|%d|%d", &x, &y, &z, &type_raw, &fuel_units, &ignited, &is_open);

                if(parsed == 5)
                {
                    ignited = fuel_units;
                    fuel_units = type_raw;
                    type_raw = 0;
                }
                else if(parsed < 5)
                {
                    parsed = 0;
                }

                if(parsed >= 5)
                {
                    furn = furniture_at_3d(&atlas[index], x, y, z);
                    if(furn && (type_raw == 0 || furn->type == (FurnitureType)type_raw))
                    {
                        if(fuel_units < 0)
                            fuel_units = 0;
                        if(fuel_units > FURNITURE_FORGE_MAX_FUEL_UNITS)
                            fuel_units = FURNITURE_FORGE_MAX_FUEL_UNITS;
                        furn->is_open = (parsed >= 7 && is_open != 0) ? 1 : 0;
                        furn->fuel_units = fuel_units;
                        furn->is_ignited = (ignited != 0 && fuel_units > 0) ? 1 : 0;
                        furniture_refresh(furn);
                    }
                }
            }
            else
            {
                int area_index;
                int mutation_index;
                int discovered_index;
                if(sscanf(key, "tile_mutation_%d_%d", &area_index, &mutation_index) == 2)
                {
                    int x;
                    int y;
                    int z = AREA_GROUND_Z;
                    int state_raw;
                    int layer_raw = TILE_LAYER_WALL;
                    int parsed = 0;

                    (void)mutation_index;

                    if(area_index < 0 || area_index >= MAX_AREAS)
                        continue;

                    parsed = sscanf(value, "%d|%d|%d|%d|%d", &x, &y, &z, &state_raw, &layer_raw);
                    if(parsed < 4)
                    {
                        z = AREA_GROUND_Z;
                        parsed = sscanf(value, "%d|%d|%d|%d", &x, &y, &state_raw, &layer_raw);
                    }

                    if(parsed >= 3)
                    {
                        TileMutationState state = (TileMutationState)state_raw;
                        TileLayer layer = TILE_LAYER_WALL;

                        if(parsed == 5 || parsed == 4)
                        {
                            if(layer_raw >= 0 && layer_raw < TILE_LAYER_COUNT)
                                layer = (TileLayer)layer_raw;
                        }

                        if(state > TILE_MUTATION_STATE_NONE && state <= TILE_MUTATION_STATE_TREE_STUMP)
                        {
                            if(layer == TILE_LAYER_WALL)
                                atlas_set_tile_mutation_at_z(&atlas[area_index], x, y, z, state);
                        }
                    }
                    continue;
                }

                if(sscanf(key, "tree_state_%d_%d", &area_index, &discovered_index) == 2)
                {
                    int x;
                    int y;
                    int z = AREA_GROUND_Z;
                    int structure_points = 0;
                    int species_raw = (int)TREE_SPECIES_OAK;
                    int parsed;

                    if(area_index < 0 || area_index >= MAX_AREAS)
                        continue;

                    parsed = sscanf(value, "%d|%d|%d|%d|%d", &x, &y, &z, &structure_points, &species_raw);
                    if(parsed < 5)
                    {
                        species_raw = (int)TREE_SPECIES_OAK;
                        parsed = sscanf(value, "%d|%d|%d|%d", &x, &y, &z, &structure_points);
                    }
                    if(parsed < 4)
                    {
                        z = AREA_GROUND_Z;
                        species_raw = (int)TREE_SPECIES_OAK;
                        parsed = sscanf(value, "%d|%d|%d", &x, &y, &structure_points);
                    }

                    if(parsed >= 3 && structure_points >= 0)
                    {
                        Area* area = &atlas[area_index];
                        TreeDurabilityState* target_state = NULL;
                        TreeSpecies species = (species_raw > (int)TREE_SPECIES_NONE && species_raw < (int)TREE_SPECIES_COUNT)
                            ? (TreeSpecies)species_raw
                            : TREE_SPECIES_OAK;

                        for(int tree_i = 0; tree_i < MAX_AREA_TREE_STATES; tree_i++)
                        {
                            TreeDurabilityState* entry = &area->tree_states[tree_i];

                            if(entry->active)
                            {
                                if(entry->x == x && entry->y == y && entry->z == z)
                                {
                                    target_state = entry;
                                    break;
                                }
                                continue;
                            }

                            if(!target_state)
                                target_state = entry;
                        }

                        if(target_state && !target_state->active)
                            area->tree_state_count++;

                        if(target_state)
                        {
                            Tile* wall_tile;

                            target_state->active = 1;
                            target_state->x = x;
                            target_state->y = y;
                            target_state->z = z;
                            target_state->structure_points = structure_points;
                            target_state->species = species;

                            wall_tile = map_tile_at_layer_z(area, x, y, z, TILE_LAYER_WALL);
                            if(wall_tile && tile_is_tree_stump(wall_tile))
                                *wall_tile = tile_tree_stump_for_species(species);
                        }
                    }
                    continue;
                }

                if(sscanf(key, "area_discovered_%d_%d", &area_index, &discovered_index) == 2)
                {
                    int x;
                    int y;

                    (void)discovered_index;

                    if(area_index < 0 || area_index >= MAX_AREAS)
                        continue;

                    if(sscanf(value, "%d|%d", &x, &y) == 2)
                        map_mark_tile_discovered(&atlas[area_index], x, y);
                    continue;
                }

                if(sscanf(key, "area_discovered_count_%d", &area_index) == 1)
                {
                    (void)atoi(value);
                    continue;
                }
            }
        }
        if(sscanf(key, "journal_%d", &index) == 1 && index >= 0 && index < JOURNAL_MAX_ENTRIES)
        {
            snprintf(player->journal_entries[index], JOURNAL_ENTRY_LENGTH, "%s", value);
        }
        else if(sscanf(key, "journal_ts_%d", &index) == 1 && index >= 0 && index < JOURNAL_MAX_ENTRIES)
        {
            snprintf(player->journal_timestamps[index], JOURNAL_TIMESTAMP_LENGTH, "%s", value);
        }
    }

    fclose(file);

    if(player->created_timestamp[0] == '\0' && player->last_saved_timestamp[0] != '\0')
        snprintf(player->created_timestamp, sizeof(player->created_timestamp), "%s", player->last_saved_timestamp);
    if(player->last_saved_timestamp[0] == '\0' && player->created_timestamp[0] != '\0')
        snprintf(player->last_saved_timestamp, sizeof(player->last_saved_timestamp), "%s", player->created_timestamp);

    update_dynamic_container_slots(&player->character);
    actor_ensure_base_attributes(&player->character.actor);
    player_apply_derived_maximums(player);
    if(player->character.actor.race_id[0] == '\0')
    {
        const RaceTemplate* default_race = race_default_template();
        if(default_race)
            snprintf(player->character.actor.race_id, sizeof(player->character.actor.race_id), "%s", default_race->id);
        else
            snprintf(player->character.actor.race_id, sizeof(player->character.actor.race_id), "%s", "human");
    }
    if(save_version < 11 && player->character.actor.action_points <= 0)
        player->character.actor.action_points = player->character.actor.max_action_points;
    if(player->selected_attack_mode < ATTACK_MODE_NONE || player->selected_attack_mode > ATTACK_MODE_DEADEYE)
        player->selected_attack_mode = ATTACK_MODE_PUNCH;
    if(player->character.versatile_grip_mode < WEAPON_GRIP_ONE_HANDED || player->character.versatile_grip_mode > WEAPON_GRIP_TWO_HANDED)
        player->character.versatile_grip_mode = WEAPON_GRIP_ONE_HANDED;
    player->selected_attack_mode = combat_valid_attack_mode_for_character(&player->character, player->selected_attack_mode);

    if(player->target_lock.kind < TARGET_LOCK_NONE || player->target_lock.kind > TARGET_LOCK_WORLD_ITEM)
        target_lock_clear(player);
    else
        target_lock_resolve(player, NULL, 1);

    if(save_version < 6)
    {
        if(player->character.actor.entity.z < AREA_GROUND_Z)
            player->character.actor.entity.z += AREA_GROUND_Z;
        if(player->target_lock.active && player->target_lock.z < AREA_GROUND_Z)
            player->target_lock.z += AREA_GROUND_Z;

        for(int i = 0; i < MAX_CREATURES; i++)
        {
            if(creatures[i].alive && creatures[i].actor.entity.z < AREA_GROUND_Z)
                creatures[i].actor.entity.z += AREA_GROUND_Z;
        }

        for(int i = 0; i < MAX_WORLD_ITEMS; i++)
        {
            if(world_items[i].active && world_items[i].item.object.base.z < AREA_GROUND_Z)
                world_items[i].item.object.base.z += AREA_GROUND_Z;
        }
    }

    if(player->character.actor.entity.z < AREA_MIN_Z)
        player->character.actor.entity.z = AREA_MIN_Z;
    if(player->character.actor.entity.z > AREA_MAX_Z)
        player->character.actor.entity.z = AREA_MAX_Z;

    if(player->target_lock.z < AREA_MIN_Z)
        player->target_lock.z = AREA_MIN_Z;
    if(player->target_lock.z > AREA_MAX_Z)
        player->target_lock.z = AREA_MAX_Z;

    for(int i = 0; i < MAX_CREATURES; i++)
    {
        if(!creatures[i].alive)
            continue;

        if(creatures[i].actor.entity.z < AREA_MIN_Z)
            creatures[i].actor.entity.z = AREA_MIN_Z;
        if(creatures[i].actor.entity.z > AREA_MAX_Z)
            creatures[i].actor.entity.z = AREA_MAX_Z;
    }

    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        if(!world_items[i].active)
            continue;

        if(world_items[i].item.object.base.z < AREA_MIN_Z)
            world_items[i].item.object.base.z = AREA_MIN_Z;
        if(world_items[i].item.object.base.z > AREA_MAX_Z)
            world_items[i].item.object.base.z = AREA_MAX_Z;
    }

    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        if(!world_containers[i].active)
            continue;

        if(world_containers[i].item_count < 0)
            world_containers[i].item_count = 0;
        if(world_containers[i].item_count > WORLD_CONTAINER_CAPACITY)
            world_containers[i].item_count = WORLD_CONTAINER_CAPACITY;
    }

    if(has_overworld_x && has_overworld_y)
        world_map_set_overworld_position(overworld_x, overworld_y);

    if(player->exhaustion < 0)
        player->exhaustion = 0;

    savegame_clamp_player_position(player);

    if(save_version < 5 && current_area)
    {
        int vision_range = actor_area_vision_range(&player->character.actor);
        map_reveal_from_point(current_area,
                              player->character.actor.entity.x,
                              player->character.actor.entity.y,
                              vision_range);
    }

    return 1;
}