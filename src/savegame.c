#include "savegame.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "atlas.h"
#include "bestiary.h"
#include "inventory.h"
#include "item_data.h"
#include "log.h"
#include "target_lock.h"
#include "world_map.h"
#include "world_items.h"

#define SAVE_EQUIP_SLOT_COUNT 28
#define SAVEGAME_VERSION 9

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

/**
 * @brief Populate an array of 28 pointers to all equipped item slots in order.
 *        Array order: weapons (2), armor (9), clothing (8), accessories (7), bags (2).
 * @param c The character to collect equipment from.
 * @param slots Array of 28 Item* pointers (must be pre-allocated).
 * @note Used for saving and loading character equipment state during persistence.
 */
static void collect_equipment_slots(Character* c, Item** slots)
{
    slots[0] = &c->equipped_right_hand;
    slots[1] = &c->equipped_left_hand;

    slots[2] = &c->equipped_armor_head;
    slots[3] = &c->equipped_armor_face;
    slots[4] = &c->equipped_armor_shoulders;
    slots[5] = &c->equipped_armor_chest;
    slots[6] = &c->equipped_armor_arms;
    slots[7] = &c->equipped_armor_hands;
    slots[8] = &c->equipped_armor_waist;
    slots[9] = &c->equipped_armor_legs;
    slots[10] = &c->equipped_armor_feet;

    slots[11] = &c->equipped_clothing_head;
    slots[12] = &c->equipped_clothing_face;
    slots[13] = &c->equipped_clothing_shoulders;
    slots[14] = &c->equipped_clothing_chest;
    slots[15] = &c->equipped_clothing_hands;
    slots[16] = &c->equipped_clothing_waist;
    slots[17] = &c->equipped_clothing_legs;
    slots[18] = &c->equipped_clothing_feet;

    slots[19] = &c->equipped_accessory_neck;
    slots[20] = &c->equipped_accessory_bracelet_right;
    slots[21] = &c->equipped_accessory_bracelet_left;
    slots[22] = &c->equipped_accessory_finger_right;
    slots[23] = &c->equipped_accessory_finger_left;
    slots[24] = &c->equipped_accessory_trinket_0;
    slots[25] = &c->equipped_accessory_trinket_1;

    slots[26] = &c->equipped_bag_backpack;
    slots[27] = &c->equipped_bag_beltpouch;
}

/**
 * @brief Clear an item slot, setting it to ITEM_TYPE_NONE with default values.
 * @param item Pointer to the Item to clear.
 */
static void clear_item(Item* item)
{
    item_init(item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
}

/**
 * @brief Serialize an item to a key=value line in INI format.
 *        Format: key=ItemName|quantity (e.g., "right_hand=Iron Sword|1").
 * @param file The FILE* to write to (should be opened for writing).
 * @param key The INI key name for this equipment slot.
 * @param item The Item to serialize (NULL or ITEM_TYPE_NONE becomes "None|0").
 */
static void save_item(FILE* file, const char* key, const Item* item)
{
    const char* name = (!item || item->type == ITEM_TYPE_NONE) ? "None" : item->name;
    int quantity = (!item || item->type == ITEM_TYPE_NONE) ? 0 : item->quantity;
    fprintf(file, "%s=%s|%d\n", key, name, quantity);
}

/**
 * @brief Deserialize an item from INI value format (ItemName|quantity).
 *        Looks up the item template by name and reconstructs the item instance.
 * @param item Pointer to the Item to populate (will be overwritten).
 * @param value The de-serialized value string (e.g., "Iron Sword|1").
 * @note If template lookup fails or value is "None", item is cleared to ITEM_TYPE_NONE.
 */
static void load_item_value(Item* item, const char* value)
{
    char buffer[128];
    char* sep;
    const ItemTemplate* tmpl;
    int quantity;

    if(!item || !value)
        return;

    snprintf(buffer, sizeof(buffer), "%s", value);
    sep = strrchr(buffer, '|');
    if(!sep)
    {
        clear_item(item);
        return;
    }

    *sep = '\0';
    quantity = atoi(sep + 1);
    if(strcmp(buffer, "None") == 0 || buffer[0] == '\0')
    {
        clear_item(item);
        return;
    }

    tmpl = item_template_by_name(buffer);
    if(!tmpl)
    {
        clear_item(item);
        return;
    }

    item_init_from_template(item, tmpl, -1, -1);
    item->quantity = quantity;
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
    fprintf(file, "overland_exhaustion=%d\n", player->overland_exhaustion);
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
    fprintf(file, "husbandry_skill=%d\n", player->character.actor.husbandry_skill);
    fprintf(file, "husbandry_skill_xp=%d\n", player->character.actor.husbandry_skill_xp);

    fprintf(file, "inventory_count=%d\n", player->character.inventory_count);
    for(int i = 0; i < INVENTORY_SIZE; i++)
    {
        char key[32];
        snprintf(key, sizeof(key), "inventory_%d", i);
        save_item(file, key, &player->character.inventory[i]);
    }

    collect_equipment_slots((Character*)&player->character, equip_slots);
    for(int i = 0; i < SAVE_EQUIP_SLOT_COUNT; i++)
    {
        char key[32];
        snprintf(key, sizeof(key), "equip_%d", i);
        save_item(file, key, equip_slots[i]);
    }

    fprintf(file, "backpack_count=%d\n", player->character.backpack_count);
    for(int i = 0; i < BACKPACK_CAPACITY; i++)
    {
        char key[32];
        snprintf(key, sizeof(key), "backpack_%d", i);
        save_item(file, key, &player->character.backpack_contents[i]);
    }

    fprintf(file, "beltpouch_count=%d\n", player->character.beltpouch_count);
    for(int i = 0; i < BELTPOUCH_CAPACITY; i++)
    {
        char key[32];
        snprintf(key, sizeof(key), "beltpouch_%d", i);
        save_item(file, key, &player->character.beltpouch_contents[i]);
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
        fprintf(file, "%s=%s|%s|%d|%d|%d|%d\n",
                key,
                world_items[i].area_name,
                world_items[i].item.name,
                world_items[i].item.entity.x,
                world_items[i].item.entity.y,
                world_items[i].item.entity.z,
                world_items[i].item.quantity);
    }

    for(int area_i = 0; area_i < MAX_AREAS; area_i++)
    {
        int mutation_write_index = 0;
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

        for(int mut_i = 0; mut_i < MAX_AREA_TILE_MUTATIONS; mut_i++)
        {
            const TileMutation* mutation = &atlas[area_i].tile_mutations[mut_i];
            if(!mutation->active)
                continue;

                fprintf(file, "tile_mutation_%d_%d=%d|%d|%d|%d\n",
                    area_i,
                    mutation_write_index,
                    mutation->x,
                    mutation->y,
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

    inventory_init(&player->character);
    player->overland_exhaustion = 0;
    player->travelling = 0;
    actor_ensure_base_attributes(&player->character.actor);
    bestiary_init();
    world_items_init();
    for(int area_i = 0; area_i < MAX_AREAS; area_i++)
    {
        map_clear_discovery(&atlas[area_i]);
        atlas_clear_tile_mutations(&atlas[area_i]);
    }
    collect_equipment_slots(&player->character, equip_slots);

    while(fgets(line, sizeof(line), file))
    {
        char* equals = strchr(line, '=');
        char* key;
        char* value;
        int index;

        if(!equals)
            continue;

        *equals = '\0';
        key = line;
        value = equals + 1;
        value[strcspn(value, "\r\n")] = '\0';

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
        else if(strcmp(key, "overland_exhaustion") == 0)
            player->overland_exhaustion = atoi(value);
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
        else if(strcmp(key, "husbandry_skill") == 0)
            player->character.actor.husbandry_skill = atoi(value);
        else if(strcmp(key, "husbandry_skill_xp") == 0)
            player->character.actor.husbandry_skill_xp = atoi(value);
        else if(strcmp(key, "inventory_count") == 0)
            player->character.inventory_count = atoi(value);
        else if(sscanf(key, "inventory_%d", &index) == 1 && index >= 0 && index < INVENTORY_SIZE)
            load_item_value(&player->character.inventory[index], value);
        else if(sscanf(key, "equip_%d", &index) == 1 && index >= 0 && index < SAVE_EQUIP_SLOT_COUNT)
            load_item_value(equip_slots[index], value);
        else if(strcmp(key, "backpack_count") == 0)
            player->character.backpack_count = atoi(value);
        else if(sscanf(key, "backpack_%d", &index) == 1 && index >= 0 && index < BACKPACK_CAPACITY)
            load_item_value(&player->character.backpack_contents[index], value);
        else if(strcmp(key, "beltpouch_count") == 0)
            player->character.beltpouch_count = atoi(value);
        else if(sscanf(key, "beltpouch_%d", &index) == 1 && index >= 0 && index < BELTPOUCH_CAPACITY)
            load_item_value(&player->character.beltpouch_contents[index], value);
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
                int x;
                int y;
                int z;
                int quantity;
                int matched;
                const ItemTemplate* tmpl;

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

                if(matched == 6 || matched == 5)
                {
                    tmpl = item_template_by_name(item_name);
                    if(tmpl)
                    {
                        world_items[index].active = 1;
                        snprintf(world_items[index].area_name, sizeof(world_items[index].area_name), "%s", area_name);
                        item_init_from_template(&world_items[index].item, tmpl, x, y);
                        world_items[index].item.entity.z = z;
                        world_items[index].item.quantity = quantity;
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
                    int state_raw;
                    int layer_raw = TILE_LAYER_STRUCTURE;

                    (void)mutation_index;

                    if(area_index < 0 || area_index >= MAX_AREAS)
                        continue;

                    if(sscanf(value, "%d|%d|%d|%d", &x, &y, &state_raw, &layer_raw) >= 3)
                    {
                        TileMutationState state = (TileMutationState)state_raw;
                        TileLayer layer = TILE_LAYER_STRUCTURE;

                        if(sscanf(value, "%d|%d|%d|%d", &x, &y, &state_raw, &layer_raw) == 4)
                        {
                            if(layer_raw >= 0 && layer_raw < TILE_LAYER_COUNT)
                                layer = (TileLayer)layer_raw;
                        }

                        if(state > TILE_MUTATION_STATE_NONE && state <= TILE_MUTATION_STATE_DOOR_OPEN)
                        {
                            if(layer == TILE_LAYER_STRUCTURE)
                                atlas_set_tile_mutation(&atlas[area_index], x, y, state);
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

    actor_ensure_base_attributes(&player->character.actor);
    player_apply_derived_maximums(player);
    if(player->selected_attack_mode < ATTACK_MODE_NONE || player->selected_attack_mode > ATTACK_MODE_SMASH)
        player->selected_attack_mode = ATTACK_MODE_PUNCH;

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
            if(world_items[i].active && world_items[i].item.entity.z < AREA_GROUND_Z)
                world_items[i].item.entity.z += AREA_GROUND_Z;
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

        if(world_items[i].item.entity.z < AREA_MIN_Z)
            world_items[i].item.entity.z = AREA_MIN_Z;
        if(world_items[i].item.entity.z > AREA_MAX_Z)
            world_items[i].item.entity.z = AREA_MAX_Z;
    }

    if(has_overworld_x && has_overworld_y)
        world_map_set_overworld_position(overworld_x, overworld_y);

    if(player->overland_exhaustion < 0)
        player->overland_exhaustion = 0;

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