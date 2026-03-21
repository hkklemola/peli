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

extern WorldItem world_items[MAX_WORLD_ITEMS];

/**
 * @brief Initialize the world items array to empty state.
 */
void world_items_init(void);

/**
 * @brief Find a world item at the given area coordinates.
 * @param x The x-coordinate in the current area.
 * @param y The y-coordinate in the current area.
 * @return Pointer to the WorldItem if found, NULL otherwise.
 */
WorldItem* world_item_at(int x, int y);

/**
 * @brief Drop an item at a location in the game world.
 * @param item The item to drop (contents will be copied).
 * @param area_name The area where the item is dropped.
 * @param x The x-coordinate within the area.
 * @param y The y-coordinate within the area.
 * @return 1 on success, 0 if the world items array is full.
 */
int world_item_drop(const Item* item, const char* area_name, int x, int y);

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

#endif