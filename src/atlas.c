#include "atlas.h"
#include "character.h"
#include "log.h"
#include "tileset.h"
#include "tile.h"
#include "world_map.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Purpose:
 *   Builds and manages the world-area atlas.
 *
 * Functions:
 *   - atlas_init: initializes named areas and generates their maps.
 *   - atlas_travel: switches currently active area.
 *   - atlas_find_location: looks up an area by name.
 */

Area atlas[MAX_AREAS];
int atlas_location_count = ATLAS_FIXED_AREA_COUNT;
Area* current_area = NULL;
static LocationKnowledge knowledge_tiers[MAX_AREAS] = { LOCATION_KNOWLEDGE_UNAWARE };
AtlasLocationInfo atlas_location_info[MAX_AREAS];

static const char* atlas_default_names[MAX_AREAS] = {
    "The Glade of Beginnings",
    "Goblin Warrens",
    "Ancient Crypt",
    "Market Town",
    "Old Mine",
    "Castle Ruins",
    "Village",
    "Forest Lake",
    "",
};

static const LocationType atlas_default_types[MAX_AREAS] = {
    LOCATION_STARTER,
    LOCATION_DUNGEON,
    LOCATION_CRYPT,
    LOCATION_TOWN,
    LOCATION_CAVERN,
    LOCATION_DUNGEON,
    LOCATION_VILLAGE,
    LOCATION_CAVERN,
    LOCATION_TOWN,
};

static const int atlas_default_world_x[MAX_AREAS] = { 50, 54, 46, 52, 50, 58, 50, 42, 50 };
static const int atlas_default_world_y[MAX_AREAS] = { 50, 47, 45, 53, 42, 50, 58, 50, 50 };

static unsigned int atlas_generated_seed(int world_x, int world_y, WorldMapBiome biome)
{
    unsigned int seed = 2166136261u;
    seed ^= (unsigned int)(world_x + 257);
    seed *= 16777619u;
    seed ^= (unsigned int)(world_y + 911);
    seed *= 16777619u;
    seed ^= (unsigned int)biome;
    seed *= 16777619u;
    return seed;
}

static void atlas_generate_area_if_needed(int index)
{
    if(index < 0 || index >= MAX_AREAS)
        return;

    if(atlas[index].map_generated)
        return;

    map_generate_area(&atlas[index]);

    if(!atlas[index].map_generated)
        return;

    if(index == 4)
    {
        int sx;
        int sy;
        if(find_floor_tile_for_stairs(&atlas[index], &sx, &sy))
            place_stairs_tile(&atlas[index], sx, sy);
        else
            place_stairs_tile(&atlas[index], atlas[index].width / 2, atlas[index].height / 2);
    }

    atlas_apply_tile_mutations(&atlas[index]);
}

static int atlas_equals_ignore_case(const char* left, const char* right)
{
    if(!left || !right)
        return 0;

    while(*left && *right)
    {
        if(tolower((unsigned char)*left) != tolower((unsigned char)*right))
            return 0;
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static void atlas_trim(char* text)
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

static int atlas_parse_location_type(const char* value, LocationType* out_type)
{
    if(atlas_equals_ignore_case(value, "UNKNOWN")) *out_type = LOCATION_UNKNOWN;
    else if(atlas_equals_ignore_case(value, "STARTER")) *out_type = LOCATION_STARTER;
    else if(atlas_equals_ignore_case(value, "DUNGEON")) *out_type = LOCATION_DUNGEON;
    else if(atlas_equals_ignore_case(value, "CRYPT")) *out_type = LOCATION_CRYPT;
    else if(atlas_equals_ignore_case(value, "CAVERN")) *out_type = LOCATION_CAVERN;
    else if(atlas_equals_ignore_case(value, "VILLAGE")) *out_type = LOCATION_VILLAGE;
    else if(atlas_equals_ignore_case(value, "TOWN")) *out_type = LOCATION_TOWN;
    else return 0;
    return 1;
}

static int atlas_parse_generation_mode(const char* value, LocationGenerationMode* out_mode)
{
    if(atlas_equals_ignore_case(value, "PROCEDURAL"))
    {
        *out_mode = LOCATION_GENERATION_PROCEDURAL;
        return 1;
    }
    if(atlas_equals_ignore_case(value, "PREDEFINED"))
    {
        *out_mode = LOCATION_GENERATION_PREDEFINED;
        return 1;
    }
    return 0;
}

static void atlas_next_csv_field(char** cursor, char* out, size_t out_size)
{
    char* start;
    char* end;
    size_t length;
    int quoted = 0;

    if(!cursor || !*cursor || !out || out_size == 0)
        return;

    start = *cursor;
    while(*start && isspace((unsigned char)*start))
        start++;

    if(*start == '"')
    {
        quoted = 1;
        start++;
    }

    end = start;
    while(*end)
    {
        if(quoted)
        {
            if(*end == '"')
                break;
        }
        else if(*end == ',' || *end == ';' || *end == '\n' || *end == '\r')
            break;
        end++;
    }

    length = (size_t)(end - start);
    if(length >= out_size)
        length = out_size - 1;

    memcpy(out, start, length);
    out[length] = '\0';
    atlas_trim(out);

    if(quoted && *end == '"')
        end++;
    while(*end && *end != ',' && *end != ';' && *end != '\n' && *end != '\r')
        end++;
    if(*end == ',' || *end == ';')
        end++;

    *cursor = end;
}

static int atlas_try_resolve_path(const char* relative_path, char* out_path, size_t out_size)
{
    static const char* roots[] = { "data/templates", "build/data/templates" };

    if(!relative_path || !out_path || out_size == 0)
        return 0;

    for(int i = 0; i < (int)(sizeof(roots) / sizeof(roots[0])); i++)
    {
        FILE* file;
        snprintf(out_path, out_size, "%s/%s", roots[i], relative_path);
        file = fopen(out_path, "r");
        if(file)
        {
            fclose(file);
            return 1;
        }
    }

    return 0;
}

static void atlas_seed_default_areas(void)
{
    atlas_location_count = ATLAS_FIXED_AREA_COUNT;
    for(int i = 0; i < MAX_AREAS; i++)
    {
        if(i < ATLAS_FIXED_AREA_COUNT)
            snprintf(atlas[i].name, sizeof(atlas[i].name), "%s", atlas_default_names[i]);
        else
            snprintf(atlas[i].name, sizeof(atlas[i].name), "Generated Zone");

        atlas[i].type = (i < ATLAS_FIXED_AREA_COUNT) ? atlas_default_types[i] : LOCATION_TOWN;
        atlas[i].generation_mode = LOCATION_GENERATION_PROCEDURAL;
        atlas[i].width = AREA_DEFAULT_WIDTH;
        atlas[i].height = AREA_DEFAULT_HEIGHT;
        atlas[i].world_x = (i < ATLAS_FIXED_AREA_COUNT) ? atlas_default_world_x[i] : 50;
        atlas[i].world_y = (i < ATLAS_FIXED_AREA_COUNT) ? atlas_default_world_y[i] : 50;
        atlas[i].is_generated = (i >= ATLAS_FIXED_AREA_COUNT) ? 1 : 0;
        atlas[i].generation_seed = 0;
        atlas[i].biome = BIOME_NONE;
        atlas[i].predefined_map_path[0] = '\0';
        atlas[i].map = NULL;
        atlas[i].discovered = NULL;
        atlas[i].entity_marker_active = NULL;
        atlas[i].entity_marker_symbol = NULL;
        atlas[i].entity_marker_color = NULL;
        atlas[i].entity_marker_z = NULL;
        atlas[i].map_generated = 0;
        atlas[i].furniture_count = 0;
    }
}

static int atlas_try_load_world_map_tile_csv(const char* path)
{
    FILE* file;
    size_t line_capacity = ((size_t)WORLD_MAP_WIDTH * 128u) + 1024u;
    char* line;
    int max_index_seen = -1;
    int next_auto_index = 0;
    int loaded = 0;
    int y = 0;

    if(!path || path[0] == '\0')
        return 0;

    file = fopen(path, "r");
    if(!file)
        return 0;

    line = (char*)malloc(line_capacity);
    if(!line)
    {
        fclose(file);
        return 0;
    }

    while(y < WORLD_MAP_HEIGHT && fgets(line, (int)line_capacity, file))
    {
        char* cursor;
        char* content;
        int row_has_data = 0;

        atlas_trim(line);
        content = line;
        if(content[0] == '"')
            content++;
        if(content[0] == '\0' || content[0] == '#' || content[0] == ';')
            continue;

        cursor = line;
        for(int x = 0; x < WORLD_MAP_WIDTH; x++)
        {
            char field[256] = "";

            atlas_next_csv_field(&cursor, field, sizeof(field));
            atlas_trim(field);
            if(field[0] != '\0')
            {
                char cell[256];
                char* token_cursor;
                char name[sizeof(atlas[0].name)] = "";
                char type_text[32] = "";
                char width_text[32] = "";
                char height_text[32] = "";
                char generation_mode_text[32] = "";
                char predefined_map[ATLAS_PREDEFINED_MAP_PATH_LENGTH] = "";
                int idx = -1;

                row_has_data = 1;
                snprintf(cell, sizeof(cell), "%s", field);
                token_cursor = cell;

                while(token_cursor && *token_cursor)
                {
                    char* token = token_cursor;
                    char* next = strchr(token_cursor, '|');
                    char* equals;

                    if(next)
                    {
                        *next = '\0';
                        token_cursor = next + 1;
                    }
                    else
                        token_cursor = NULL;

                    atlas_trim(token);
                    if(token[0] == '\0')
                        continue;

                    equals = strchr(token, '=');
                    if(!equals)
                        continue;

                    *equals = '\0';
                    atlas_trim(token);
                    atlas_trim(equals + 1);

                    if(atlas_equals_ignore_case(token, "loc")
                       || atlas_equals_ignore_case(token, "location"))
                        snprintf(name, sizeof(name), "%s", equals + 1);
                    else if(atlas_equals_ignore_case(token, "index")
                            || atlas_equals_ignore_case(token, "idx"))
                        idx = atoi(equals + 1);
                    else if(atlas_equals_ignore_case(token, "type"))
                        snprintf(type_text, sizeof(type_text), "%s", equals + 1);
                    else if(atlas_equals_ignore_case(token, "w")
                            || atlas_equals_ignore_case(token, "width"))
                        snprintf(width_text, sizeof(width_text), "%s", equals + 1);
                    else if(atlas_equals_ignore_case(token, "h")
                            || atlas_equals_ignore_case(token, "height"))
                        snprintf(height_text, sizeof(height_text), "%s", equals + 1);
                    else if(atlas_equals_ignore_case(token, "gen")
                            || atlas_equals_ignore_case(token, "generation")
                            || atlas_equals_ignore_case(token, "generation_mode"))
                        snprintf(generation_mode_text, sizeof(generation_mode_text), "%s", equals + 1);
                    else if(atlas_equals_ignore_case(token, "map")
                            || atlas_equals_ignore_case(token, "predefined_map"))
                        snprintf(predefined_map, sizeof(predefined_map), "%s", equals + 1);
                }

                if(name[0] != '\0')
                {
                    if(idx < 0)
                        idx = next_auto_index;
                    if(idx >= 0 && idx < ATLAS_FIXED_AREA_COUNT)
                    {
                        snprintf(atlas[idx].name, sizeof(atlas[idx].name), "%s", name);
                        atlas[idx].world_x = x;
                        atlas[idx].world_y = y;
                        if(type_text[0] != '\0')
                            (void)atlas_parse_location_type(type_text, &atlas[idx].type);
                        if(width_text[0] != '\0')
                            atlas[idx].width = atoi(width_text);
                        if(height_text[0] != '\0')
                            atlas[idx].height = atoi(height_text);
                        if(generation_mode_text[0] != '\0')
                            (void)atlas_parse_generation_mode(generation_mode_text, &atlas[idx].generation_mode);
                        if(predefined_map[0] != '\0')
                        {
                            char resolved[ATLAS_PREDEFINED_MAP_PATH_LENGTH];
                            if(atlas_try_resolve_path(predefined_map, resolved, sizeof(resolved)))
                                snprintf(atlas[idx].predefined_map_path, sizeof(atlas[idx].predefined_map_path), "%s", resolved);
                            else
                                snprintf(atlas[idx].predefined_map_path, sizeof(atlas[idx].predefined_map_path), "%s", predefined_map);
                        }

                        if(idx >= next_auto_index)
                            next_auto_index = idx + 1;
                        if(idx > max_index_seen)
                            max_index_seen = idx;
                        loaded++;
                    }
                }
            }

            if(!cursor || *cursor == '\0')
                break;
        }

        if(row_has_data)
            y++;
    }

    free(line);
    fclose(file);

    if(max_index_seen >= 0)
        atlas_location_count = max_index_seen + 1;
    if(atlas_location_count < 1)
        atlas_location_count = 1;
    if(atlas_location_count > ATLAS_FIXED_AREA_COUNT)
        atlas_location_count = ATLAS_FIXED_AREA_COUNT;

    return loaded > 0;
}

static int atlas_try_load_location_csv(const char* path)
{
    FILE* file;
    char line[512];
    int max_index_seen = -1;
    int loaded = 0;

    if(!path || path[0] == '\0')
        return 0;

    file = fopen(path, "r");
    if(!file)
        return 0;

    while(fgets(line, sizeof(line), file))
    {
        char* cursor = line;
        char index_text[32] = "";
        char name[sizeof(atlas[0].name)] = "";
        char type_text[32] = "";
        char world_x_text[32] = "";
        char world_y_text[32] = "";
        char width_text[32] = "";
        char height_text[32] = "";
        char generation_mode_text[32] = "";
        char predefined_map[ATLAS_PREDEFINED_MAP_PATH_LENGTH] = "";
        int idx;

        atlas_trim(line);
        if(line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        atlas_next_csv_field(&cursor, index_text, sizeof(index_text));
        atlas_next_csv_field(&cursor, name, sizeof(name));
        atlas_next_csv_field(&cursor, type_text, sizeof(type_text));
        atlas_next_csv_field(&cursor, world_x_text, sizeof(world_x_text));
        atlas_next_csv_field(&cursor, world_y_text, sizeof(world_y_text));
        atlas_next_csv_field(&cursor, width_text, sizeof(width_text));
        atlas_next_csv_field(&cursor, height_text, sizeof(height_text));
        atlas_next_csv_field(&cursor, generation_mode_text, sizeof(generation_mode_text));
        atlas_next_csv_field(&cursor, predefined_map, sizeof(predefined_map));

        if(atlas_equals_ignore_case(index_text, "index"))
            continue;

        idx = atoi(index_text);
        if(idx < 0 || idx >= ATLAS_FIXED_AREA_COUNT)
            continue;

        if(name[0] != '\0')
            snprintf(atlas[idx].name, sizeof(atlas[idx].name), "%s", name);
        if(type_text[0] != '\0')
            (void)atlas_parse_location_type(type_text, &atlas[idx].type);
        if(world_x_text[0] != '\0')
            atlas[idx].world_x = atoi(world_x_text);
        if(world_y_text[0] != '\0')
            atlas[idx].world_y = atoi(world_y_text);
        if(width_text[0] != '\0')
            atlas[idx].width = atoi(width_text);
        if(height_text[0] != '\0')
            atlas[idx].height = atoi(height_text);
        if(generation_mode_text[0] != '\0')
            (void)atlas_parse_generation_mode(generation_mode_text, &atlas[idx].generation_mode);
        if(predefined_map[0] != '\0')
        {
            char resolved[ATLAS_PREDEFINED_MAP_PATH_LENGTH];
            if(atlas_try_resolve_path(predefined_map, resolved, sizeof(resolved)))
                snprintf(atlas[idx].predefined_map_path, sizeof(atlas[idx].predefined_map_path), "%s", resolved);
            else
                snprintf(atlas[idx].predefined_map_path, sizeof(atlas[idx].predefined_map_path), "%s", predefined_map);
        }

        if(idx > max_index_seen)
            max_index_seen = idx;
        loaded++;
    }

    fclose(file);

    if(max_index_seen >= 0)
        atlas_location_count = max_index_seen + 1;
    if(atlas_location_count < 1)
        atlas_location_count = 1;
    if(atlas_location_count > ATLAS_FIXED_AREA_COUNT)
        atlas_location_count = ATLAS_FIXED_AREA_COUNT;

    return loaded > 0;
}

static int atlas_try_load_location_config(void)
{
    char path[260];
    FILE* file;
    char line[256];
    int target_index = -1;
    int max_index_seen = -1;

    if(atlas_try_resolve_path("maps/world_map_tiles.csv", path, sizeof(path))
       && atlas_try_load_world_map_tile_csv(path))
        return 1;

    if(atlas_try_resolve_path("locations.csv", path, sizeof(path))
       && atlas_try_load_location_csv(path))
        return 1;

    if(!atlas_try_resolve_path("locations.ini", path, sizeof(path)))
        return 0;

    file = fopen(path, "r");
    if(!file)
        return 0;

    while(fgets(line, sizeof(line), file))
    {
        char* equals;

        atlas_trim(line);
        if(line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        if(line[0] == '[')
        {
            target_index = -1;
            continue;
        }

        equals = strchr(line, '=');
        if(!equals)
            continue;

        *equals = '\0';
        atlas_trim(line);
        atlas_trim(equals + 1);

        if(atlas_equals_ignore_case(line, "index"))
        {
            int idx = atoi(equals + 1);
            if(idx < 0 || idx >= ATLAS_FIXED_AREA_COUNT)
            {
                target_index = -1;
                continue;
            }
            target_index = idx;
            if(idx > max_index_seen)
                max_index_seen = idx;
            continue;
        }

        if(target_index < 0 || target_index >= ATLAS_FIXED_AREA_COUNT)
            continue;

        if(atlas_equals_ignore_case(line, "name"))
            snprintf(atlas[target_index].name, sizeof(atlas[target_index].name), "%s", equals + 1);
        else if(atlas_equals_ignore_case(line, "type"))
            (void)atlas_parse_location_type(equals + 1, &atlas[target_index].type);
        else if(atlas_equals_ignore_case(line, "width"))
            atlas[target_index].width = atoi(equals + 1);
        else if(atlas_equals_ignore_case(line, "height"))
            atlas[target_index].height = atoi(equals + 1);
        else if(atlas_equals_ignore_case(line, "world_x"))
            atlas[target_index].world_x = atoi(equals + 1);
        else if(atlas_equals_ignore_case(line, "world_y"))
            atlas[target_index].world_y = atoi(equals + 1);
        else if(atlas_equals_ignore_case(line, "generation_mode"))
            (void)atlas_parse_generation_mode(equals + 1, &atlas[target_index].generation_mode);
        else if(atlas_equals_ignore_case(line, "predefined_map"))
        {
            char resolved[ATLAS_PREDEFINED_MAP_PATH_LENGTH];
            if(atlas_try_resolve_path(equals + 1, resolved, sizeof(resolved)))
                snprintf(atlas[target_index].predefined_map_path, sizeof(atlas[target_index].predefined_map_path), "%s", resolved);
            else
                snprintf(atlas[target_index].predefined_map_path, sizeof(atlas[target_index].predefined_map_path), "%s", equals + 1);
        }
    }

    fclose(file);

    if(max_index_seen >= 0)
        atlas_location_count = max_index_seen + 1;
    if(atlas_location_count < 1)
        atlas_location_count = 1;
    if(atlas_location_count > ATLAS_FIXED_AREA_COUNT)
        atlas_location_count = ATLAS_FIXED_AREA_COUNT;

    return 1;
}

static void atlas_apply_world_map_knowledge(int index)
{
    WorldMapTile* tile;

    if(index < 0 || index >= atlas_location_count)
        return;

    tile = world_map_get_tile(atlas[index].world_x, atlas[index].world_y);
    if(!tile)
        return;

    tile->zone_index = index;
    tile->discovered = (knowledge_tiers[index] >= LOCATION_KNOWLEDGE_LOCATED) ? 1 : 0;
    tile->visited = (knowledge_tiers[index] >= LOCATION_KNOWLEDGE_VISITED) ? 1 : 0;
}

static void atlas_timestamp_now(char out[ATLAS_TIMESTAMP_LENGTH])
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    if(!out)
        return;

    if(!tm_info)
    {
        snprintf(out, ATLAS_TIMESTAMP_LENGTH, "unknown-time");
        return;
    }

    strftime(out, ATLAS_TIMESTAMP_LENGTH, "%Y-%m-%d %H:%M", tm_info);
}

static int atlas_info_is_empty(const char* text)
{
    return !text || text[0] == '\0';
}

static void atlas_set_timestamp_once(char* field)
{
    if(!field || !atlas_info_is_empty(field))
        return;
    atlas_timestamp_now(field);
}

static void atlas_set_timestamp_now(char* field)
{
    if(!field)
        return;
    atlas_timestamp_now(field);
}

static void atlas_record_visit_timestamp(int index)
{
    AtlasLocationInfo* info;

    if(index < 0 || index >= atlas_location_count)
        return;

    info = &atlas_location_info[index];
    atlas_set_timestamp_once(info->first_visit_ts);
    atlas_set_timestamp_now(info->latest_visit_ts);
}

static const TreeDurabilityState* atlas_tree_state_at(const Area* area, int x, int y, int z)
{
    if(!area)
        return NULL;

    for(int i = 0; i < MAX_AREA_TREE_STATES; ++i)
    {
        const TreeDurabilityState* entry = &area->tree_states[i];
        if(!entry->active)
            continue;
        if(entry->x == x && entry->y == y && entry->z == z)
            return entry;
    }

    return NULL;
}

// Reset all tile mutation records for one area.
void atlas_clear_tile_mutations(Area* area) {
    if(!area)
        return;

    area->tile_mutation_count = 0;
    memset(area->tile_mutations, 0, sizeof(area->tile_mutations));
    area->tree_state_count = 0;
    memset(area->tree_states, 0, sizeof(area->tree_states));
}

// Apply one mutation state onto area tile data.
int atlas_apply_tile_mutation(Area* area, const TileMutation* mutation) {
    Tile* tile;
    int z;

    if(!area || !mutation || !mutation->active)
        return 0;
    if(mutation->x < 0 || mutation->x >= area->width || mutation->y < 0 || mutation->y >= area->height)
        return 0;

    z = (mutation->z >= AREA_GROUND_Z) ? mutation->z : AREA_GROUND_Z;
    tile = map_tile_at_layer_z(area, mutation->x, mutation->y, z, mutation->layer);
    if(!tile)
        return 0;

    switch(mutation->state)
    {
        case TILE_MUTATION_STATE_DOOR_OPEN:
            tile->symbol = '/';
            tile->color = RENDER_COLOR_BROWN;
            snprintf(tile->name, sizeof(tile->name), "Open Door");
            tile->interactable = 1;
            tile->blocks_movement = 0;
            tile->blocks_sight = 0;
            tile->blocks_projectile = 0;
            return 1;
        case TILE_MUTATION_STATE_DOOR_CLOSED:
            tile->symbol = '+';
            tile->color = RENDER_COLOR_BROWN;
            snprintf(tile->name, sizeof(tile->name), "Door");
            tile->interactable = 1;
            tile->blocks_movement = 1;
            tile->blocks_sight = 1;
            tile->blocks_projectile = 1;
            return 1;
        case TILE_MUTATION_STATE_TREE_STUMP:
        {
            const TreeDurabilityState* tree_state = atlas_tree_state_at(area, mutation->x, mutation->y, z);
            *tile = tree_state ? tile_tree_stump_for_species(tree_state->species) : TILE_TREE_STUMP;
            return 1;
        }
        case TILE_MUTATION_STATE_NONE:
        default:
            return 0;
    }
}

// Apply all stored mutations to one area map.
void atlas_apply_tile_mutations(Area* area) {
    if(!area)
        return;

    for(int i = 0; i < MAX_AREA_TILE_MUTATIONS; i++)
        atlas_apply_tile_mutation(area, &area->tile_mutations[i]);
}

// Set/update one tile mutation record and apply to area map.
int atlas_set_tile_mutation_at_z(Area* area, int x, int y, int z, TileMutationState state) {
    int free_index = -1;
    TileMutation mutation = {0};

    if(!area)
        return 0;
    if(x < 0 || x >= area->width || y < 0 || y >= area->height)
        return 0;
    if(state == TILE_MUTATION_STATE_NONE)
        return 0;
    if(z < AREA_GROUND_Z || z > AREA_MAX_Z)
        z = AREA_GROUND_Z;

    for(int i = 0; i < MAX_AREA_TILE_MUTATIONS; i++)
    {
        TileMutation* entry = &area->tile_mutations[i];

        if(!entry->active)
        {
            if(free_index < 0)
                free_index = i;
            continue;
        }

        if(entry->x == x && entry->y == y && entry->z == z)
        {
            entry->z = z;
            entry->layer = TILE_LAYER_WALL;
            entry->state = state;
            return atlas_apply_tile_mutation(area, entry);
        }
    }

    if(free_index < 0)
        return 0;

    mutation.active = 1;
    mutation.x = x;
    mutation.y = y;
    mutation.z = z;
    mutation.layer = TILE_LAYER_WALL;
    mutation.state = state;
    area->tile_mutations[free_index] = mutation;
    area->tile_mutation_count++;
    return atlas_apply_tile_mutation(area, &area->tile_mutations[free_index]);
}

int atlas_set_tile_mutation(Area* area, int x, int y, TileMutationState state) {
    return atlas_set_tile_mutation_at_z(area, x, y, character_z(), state);
}

void atlas_set_knowledge(int index, LocationKnowledge knowledge)
{
    LocationKnowledge previous;

    if(index < 0 || index >= atlas_location_count)
        return;

    if(knowledge < LOCATION_KNOWLEDGE_UNAWARE)
        knowledge = LOCATION_KNOWLEDGE_UNAWARE;
    if(knowledge > LOCATION_KNOWLEDGE_VISITED)
        knowledge = LOCATION_KNOWLEDGE_VISITED;

    previous = knowledge_tiers[index];
    knowledge_tiers[index] = knowledge;

    if(previous < LOCATION_KNOWLEDGE_AWARE && knowledge >= LOCATION_KNOWLEDGE_AWARE)
        atlas_set_timestamp_once(atlas_location_info[index].first_aware_ts);

    if(previous < LOCATION_KNOWLEDGE_LOCATED && knowledge >= LOCATION_KNOWLEDGE_LOCATED)
        atlas_set_timestamp_once(atlas_location_info[index].first_located_ts);

    if(knowledge >= LOCATION_KNOWLEDGE_LOCATED)
    {
        char coordinate_hint[ATLAS_LOCATION_HINT_LENGTH];
        snprintf(coordinate_hint,
                 sizeof(coordinate_hint),
                 "Coordinates recorded: (%d,%d)",
                 atlas[index].world_x,
                 atlas[index].world_y);
        atlas_add_location_hint(index, coordinate_hint);
    }

    if(previous < LOCATION_KNOWLEDGE_SCOUTED && knowledge >= LOCATION_KNOWLEDGE_SCOUTED)
        atlas_set_timestamp_once(atlas_location_info[index].first_scouted_ts);

    if(knowledge >= LOCATION_KNOWLEDGE_VISITED)
        atlas_record_visit_timestamp(index);

    atlas_apply_world_map_knowledge(index);
}

void atlas_upgrade_knowledge(int index, LocationKnowledge knowledge)
{
    if(index < 0 || index >= atlas_location_count)
        return;

    if(knowledge > knowledge_tiers[index])
        atlas_set_knowledge(index, knowledge);
}

LocationKnowledge atlas_get_knowledge(int index)
{
    if(index < 0 || index >= atlas_location_count)
        return LOCATION_KNOWLEDGE_UNAWARE;
    return knowledge_tiers[index];
}

int atlas_is_known(int index)
{
    return atlas_get_knowledge(index) >= LOCATION_KNOWLEDGE_AWARE;
}

int atlas_is_located(int index)
{
    return atlas_get_knowledge(index) >= LOCATION_KNOWLEDGE_LOCATED;
}

int atlas_is_scouted(int index)
{
    return atlas_get_knowledge(index) >= LOCATION_KNOWLEDGE_SCOUTED;
}

int atlas_is_visited(int index)
{
    return atlas_get_knowledge(index) >= LOCATION_KNOWLEDGE_VISITED;
}

int atlas_can_fast_travel(int index)
{
    return atlas_is_visited(index);
}

int atlas_known_count(void)
{
    int count = 0;
    for(int i = 0; i < atlas_location_count; i++)
        count += atlas_is_known(i) ? 1 : 0;
    return count;
}

void atlas_sync_world_map(void)
{
    for(int i = 0; i < atlas_location_count; i++)
    {
        atlas[i].biome = world_map_get_biome(atlas[i].world_x, atlas[i].world_y);
        world_map_set_zone(atlas[i].world_x, atlas[i].world_y, i);
        atlas_apply_world_map_knowledge(i);
    }
}

int atlas_prepare_generated_area(int world_x, int world_y, int* out_index)
{
    int slot = ATLAS_FIXED_AREA_COUNT;
    WorldMapTile* tile;

    if(slot < 0 || slot >= MAX_AREAS)
        return 0;
    if(world_x < 0 || world_x >= WORLD_MAP_WIDTH || world_y < 0 || world_y >= WORLD_MAP_HEIGHT)
        return 0;

    tile = world_map_get_tile(world_x, world_y);
    if(!tile || !tile->discovered)
        return 0;

    if(atlas[slot].is_generated && atlas[slot].world_x == world_x && atlas[slot].world_y == world_y)
    {
        atlas_generate_area_if_needed(slot);
        if(!atlas[slot].map_generated)
            return 0;
        if(out_index)
            *out_index = slot;
        return 1;
    }

    atlas[slot].is_generated = 1;
    atlas[slot].type = LOCATION_TOWN;
    atlas[slot].generation_mode = LOCATION_GENERATION_PROCEDURAL;
    atlas[slot].width = AREA_DEFAULT_WIDTH;
    atlas[slot].height = AREA_DEFAULT_HEIGHT;
    atlas[slot].world_x = world_x;
    atlas[slot].world_y = world_y;
    atlas[slot].biome = world_map_get_biome(world_x, world_y);
    atlas[slot].generation_seed = atlas_generated_seed(world_x, world_y, atlas[slot].biome);
    atlas[slot].predefined_map_path[0] = '\0';

    snprintf(atlas[slot].name,
             sizeof(atlas[slot].name),
             "%s %d,%d",
             world_map_biome_name(atlas[slot].biome),
             world_x,
             world_y);

    atlas_clear_tile_mutations(&atlas[slot]);
    atlas[slot].map_generated = 0;
    atlas_generate_area_if_needed(slot);
    if(!atlas[slot].map_generated)
        return 0;

    if(out_index)
        *out_index = slot;
    return 1;
}

int atlas_is_generated_index(int index)
{
    return index >= ATLAS_FIXED_AREA_COUNT && index < MAX_AREAS;
}

int atlas_add_location_hint(int index, const char* hint_text)
{
    AtlasLocationInfo* info;

    if(index < 0 || index >= atlas_location_count || !hint_text || hint_text[0] == '\0')
        return 0;

    info = &atlas_location_info[index];

    for(int i = 0; i < info->hint_count; i++)
    {
        if(strcmp(info->hints[i], hint_text) == 0)
            return 1;
    }

    if(info->hint_count >= ATLAS_LOCATION_HINT_MAX)
        return 0;

    snprintf(info->hints[info->hint_count], ATLAS_LOCATION_HINT_LENGTH, "%s", hint_text);
    info->hints[info->hint_count][ATLAS_LOCATION_HINT_LENGTH - 1] = '\0';
    info->hint_count++;
    return 1;
}

const AtlasLocationInfo* atlas_get_location_info(int index)
{
    if(index < 0 || index >= atlas_location_count)
        return NULL;
    return &atlas_location_info[index];
}

void atlas_set_location_timestamp_aware(int index, const char* ts)
{
    if(index < 0 || index >= atlas_location_count)
        return;
    snprintf(atlas_location_info[index].first_aware_ts, ATLAS_TIMESTAMP_LENGTH, "%s", ts ? ts : "");
}

void atlas_set_location_timestamp_located(int index, const char* ts)
{
    if(index < 0 || index >= atlas_location_count)
        return;
    snprintf(atlas_location_info[index].first_located_ts, ATLAS_TIMESTAMP_LENGTH, "%s", ts ? ts : "");
}

void atlas_set_location_timestamp_scouted(int index, const char* ts)
{
    if(index < 0 || index >= atlas_location_count)
        return;
    snprintf(atlas_location_info[index].first_scouted_ts, ATLAS_TIMESTAMP_LENGTH, "%s", ts ? ts : "");
}

void atlas_set_location_timestamp_first_visit(int index, const char* ts)
{
    if(index < 0 || index >= atlas_location_count)
        return;
    snprintf(atlas_location_info[index].first_visit_ts, ATLAS_TIMESTAMP_LENGTH, "%s", ts ? ts : "");
}

void atlas_set_location_timestamp_latest_visit(int index, const char* ts)
{
    if(index < 0 || index >= atlas_location_count)
        return;
    snprintf(atlas_location_info[index].latest_visit_ts, ATLAS_TIMESTAMP_LENGTH, "%s", ts ? ts : "");
}

void atlas_clear_location_hints(int index)
{
    if(index < 0 || index >= atlas_location_count)
        return;

    atlas_location_info[index].hint_count = 0;
    for(int i = 0; i < ATLAS_LOCATION_HINT_MAX; i++)
        atlas_location_info[index].hints[i][0] = '\0';
}

// Initialize all atlas areas and select the first area as active.
void atlas_init() {
    memset(knowledge_tiers, 0, sizeof(knowledge_tiers));
    memset(atlas_location_info, 0, sizeof(atlas_location_info));
    world_map_signposts_init();

    atlas_seed_default_areas();
    atlas_try_load_location_config();

    for(int i = 0; i < atlas_location_count; i++)
    {
        atlas_clear_tile_mutations(&atlas[i]);
        atlas[i].map_generated = 0;
    }

    atlas_generate_area_if_needed(0);

    atlas_set_knowledge(0, LOCATION_KNOWLEDGE_VISITED);
    current_area = &atlas[0];
}

// Switch current area to the given index when valid.
void atlas_travel(int index) {
    if(index < 0 || index >= MAX_AREAS)
        return;

    atlas_generate_area_if_needed(index);
    if(!atlas[index].map_generated)
        return;

    current_area = &atlas[index];
    if(index < atlas_location_count)
    {
        atlas_upgrade_knowledge(index, LOCATION_KNOWLEDGE_VISITED);
        atlas_record_visit_timestamp(index);
    }
    log_add("You travel to %s.", current_area->name);
}

// Return atlas index for a location name, or -1 if not found.
int atlas_find_location(const char* name) {
    if(!name) return -1;
    for(int i = 0; i < atlas_location_count; i++) {
        if(strcmp(atlas[i].name, name) == 0)
            return i;
    }
    return -1;
}

