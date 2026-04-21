#ifndef OVERLAY_NAV_H
#define OVERLAY_NAV_H

#include "player.h"

/**
 * @file overlay_nav.h
 * @brief Overlay screen navigation and hotkey management.
 *
 * Manages overlay screen transitions (inventory, character sheet, log, journal, map)
 * including hotkey input routing and request queuing for chained overlays.
 */

/** @enum OverlayType
 *  @brief Identifiers for different overlay screens in the game.
 */
typedef enum OverlayType {
    /** No overlay active. */
    OVERLAY_TYPE_NONE = 0,
    /** Inventory overlay (view/manage items). */
    OVERLAY_TYPE_INVENTORY,
    /** Character sheet overlay (view stats, attributes, skills). */
    OVERLAY_TYPE_CHARACTER,
    /** Combat log overlay (view recent combat messages). */
    OVERLAY_TYPE_LOG,
    /** Quest/event journal overlay (view quest log, events, discoveries). */
    OVERLAY_TYPE_JOURNAL,
    /** Codex overlay (Atlas, Bestiary, Crafting Compendium). */
    OVERLAY_TYPE_CODEX
} OverlayType;

/**
 * @brief Parse a global overlay hotkey and return the corresponding overlay type.
 * @param key The input key code.
 * @param out_type Pointer to OverlayType to store the result.
 * @return 1 if key maps to an overlay, 0 if it is not an overlay hotkey.
 */
int overlay_type_from_key(int key, OverlayType* out_type);

/**
 * @brief Request to open another overlay when the current one closes.
 *        Allows chained overlay navigation (e.g., open journal from inventory).
 * @param next_overlay The OverlayType to open next (can be OVERLAY_TYPE_NONE).
 */
void overlay_request(OverlayType next_overlay);

/**
 * @brief Read and clear the pending overlay request.
 * @return The requested OverlayType, or OVERLAY_TYPE_NONE if no request is pending.
 */
OverlayType overlay_take_request(void);

/**
 * @brief Open an overlay screen and handle navigation loops.
 *        Continues switching overlays while requests are queued.
 * @param initial_overlay The first overlay to open.
 * @param player Pointer to the Player for passing context to overlay systems.
 */
void overlay_open(OverlayType initial_overlay, Player* player);

#endif
