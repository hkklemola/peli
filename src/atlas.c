#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/tileset.h"
#include "d:/projekti/peli/include/tile.h"

#include <stdio.h>
#include <string.h>

Area atlas[MAX_AREAS];
Area* current_area = NULL;

// fill area with a default tile
static void generate_area(Area* area, const Tile* tile) {
    for(int y = 0; y < MAP_HEIGHT; y++)
        for(int x = 0; x < MAP_WIDTH; x++)
            area->map[y][x] = *tile;
}

void atlas_init() {
    // Example areas
    strcpy(atlas[0].name, "Dungeon Entrance");
    generate_area(&atlas[0], &TILE_FLOOR);

    strcpy(atlas[1].name, "Goblin Warrens");
    generate_area(&atlas[1], &TILE_FLOOR);

    strcpy(atlas[2].name, "Ancient Crypt");
    generate_area(&atlas[2], &TILE_FLOOR);

    // Fill remaining areas
    for(int i = 3; i < MAX_AREAS; i++) {
        sprintf(atlas[i].name, "Area %d", i + 1);
        generate_area(&atlas[i], &TILE_FLOOR);
    }

    current_area = &atlas[0];
}

void atlas_travel(int index) {
    if(index < 0 || index >= MAX_AREAS)
        return;

    current_area = &atlas[index];
    log_add("You travel to %s.", current_area->name);
}