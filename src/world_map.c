#include "world_map.h"

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

void world_map_init(void)
{
    for(int y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        for(int x = 0; x < WORLD_MAP_WIDTH; x++)
        {
            world_map[y][x].zone_index = -1;
            world_map[y][x].discovered = 0;
            world_map[y][x].visited = 0;
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

void world_map_mark_visited(int x, int y)
{
    WorldMapTile* tile = world_map_get_tile(x, y);
    if(!tile)
        return;

    tile->visited = 1;
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
