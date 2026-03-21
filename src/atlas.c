#include "atlas.h"
#include "log.h"
#include "tileset.h"
#include "tile.h"

#include <stdio.h>
#include <string.h>

/*
 * Purpose:
 *   Builds and manages the world-area atlas.
 *
 * Functions:
 *   - atlas_init: initializes named areas and generates their maps.
 *   - atlas_travel: switches currently active area.
 *   - atlas_find_location: looks up an area by name.
 */

Area atlas[MAX_AREAS];
Area* current_area = NULL;
static int discovered[MAX_AREAS] = {0};

// Reset all tile mutation records for one area.
void atlas_clear_tile_mutations(Area* area) {
    if(!area)
        return;

    area->tile_mutation_count = 0;
    memset(area->tile_mutations, 0, sizeof(area->tile_mutations));
}

// Apply one mutation state onto area tile data.
int atlas_apply_tile_mutation(Area* area, const TileMutation* mutation) {
    Tile* tile;

    if(!area || !mutation || !mutation->active)
        return 0;
    if(mutation->x < 0 || mutation->x >= area->width || mutation->y < 0 || mutation->y >= area->height)
        return 0;

    tile = &area->map[mutation->y][mutation->x];

    switch(mutation->state)
    {
        case TILE_MUTATION_STATE_DOOR_OPEN:
            tile->symbol = '/';
            tile->color = RENDER_COLOR_BROWN;
            snprintf(tile->name, sizeof(tile->name), "Open Door");
            tile->interactable = 1;
            tile->blocks_movement = 0;
            tile->blocks_sight = 0;
            tile->blocks_projectile = 0;
            return 1;
        case TILE_MUTATION_STATE_DOOR_CLOSED:
            tile->symbol = '+';
            tile->color = RENDER_COLOR_BROWN;
            snprintf(tile->name, sizeof(tile->name), "Door");
            tile->interactable = 1;
            tile->blocks_movement = 1;
            tile->blocks_sight = 1;
            tile->blocks_projectile = 1;
            return 1;
        case TILE_MUTATION_STATE_NONE:
        default:
            return 0;
    }
}

// Apply all stored mutations to one area map.
void atlas_apply_tile_mutations(Area* area) {
    if(!area)
        return;

    for(int i = 0; i < MAX_AREA_TILE_MUTATIONS; i++)
        atlas_apply_tile_mutation(area, &area->tile_mutations[i]);
}

// Set/update one tile mutation record and apply to area map.
int atlas_set_tile_mutation(Area* area, int x, int y, TileMutationState state) {
    int free_index = -1;
    TileMutation mutation;

    if(!area)
        return 0;
    if(x < 0 || x >= area->width || y < 0 || y >= area->height)
        return 0;
    if(state == TILE_MUTATION_STATE_NONE)
        return 0;

    for(int i = 0; i < MAX_AREA_TILE_MUTATIONS; i++)
    {
        TileMutation* entry = &area->tile_mutations[i];

        if(!entry->active)
        {
            if(free_index < 0)
                free_index = i;
            continue;
        }

        if(entry->x == x && entry->y == y)
        {
            entry->state = state;
            return atlas_apply_tile_mutation(area, entry);
        }
    }

    if(free_index < 0)
        return 0;

    mutation.active = 1;
    mutation.x = x;
    mutation.y = y;
    mutation.state = state;
    area->tile_mutations[free_index] = mutation;
    area->tile_mutation_count++;
    return atlas_apply_tile_mutation(area, &area->tile_mutations[free_index]);
}

// Mark an area as discovered.
void atlas_mark_discovered(int index) {
    if(index < 0 || index >= MAX_AREAS)
        return;
    discovered[index] = 1;
}

// Check whether an area is discovered.
int atlas_is_discovered(int index) {
    if(index < 0 || index >= MAX_AREAS)
        return 0;
    return discovered[index];
}

// Return number of discovered areas.
int atlas_discovered_count(void) {
    int count = 0;
    for(int i = 0; i < MAX_AREAS; i++)
        count += discovered[i] ? 1 : 0;
    return count;
}

// Initialize all atlas areas and select the first area as active.
void atlas_init() {
    static const char* area_names[MAX_AREAS] = {
        "The Glade of Beginnings",
        "Goblin Warrens",
        "Ancient Crypt",
        "Market Town",
        "Shale Tunnels",
        "Amber Hollow",
        "Moss Catacombs",
        "Windscar Outpost"
    };

    memset(discovered, 0, sizeof(discovered));

    // Example known locations
    strcpy(atlas[0].name, area_names[0]);
    atlas[0].type = LOCATION_STARTER;
    atlas[0].width = AREA_DEFAULT_WIDTH;
    atlas[0].height = AREA_DEFAULT_HEIGHT;
    atlas_clear_tile_mutations(&atlas[0]);
    map_generate_area(&atlas[0]);

    strcpy(atlas[1].name, area_names[1]);
    atlas[1].type = LOCATION_DUNGEON;
    atlas[1].width = AREA_DEFAULT_WIDTH;
    atlas[1].height = AREA_DEFAULT_HEIGHT;
    atlas_clear_tile_mutations(&atlas[1]);
    map_generate_area(&atlas[1]);

    strcpy(atlas[2].name, area_names[2]);
    atlas[2].type = LOCATION_CRYPT;
    atlas[2].width = AREA_DEFAULT_WIDTH;
    atlas[2].height = AREA_DEFAULT_HEIGHT;
    atlas_clear_tile_mutations(&atlas[2]);
    map_generate_area(&atlas[2]);

    strcpy(atlas[3].name, area_names[3]);
    atlas[3].type = LOCATION_TOWN;
    atlas[3].width = AREA_DEFAULT_WIDTH;
    atlas[3].height = AREA_DEFAULT_HEIGHT;
    atlas_clear_tile_mutations(&atlas[3]);
    map_generate_area(&atlas[3]);

    // Fill remaining areas with explicit names.
    for(int i = 4; i < MAX_AREAS; i++) {
        strcpy(atlas[i].name, area_names[i]);
        atlas[i].type = LOCATION_UNKNOWN;
        atlas[i].width = AREA_DEFAULT_WIDTH;
        atlas[i].height = AREA_DEFAULT_HEIGHT;
        atlas_clear_tile_mutations(&atlas[i]);
        map_generate_area(&atlas[i]);
    }

    atlas_mark_discovered(0);
    current_area = &atlas[0];
}

// Switch current area to the given index when valid.
void atlas_travel(int index) {
    if(index < 0 || index >= MAX_AREAS)
        return;

    current_area = &atlas[index];
    atlas_mark_discovered(index);
    log_add("You travel to %s.", current_area->name);
}

// Return atlas index for a location name, or -1 if not found.
int atlas_find_location(const char* name) {
    if(!name) return -1;
    for(int i = 0; i < MAX_AREAS; i++) {
        if(strcmp(atlas[i].name, name) == 0)
            return i;
    }
    return -1;
}

