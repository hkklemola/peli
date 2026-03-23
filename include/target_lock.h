#ifndef TARGET_LOCK_H
#define TARGET_LOCK_H

#include "player.h"

/**
 * @file target_lock.h
 * @brief Target lock resolution and management for combat/interaction hotkeys.
 *
 * Manages persistent target locks that allow the player to quick-access creatures
 * or items by hotkey, with validation against current game state.
 */

/** @struct TargetLockResolved
 *  @brief Resolved target lock with current position and validity information.
 */
typedef struct TargetLockResolved
{
    /** @brief The kind of target (creature or world item). */
    TargetLockKind kind;
    /** @brief The slot index (0-based) in the player's lock array. */
    int slot_index;
    /** @brief Display name of the target (creature name, item name, etc.). */
    const char* name;
    /** @brief Current world x-coordinate of the target. */
    int x;
    /** @brief Current world y-coordinate of the target. */
    int y;
    /** @brief Current world z-coordinate (zone floor/depth) of the target. */
    int z;
} TargetLockResolved;

/**
 * @brief Get the display name of a target lock kind.
 * @param kind The TargetLockKind enum value.
 * @return A human-readable string (e.g., "creature", "item").
 */
const char* target_lock_kind_name(TargetLockKind kind);

/**
 * @brief Format a resolved target lock into a human-readable description.
 * @param lock The resolved target lock.
 * @param out Output string buffer.
 * @param out_size Size of the output buffer in bytes.
 */
void target_lock_describe(const TargetLockResolved* lock, char* out, int out_size);

/**
 * @brief Clear all target locks for a player.
 * @param p The player whose locks should be cleared.
 */
void target_lock_clear(Player* p);

/**
 * @brief Set a target lock slot to a creature in the current area.
 * @param p The player.
 * @param slot_index The lock slot (0-based).
 * @param area_name The area where the creature is located.
 * @return 1 on success, 0 if no such creature found or slot is invalid.
 */
int target_lock_set_creature(Player* p, int slot_index, const char* area_name);

/**
 * @brief Set a target lock slot to a world item in the current area.
 * @param p The player.
 * @param slot_index The lock slot (0-based).
 * @param area_name The area where the item is located.
 * @return 1 on success, 0 if no such item found or slot is invalid.
 */
int target_lock_set_world_item(Player* p, int slot_index, const char* area_name);

/**
 * @brief Check if a target lock slot matches a creature in the current area.
 * @param p The player.
 * @param slot_index The lock slot (0-based).
 * @param area_name The area to check.
 * @return 1 if the slot contains a creature lock matching the area, 0 otherwise.
 */
int target_lock_matches_creature(const Player* p, int slot_index, const char* area_name);

/**
 * @brief Check if a target lock slot matches a world item in the current area.
 * @param p The player.
 * @param slot_index The lock slot (0-based).
 * @param area_name The area to check.
 * @return 1 if the slot contains an item lock matching the area, 0 otherwise.
 */
int target_lock_matches_world_item(const Player* p, int slot_index, const char* area_name);

/**
 * @brief Resolve a target lock slot to current game state.
 * @param p The player.
 * @param out Output structure for the resolved target (position, name, etc.).
 * @param clear_invalid If 1, clear the lock if target is invalid in current area.
 * @return 1 if lock is valid and resolved, 0 if invalid or not set.
 */
int target_lock_resolve(Player* p, TargetLockResolved* out, int clear_invalid);

/**
 * @brief Resolve a target lock and log if invalidated by area change.
 * @param p The player.
 * @param out Output structure for the resolved target.
 * @param log_on_invalidate If 1, emit a log message when lock becomes invalid.
 * @return 1 if lock is valid and resolved, 0 if invalid.
 */
int target_lock_resolve_live(Player* p, TargetLockResolved* out, int log_on_invalidate);

#endif