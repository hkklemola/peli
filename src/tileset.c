#include "tileset.h"

/*
 * Purpose:
 *   Defines immutable tile presets reused by map generation.
 */

const Tile TILE_STONE_FLOOR = {
    .symbol = '.',
    .color = RENDER_COLOR_DARK_GRAY,
    .name = "Stone Floor",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_DIRT_FLOOR = {
    .symbol = '.',
    .color = RENDER_COLOR_BROWN,
    .name = "Dirt Floor",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_GRASS = {
    .symbol = '.',
    .color = RENDER_COLOR_GREEN,
    .name = "Grass",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_TREE = {
    .symbol = 'T',
    .color = RENDER_COLOR_GREEN,
    .name = "Tree",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

const Tile TILE_OUT_OF_BOUNDS = {
    .symbol = '~',
    .color = RENDER_COLOR_DARK_GRAY,
    .name = "Out of Bounds",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

const Tile TILE_WALL = {
    .symbol = '#',
    .color = RENDER_COLOR_LIGHT_GRAY,
    .name = "Wall",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

