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
#define ATLAS_TIMESTAMP_LENGTH 20
#define ATLAS_LOCATION_HINT_MAX 16
#define ATLAS_LOCATION_HINT_LENGTH 128
#define ATLAS_PREDEFINED_MAP_PATH_LENGTH 128

typedef enum TileMutationState {
    TILE_MUTATION_STATE_NONE = 0,
    TILE_MUTATION_STATE_DOOR_CLOSED,
    TILE_MUTATION_STATE_DOOR_OPEN,
} TileMutationState;

typedef struct TileMutation {
    int active;
    int x;
    int y;
    TileLayer layer;
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

typedef enum {
    LOCATION_KNOWLEDGE_UNAWARE = 0,
    LOCATION_KNOWLEDGE_AWARE,
    LOCATION_KNOWLEDGE_LOCATED,
    LOCATION_KNOWLEDGE_VISITED,
} LocationKnowledge;

typedef enum {
    LOCATION_GENERATION_PROCEDURAL = 0,
    LOCATION_GENERATION_PREDEFINED,
} LocationGenerationMode;

typedef struct AtlasLocationInfo {
    char first_aware_ts[ATLAS_TIMESTAMP_LENGTH];
    char first_located_ts[ATLAS_TIMESTAMP_LENGTH];
    char first_visit_ts[ATLAS_TIMESTAMP_LENGTH];
    char latest_visit_ts[ATLAS_TIMESTAMP_LENGTH];
    int hint_count;
    char hints[ATLAS_LOCATION_HINT_MAX][ATLAS_LOCATION_HINT_LENGTH];
} AtlasLocationInfo;

// Represents a playable area in the game
typedef struct Area {
    char name[32];                       // Name of the area
    LocationType type;                   // What generation rules to use
    LocationGenerationMode generation_mode;
    int width;
    int height;
    int world_x;
    int world_y;
    char predefined_map_path[ATLAS_PREDEFINED_MAP_PATH_LENGTH];
    Tile map[MAP_HEIGHT][MAP_WIDTH][TILE_LAYER_COUNT]; // Layered tile data for the map
    int tile_mutation_count;
    TileMutation tile_mutations[MAX_AREA_TILE_MUTATIONS];
} Area;

// Pointer to the currently active area
extern Area* current_area;

// Storage for all areas
extern Area atlas[MAX_AREAS];
extern int atlas_location_count;

// Atlas page metadata for each location.
extern AtlasLocationInfo atlas_location_info[MAX_AREAS];

// Build the world atlas and populate map data for each area.
void atlas_init();

// Set the current area to the given atlas index.
void atlas_travel(int index);

// Set one area's knowledge tier exactly.
void atlas_set_knowledge(int index, LocationKnowledge knowledge);

// Upgrade one area's knowledge tier without downgrading.
void atlas_upgrade_knowledge(int index, LocationKnowledge knowledge);

// Return the current knowledge tier for one area.
LocationKnowledge atlas_get_knowledge(int index);

// Return 1 when the area is at least aware.
int atlas_is_known(int index);

// Return 1 when the area is at least located.
int atlas_is_located(int index);

// Return 1 when the area has been visited.
int atlas_is_visited(int index);

// Return 1 when the area is eligible for fast travel.
int atlas_can_fast_travel(int index);

// Return number of atlas-visible known areas.
int atlas_known_count(void);

// Return the area index for a name, or -1 when not found.
int atlas_find_location(const char* name);

// Register atlas zones into the world map and sync visibility flags.
void atlas_sync_world_map(void);

// Add one hint/info line to a location page (deduplicated by exact text).
int atlas_add_location_hint(int index, const char* hint_text);

// Return per-location metadata for atlas page rendering and persistence.
const AtlasLocationInfo* atlas_get_location_info(int index);

// Set per-location metadata fields (used by save/load restore).
void atlas_set_location_timestamp_aware(int index, const char* ts);
void atlas_set_location_timestamp_located(int index, const char* ts);
void atlas_set_location_timestamp_first_visit(int index, const char* ts);
void atlas_set_location_timestamp_latest_visit(int index, const char* ts);
void atlas_clear_location_hints(int index);

// Clear tile mutations for one area.
void atlas_clear_tile_mutations(Area* area);

// Apply one tile mutation to an area map.
int atlas_apply_tile_mutation(Area* area, const TileMutation* mutation);

// Apply all stored tile mutations for one area.
void atlas_apply_tile_mutations(Area* area);

// Set or update one tile mutation and apply it to map.
int atlas_set_tile_mutation(Area* area, int x, int y, TileMutationState state);

#endif

