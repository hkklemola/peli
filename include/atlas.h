#ifndef ATLAS_H
#define ATLAS_H

#include "map.h"
#include "tile.h"
#include "tileset.h"

/*
 * Purpose:
 *   Declares world-area storage and travel helpers.
 *
 * Functions:
 *   - atlas_init: builds the area list and generates map data per area type.
 *   - atlas_travel: switches the active area by index.
 *   - atlas_find_location: finds an area index by display name.
 */

// Maximum number of areas stored in the atlas
#define MAX_AREAS 8
#define MAX_AREA_TILE_MUTATIONS 1024

typedef enum TileMutationState {
    TILE_MUTATION_STATE_NONE = 0,
    TILE_MUTATION_STATE_DOOR_CLOSED,
    TILE_MUTATION_STATE_DOOR_OPEN,
} TileMutationState;

typedef struct TileMutation {
    int active;
    int x;
    int y;
    TileMutationState state;
} TileMutation;

// Known location types used by map generation rules
typedef enum {
    LOCATION_UNKNOWN,
    LOCATION_STARTER,
    LOCATION_DUNGEON,
    LOCATION_CRYPT,
    LOCATION_CAVERN,
    LOCATION_TOWN,
} LocationType;

// Represents a playable area in the game
typedef struct Area {
    char name[32];                       // Name of the area
    LocationType type;                   // What generation rules to use
    int width;
    int height;
    Tile map[MAP_HEIGHT][MAP_WIDTH];     // Tile data for the map
    int tile_mutation_count;
    TileMutation tile_mutations[MAX_AREA_TILE_MUTATIONS];
} Area;

// Pointer to the currently active area
extern Area* current_area;

// Storage for all areas
extern Area atlas[MAX_AREAS];

// Build the world atlas and populate map data for each area.
void atlas_init();

// Set the current area to the given atlas index.
void atlas_travel(int index);

// Mark an area as discovered by the player.
void atlas_mark_discovered(int index);

// Check whether an area is discovered.
int atlas_is_discovered(int index);

// Return number of discovered areas.
int atlas_discovered_count(void);

// Return the area index for a name, or -1 when not found.
int atlas_find_location(const char* name);

// Clear tile mutations for one area.
void atlas_clear_tile_mutations(Area* area);

// Apply one tile mutation to an area map.
int atlas_apply_tile_mutation(Area* area, const TileMutation* mutation);

// Apply all stored tile mutations for one area.
void atlas_apply_tile_mutations(Area* area);

// Set or update one tile mutation and apply it to map.
int atlas_set_tile_mutation(Area* area, int x, int y, TileMutationState state);

#endif

