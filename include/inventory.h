#ifndef INVENTORY_H
#define INVENTORY_H

#include "character.h"

/*
 * Purpose:
 *   Declares inventory/equipment management and inventory UI flows.
 *
 * Functions:
 *   - inventory_init/add/remove/use: core item container operations.
 *   - inventory_equip/unequip: equipment slot transitions.
 *   - inventory_print/menu/quick_equip: text and overlay interaction helpers.
 */

// Reset inventory and equipped-item slots to defaults.
void inventory_init(Character* c);

// Add one item instance to inventory; returns 1 on success.
int inventory_add(Character* c, const Item* item);

// Remove an item at slot index; returns 1 on success.
int inventory_remove(Character* c, int slot);

// Use item at slot index (consumables); returns 1 on success.
int inventory_use(Character* c, int slot);

// Equip an inventory item by slot index; returns 1 on success.
int inventory_equip(Character* c, int slot);

// Unequip an item type from its slot; returns 1 on success.
int inventory_unequip(Character* c, ItemType type);

// Print inventory and equipment summary to stdout.
void inventory_print(const Character* c);

// Open the full inventory interaction overlay loop.
void inventory_menu(Character* c);

// Open the quick-equip overlay loop.
void inventory_quick_equip(Character* c);

// Count total quantity of a named item across inventory and equipped bags.
int inventory_count_by_name(const Character* c, const char* item_name);

// Consume quantity of a named item across inventory and equipped bags.
// Returns 1 when full amount was consumed, 0 otherwise.
int inventory_consume_by_name(Character* c, const char* item_name, int amount);

#endif

