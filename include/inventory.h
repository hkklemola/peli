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


// Reset inventory and all equipment slots to defaults. Returns 1 on success, 0 on error (e.g., null pointer).
int inventory_init(Character* c);

// Add one item instance to inventory; returns 1 on success.
int inventory_add(Character* c, const Item* item);

// Remove an item at slot index; returns 1 on success.
int inventory_remove(Character* c, int slot);

// Use item at slot index (consumables); returns 1 on success.
int inventory_use(Character* c, int slot);

// Return the preferred/default equipment slot for an item type.
EquipmentSlotType equipment_slot_for_item_type(ItemType type);

// Equip an inventory item by slot index to a given equipment slot; returns 1 on success.
int inventory_equip(Character* c, int inv_slot, int equip_slot);

// Auto-equip an inventory item to the first compatible empty equipment slot.
int inventory_auto_equip(Character* c, int inv_slot);

// Equip an item to a slot by EquipmentSlotType (new API)
int inventory_equip_to_slot(Character* c, int inv_slot, EquipmentSlotType slot_type);

// Unequip by EquipmentSlotType (new API)
int inventory_unequip_slot(Character* c, EquipmentSlotType slot_type);

// Unequip by ItemType (legacy, but used in codebase)
int inventory_unequip(Character* c, ItemType type);

// Print inventory and equipment summary to stdout.
void inventory_print(const Character* c);

// Open the full inventory interaction overlay loop.
void inventory_menu(Character* c);

// Open the quick-equip overlay loop.




#endif

