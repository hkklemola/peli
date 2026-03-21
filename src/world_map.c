#include "world_map.h"

WorldMapTile world_map[WORLD_MAP_HEIGHT][WORLD_MAP_WIDTH];

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
