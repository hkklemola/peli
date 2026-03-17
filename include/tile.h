#ifndef TILE_H
#define TILE_H

#include <stdio.h>

// Tile structure
typedef struct {
    char symbol;       // what to display
    char name[32];     // name of the tile
    int walkable;      // 1 = player/creature can move
    int opaque;        // 1 = blocks FOV/vision
    int interactable;  // 1 = can trigger interaction
} Tile;

// Helpers to create tiles
Tile tile_floor();
Tile tile_wall();
Tile tile_door();

#endif