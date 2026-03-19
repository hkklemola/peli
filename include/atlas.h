#ifndef ATLAS_H
#define ATLAS_H

#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/tile.h"
#include "d:/projekti/peli/include/tileset.h"

// Maximum number of areas stored in the atlas
#define MAX_AREAS 8

// Represents a playable area in the game
typedef struct {
    char name[32];                       // Name of the area
    Tile map[MAP_HEIGHT][MAP_WIDTH];     // Tile data for the map
} Area;

// Pointer to the currently active area
extern Area* current_area;

// Storage for all areas
extern Area atlas[MAX_AREAS];

// Initialize all areas in the atlas
void atlas_init();

// Change the current area
void atlas_travel(int index);

#endif