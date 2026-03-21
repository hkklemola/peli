#ifndef WORLD_MAP_H
#define WORLD_MAP_H

#include <stdint.h>

#define WORLD_MAP_WIDTH 100
#define WORLD_MAP_HEIGHT 100
#define WORLD_MAP_TILE_METERS 1000  // Each world map tile represents 1 km x 1 km

typedef struct {
    int zone_index;   // -1 means no zone assigned
    int discovered;   // 0 or 1
    int visited;      // 0 or 1
} WorldMapTile;

extern WorldMapTile world_map[WORLD_MAP_HEIGHT][WORLD_MAP_WIDTH];

// Initialize world map state (default unassigned, unvisited, undiscovered)
void world_map_init(void);

// Register a zone at world map coordinates.
void world_map_set_zone(int x, int y, int zone_index);

// Mark a world map tile as discovered.
void world_map_mark_discovered(int x, int y);

// Mark a world map tile as visited.
void world_map_mark_visited(int x, int y);

// Query zone info by world coordinate.
WorldMapTile* world_map_get_tile(int x, int y);

#endif
