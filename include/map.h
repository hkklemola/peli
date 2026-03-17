#include "d:/projekti/peli/include/tile.h"

#ifndef MAP_H
#define MAP_H

#define MAP_WIDTH 160
#define MAP_HEIGHT 50

// Maximum room settings
#define MAX_ROOMS 18
#define ROOM_MIN_SIZE 8
#define ROOM_MAX_SIZE 16

void generate_map(); // generates dungeon layout in current_area

#endif