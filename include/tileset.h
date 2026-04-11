#ifndef TILESET_H
#define TILESET_H

#include "tile.h"

/*
 * Purpose:
 *   Declares reusable constant tile presets used by map generation.
 */

// Basic tile types
extern const Tile TILE_EMPTY;
extern const Tile TILE_STONE_FLOOR;

// Ground layer tiles
extern const Tile TILE_DIRT;
extern const Tile TILE_SAND;
extern const Tile TILE_MUD;
extern const Tile TILE_SHALLOW_WATER;
extern const Tile TILE_GRAVEL;
extern const Tile TILE_ROCK;

// Floor layer tiles
extern const Tile TILE_WOOD_PLANK;
extern const Tile TILE_CLAY_BRICK;
extern const Tile TILE_STONE_TILE;
extern const Tile TILE_MARBLE_TILE;
extern const Tile TILE_STRAW;

// General tiles
extern const Tile TILE_GRASS;
extern const Tile TILE_TREE;
extern const Tile TILE_TREE_STUMP;
extern const Tile TILE_OUT_OF_BOUNDS;

// Structure layer tiles (walls)
extern const Tile TILE_STONE_BRICK_WALL;
extern const Tile TILE_LOG_WALL;
extern const Tile TILE_CLAY_BRICK_WALL;
extern const Tile TILE_CAVE_WALL;
extern const Tile TILE_PLANK_WALL;

// Transition tiles
extern const Tile TILE_STAIRS_UP;
extern const Tile TILE_STAIRS_DOWN;

#endif


