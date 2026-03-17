#include "d:/projekti/peli/include/tileset.h"

const Tile TILE_FLOOR = {
    .symbol = '.',
    .walkable = true,
    .blocks_sight = false,
    .name = "Floor"
};

const Tile TILE_WALL = {
    .symbol = '#',
    .walkable = false,
    .blocks_sight = true,
    .name = "Wall"
};