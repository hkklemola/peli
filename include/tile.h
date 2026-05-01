#ifndef TILE_H
#define TILE_H

#include <stdio.h>

#include "render_color.h"

typedef struct Area Area;

/*
 * Purpose:
 *   Defines tile material flags and constructors for canonical tile types.
 *
 * Functions:
 *   - tile_stone_floor: builds default stone-floor tile flags.
 *   - tile_dirt_floor: builds default dirt-floor tile flags.
 *   - tile_grass: builds default grass tile flags.
 *   - tile_tree: builds default tree tile flags.
 *   - tile_out_of_bounds: builds default out-of-bounds tile flags.
 *   - tile_wall: builds default wall tile flags.
 *   - tile_door: builds default closed-door tile flags.
 */

// Tile structure
typedef enum TileLayer {
    TILE_LAYER_GROUND = 0,
    TILE_LAYER_FLOOR,
    TILE_LAYER_WALL,
    TILE_LAYER_DECOR,
    TILE_LAYER_EFFECT,
    TILE_LAYER_COUNT
} TileLayer;

typedef enum TileSurfaceKind {
    TILE_SURFACE_EMPTY = 0,
    TILE_SURFACE_NATURAL,
    TILE_SURFACE_CONSTRUCTED,
    TILE_SURFACE_WALL,
    TILE_SURFACE_HAZARD
} TileSurfaceKind;

typedef enum TreeSpecies {
    TREE_SPECIES_NONE = 0,
    TREE_SPECIES_OAK,
    TREE_SPECIES_SPRUCE,
    TREE_SPECIES_PINE,
    TREE_SPECIES_BIRCH,
    TREE_SPECIES_YEW,
    TREE_SPECIES_MAPLE,
    TREE_SPECIES_COUNT
} TreeSpecies;

typedef struct TreeSpeciesInfo {
    TreeSpecies species;
    const char* tree_name;
    const char* stump_name;
    const char* log_name;
    unsigned char tree_symbol;
    unsigned char stump_symbol;
    int tree_color;
    int stump_color;
    int hardness;
    int max_structure_points;
} TreeSpeciesInfo;

typedef struct {
    unsigned char symbol;       // what to display
    int color;         // display color (legacy ANSI code or 0-255 palette index)
    char name[32];     // name of the tile
    TileLayer layer;   // default logical layer for this tile kind
    int hide_below;    // 1 hides lower layers in inspection/visibility traversal
    int interactable;  // 1 = can trigger interaction
    int blocks_movement;   // 1 = blocks movement/collision
    int blocks_sight;      // 1 = blocks line of sight
    int blocks_projectile; // 1 = blocks projectiles
    int fishable;          // 1 = can be fished in
    int harvestable;       // 1 = can be harvested with herbalism tools
} Tile;

// Construct an empty tile used for unoccupied layers.
Tile tile_empty();

// Construct a default stone-floor tile instance.
Tile tile_stone_floor();

// Construct a default dirt tile instance (ground layer).
Tile tile_dirt();

// Construct a default sand tile instance (ground layer).
Tile tile_sand();

// Construct a default mud tile instance (ground layer).
Tile tile_mud();

// Construct a default shallow-water tile instance (ground layer).
Tile tile_shallow_water();

// Construct a default gravel tile instance (ground layer).
Tile tile_gravel();

// Construct a default rock tile instance (ground layer).
Tile tile_rock();

// Construct a default wood plank tile instance (floor layer).
Tile tile_wood_plank();

// Construct a default clay brick tile instance (floor layer).
Tile tile_clay_brick();

// Construct a default stone tile instance (floor layer).
Tile tile_stone_tile();

// Construct a default marble tile instance (floor layer).
Tile tile_marble_tile();

// Construct a default straw tile instance (floor layer).
Tile tile_straw();

// Construct a default grass tile instance.
Tile tile_grass();

// Construct a default tree tile instance.
Tile tile_tree();

// Construct a species-specific tree tile instance.
Tile tile_tree_for_species(TreeSpecies species);

// Construct a default tree stump tile instance.
Tile tile_tree_stump();

// Construct a species-specific tree stump tile instance.
Tile tile_tree_stump_for_species(TreeSpecies species);

// Construct a default out-of-bounds tile instance.
Tile tile_out_of_bounds();

// Construct a default stone brick wall tile instance (structure layer).

// Construct a default stone brick wall tile instance (wall layer).
Tile tile_stone_brick_wall();
// Construct a default log wall tile instance (wall layer).
Tile tile_log_wall();
// Construct a default clay brick wall tile instance (wall layer).
Tile tile_clay_brick_wall();
// Construct a default cave wall tile instance (wall layer).
Tile tile_cave_wall();
// Construct a default plank wall tile instance (wall layer).
Tile tile_plank_wall();

// Construct a default closed-door tile instance (wall layer).
Tile tile_door();

// Return 1 if tile is visually empty, 0 otherwise.
int tile_is_empty(const Tile* tile);

// Return 1 if tile is a wall tile used for map rendering connections.
int tile_is_wall_tile(const Tile* tile);

// Return 1 if tile is a fence-like wall tile rendered with single-line box drawing.
int tile_is_fence_tile(const Tile* tile);

// Return 1 if tile is a thick wall tile rendered with double-line box drawing.
int tile_is_double_line_wall(const Tile* tile);

// Return 1 if tile is any staircase tile.
int tile_is_staircase(const Tile* tile);

// Return 1 if staircase orientation at (x,y,z) is horizontal (E-W), 0 if vertical (N-S).
int tile_stair_is_horizontal_at(const Area* area, int x, int y, int z);

// Return 1 if a staircase at (x,y,z) connects to a staircase at z+sign(dz).
int tile_stair_connected_step(const Area* area, int x, int y, int z, int dz);

// Return +1 if entering stair tile from (from_x,from_y,z) leads upward, -1 if downward,
// or 0 when the stair is not enterable from that side.
int tile_stair_entry_delta_z(const Area* area,
                             int from_x,
                             int from_y,
                             int z,
                             int stair_x,
                             int stair_y);

// Return metadata for a known tree species, or a safe default for unknown values.
const TreeSpeciesInfo* tree_species_info(TreeSpecies species);

// Return 1 when the tile is a standing tree.
int tile_is_tree(const Tile* tile);

// Return 1 when the tile is a tree stump.
int tile_is_tree_stump(const Tile* tile);

// Resolve a tree or stump tile to its species, or TREE_SPECIES_NONE when not applicable.
TreeSpecies tile_tree_species(const Tile* tile);

// Classify tile semantics independent of render layer usage.
TileSurfaceKind tile_surface_kind(const Tile* tile);

// Return 1 if the tile is fishable, 0 otherwise.
int tile_is_fishable(const Tile* tile);

// Return 1 if the tile is harvestable, 0 otherwise.
int tile_is_harvestable(const Tile* tile);

// Return 1 when a tile surface kind is valid for a target layer.
int tile_layer_accepts_surface(TileLayer layer, TileSurfaceKind kind);

#endif
