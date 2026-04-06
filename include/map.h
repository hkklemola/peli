#ifndef MAP_H
#define MAP_H

#include "tile.h"

// Forward declaration to avoid circular dependency with atlas.h
typedef struct Area Area;

/*
 * Purpose:
 *   Declares map dimensions and area-generation entry points.
 *
 * Functions:
 *   - map_generate_area: generates map tiles based on area type.
 *   - generate_map: regenerates map for current active area.
 */

#define AREA_MAX_WIDTH 1000
#define AREA_MAX_HEIGHT 1000
#define AREA_DEFAULT_WIDTH 1000
#define AREA_DEFAULT_HEIGHT 1000

/* Legacy storage aliases kept for fixed-cap area arrays. */
#define MAP_WIDTH AREA_MAX_WIDTH
#define MAP_HEIGHT AREA_MAX_HEIGHT

// Handcrafted starter-area region inside the full map.
// Now the full map is the starter area.
#define STARTER_AREA_WIDTH AREA_DEFAULT_WIDTH
#define STARTER_AREA_HEIGHT AREA_DEFAULT_HEIGHT
#define STARTER_AREA_X 0
#define STARTER_AREA_Y 0

#define STARTER_HUT_WIDTH 12
#define STARTER_HUT_HEIGHT 8
#define STARTER_HUT_OFFSET_X 8
#define STARTER_HUT_OFFSET_Y -4

/* Spawn inside the Starter Hut, on the floor tile right next to the bed. */
#define STARTER_PLAYER_START_X ((AREA_DEFAULT_WIDTH / 2) + STARTER_HUT_OFFSET_X + 3)
#define STARTER_PLAYER_START_Y ((AREA_DEFAULT_HEIGHT / 2) + STARTER_HUT_OFFSET_Y + 2)

#define HERMIT_TOWER_WIDTH 9
#define HERMIT_TOWER_HEIGHT 9
#define HERMIT_TOWER_OFFSET_X -20
#define HERMIT_TOWER_OFFSET_Y -10
#define HERMIT_TOWER_MAX_FLOORS 5
#define HERMIT_TOWER_FLOOR_Z_STEP 5
#define HERMIT_TOWER_TOTAL_Z_LEVELS (1 + ((HERMIT_TOWER_MAX_FLOORS - 1) * HERMIT_TOWER_FLOOR_Z_STEP))

#define MAX_AREA_FLOORS HERMIT_TOWER_TOTAL_Z_LEVELS
#define AREA_UPPER_FLOOR_MAX_WIDTH 32
#define AREA_UPPER_FLOOR_MAX_HEIGHT 32

#define AREA_MIN_Z 0
#define AREA_GROUND_Z 50
#define AREA_MAX_Z 99
#define HERMIT_TOWER_BASE_Z AREA_GROUND_Z
#define HERMIT_TOWER_TOP_Z (HERMIT_TOWER_BASE_Z + ((HERMIT_TOWER_MAX_FLOORS - 1) * HERMIT_TOWER_FLOOR_Z_STEP))

// Default viewport (display) size (runtime config may override)
#define VIEW_WIDTH 120
#define VIEW_HEIGHT 20

// Maximum room settings
#define MAX_ROOMS 18
#define ROOM_MIN_SIZE 8
#define ROOM_MAX_SIZE 16



// Return a mutable tile pointer at area coordinate/layer on the active viewed floor, or NULL when invalid.
Tile* map_tile_at_layer(Area* area, int x, int y, TileLayer layer);

// Return a mutable tile pointer at a specific z-level/layer, or NULL when invalid.
Tile* map_tile_at_layer_z(Area* area, int x, int y, int z, TileLayer layer);

// Return highest visible static tile at coordinate, or NULL when no static tile exists.
const Tile* map_top_visible_tile(const Area* area, int x, int y, TileLayer* out_layer);

// Return highest visible static tile at coordinate for a requested view floor.
const Tile* map_top_visible_tile_at_view(const Area* area, int x, int y, int view_floor, TileLayer* out_layer);

// Return highest supported floor index for area rendering/view controls.
int map_max_view_floor(const Area* area);

// Clamp a requested view floor into area-supported bounds.
int map_clamp_view_floor(const Area* area, int floor);

// Return 1 when tile has been discovered, 0 otherwise.
int map_is_tile_discovered(const Area* area, int x, int y);

// Mark one tile as discovered.
void map_mark_tile_discovered(Area* area, int x, int y);

// Clear discovery state for all tiles in an area.
void map_clear_discovery(Area* area);

// Clear all last-known entity markers in an area.
void map_clear_entity_markers(Area* area);

// Set one last-known entity marker at tile and z.
void map_set_entity_marker(Area* area, int x, int y, int z, char symbol, int color);

// Clear one last-known entity marker at tile for matching z.
void map_clear_entity_marker(Area* area, int x, int y, int z);

// Get one last-known entity marker at tile for matching z.
int map_get_entity_marker(const Area* area, int x, int y, int z, char* out_symbol, int* out_color);

// Reveal tiles around origin using LOS checks and vision range.
void map_reveal_from_point(Area* area, int origin_x, int origin_y, int vision_range);

// Return whether any static layer blocks movement at coordinate.
int map_cell_blocks_movement(const Area* area, int x, int y);

// Return whether any static layer blocks sight at coordinate.
int map_cell_blocks_sight(const Area* area, int x, int y);

// Collect visible static layers from top to bottom until hide_below stops traversal.
int map_collect_visible_static_layers(const Area* area, int x, int y, const Tile** out_tiles, TileLayer* out_layers, int max_count);

// Generate map tiles for a specific area instance.
void map_generate_area(Area* area);

// Floor location helpers for stair placement.
int find_floor_tile_for_stairs(const Area* area, int* out_x, int* out_y);
void place_stairs_tile(Area* area, int x, int y);

// Regenerate map for current active area.
void generate_map();

// Spawn the Starter Hut structure with chests at target top-left coordinates.
void map_spawn_starter_hut(Area* area, int origin_x, int origin_y);

// Spawn the Hermit Tower footprint with interior stairs near starter spawn.
void map_spawn_hermit_tower(Area* area, int origin_x, int origin_y);

// Determine whether a line of sight exists between two coordinates.
// Returns 1 when target is visible from source, 0 otherwise.
int map_has_line_of_sight(int x0, int y0, int x1, int y1);

// Determine whether a projectile can travel between two coordinates.
// Returns 1 when path is unobstructed by projectile-blocking tiles.
int map_has_projectile_path(int x0, int y0, int x1, int y1);

#endif

