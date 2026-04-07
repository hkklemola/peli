#include "tileset.h"

/*
 * Purpose:
 *   Defines immutable tile presets reused by map generation.
 */

const Tile TILE_STONE_FLOOR = {
    .layer = TILE_LAYER_FLOOR,
    .hide_below = 1,
    .symbol = '.',
    .color = RENDER_COLOR_DARK_GRAY,
    .name = "Stone Floor",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_EMPTY = {
    .layer = TILE_LAYER_GROUND,
    .hide_below = 0,
    .symbol = '\0',
    .color = RENDER_COLOR_DEFAULT,
    .name = "",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_DIRT = {
    .layer = TILE_LAYER_GROUND,
    .hide_below = 0,
    .symbol = '.',
    .color = RENDER_COLOR_LIGHT_YELLOW,
    .name = "Dirt",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_SAND = {
    .layer = TILE_LAYER_GROUND,
    .hide_below = 0,
    .symbol = '.',
    .color = RENDER_COLOR_LIGHT_YELLOW,
    .name = "Sand",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_MUD = {
    .layer = TILE_LAYER_GROUND,
    .hide_below = 0,
    .symbol = '.',
    .color = RENDER_COLOR_DARK_GRAY,
    .name = "Mud",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_GRAVEL = {
    .layer = TILE_LAYER_GROUND,
    .hide_below = 0,
    .symbol = '.',
    .color = RENDER_COLOR_LIGHT_GRAY,
    .name = "Gravel",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_ROCK = {
    .layer = TILE_LAYER_GROUND,
    .hide_below = 0,
    .symbol = '.',
    .color = RENDER_COLOR_DARK_GRAY,
    .name = "Rock",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_WOOD_PLANK = {
    .layer = TILE_LAYER_FLOOR,
    .hide_below = 1,
    .symbol = '.',
    .color = RENDER_COLOR_BROWN,
    .name = "Wood Plank",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_CLAY_BRICK = {
    .layer = TILE_LAYER_FLOOR,
    .hide_below = 1,
    .symbol = '.',
    .color = RENDER_COLOR_LIGHT_RED,
    .name = "Clay Brick",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_STONE_TILE = {
    .layer = TILE_LAYER_FLOOR,
    .hide_below = 1,
    .symbol = '.',
    .color = RENDER_COLOR_DARK_GRAY,
    .name = "Stone Tile",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_MARBLE_TILE = {
    .layer = TILE_LAYER_FLOOR,
    .hide_below = 1,
    .symbol = '.',
    .color = RENDER_COLOR_WHITE,
    .name = "Marble Tile",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_STRAW = {
    .layer = TILE_LAYER_FLOOR,
    .hide_below = 1,
    .symbol = '.',
    .color = RENDER_COLOR_LIGHT_YELLOW,
    .name = "Straw",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_GRASS = {
    .layer = TILE_LAYER_GROUND,
    .hide_below = 1,
    .symbol = '.',
    .color = RENDER_COLOR_GREEN,
    .name = "Grass",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_TREE = {
    .layer = TILE_LAYER_WALL,
    .hide_below = 1,
    .symbol = 'T',
    .color = RENDER_COLOR_GREEN,
    .name = "Oak Tree",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

const Tile TILE_TREE_STUMP = {
    .layer = TILE_LAYER_WALL,
    .hide_below = 0,
    .symbol = 't',
    .color = RENDER_COLOR_BROWN,
    .name = "Oak Stump",
    .interactable = 0,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_OUT_OF_BOUNDS = {
    .layer = TILE_LAYER_GROUND,
    .hide_below = 1,
    .symbol = '~',
    .color = RENDER_COLOR_DARK_GRAY,
    .name = "Out of Bounds",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

const Tile TILE_STONE_BRICK_WALL = {
    .layer = TILE_LAYER_WALL,
    .hide_below = 0,
    .symbol = '#',
    .color = RENDER_COLOR_LIGHT_GRAY,
    .name = "Stone Brick Wall",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

const Tile TILE_LOG_WALL = {
    .layer = TILE_LAYER_WALL,
    .hide_below = 0,
    .symbol = '#',
    .color = RENDER_COLOR_BROWN,
    .name = "Log Wall",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

const Tile TILE_CLAY_BRICK_WALL = {
    .layer = TILE_LAYER_WALL,
    .hide_below = 0,
    .symbol = '#',
    .color = RENDER_COLOR_LIGHT_RED,
    .name = "Clay Brick Wall",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

const Tile TILE_CAVE_WALL = {
    .layer = TILE_LAYER_WALL,
    .hide_below = 0,
    .symbol = '#',
    .color = RENDER_COLOR_DARK_GRAY,
    .name = "Cave Wall",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

const Tile TILE_PLANK_WALL = {
    .layer = TILE_LAYER_WALL,
    .hide_below = 0,
    .symbol = '#',
    .color = RENDER_COLOR_BROWN,
    .name = "Plank Wall",
    .interactable = 0,
    .blocks_movement = 1,
    .blocks_sight = 1,
    .blocks_projectile = 1
};

const Tile TILE_STAIRS_UP = {
    .layer = TILE_LAYER_WALL,
    .hide_below = 0,
    .symbol = '<',
    .color = RENDER_COLOR_LIGHT_YELLOW,
    .name = "Staircase",
    .interactable = 1,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

const Tile TILE_STAIRS_DOWN = {
    .layer = TILE_LAYER_WALL,
    .hide_below = 0,
    .symbol = '>',
    .color = RENDER_COLOR_LIGHT_YELLOW,
    .name = "Staircase",
    .interactable = 1,
    .blocks_movement = 0,
    .blocks_sight = 0,
    .blocks_projectile = 0
};

