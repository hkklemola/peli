#include "d:/projekti/peli/include/tileset.h"

const Tile TILE_FLOOR = {
    .symbol = '.',
    .walkable = 1,
    .opaque = 0,
    .name = "Floor"
};

const Tile TILE_WALL = {
    .symbol = '#',
    .walkable = 0,
    .opaque = 1,
    .name = "Wall"
};