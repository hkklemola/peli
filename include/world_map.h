#ifndef WORLD_MAP_H
#define WORLD_MAP_H

#include <stdint.h>

/**
 * @file world_map.h
 * @brief High-level world map tiles and zone discovery tracking.
 *
 * Tracks discovered/visited status of world map tiles and assignment of zones
 * (dungeons, towns, etc.) to world coordinates for fast spatial lookup.
 */

#define WORLD_MAP_WIDTH 1000
#define WORLD_MAP_HEIGHT 1000
#define WORLD_MAP_TILE_METERS 1000  /**< Each world map tile represents 1 km x 1 km. */
#define WORLD_MAP_MAX_ROAD_TIER 3
#define WORLD_MAP_SIGNPOST_MAX_AREAS 64
#define WORLD_MAP_SIGNPOST_DIRECTION_LENGTH 32
#define WORLD_MAP_SIGNPOST_HINT_LENGTH 128

typedef struct {
    int destination_index;
    char direction[WORLD_MAP_SIGNPOST_DIRECTION_LENGTH];
    char hint_text[WORLD_MAP_SIGNPOST_HINT_LENGTH];
} SignpostSign;

typedef struct {
    int area_index;
    int x;
    int y;
    int z;
    int visited;
    int sign_count;
    int sign_capacity;
    SignpostSign* signs;
} SignpostInstance;

typedef enum {
    WORLD_MAP_ROAD_TIER_NONE = 0,
    WORLD_MAP_ROAD_TIER_TRAIL = 1,
    WORLD_MAP_ROAD_TIER_PAVED = 2,
    WORLD_MAP_ROAD_TIER_HIGHWAY = 3,
} WorldMapRoadTier;

/** @enum WorldMapBiome
 *  @brief Terrain/biome type for a wilderness world map tile.
 */
typedef enum {
    BIOME_NONE = 0,
    BIOME_GRASSLANDS,
    BIOME_FOREST,
    BIOME_FARMLANDS,
    BIOME_DESERT,
    BIOME_TUNDRA,
    BIOME_RIVER,
    BIOME_LAKE,
    BIOME_SEA,
    BIOME_SAVANNAH,
    BIOME_MOUNTAINS,
    BIOME_FOOTHILLS,
    BIOME_SWAMP,
    BIOME_JUNGLE,
    BIOME_COUNT
} WorldMapBiome;

/** @struct WorldMapTile
 *  @brief State of one world map tile.
 */
typedef struct {
    /** @brief Index of assigned zone, or -1 if unassigned. */
    int zone_index;
    /** @brief 1 if tile has been discovered by the player, 0 otherwise. */
    int discovered;
    /** @brief 1 if tile has been visited by the player, 0 otherwise. */
    int visited;
    /** @brief Terrain biome type for wilderness tiles. */
    WorldMapBiome biome;
    /** @brief Road quality tier (0 = none). */
    int road_tier;
} WorldMapTile;

extern WorldMapTile world_map[WORLD_MAP_HEIGHT][WORLD_MAP_WIDTH];

/**
 * @brief Initialize the world map to default unassigned, unvisited, undiscovered state.
 */
void world_map_init(void);

/**
 * @brief Register a zone at the given world map coordinates.
 * @param x The x-coordinate on the world map.
 * @param y The y-coordinate on the world map.
 * @param zone_index The zone identifier to assign, or -1 for unassigned.
 */
void world_map_set_zone(int x, int y, int zone_index);

/**
 * @brief Mark a world map tile as discovered by the player.
 * @param x The x-coordinate on the world map.
 * @param y The y-coordinate on the world map.
 */
void world_map_mark_discovered(int x, int y);

/**
 * @brief Mark a world map tile as scouted (permanently visible) without visiting it.
 * @param x The x-coordinate on the world map.
 * @param y The y-coordinate on the world map.
 */
void world_map_mark_scouted(int x, int y);

/**
 * @brief Mark a world map tile as visited by the player.
 * @param x The x-coordinate on the world map.
 * @param y The y-coordinate on the world map.
 */
void world_map_mark_visited(int x, int y);

/**
 * @brief Set road tier for a world map tile.
 * @param x The x-coordinate on the world map.
 * @param y The y-coordinate on the world map.
 * @param road_tier New tier in range [WORLD_MAP_ROAD_TIER_NONE, WORLD_MAP_MAX_ROAD_TIER].
 */
void world_map_set_road_tier(int x, int y, int road_tier);

/**
 * @brief Get road tier at a world map coordinate.
 * @param x The x-coordinate on the world map.
 * @param y The y-coordinate on the world map.
 * @return Road tier or WORLD_MAP_ROAD_TIER_NONE when out of bounds.
 */
int world_map_get_road_tier(int x, int y);

/**
 * @brief Draw a deterministic road between two overworld points.
 * @param x0 Start x-coordinate.
 * @param y0 Start y-coordinate.
 * @param x1 End x-coordinate.
 * @param y1 End y-coordinate.
 * @param road_tier Tier to stamp along the full route.
 * @note Overlapping routes keep the highest tier already present.
 */
void world_map_draw_road(int x0, int y0, int x1, int y1, int road_tier);

/**
 * @brief Convert a road tier into per-step overland stamina cost.
 * @param road_tier Road quality tier.
 * @return Stamina cost for a movement step on that tier.
 */
int world_map_road_tier_stamina_cost(int road_tier);

/**
 * @brief Resolve per-step overland stamina cost for moving onto tile x,y.
 * @param x Destination x-coordinate.
 * @param y Destination y-coordinate.
 * @return Stamina cost for moving into the destination tile.
 */
int world_map_step_stamina_cost(int x, int y);

/**
 * @brief Query the state of a world map tile by coordinates.
 * @param x The x-coordinate on the world map.
 * @param y The y-coordinate on the world map.
 * @return Pointer to the WorldMapTile, or NULL if coordinates are out of bounds.
 */
WorldMapTile* world_map_get_tile(int x, int y);

/**
 * @brief Find coordinates for a zone index on the world map.
 * @param zone_index Zone index to find.
 * @param out_x Output x-coordinate when found.
 * @param out_y Output y-coordinate when found.
 * @return 1 when found, 0 otherwise.
 */
int world_map_find_zone(int zone_index, int* out_x, int* out_y);

/**
 * @brief Persist the current overworld exploration position.
 * @param x Overworld x-coordinate.
 * @param y Overworld y-coordinate.
 */
void world_map_set_overworld_position(int x, int y);

/**
 * @brief Get persisted overworld exploration position.
 * @param out_x Output x-coordinate.
 * @param out_y Output y-coordinate.
 * @return 1 when a persisted position exists, 0 otherwise.
 */
int world_map_get_overworld_position(int* out_x, int* out_y);

/**
 * @brief Set the biome type for a world map tile.
 */
void world_map_set_biome(int x, int y, WorldMapBiome biome);

/**
 * @brief Get the biome type for a world map tile.
 */
WorldMapBiome world_map_get_biome(int x, int y);

/**
 * @brief Return the human-readable name for a biome type.
 */
const char* world_map_biome_name(WorldMapBiome biome);

/**
 * @brief Load biome data from a legacy character-grid text file or a CSV grid.
 * @param path Path to the biome map file. When a matching `.csv` exists it is preferred.
 */
void world_map_load_biomes(const char* path);

/**
 * @brief Clear and reinitialize all signpost instance registries.
 */
void world_map_signposts_init(void);

/**
 * @brief Clear one area's signpost instances.
 */
void world_map_signposts_clear_area(int area_index);

/**
 * @brief Register or fetch one signpost instance by area and tile coordinates.
 * @return Mutable signpost instance pointer, or NULL on invalid input/allocation failure.
 */
SignpostInstance* world_map_signpost_register(int area_index, int x, int y, int z);

/**
 * @brief Add one sign entry to a signpost instance.
 *
 * Destination index must be unique within one signpost.
 *
 * @return 1 on success, 0 on validation failure or allocation failure.
 */
int world_map_signpost_add_sign(int area_index,
                                int x,
                                int y,
                                int z,
                                int destination_index,
                                const char* direction,
                                const char* hint_text);

/**
 * @brief Lookup signpost instance by area and coordinates.
 */
const SignpostInstance* world_map_signpost_at(int area_index, int x, int y, int z);

/**
 * @brief Mutable lookup variant for interaction/state updates.
 */
SignpostInstance* world_map_signpost_at_mut(int area_index, int x, int y, int z);

#endif
