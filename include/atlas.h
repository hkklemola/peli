#ifndef ATLAS_H
#define ATLAS_H

#include "map.h"
#include "tile.h"
#include "tileset.h"
#include "furniture.h"
#include "world_map.h"

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
#define ATLAS_FIXED_AREA_COUNT 8
#define ATLAS_GENERATED_SLOT_COUNT 1
#define MAX_AREAS (ATLAS_FIXED_AREA_COUNT + ATLAS_GENERATED_SLOT_COUNT)
#define MAX_AREA_TILE_MUTATIONS 1024
#define MAX_AREA_TREE_STATES 512
#define ATLAS_TIMESTAMP_LENGTH 20
#define ATLAS_LOCATION_HINT_MAX 16
#define ATLAS_LOCATION_HINT_LENGTH 128
#define ATLAS_PREDEFINED_MAP_PATH_LENGTH 128

typedef enum TileMutationState {
    TILE_MUTATION_STATE_NONE = 0,
    TILE_MUTATION_STATE_DOOR_CLOSED,
    TILE_MUTATION_STATE_DOOR_OPEN,
    TILE_MUTATION_STATE_TREE_STUMP,
} TileMutationState;

typedef struct TileMutation {
    int active;
    int x;
    int y;
    int z;
    TileLayer layer;
    TileMutationState state;
} TileMutation;

typedef struct TreeDurabilityState {
    int active;
    int x;
    int y;
    int z;
    int structure_points;
    TreeSpecies species;
} TreeDurabilityState;

// Known location types used by map generation rules
typedef enum {
    LOCATION_UNKNOWN,
    LOCATION_STARTER,
    LOCATION_DUNGEON,
    LOCATION_CRYPT,
    LOCATION_CAVERN,
    LOCATION_VILLAGE,
    LOCATION_TOWN,
} LocationType;

typedef enum {
    LOCATION_KNOWLEDGE_UNAWARE = 0,
    LOCATION_KNOWLEDGE_AWARE,
    LOCATION_KNOWLEDGE_LOCATED,
    LOCATION_KNOWLEDGE_SCOUTED,
    LOCATION_KNOWLEDGE_VISITED,
} LocationKnowledge;

typedef enum {
    LOCATION_GENERATION_PROCEDURAL = 0,
    LOCATION_GENERATION_PREDEFINED,
} LocationGenerationMode;

typedef struct AtlasLocationInfo {
    char first_aware_ts[ATLAS_TIMESTAMP_LENGTH];
    char first_located_ts[ATLAS_TIMESTAMP_LENGTH];
    char first_scouted_ts[ATLAS_TIMESTAMP_LENGTH];
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
    int is_generated;
    unsigned int generation_seed;
    WorldMapBiome biome;
    int farmland;
    char predefined_map_path[ATLAS_PREDEFINED_MAP_PATH_LENGTH];
    Tile (*map)[MAP_WIDTH][TILE_LAYER_COUNT]; // Dynamically allocated ground-floor layered tile data for the area
    int upper_floor_origin_x;
    int upper_floor_origin_y;
    int upper_floor_width;
    int upper_floor_height;
    int upper_floor_count;
    Tile upper_floor_maps[MAX_AREA_FLOORS - 1][AREA_UPPER_FLOOR_MAX_HEIGHT][AREA_UPPER_FLOOR_MAX_WIDTH][TILE_LAYER_COUNT];
    int (*discovered)[MAP_WIDTH];
    unsigned char (*entity_marker_active)[MAP_WIDTH];
    unsigned char (*entity_marker_symbol)[MAP_WIDTH];
    int (*entity_marker_color)[MAP_WIDTH];
    int (*entity_marker_z)[MAP_WIDTH];
    int map_generated;
    int tile_mutation_count;
    TileMutation tile_mutations[MAX_AREA_TILE_MUTATIONS];
    int tree_state_count;
    TreeDurabilityState tree_states[MAX_AREA_TREE_STATES];

    // Furniture entities that reside in this area (chests, barrels, chairs, tables, doors)
    struct Furniture furniture[MAX_AREA_FURNITURE];
    int furniture_count;
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

// Return 1 when the area has been scouted.
int atlas_is_scouted(int index);

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

// Prepare a generated wilderness area for one world tile and return its atlas index.
int atlas_prepare_generated_area(int world_x, int world_y, int* out_index);

// Return 1 when index points to a generated runtime slot.
int atlas_is_generated_index(int index);

// Add one hint/info line to a location page (deduplicated by exact text).
int atlas_add_location_hint(int index, const char* hint_text);

// Return per-location metadata for atlas page rendering and persistence.
const AtlasLocationInfo* atlas_get_location_info(int index);

// Set per-location metadata fields (used by save/load restore).
void atlas_set_location_timestamp_aware(int index, const char* ts);
void atlas_set_location_timestamp_located(int index, const char* ts);
void atlas_set_location_timestamp_scouted(int index, const char* ts);
void atlas_set_location_timestamp_first_visit(int index, const char* ts);
void atlas_set_location_timestamp_latest_visit(int index, const char* ts);
void atlas_clear_location_hints(int index);

// Clear tile mutations for one area.
void atlas_clear_tile_mutations(Area* area);

// Apply one tile mutation to an area map.
int atlas_apply_tile_mutation(Area* area, const TileMutation* mutation);

// Apply all stored tile mutations for one area.
void atlas_apply_tile_mutations(Area* area);

// Set or update one tile mutation and apply it to map at the current player z.
int atlas_set_tile_mutation(Area* area, int x, int y, TileMutationState state);

// Set or update one tile mutation and apply it to map at an explicit z-level.
int atlas_set_tile_mutation_at_z(Area* area, int x, int y, int z, TileMutationState state);

#endif

