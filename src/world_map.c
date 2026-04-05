#include "world_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void world_map_init(void)
{
    for(int y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        for(int x = 0; x < WORLD_MAP_WIDTH; x++)
        {
            world_map[y][x].zone_index = -1;
            world_map[y][x].discovered = 0;
            world_map[y][x].visited = 0;
            world_map[y][x].biome = BIOME_NONE;
            world_map[y][x].road_tier = WORLD_MAP_ROAD_TIER_NONE;
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
        case BIOME_FARMLANDS:  return "Farmlands";
        case BIOME_DESERT:     return "Desert";
        case BIOME_TUNDRA:     return "Tundra";
        case BIOME_RIVER:      return "River";
        case BIOME_LAKE:       return "Lake";
        case BIOME_SEA:        return "Sea";
        case BIOME_SAVANNAH:   return "Savannah";
        case BIOME_MOUNTAINS:  return "Mountains";
        case BIOME_FOOTHILLS:  return "Foothills";
        case BIOME_SWAMP:      return "Swamp";
        case BIOME_JUNGLE:     return "Jungle";
        default:               return "Wilderness";
    }
}

void world_map_load_biomes(const char* path)
{
    FILE* f;
    int y;
    int x;
    int ch;
    WorldMapBiome biome;

    f = fopen(path, "r");
    if(!f)
        return;

    for(y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        x = 0;
        while(x < WORLD_MAP_WIDTH)
        {
            ch = fgetc(f);
            if(ch == EOF)
                goto done;
            if(ch == '\n' || ch == '\r')
                continue;

            switch(ch)
            {
                case '.': biome = BIOME_GRASSLANDS; break;
                case '"': biome = BIOME_FOREST;     break;
                case '%': biome = BIOME_FARMLANDS;  break;
                case '~': biome = BIOME_DESERT;     break;
                case '\'': biome = BIOME_TUNDRA;   break;
                case 'r': biome = BIOME_RIVER;      break;
                case 'l': biome = BIOME_LAKE;       break;
                case 's': biome = BIOME_SEA;        break;
                case ',': biome = BIOME_SAVANNAH;   break;
                case '^': biome = BIOME_MOUNTAINS;  break;
                case 'n': biome = BIOME_FOOTHILLS;  break;
                case 'm': biome = BIOME_SWAMP;      break;
                case 'j': biome = BIOME_JUNGLE;     break;
                default:  biome = BIOME_NONE;       break;
            }
            world_map[y][x].biome = biome;
            x++;
        }
    }
done:
    fclose(f);
}
