#include "world_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * @file world_map.c
 * @brief Implementation of the high-level world map state (discovery, zones).
 *
 * Maintains the 100x100 world map grid state tracking discovered/visited tiles
 * and zone assignments for fast spatial queries and discovered-location tracking.
 */

WorldMapTile world_map[WORLD_MAP_HEIGHT][WORLD_MAP_WIDTH];
static int g_overworld_x = 0;
static int g_overworld_y = 0;
static int g_has_overworld_position = 0;

typedef struct {
    SignpostInstance* instances;
    int count;
    int capacity;
} SignpostAreaRegistry;

static SignpostAreaRegistry g_signpost_registry[WORLD_MAP_SIGNPOST_MAX_AREAS];

static int world_map_signpost_area_valid(int area_index)
{
    return area_index >= 0 && area_index < WORLD_MAP_SIGNPOST_MAX_AREAS;
}

static void world_map_signpost_free_instance(SignpostInstance* instance)
{
    if(!instance)
        return;
    free(instance->signs);
    instance->signs = NULL;
    instance->sign_count = 0;
    instance->sign_capacity = 0;
}

static int world_map_signpost_ensure_instance_capacity(SignpostAreaRegistry* registry, int needed)
{
    int new_capacity;
    SignpostInstance* resized;

    if(!registry || needed <= registry->capacity)
        return 1;

    new_capacity = (registry->capacity > 0) ? registry->capacity : 4;
    while(new_capacity < needed)
        new_capacity *= 2;

    resized = (SignpostInstance*)realloc(registry->instances, (size_t)new_capacity * sizeof(SignpostInstance));
    if(!resized)
        return 0;

    registry->instances = resized;
    registry->capacity = new_capacity;
    return 1;
}

static int world_map_signpost_ensure_sign_capacity(SignpostInstance* instance, int needed)
{
    int new_capacity;
    SignpostSign* resized;

    if(!instance || needed <= instance->sign_capacity)
        return 1;

    new_capacity = (instance->sign_capacity > 0) ? instance->sign_capacity : 4;
    while(new_capacity < needed)
        new_capacity *= 2;

    resized = (SignpostSign*)realloc(instance->signs, (size_t)new_capacity * sizeof(SignpostSign));
    if(!resized)
        return 0;

    instance->signs = resized;
    instance->sign_capacity = new_capacity;
    return 1;
}

static SignpostInstance* world_map_signpost_find(SignpostAreaRegistry* registry, int x, int y, int z)
{
    if(!registry)
        return NULL;

    for(int i = 0; i < registry->count; i++)
    {
        SignpostInstance* candidate = &registry->instances[i];
        if(candidate->x == x && candidate->y == y && candidate->z == z)
            return candidate;
    }

    return NULL;
}

static int world_map_clamp_road_tier(int road_tier)
{
    if(road_tier < WORLD_MAP_ROAD_TIER_NONE)
        return WORLD_MAP_ROAD_TIER_NONE;
    if(road_tier > WORLD_MAP_MAX_ROAD_TIER)
        return WORLD_MAP_MAX_ROAD_TIER;
    return road_tier;
}

static int world_map_clamp_river_tier(int river_tier)
{
    if(river_tier < WORLD_MAP_RIVER_NONE)
        return WORLD_MAP_RIVER_NONE;
    if(river_tier > WORLD_MAP_RIVER_GIGANTIC)
        return WORLD_MAP_RIVER_GIGANTIC;
    return river_tier;
}

static int world_map_clamp_lake_tier(int lake_tier)
{
    if(lake_tier < WORLD_MAP_LAKE_NONE)
        return WORLD_MAP_LAKE_NONE;
    if(lake_tier > WORLD_MAP_LAKE_LARGE)
        return WORLD_MAP_LAKE_LARGE;
    return lake_tier;
}

static int world_map_equals_ignore_case(const char* left, const char* right)
{
    if(!left || !right)
        return 0;

    while(*left && *right)
    {
        int lc = tolower((unsigned char)*left);
        int rc = tolower((unsigned char)*right);
        if(lc != rc)
            return 0;
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static void world_map_trim(char* text)
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

static WorldMapBiome world_map_parse_biome_token(const char* token)
{
    if(!token || token[0] == '\0')
        return BIOME_NONE;

    if(strcmp(token, ".") == 0
       || world_map_equals_ignore_case(token, "GR")
       || world_map_equals_ignore_case(token, "GRASS")
       || world_map_equals_ignore_case(token, "GRASSLANDS"))
        return BIOME_GRASSLANDS;
    if(strcmp(token, "\"") == 0
       || world_map_equals_ignore_case(token, "FO")
       || world_map_equals_ignore_case(token, "FOREST"))
        return BIOME_FOREST;
    if(strcmp(token, "~") == 0
       || world_map_equals_ignore_case(token, "DE")
       || world_map_equals_ignore_case(token, "DESERT"))
        return BIOME_DESERT;
    if(strcmp(token, "'") == 0
       || world_map_equals_ignore_case(token, "TU")
       || world_map_equals_ignore_case(token, "TUNDRA"))
        return BIOME_TUNDRA;
    if(world_map_equals_ignore_case(token, "TA")
       || world_map_equals_ignore_case(token, "TAIGA"))
        return BIOME_TAIGA;
    if(world_map_equals_ignore_case(token, "SH")
       || world_map_equals_ignore_case(token, "SHRUBLAND"))
        return BIOME_SHRUBLAND;
    if(world_map_equals_ignore_case(token, "ST")
       || world_map_equals_ignore_case(token, "STEPPE"))
        return BIOME_STEPPE;
    if(world_map_equals_ignore_case(token, "GL")
       || world_map_equals_ignore_case(token, "GLACIER")
       || world_map_equals_ignore_case(token, "ICE"))
        return BIOME_GLACIER;
    if(strcmp(token, "s") == 0
       || world_map_equals_ignore_case(token, "SE")
       || world_map_equals_ignore_case(token, "SEA")
       || world_map_equals_ignore_case(token, "OCEAN"))
        return BIOME_SEA;
    if(strcmp(token, ",") == 0
       || world_map_equals_ignore_case(token, "SA")
       || world_map_equals_ignore_case(token, "SAVANNAH"))
        return BIOME_SAVANNAH;
    if(strcmp(token, "^") == 0
       || world_map_equals_ignore_case(token, "MO")
       || world_map_equals_ignore_case(token, "MOUNTAIN")
       || world_map_equals_ignore_case(token, "MOUNTAINS"))
        return BIOME_MOUNTAINS;
    if(strcmp(token, "n") == 0
       || world_map_equals_ignore_case(token, "HI")
       || world_map_equals_ignore_case(token, "FH")
       || world_map_equals_ignore_case(token, "FOOTHILL")
       || world_map_equals_ignore_case(token, "FOOTHILLS"))
        return BIOME_FOOTHILLS;
    if(strcmp(token, "m") == 0
       || world_map_equals_ignore_case(token, "SW")
       || world_map_equals_ignore_case(token, "SWAMP"))
        return BIOME_SWAMP;
    if(strcmp(token, "j") == 0
       || world_map_equals_ignore_case(token, "JU")
       || world_map_equals_ignore_case(token, "JUNGLE"))
        return BIOME_JUNGLE;

    return BIOME_NONE;
}

static int world_map_parse_river_tier_token(const char* token)
{
    if(!token || token[0] == '\0')
        return WORLD_MAP_RIVER_NONE;

    if(world_map_equals_ignore_case(token, "none"))
        return WORLD_MAP_RIVER_NONE;
    if(world_map_equals_ignore_case(token, "minor"))
        return WORLD_MAP_RIVER_MINOR;
    if(world_map_equals_ignore_case(token, "major"))
        return WORLD_MAP_RIVER_MAJOR;
    if(world_map_equals_ignore_case(token, "tiny"))
        return WORLD_MAP_RIVER_TINY;
    if(world_map_equals_ignore_case(token, "small"))
        return WORLD_MAP_RIVER_SMALL;
    if(world_map_equals_ignore_case(token, "medium"))
        return WORLD_MAP_RIVER_MEDIUM;
    if(world_map_equals_ignore_case(token, "large"))
        return WORLD_MAP_RIVER_LARGE;
    if(world_map_equals_ignore_case(token, "massive"))
        return WORLD_MAP_RIVER_MASSIVE;
    if(world_map_equals_ignore_case(token, "gigantic"))
        return WORLD_MAP_RIVER_GIGANTIC;

    return world_map_clamp_river_tier(atoi(token));
}

static int world_map_parse_lake_tier_token(const char* token)
{
    if(!token || token[0] == '\0')
        return WORLD_MAP_LAKE_NONE;

    if(world_map_equals_ignore_case(token, "none"))
        return WORLD_MAP_LAKE_NONE;
    if(world_map_equals_ignore_case(token, "small"))
        return WORLD_MAP_LAKE_SMALL;
    if(world_map_equals_ignore_case(token, "large"))
        return WORLD_MAP_LAKE_LARGE;

    return world_map_clamp_lake_tier(atoi(token));
}

static int world_map_token_is_legacy_river(const char* token)
{
    return token && (strcmp(token, "r") == 0
                     || world_map_equals_ignore_case(token, "RI")
                     || world_map_equals_ignore_case(token, "RIVER"));
}

static int world_map_token_is_legacy_lake(const char* token)
{
    return token && (strcmp(token, "l") == 0
                     || world_map_equals_ignore_case(token, "LA")
                     || world_map_equals_ignore_case(token, "LAKE"));
}

static void world_map_init_tile_cell_metadata(WorldMapTileCellMetadata* metadata)
{
    if(!metadata)
        return;

    metadata->biome = BIOME_NONE;
    metadata->road_tier = WORLD_MAP_ROAD_TIER_NONE;
    metadata->river_tier = WORLD_MAP_RIVER_NONE;
    metadata->lake_tier = WORLD_MAP_LAKE_NONE;
    metadata->location_name[0] = '\0';
    metadata->location_type_text[0] = '\0';
    metadata->location_index = -1;
    metadata->generation_mode_text[0] = '\0';
    metadata->width = 0;
    metadata->height = 0;
    metadata->location_local_x = -1;
    metadata->location_local_y = -1;
    metadata->predefined_map_path[0] = '\0';
}

static int world_map_parse_road_tier_token(const char* token);

void world_map_parse_tile_cell(const char* cell, WorldMapTileCellMetadata* metadata)
{
    char local_cell[512];
    char* cursor;

    if(!metadata)
        return;

    world_map_init_tile_cell_metadata(metadata);
    if(!cell || cell[0] == '\0')
        return;

    snprintf(local_cell, sizeof(local_cell), "%s", cell);
    cursor = local_cell;
    while(cursor && *cursor)
    {
        char* token = cursor;
        char* next = strchr(cursor, '|');
        char* equals;

        if(next)
        {
            *next = '\0';
            cursor = next + 1;
        }
        else
        {
            cursor = NULL;
        }

        world_map_trim(token);
        if(token[0] == '\0')
            continue;

        equals = strchr(token, '=');
        if(!equals)
        {
            if(world_map_token_is_legacy_river(token))
            {
                metadata->river_tier = WORLD_MAP_RIVER_MAJOR;
                continue;
            }
            if(world_map_token_is_legacy_lake(token))
            {
                metadata->lake_tier = WORLD_MAP_LAKE_LARGE;
                continue;
            }

            WorldMapBiome biome = world_map_parse_biome_token(token);
            if(biome != BIOME_NONE || world_map_equals_ignore_case(token, "none"))
                metadata->biome = biome;
            continue;
        }

        *equals = '\0';
        world_map_trim(token);
        world_map_trim(equals + 1);
        const char* value = equals + 1;

        if(world_map_equals_ignore_case(token, "biome"))
        {
            if(world_map_token_is_legacy_river(value))
                metadata->river_tier = WORLD_MAP_RIVER_MAJOR;
            else if(world_map_token_is_legacy_lake(value))
                metadata->lake_tier = WORLD_MAP_LAKE_LARGE;
            else
                metadata->biome = world_map_parse_biome_token(value);
        }
        else if(world_map_equals_ignore_case(token, "road"))
            metadata->road_tier = world_map_parse_road_tier_token(value);
        else if(world_map_equals_ignore_case(token, "river"))
            metadata->river_tier = world_map_parse_river_tier_token(value);
        else if(world_map_equals_ignore_case(token, "lake"))
            metadata->lake_tier = world_map_parse_lake_tier_token(value);
        else if(world_map_equals_ignore_case(token, "loc")
                || world_map_equals_ignore_case(token, "location"))
            snprintf(metadata->location_name,
                     sizeof(metadata->location_name),
                     "%s",
                     value);
        else if(world_map_equals_ignore_case(token, "type"))
            snprintf(metadata->location_type_text,
                     sizeof(metadata->location_type_text),
                     "%s",
                     value);
        else if(world_map_equals_ignore_case(token, "index")
                || world_map_equals_ignore_case(token, "idx"))
            metadata->location_index = atoi(value);
        else if(world_map_equals_ignore_case(token, "gen")
                || world_map_equals_ignore_case(token, "generation")
                || world_map_equals_ignore_case(token, "generation_mode"))
            snprintf(metadata->generation_mode_text,
                     sizeof(metadata->generation_mode_text),
                     "%s",
                     value);
        else if(world_map_equals_ignore_case(token, "w")
                || world_map_equals_ignore_case(token, "width"))
            metadata->width = atoi(value);
        else if(world_map_equals_ignore_case(token, "h")
                || world_map_equals_ignore_case(token, "height"))
            metadata->height = atoi(value);
        else if(world_map_equals_ignore_case(token, "x")
                || world_map_equals_ignore_case(token, "local_x"))
            metadata->location_local_x = atoi(value);
        else if(world_map_equals_ignore_case(token, "y")
                || world_map_equals_ignore_case(token, "local_y"))
            metadata->location_local_y = atoi(value);
        else if(world_map_equals_ignore_case(token, "map")
                || world_map_equals_ignore_case(token, "predefined_map"))
            snprintf(metadata->predefined_map_path,
                     sizeof(metadata->predefined_map_path),
                     "%s",
                     value);
    }
}

static int world_map_parse_road_tier_token(const char* token)
{
    if(!token || token[0] == '\0')
        return WORLD_MAP_ROAD_TIER_NONE;

    if(world_map_equals_ignore_case(token, "none"))
        return WORLD_MAP_ROAD_TIER_NONE;
    if(world_map_equals_ignore_case(token, "trail"))
        return WORLD_MAP_ROAD_TIER_TRAIL;
    if(world_map_equals_ignore_case(token, "paved"))
        return WORLD_MAP_ROAD_TIER_PAVED;
    if(world_map_equals_ignore_case(token, "highway"))
        return WORLD_MAP_ROAD_TIER_HIGHWAY;

    return world_map_clamp_road_tier(atoi(token));
}

static int world_map_tile_has_water_feature(const WorldMapTile* tile)
{
    return tile && (tile->river_tier > WORLD_MAP_RIVER_NONE || tile->lake_tier > WORLD_MAP_LAKE_NONE);
}

static WorldMapBiome world_map_infer_base_biome_at(int x, int y, const WorldMapBiome* snapshot)
{
    int counts[BIOME_COUNT] = {0};
    int best_count = 0;
    WorldMapBiome best_biome = BIOME_GRASSLANDS;

    if(!snapshot)
        return BIOME_GRASSLANDS;

    for(int dy = -1; dy <= 1; dy++)
    {
        for(int dx = -1; dx <= 1; dx++)
        {
            int nx;
            int ny;
            WorldMapBiome biome;

            if(dx == 0 && dy == 0)
                continue;

            nx = x + dx;
            ny = y + dy;
            if(nx < 0 || ny < 0 || nx >= WORLD_MAP_WIDTH || ny >= WORLD_MAP_HEIGHT)
                continue;

            biome = snapshot[(size_t)ny * (size_t)WORLD_MAP_WIDTH + (size_t)nx];
            if(biome <= BIOME_NONE || biome >= BIOME_COUNT)
                continue;

            counts[biome]++;
            if(counts[biome] > best_count)
            {
                best_count = counts[biome];
                best_biome = biome;
            }
        }
    }

    return best_biome;
}

static void world_map_assign_base_biomes_for_water_features(void)
{
    size_t tile_count = (size_t)WORLD_MAP_WIDTH * (size_t)WORLD_MAP_HEIGHT;
    WorldMapBiome* snapshot;

    snapshot = (WorldMapBiome*)malloc(tile_count * sizeof(WorldMapBiome));
    if(!snapshot)
        return;

    for(int y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        for(int x = 0; x < WORLD_MAP_WIDTH; x++)
            snapshot[(size_t)y * (size_t)WORLD_MAP_WIDTH + (size_t)x] = world_map[y][x].biome;
    }

    for(int y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        for(int x = 0; x < WORLD_MAP_WIDTH; x++)
        {
            WorldMapTile* tile = &world_map[y][x];
            if(tile->biome != BIOME_NONE)
                continue;
            if(!world_map_tile_has_water_feature(tile))
                continue;

            tile->biome = world_map_infer_base_biome_at(x, y, snapshot);
        }
    }

    free(snapshot);
}

static void world_map_apply_cell_data(int x, int y, const char* field)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    WorldMapTileCellMetadata metadata;

    if(!tile || !field)
        return;

    world_map_parse_tile_cell(field, &metadata);
    tile->biome = metadata.biome;
    tile->road_tier = metadata.road_tier;
    tile->river_tier = metadata.river_tier;
    tile->lake_tier = metadata.lake_tier;
}

#define WORLD_MAP_BINARY_MAGIC "WMP1"
#define WORLD_MAP_BINARY_WIDTH 1000
#define WORLD_MAP_BINARY_HEIGHT 1000

static uint16_t world_map_pack_tile_value(const WorldMapTile* tile)
{
    return (uint16_t)((tile->biome & 0xF)
        | ((tile->road_tier & 0x3) << 4)
        | ((tile->river_tier & 0xF) << 6)
        | ((tile->lake_tier & 0x3) << 10));
}

static void world_map_unpack_tile_value(WorldMapTile* tile, uint16_t value)
{
    if(!tile)
        return;

    tile->biome = (WorldMapBiome)(value & 0xF);
    tile->road_tier = (value >> 4) & 0x3;
    tile->river_tier = (value >> 6) & 0xF;
    tile->lake_tier = (value >> 10) & 0x3;
}

static int world_map_build_binary_path(const char* csv_path, char* out_path, size_t out_path_size)
{
    const char* ext;

    if(!csv_path || !out_path || out_path_size == 0)
        return 0;

    ext = strrchr(csv_path, '.');
    if(ext && strcmp(ext, ".csv") == 0)
    {
        size_t prefix_length = (size_t)(ext - csv_path);
        if(prefix_length + 5 > out_path_size)
            return 0;

        memcpy(out_path, csv_path, prefix_length);
        memcpy(out_path + prefix_length, ".bin", 5);
        return 1;
    }

    if(strlen(csv_path) + 5 > out_path_size)
        return 0;

    snprintf(out_path, out_path_size, "%s.bin", csv_path);
    return 1;
}

static int world_map_load_tiles_binary(const char* path)
{
    FILE* f;
    char magic[5];
    uint8_t size_buffer[2];
    uint16_t width;
    uint16_t height;
    int y;

    if(!path || path[0] == '\0')
        return 0;

    f = fopen(path, "rb");
    if(!f)
        return 0;

    if(fread(magic, 1, 4, f) != 4)
    {
        fclose(f);
        return 0;
    }
    magic[4] = '\0';
    if(strcmp(magic, WORLD_MAP_BINARY_MAGIC) != 0)
    {
        fclose(f);
        return 0;
    }

    if(fread(size_buffer, 1, 2, f) != 2)
    {
        fclose(f);
        return 0;
    }
    width = (uint16_t)size_buffer[0] | ((uint16_t)size_buffer[1] << 8);

    if(fread(size_buffer, 1, 2, f) != 2)
    {
        fclose(f);
        return 0;
    }
    height = (uint16_t)size_buffer[0] | ((uint16_t)size_buffer[1] << 8);

    if(width != WORLD_MAP_WIDTH || height != WORLD_MAP_HEIGHT)
    {
        fclose(f);
        return 0;
    }

    for(y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        int x;
        for(x = 0; x < WORLD_MAP_WIDTH; x++)
        {
            uint16_t packed_value;
            WorldMapTile* tile;

            if(fread(&packed_value, sizeof(packed_value), 1, f) != 1)
            {
                fclose(f);
                return 0;
            }

            tile = world_map_get_tile(x, y);
            if(!tile)
            {
                fclose(f);
                return 0;
            }

            world_map_unpack_tile_value(tile, packed_value);
        }
    }

    fclose(f);
    return 1;
}

void world_map_init(void)
{    for(int y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        for(int x = 0; x < WORLD_MAP_WIDTH; x++)
        {
            world_map[y][x].zone_index = -1;
            world_map[y][x].discovered = 0;
            world_map[y][x].visited = 0;
            world_map[y][x].biome = BIOME_NONE;
            world_map[y][x].road_tier = WORLD_MAP_ROAD_TIER_NONE;
            world_map[y][x].river_tier = WORLD_MAP_RIVER_NONE;
            world_map[y][x].lake_tier = WORLD_MAP_LAKE_NONE;
        }
    }

    g_overworld_x = 0;
    g_overworld_y = 0;
    g_has_overworld_position = 0;
}

void world_map_signposts_init(void)
{
    for(int i = 0; i < WORLD_MAP_SIGNPOST_MAX_AREAS; i++)
        world_map_signposts_clear_area(i);
}

void world_map_signposts_clear_area(int area_index)
{
    SignpostAreaRegistry* registry;

    if(!world_map_signpost_area_valid(area_index))
        return;

    registry = &g_signpost_registry[area_index];
    for(int i = 0; i < registry->count; i++)
        world_map_signpost_free_instance(&registry->instances[i]);

    free(registry->instances);
    registry->instances = NULL;
    registry->count = 0;
    registry->capacity = 0;
}

SignpostInstance* world_map_signpost_register(int area_index, int x, int y, int z)
{
    SignpostAreaRegistry* registry;
    SignpostInstance* signpost;

    if(!world_map_signpost_area_valid(area_index))
        return NULL;

    registry = &g_signpost_registry[area_index];
    signpost = world_map_signpost_find(registry, x, y, z);
    if(signpost)
        return signpost;

    if(!world_map_signpost_ensure_instance_capacity(registry, registry->count + 1))
        return NULL;

    signpost = &registry->instances[registry->count++];
    memset(signpost, 0, sizeof(*signpost));
    signpost->area_index = area_index;
    signpost->x = x;
    signpost->y = y;
    signpost->z = z;
    return signpost;
}

int world_map_signpost_add_sign(int area_index,
                                int x,
                                int y,
                                int z,
                                int destination_index,
                                const char* direction,
                                const char* hint_text)
{
    SignpostInstance* signpost;
    SignpostSign* sign;

    if(destination_index < 0)
        return 0;

    signpost = world_map_signpost_register(area_index, x, y, z);
    if(!signpost)
        return 0;

    for(int i = 0; i < signpost->sign_count; i++)
    {
        if(signpost->signs[i].destination_index == destination_index)
            return 0;
    }

    if(!world_map_signpost_ensure_sign_capacity(signpost, signpost->sign_count + 1))
        return 0;

    sign = &signpost->signs[signpost->sign_count++];
    sign->destination_index = destination_index;
    snprintf(sign->direction, WORLD_MAP_SIGNPOST_DIRECTION_LENGTH, "%s", (direction && direction[0]) ? direction : "Unknown");
    snprintf(sign->hint_text, WORLD_MAP_SIGNPOST_HINT_LENGTH, "%s", (hint_text && hint_text[0]) ? hint_text : "Signpost");
    return 1;
}

const SignpostInstance* world_map_signpost_at(int area_index, int x, int y, int z)
{
    if(!world_map_signpost_area_valid(area_index))
        return NULL;
    return world_map_signpost_find(&g_signpost_registry[area_index], x, y, z);
}

SignpostInstance* world_map_signpost_at_mut(int area_index, int x, int y, int z)
{
    if(!world_map_signpost_area_valid(area_index))
        return NULL;
    return world_map_signpost_find(&g_signpost_registry[area_index], x, y, z);
}

WorldMapTile* world_map_get_tile(int x, int y)
{
    if(x < 0 || x >= WORLD_MAP_WIDTH || y < 0 || y >= WORLD_MAP_HEIGHT)
        return 0;

    return &world_map[y][x];
}

void world_map_set_zone(int x, int y, int zone_index)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return;

    tile->zone_index = zone_index;
}

void world_map_mark_discovered(int x, int y)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return;

    tile->discovered = 1;
}

void world_map_mark_scouted(int x, int y)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return;

    // Scouting grants persistent visibility but is not a visit.
    tile->discovered = 1;
}

void world_map_mark_visited(int x, int y)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return;

    tile->visited = 1;
}

void world_map_set_road_tier(int x, int y, int road_tier)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return;

    tile->road_tier = world_map_clamp_road_tier(road_tier);
}

int world_map_get_road_tier(int x, int y)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return WORLD_MAP_ROAD_TIER_NONE;
    return world_map_clamp_road_tier(tile->road_tier);
}

void world_map_set_river_tier(int x, int y, int river_tier)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return;

    tile->river_tier = world_map_clamp_river_tier(river_tier);
}

int world_map_get_river_tier(int x, int y)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return WORLD_MAP_RIVER_NONE;
    return world_map_clamp_river_tier(tile->river_tier);
}

void world_map_set_lake_tier(int x, int y, int lake_tier)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return;

    tile->lake_tier = world_map_clamp_lake_tier(lake_tier);
}

int world_map_get_lake_tier(int x, int y)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return WORLD_MAP_LAKE_NONE;
    return world_map_clamp_lake_tier(tile->lake_tier);
}

void world_map_draw_road(int x0, int y0, int x1, int y1, int road_tier)
{
    int dx;
    int sx;
    int dy;
    int sy;
    int err;
    int x;
    int y;

    road_tier = world_map_clamp_road_tier(road_tier);
    if(road_tier <= WORLD_MAP_ROAD_TIER_NONE)
        return;

    dx = abs(x1 - x0);
    sx = (x0 < x1) ? 1 : -1;
    dy = -abs(y1 - y0);
    sy = (y0 < y1) ? 1 : -1;
    err = dx + dy;
    x = x0;
    y = y0;

    while(1)
    {
        WorldMapTile* tile = world_map_get_tile(x, y);
        if(tile && tile->road_tier < road_tier)
            tile->road_tier = road_tier;

        if(x == x1 && y == y1)
            break;

        {
            int e2 = 2 * err;
            if(e2 >= dy)
            {
                err += dy;
                x += sx;
            }
            if(e2 <= dx)
            {
                err += dx;
                y += sy;
            }
        }
    }
}

int world_map_road_tier_stamina_cost(int road_tier)
{
    switch(world_map_clamp_road_tier(road_tier))
    {
        case WORLD_MAP_ROAD_TIER_HIGHWAY:
        case WORLD_MAP_ROAD_TIER_PAVED:
            return 0;
        case WORLD_MAP_ROAD_TIER_TRAIL:
        case WORLD_MAP_ROAD_TIER_NONE:
        default:
            return 1;
    }
}

int world_map_step_stamina_cost(int x, int y)
{
    return world_map_road_tier_stamina_cost(world_map_get_road_tier(x, y));
}

int world_map_find_zone(int zone_index, int* out_x, int* out_y)
{
    if(zone_index < 0 || !out_x || !out_y)
        return 0;

    for(int y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        for(int x = 0; x < WORLD_MAP_WIDTH; x++)
        {
            if(world_map[y][x].zone_index != zone_index)
                continue;

            *out_x = x;
            *out_y = y;
            return 1;
        }
    }

    return 0;
}

void world_map_set_overworld_position(int x, int y)
{
    if(x < 0 || x >= WORLD_MAP_WIDTH || y < 0 || y >= WORLD_MAP_HEIGHT)
        return;

    g_overworld_x = x;
    g_overworld_y = y;
    g_has_overworld_position = 1;
}

int world_map_get_overworld_position(int* out_x, int* out_y)
{
    if(!g_has_overworld_position || !out_x || !out_y)
        return 0;

    *out_x = g_overworld_x;
    *out_y = g_overworld_y;
    return 1;
}

void world_map_set_biome(int x, int y, WorldMapBiome biome)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return;
    tile->biome = biome;
}

WorldMapBiome world_map_get_biome(int x, int y)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return BIOME_NONE;
    return tile->biome;
}

const char* world_map_biome_name(WorldMapBiome biome)
{
    switch(biome)
    {
        case BIOME_GRASSLANDS: return "Grasslands";
        case BIOME_FOREST:     return "Forest";
        case BIOME_DESERT:     return "Desert";
        case BIOME_TUNDRA:     return "Tundra";
        case BIOME_TAIGA:      return "Taiga";
        case BIOME_SHRUBLAND:  return "Shrubland";
        case BIOME_STEPPE:     return "Steppe";
        case BIOME_GLACIER:    return "Glacier";
        case BIOME_SEA:        return "Sea";
        case BIOME_SAVANNAH:   return "Savannah";
        case BIOME_MOUNTAINS:  return "Mountains";
        case BIOME_FOOTHILLS:  return "Foothills";
        case BIOME_SWAMP:      return "Swamp";
        case BIOME_JUNGLE:     return "Jungle";
        default:               return "Wilderness";
    }
}

void world_map_load_tiles(const char* path)
{
    FILE* f;
    int y;
    int x;
    char bin_path[260];

    if(!path || path[0] == '\0')
        return;

    if(world_map_build_binary_path(path, bin_path, sizeof(bin_path))
       && world_map_load_tiles_binary(bin_path))
    {
        world_map_assign_base_biomes_for_water_features();
        return;
    }

    f = fopen(path, "r");
    if(!f)
        return;

    {
        size_t line_capacity = ((size_t)WORLD_MAP_WIDTH * 128u) + 1024u;
        char* line = (char*)malloc(line_capacity);

        if(!line)
        {
            fclose(f);
            return;
        }

        y = 0;
        while(y < WORLD_MAP_HEIGHT && fgets(line, (int)line_capacity, f))
        {
            char* cursor;
            char* content;
            int found_value = 0;

            world_map_trim(line);
            content = line;
            if(content[0] == '"')
                content++;
            if(content[0] == '\0' || content[0] == '#' || content[0] == ';')
                continue;

            cursor = line;
            x = 0;
            while(x < WORLD_MAP_WIDTH)
            {
                char field[64];
                int i = 0;

                while(*cursor && *cursor != ',' && *cursor != ';' && *cursor != '\n' && *cursor != '\r')
                {
                    if(i + 1 < (int)sizeof(field))
                        field[i++] = *cursor;
                    cursor++;
                }
                field[i] = '\0';
                world_map_trim(field);
                world_map_apply_cell_data(x, y, field);
                if(field[0] != '\0')
                    found_value = 1;
                x++;

                if(*cursor == ',' || *cursor == ';')
                {
                    cursor++;
                    continue;
                }
                break;
            }

            if(found_value)
                y++;
        }

        world_map_assign_base_biomes_for_water_features();
        free(line);
        fclose(f);
    }
}
