#ifndef TILE_H
#define TILE_H

#include <stdbool.h>

typedef struct {
    char symbol;        // what appears on the screen
    bool walkable;      // can creatures move through it?
    bool blocks_sight;  // does it block line of sight?
    const char* name;   // "Wall", "Floor", "Door", etc.
} Tile;

#endif