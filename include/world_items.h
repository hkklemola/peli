#ifndef WORLD_ITEMS_H
#define WORLD_ITEMS_H

#include "item.h"

typedef struct Character Character;

/**
 * @file world_items.h
 * @brief Item world state and ground-item management.
 *
 * Manages items dropped in the game world across all areas, including persistence
 * state, location tracking, and lookup helpers for ground-based item interactions.
 */

#define MAX_WORLD_ITEMS 256
#define MAX_WORLD_CONTAINERS 256
#define WORLD_CONTAINER_CAPACITY 64
#define MAX_WORLD_CORPSES 128
#define MAX_WORLD_CORPSE_LOOT_ENTRIES 8

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
    /** @brief Tile coordinates and depth of the container in the owning area. */
    int x;
    int y;
    int z;
    /** @brief User-facing container label. */
    char label[48];
    /** @brief Stored item instances in this container. */
    Item items[WORLD_CONTAINER_CAPACITY];
    /** @brief Number of active stored items. */
    int item_count;
} WorldContainer;

typedef enum WorldCorpseType {
    WORLD_CORPSE_CHARACTER = 0,
    WORLD_CORPSE_CREATURE,
} WorldCorpseType;

typedef struct WorldCorpseLootEntry {
    char item_name[32];
    int chance_percent;
    int min_quantity;
    int max_quantity;
} WorldCorpseLootEntry;

typedef struct WorldCorpse {
    int active;
    WorldCorpseType type;
    char area_name[32];
    char source_name[32];
    char label[64];
    int x;
    int y;
    int z;
    int world_container_index;
    int skinned;
    int butchered;
    WorldCorpseLootEntry skinning_loot[MAX_WORLD_CORPSE_LOOT_ENTRIES];
    int skinning_loot_count;
    WorldCorpseLootEntry butchering_loot[MAX_WORLD_CORPSE_LOOT_ENTRIES];
    int butchering_loot_count;
} WorldCorpse;

extern WorldItem world_items[MAX_WORLD_ITEMS];
extern WorldContainer world_containers[MAX_WORLD_CONTAINERS];
extern WorldCorpse world_corpses[MAX_WORLD_CORPSES];

/**
 * @brief Initialize the world items array to empty state.
 */
void world_items_init(void);

// Initialize world container storage to empty state.
void world_containers_init(void);

// Initialize world corpse storage to empty state.
void world_corpses_init(void);

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

int world_container_spawn_personal(const char* label);

WorldContainer* world_container_for_item(const Item* item);

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

// Find a corpse record at the given area coordinates.
WorldCorpse* world_corpse_at(int x, int y);

// 3D variant: find corpse at area coordinates and depth.
WorldCorpse* world_corpse_at_3d(int x, int y, int z);

// Return the corpse linked to a specific container index, or NULL.
WorldCorpse* world_corpse_by_container_index(int container_index);

// Spawn a new corpse record; returns slot index or -1 on failure.
int world_corpse_spawn(const WorldCorpse* corpse);

// Refresh one corpse label after state changes.
void world_corpse_refresh_label(WorldCorpse* corpse);

// Harvest skinning or butchering loot from a corpse into inventory first, then to ground as fallback.
int world_corpse_drop_loot(WorldCorpse* corpse,
                           Character* collector,
                           int skinning_phase,
                           int* out_added_to_inventory,
                           int* out_dropped_to_ground);

// Remove a corpse by array index and clear any visual marker.
int world_corpse_remove(int index);

// Remove any corpse currently linked to the given world container index.
void world_corpse_remove_by_container_index(int container_index);

// Find the array index of a world corpse pointer.
int world_corpse_index_of(const WorldCorpse* corpse);

// Find world container at the given area coordinates.
WorldContainer* world_container_at(int x, int y);

// 3D variant: find world container at area coordinates and depth.
WorldContainer* world_container_at_3d(int x, int y, int z);

// Spawn one world container at area coordinates; returns slot index or -1 on failure.
int world_container_spawn(const char* area_name, int x, int y, const char* label);

// 3D variant: spawn world container at area coordinates and depth.
int world_container_spawn_3d(const char* area_name, int x, int y, int z, const char* label);

// Add one item copy into a world container; returns 1 on success.
int world_container_add_item(int container_index, const Item* item);

// Remove item by slot from a container into out_item; returns 1 on success.
int world_container_remove_item(int container_index, int item_slot, Item* out_item);

// Remove a world container by index and clear its contents.
int world_container_remove(int container_index);

// Find the array index of a world container pointer.
int world_container_index_of(const WorldContainer* container);

#endif