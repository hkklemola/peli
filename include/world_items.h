#ifndef WORLD_ITEMS_H
#define WORLD_ITEMS_H

#include "item.h"

/**
 * @file world_items.h
 * @brief Item world state and ground-item management.
 *
 * Manages items dropped in the game world across all areas, including persistence
 * state, location tracking, and lookup helpers for ground-based item interactions.
 */

#define MAX_WORLD_ITEMS 64
#define MAX_WORLD_CONTAINERS 24
#define WORLD_CONTAINER_CAPACITY 64

/** @struct WorldItem
 *  @brief Represents an item instance in the game world.
 */
typedef struct WorldItem {
    /** @brief The item data (position, type, stats, etc.). */
    Item item;
    /** @brief The area name where this item is located. */
    char area_name[32];
    /** @brief 1 if item is active/available, 0 if slot is empty. */
    int active;
} WorldItem;

/** @struct WorldContainer
 *  @brief Represents one openable world container (e.g., chest) with item contents.
 */
typedef struct WorldContainer {
    /** @brief 1 if container slot is active. */
    int active;
    /** @brief The area name where this container is located. */
    char area_name[32];
    /** @brief Tile coordinates of the container in the owning area. */
    int x;
    int y;
    /** @brief User-facing container label. */
    char label[48];
    /** @brief Stored item instances in this container. */
    Item items[WORLD_CONTAINER_CAPACITY];
    /** @brief Number of active stored items. */
    int item_count;
} WorldContainer;

extern WorldItem world_items[MAX_WORLD_ITEMS];
extern WorldContainer world_containers[MAX_WORLD_CONTAINERS];

/**
 * @brief Initialize the world items array to empty state.
 */
void world_items_init(void);

// Initialize world container storage to empty state.
void world_containers_init(void);

/**
 * @brief Find a world item at the given area coordinates.
 * @param x The x-coordinate in the current area.
 * @param y The y-coordinate in the current area.
 * @return Pointer to the WorldItem if found, NULL otherwise.
 */
WorldItem* world_item_at(int x, int y);

// 3D variant: find world item at area coordinates and floor/depth.
WorldItem* world_item_at_3d(int x, int y, int z);

/**
 * @brief Count active world items at the given area coordinates.
 * @param x The x-coordinate in the current area.
 * @param y The y-coordinate in the current area.
 * @return Number of active items found at the location.
 */
int world_item_count_at(int x, int y);

// 3D variant: count active world items at area coordinates and floor/depth.
int world_item_count_at_3d(int x, int y, int z);

/**
 * @brief Get the Nth active world item at a location in the current area.
 * @param x The x-coordinate in the current area.
 * @param y The y-coordinate in the current area.
 * @param ordinal Zero-based index among matching items.
 * @return Pointer to the matching item, or NULL if ordinal is out of range.
 */
WorldItem* world_item_at_ordinal(int x, int y, int ordinal);

// 3D variant: get the Nth active item at area coordinates and floor/depth.
WorldItem* world_item_at_ordinal_3d(int x, int y, int z, int ordinal);

/**
 * @brief Get the next active world item on the same tile after current_item.
 * @param x The x-coordinate in the current area.
 * @param y The y-coordinate in the current area.
 * @param current_item Current selected item on this tile.
 * @return Next matching item, or first matching item if wrapping/invalid.
 */
WorldItem* world_item_next_at(int x, int y, const WorldItem* current_item);

// 3D variant: cycle next active item at area coordinates and floor/depth.
WorldItem* world_item_next_at_3d(int x, int y, int z, const WorldItem* current_item);

/**
 * @brief Drop an item at a location in the game world.
 * @param item The item to drop (contents will be copied).
 * @param area_name The area where the item is dropped.
 * @param x The x-coordinate within the area.
 * @param y The y-coordinate within the area.
 * @return 1 on success, 0 if the world items array is full.
 */
int world_item_drop(const Item* item, const char* area_name, int x, int y);

// 3D variant: drop an item at area coordinates and floor/depth.
int world_item_drop_3d(const Item* item, const char* area_name, int x, int y, int z);

/**
 * @brief Remove a world item by array index.
 * @param index The index in the world_items array.
 * @return 1 on success, 0 if index is invalid or already empty.
 */
int world_item_remove(int index);

/**
 * @brief Find the array index of a world item pointer.
 * @param world_item Pointer to a WorldItem structure.
 * @return The array index, or -1 if not found.
 */
int world_item_index_of(const WorldItem* world_item);

// Find world container at the given area coordinates.
WorldContainer* world_container_at(int x, int y);

// Spawn one world container at area coordinates; returns slot index or -1 on failure.
int world_container_spawn(const char* area_name, int x, int y, const char* label);

// Add one item copy into a world container; returns 1 on success.
int world_container_add_item(int container_index, const Item* item);

// Remove item by slot from a container into out_item; returns 1 on success.
int world_container_remove_item(int container_index, int item_slot, Item* out_item);

// Remove a world container by index and clear its contents.
int world_container_remove(int container_index);

// Find the array index of a world container pointer.
int world_container_index_of(const WorldContainer* container);

#endif