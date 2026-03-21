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
typedef struct {
    char symbol;       // what to display
    RenderColor color; // display color for the glyph
    char name[32];     // name of the tile
    int interactable;  // 1 = can trigger interaction
    int blocks_movement;   // 1 = blocks movement/collision
    int blocks_sight;      // 1 = blocks line of sight
    int blocks_projectile; // 1 = blocks projectiles
} Tile;

// Construct a default stone-floor tile instance.
Tile tile_stone_floor();

// Construct a default dirt-floor tile instance.
Tile tile_dirt_floor();

// Construct a default grass tile instance.
Tile tile_grass();

// Construct a default tree tile instance.
Tile tile_tree();

// Construct a default out-of-bounds tile instance.
Tile tile_out_of_bounds();

// Construct a default wall tile instance.
Tile tile_wall();

// Construct a default door tile instance (closed state).
Tile tile_door();

#endif
