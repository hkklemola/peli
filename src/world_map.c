#include "world_map.h"
#include <stdio.h>

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
