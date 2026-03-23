#ifndef TILE_H
#define TILE_H

#include <stdio.h>

#include "render_color.h"

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
    TILE_LAYER_STRUCTURE,
    TILE_LAYER_DECOR,
    TILE_LAYER_UNIT,
    TILE_LAYER_EFFECT,
    TILE_LAYER_COUNT
} TileLayer;

typedef enum TileSurfaceKind {
    TILE_SURFACE_EMPTY = 0,
    TILE_SURFACE_NATURAL,
    TILE_SURFACE_CONSTRUCTED,
    TILE_SURFACE_STRUCTURE,
    TILE_SURFACE_HAZARD
} TileSurfaceKind;

typedef struct {
    char symbol;       // what to display
    int color;         // display color (legacy ANSI code or 0-255 palette index)
    char name[32];     // name of the tile
    TileLayer layer;   // default logical layer for this tile kind
    int hide_below;    // 1 hides lower layers in inspection/visibility traversal
    int interactable;  // 1 = can trigger interaction
    int blocks_movement;   // 1 = blocks movement/collision
    int blocks_sight;      // 1 = blocks line of sight
    int blocks_projectile; // 1 = blocks projectiles
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

// Construct a default out-of-bounds tile instance.
Tile tile_out_of_bounds();

// Construct a default stone brick wall tile instance (structure layer).
Tile tile_stone_brick_wall();

// Construct a default log wall tile instance (structure layer).
Tile tile_log_wall();

// Construct a default clay brick wall tile instance (structure layer).
Tile tile_clay_brick_wall();

// Construct a default cave wall tile instance (structure layer).
Tile tile_cave_wall();

// Construct a default plank wall tile instance (structure layer).
Tile tile_plank_wall();

// Construct a default chest tile instance (structure layer, furniture).
Tile tile_chest();

// Construct a default chair tile instance (structure layer, furniture).
Tile tile_chair();

// Construct a default table tile instance (structure layer, furniture).
Tile tile_table();

// Construct a default barrel tile instance (structure layer, furniture).
Tile tile_barrel();

// Construct a default door tile instance (closed state).
Tile tile_door();

// Return 1 if tile is visually empty, 0 otherwise.
int tile_is_empty(const Tile* tile);

// Classify tile semantics independent of render layer usage.
TileSurfaceKind tile_surface_kind(const Tile* tile);

// Return 1 when a tile surface kind is valid for a target layer.
int tile_layer_accepts_surface(TileLayer layer, TileSurfaceKind kind);

#endif
