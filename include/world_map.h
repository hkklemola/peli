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

#define WORLD_MAP_WIDTH 100
#define WORLD_MAP_HEIGHT 100
#define WORLD_MAP_TILE_METERS 1000  /**< Each world map tile represents 1 km x 1 km. */

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
 * @brief Mark a world map tile as visited by the player.
 * @param x The x-coordinate on the world map.
 * @param y The y-coordinate on the world map.
 */
void world_map_mark_visited(int x, int y);

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

#endif
