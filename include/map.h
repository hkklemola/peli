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

// Default viewport (display) size (runtime config may override)
#define VIEW_WIDTH 120
#define VIEW_HEIGHT 20

// Maximum room settings
#define MAX_ROOMS 18
#define ROOM_MIN_SIZE 8
#define ROOM_MAX_SIZE 16

struct Area;

// Generate map tiles for a specific area instance.
void map_generate_area(struct Area* area);

// Regenerate map for current active area.
void generate_map();

// Determine whether a line of sight exists between two coordinates.
// Returns 1 when target is visible from source, 0 otherwise.
int map_has_line_of_sight(int x0, int y0, int x1, int y1);

#endif

