#ifndef MAP_H
#define MAP_H

#include "tile.h"

/*
 * Purpose:
 *   Declares map dimensions and area-generation entry points.
 *
 * Functions:
 *   - map_generate_area: generates map tiles based on area type.
 *   - generate_map: regenerates map for current active area.
 */

#define AREA_MAX_WIDTH 256
#define AREA_MAX_HEIGHT 256
#define AREA_DEFAULT_WIDTH 250
#define AREA_DEFAULT_HEIGHT 250

/* Legacy storage aliases kept for fixed-cap area arrays. */
#define MAP_WIDTH AREA_MAX_WIDTH
#define MAP_HEIGHT AREA_MAX_HEIGHT

// Handcrafted starter-area region inside the full map.
// Now the full map is the starter area.
#define STARTER_AREA_WIDTH AREA_DEFAULT_WIDTH
#define STARTER_AREA_HEIGHT AREA_DEFAULT_HEIGHT
#define STARTER_AREA_X 0
#define STARTER_AREA_Y 0
#define STARTER_PLAYER_START_X (AREA_DEFAULT_WIDTH / 2)
#define STARTER_PLAYER_START_Y (AREA_DEFAULT_HEIGHT / 2)

#define DEV_HUT_WIDTH 12
#define DEV_HUT_HEIGHT 8
#define DEV_HUT_OFFSET_X 8
#define DEV_HUT_OFFSET_Y -4

// Default viewport (display) size (runtime config may override)
#define VIEW_WIDTH 120
#define VIEW_HEIGHT 20

// Maximum room settings
#define MAX_ROOMS 18
#define ROOM_MIN_SIZE 8
#define ROOM_MAX_SIZE 16

struct Area;

// Return a mutable tile pointer at area coordinate/layer, or NULL when invalid.
Tile* map_tile_at_layer(struct Area* area, int x, int y, TileLayer layer);

// Return highest visible static tile at coordinate, or NULL when no static tile exists.
const Tile* map_top_visible_tile(const struct Area* area, int x, int y, TileLayer* out_layer);

// Return whether any static layer blocks movement at coordinate.
int map_cell_blocks_movement(const struct Area* area, int x, int y);

// Return whether any static layer blocks sight at coordinate.
int map_cell_blocks_sight(const struct Area* area, int x, int y);

// Collect visible static layers from top to bottom until hide_below stops traversal.
int map_collect_visible_static_layers(const struct Area* area, int x, int y, const Tile** out_tiles, TileLayer* out_layers, int max_count);

// Generate map tiles for a specific area instance.
void map_generate_area(struct Area* area);

// Floor location helpers for stair placement.
int find_floor_tile_for_stairs(const struct Area* area, int* out_x, int* out_y);
void place_stairs_tile(struct Area* area, int x, int y);

// Regenerate map for current active area.
void generate_map();

// Spawn the Dev Hut structure with chests at target top-left coordinates.
void map_spawn_dev_hut(struct Area* area, int origin_x, int origin_y);

// Determine whether a line of sight exists between two coordinates.
// Returns 1 when target is visible from source, 0 otherwise.
int map_has_line_of_sight(int x0, int y0, int x1, int y1);

#endif

