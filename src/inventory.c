#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "combat.h"
#include "character.h"
#include "inventory.h"
#include "input.h"
#include "item_data.h"
#include "item.h"
#include "log.h"
#include "map.h"
#include "overlay_nav.h"
#include "player.h"
#include "ui_overlay.h"
#include "world_items.h"

// Forward declarations
void update_dynamic_container_slots(Character* c);
EquipmentSlotType equipment_slot_for_item_type(ItemType type);

// Helper: get equipped item pointer by ItemType
static const Item* equipped_item_by_type(const Character* c, ItemType type)
{
    if (!c) return NULL;
    for (int i = 0; i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].item.type == type && c->equipment_slots[i].item.type != ITEM_TYPE_NONE) {
            return &c->equipment_slots[i].item;
        }
    }
    return NULL;
}

static void clear_slot_item(EquipmentSlot* slot)
{
    if (!slot)
        return;

    memset(&slot->item, 0, sizeof(slot->item));
    slot->item.type = ITEM_TYPE_NONE;
}

/*
 * Purpose:
 *   Implements inventory storage, equipment transitions, and inventory overlay UI.
 *
 * Functions:
 *   - slot_from_key: maps keyboard keys to inventory slot indices.
 *   - format_* helpers: build display strings for inventory and equipment.
 *   - draw_inventory_overlay: renders full inventory/equipment overlay state.
 *   - inventory_init/add/remove/use: core inventory operations.
 *   - inventory_equip/unequip: equipment slot management.
 *   - inventory_print/menu/quick_equip: user-facing inventory interfaces.
 */




// Helper: find first empty slot of a given type
static int find_empty_slot(Character* c, EquipmentSlotType slot_type) {
    if (!c) return -1;
    for (int i = 0; i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].slot_type == slot_type && c->equipment_slots[i].item.type == ITEM_TYPE_NONE) {
            return i;
        }
    }
    return -1;
}

static int item_type_is_armor_piece(ItemType type)
{
    switch(type)
    {
        case ITEM_TYPE_ARMOR_HEAD:
        case ITEM_TYPE_ARMOR_FACE:
        case ITEM_TYPE_ARMOR_NECK:
        case ITEM_TYPE_ARMOR_SHOULDERS:
        case ITEM_TYPE_ARMOR_CLOAK:
        case ITEM_TYPE_ARMOR_CHEST:
        case ITEM_TYPE_ARMOR_WAIST:
        case ITEM_TYPE_ARMOR_ARMS:
        case ITEM_TYPE_ARMOR_HANDS:
        case ITEM_TYPE_ARMOR_LEGS:
        case ITEM_TYPE_ARMOR_FEET:
        case ITEM_TYPE_ARMOR_BOOTS:
            return 1;
        default:
            return 0;
    }
}

static void inventory_apply_equipped_item_stats(Character* c, const Item* item, int direction)
{
    int armor_delta;

    if(!c || !item || direction == 0)
        return;

    armor_delta = item_type_is_armor_piece(item->type) ? item->power : 0;
    if(armor_delta > 0)
    {
        c->actor.armor_rating += (armor_delta * direction);
        if(c->actor.armor_rating < 0)
            c->actor.armor_rating = 0;
    }
}

static int item_type_fits_slot(ItemType type, EquipmentSlotType slot_type)
{
    switch(type)
    {
        case ITEM_TYPE_WEAPON_MAIN_HAND:
            return slot_type == EQUIP_SLOT_MAIN_HAND;
        case ITEM_TYPE_WEAPON_OFF_HAND:
            return slot_type == EQUIP_SLOT_OFF_HAND;
        case ITEM_TYPE_WEAPON_ONE_HANDED:
        case ITEM_TYPE_WEAPON_VERSATILE:
        case ITEM_TYPE_WEAPON_TWO_HANDED:
            return slot_type == EQUIP_SLOT_MAIN_HAND || slot_type == EQUIP_SLOT_OFF_HAND;
        case ITEM_TYPE_ARMOR_HEAD:
            return slot_type == EQUIP_SLOT_ARMOR_HEAD;
        case ITEM_TYPE_ARMOR_FACE:
            return slot_type == EQUIP_SLOT_ARMOR_FACE;
        case ITEM_TYPE_ARMOR_NECK:
            return slot_type == EQUIP_SLOT_ACCESSORY_NECK;
        case ITEM_TYPE_ARMOR_SHOULDERS:
        case ITEM_TYPE_ARMOR_CLOAK:
            return slot_type == EQUIP_SLOT_ARMOR_SHOULDERS;
        case ITEM_TYPE_ARMOR_CHEST:
            return slot_type == EQUIP_SLOT_ARMOR_CHEST;
        case ITEM_TYPE_ARMOR_WAIST:
            return slot_type == EQUIP_SLOT_ARMOR_WAIST;
        case ITEM_TYPE_ARMOR_ARMS:
            return slot_type == EQUIP_SLOT_ARMOR_ARMS;
        case ITEM_TYPE_ARMOR_HANDS:
            return slot_type == EQUIP_SLOT_ARMOR_HANDS;
        case ITEM_TYPE_ARMOR_LEGS:
            return slot_type == EQUIP_SLOT_ARMOR_LEGS;
        case ITEM_TYPE_ARMOR_FEET:
        case ITEM_TYPE_ARMOR_BOOTS:
            return slot_type == EQUIP_SLOT_ARMOR_FEET;
        case ITEM_TYPE_CLOTHING_HEAD:
            return slot_type == EQUIP_SLOT_CLOTHING_HEAD;
        case ITEM_TYPE_CLOTHING_FACE:
            return slot_type == EQUIP_SLOT_CLOTHING_FACE;
        case ITEM_TYPE_CLOTHING_SHOULDERS:
            return slot_type == EQUIP_SLOT_CLOTHING_SHOULDERS;
        case ITEM_TYPE_CLOTHING_CHEST:
            return slot_type == EQUIP_SLOT_CLOTHING_CHEST;
        case ITEM_TYPE_CLOTHING_HANDS:
            return slot_type == EQUIP_SLOT_CLOTHING_HANDS;
        case ITEM_TYPE_CLOTHING_WAIST:
            return slot_type == EQUIP_SLOT_CLOTHING_WAIST;
        case ITEM_TYPE_CLOTHING_LEGS:
            return slot_type == EQUIP_SLOT_CLOTHING_LEGS;
        case ITEM_TYPE_CLOTHING_FEET:
            return slot_type == EQUIP_SLOT_CLOTHING_FEET;
        case ITEM_TYPE_ACCESSORY_NECK:
            return slot_type == EQUIP_SLOT_ACCESSORY_NECK;
        case ITEM_TYPE_ACCESSORY_BRACELET:
            return slot_type == EQUIP_SLOT_ACCESSORY_BRACELET_RIGHT || slot_type == EQUIP_SLOT_ACCESSORY_BRACELET_LEFT;
        case ITEM_TYPE_ACCESSORY_FINGER:
            return slot_type == EQUIP_SLOT_ACCESSORY_FINGER_RIGHT || slot_type == EQUIP_SLOT_ACCESSORY_FINGER_LEFT;
        case ITEM_TYPE_ACCESSORY_TRINKET:
            return slot_type == EQUIP_SLOT_ACCESSORY_TRINKET_0 || slot_type == EQUIP_SLOT_ACCESSORY_TRINKET_1;
        case ITEM_TYPE_CONTAINER_BACKPACK:
            return slot_type == EQUIP_SLOT_CONTAINER_BACKPACK;
        case ITEM_TYPE_CONTAINER_POUCH:
            return slot_type == EQUIP_SLOT_CONTAINER_POUCH;
        case ITEM_TYPE_CONTAINER_QUIVER:
            return slot_type == EQUIP_SLOT_CONTAINER_QUIVER;
        default:
            return 0;
    }
}

static int find_first_empty_equip_slot(const Character* c, ItemType type)
{
    EquipmentSlotType preferred_slot;

    if(!c)
        return -1;

    preferred_slot = equipment_slot_for_item_type(type);
    if(preferred_slot != EQUIP_SLOT_NONE)
    {
        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            const EquipmentSlot* slot = &c->equipment_slots[i];
            if(slot->slot_type == preferred_slot &&
               slot->item.type == ITEM_TYPE_NONE &&
               item_type_fits_slot(type, slot->slot_type))
                return i;
        }
    }

    for(int i = 0; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->slot_type == EQUIP_SLOT_NONE || slot->item.type != ITEM_TYPE_NONE)
            continue;
        if(item_type_fits_slot(type, slot->slot_type))
            return i;
    }

    return -1;
}

static int inventory_first_slot_index(void)
{
    return EQUIP_SLOT_COUNT;
}

static int inventory_capacity_from_equipped_containers(const Character* c)
{
    int capacity = INVENTORY_SIZE;
    int carried_capacity = 0;

    if(!c)
        return 0;

    for(int i = 0; i < EQUIP_SLOT_COUNT && i < MAX_EQUIPMENT_SLOTS; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];

        if(slot->item.type == ITEM_TYPE_NONE || !slot->item.is_container || slot->item.container_capacity <= 0)
            continue;

        if(slot->slot_type == EQUIP_SLOT_CONTAINER_BACKPACK ||
           slot->slot_type == EQUIP_SLOT_CONTAINER_POUCH ||
           slot->slot_type == EQUIP_SLOT_CONTAINER_QUIVER)
            carried_capacity += slot->item.container_capacity;
    }

    if(carried_capacity > capacity)
        capacity = carried_capacity;

    if(capacity > MAX_EQUIPMENT_SLOTS - inventory_first_slot_index())
        capacity = MAX_EQUIPMENT_SLOTS - inventory_first_slot_index();
    if(capacity < 0)
        capacity = 0;

    return capacity;
}

// Add one item instance to inventory (slot_type == EQUIP_SLOT_NONE); returns 1 on success.
int inventory_add(Character* c, const Item* item) {
    int first_inventory_slot;

    if (!c || !item || item->type == ITEM_TYPE_NONE) return 0;

    first_inventory_slot = inventory_first_slot_index();
    for (int i = first_inventory_slot; i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].slot_type == EQUIP_SLOT_NONE && c->equipment_slots[i].item.type == ITEM_TYPE_NONE) {
            c->equipment_slots[i].item = *item;
            c->equipment_slots[i].item.slot_type = EQUIP_SLOT_NONE;
            return 1;
        }
    }
    return 0;
}

// Remove an item at slot index; returns 1 on success.
int inventory_remove(Character* c, int slot) {
    if (!c || slot < inventory_first_slot_index() || slot >= c->equipment_slot_count) return 0;
    if (c->equipment_slots[slot].item.type == ITEM_TYPE_NONE) return 0;
    clear_slot_item(&c->equipment_slots[slot]);
    return 1;
}

// Use item at slot index (consumables); returns 1 on success.
int inventory_use(Character* c, int slot) {
    if (!c || slot < inventory_first_slot_index() || slot >= c->equipment_slot_count) return 0;
    Item* item = &c->equipment_slots[slot].item;
    if (item->type == ITEM_TYPE_NONE) return 0;
    // TODO: Implement item use logic (e.g., apply effects)
    clear_slot_item(&c->equipment_slots[slot]);
    return 1;
}




EquipmentSlotType equipment_slot_for_item_type(ItemType type)
{
    switch(type)
    {
        case ITEM_TYPE_WEAPON_MAIN_HAND:
        case ITEM_TYPE_WEAPON_ONE_HANDED:
        case ITEM_TYPE_WEAPON_VERSATILE:
        case ITEM_TYPE_WEAPON_TWO_HANDED:
            return EQUIP_SLOT_MAIN_HAND;
        case ITEM_TYPE_WEAPON_OFF_HAND:
            return EQUIP_SLOT_OFF_HAND;
        case ITEM_TYPE_ARMOR_HEAD:
            return EQUIP_SLOT_ARMOR_HEAD;
        case ITEM_TYPE_ARMOR_FACE:
            return EQUIP_SLOT_ARMOR_FACE;
        case ITEM_TYPE_ARMOR_NECK:
        case ITEM_TYPE_ACCESSORY_NECK:
            return EQUIP_SLOT_ACCESSORY_NECK;
        case ITEM_TYPE_ARMOR_SHOULDERS:
        case ITEM_TYPE_ARMOR_CLOAK:
            return EQUIP_SLOT_ARMOR_SHOULDERS;
        case ITEM_TYPE_ARMOR_CHEST:
            return EQUIP_SLOT_ARMOR_CHEST;
        case ITEM_TYPE_ARMOR_ARMS:
            return EQUIP_SLOT_ARMOR_ARMS;
        case ITEM_TYPE_ARMOR_HANDS:
            return EQUIP_SLOT_ARMOR_HANDS;
        case ITEM_TYPE_ARMOR_WAIST:
            return EQUIP_SLOT_ARMOR_WAIST;
        case ITEM_TYPE_ARMOR_LEGS:
            return EQUIP_SLOT_ARMOR_LEGS;
        case ITEM_TYPE_ARMOR_FEET:
        case ITEM_TYPE_ARMOR_BOOTS:
            return EQUIP_SLOT_ARMOR_FEET;
        case ITEM_TYPE_CLOTHING_HEAD:
            return EQUIP_SLOT_CLOTHING_HEAD;
        case ITEM_TYPE_CLOTHING_FACE:
            return EQUIP_SLOT_CLOTHING_FACE;
        case ITEM_TYPE_CLOTHING_SHOULDERS:
            return EQUIP_SLOT_CLOTHING_SHOULDERS;
        case ITEM_TYPE_CLOTHING_CHEST:
            return EQUIP_SLOT_CLOTHING_CHEST;
        case ITEM_TYPE_CLOTHING_HANDS:
            return EQUIP_SLOT_CLOTHING_HANDS;
        case ITEM_TYPE_CLOTHING_WAIST:
            return EQUIP_SLOT_CLOTHING_WAIST;
        case ITEM_TYPE_CLOTHING_LEGS:
            return EQUIP_SLOT_CLOTHING_LEGS;
        case ITEM_TYPE_CLOTHING_FEET:
            return EQUIP_SLOT_CLOTHING_FEET;
        case ITEM_TYPE_ACCESSORY_BRACELET:
            return EQUIP_SLOT_ACCESSORY_BRACELET_RIGHT;
        case ITEM_TYPE_ACCESSORY_FINGER:
            return EQUIP_SLOT_ACCESSORY_FINGER_RIGHT;
        case ITEM_TYPE_ACCESSORY_TRINKET:
            return EQUIP_SLOT_ACCESSORY_TRINKET_0;
        case ITEM_TYPE_CONTAINER_BACKPACK:
            return EQUIP_SLOT_CONTAINER_BACKPACK;
        case ITEM_TYPE_CONTAINER_POUCH:
            return EQUIP_SLOT_CONTAINER_POUCH;
        case ITEM_TYPE_CONTAINER_QUIVER:
            return EQUIP_SLOT_CONTAINER_QUIVER;
        default:
            return EQUIP_SLOT_NONE;
    }
}

int inventory_init(Character* c)
{
    if (!c)
        return 0;

    memset(c->equipment_slots, 0, sizeof(c->equipment_slots));

    for (int i = 0; i < EQUIP_SLOT_COUNT && i < MAX_EQUIPMENT_SLOTS; ++i) {
        c->equipment_slots[i].slot_type = (EquipmentSlotType)i;
        c->equipment_slots[i].item.type = ITEM_TYPE_NONE;
        c->equipment_slots[i].is_container_slot =
            (i == EQUIP_SLOT_CONTAINER_BACKPACK ||
             i == EQUIP_SLOT_CONTAINER_POUCH ||
             i == EQUIP_SLOT_CONTAINER_QUIVER) ? 1 : 0;
    }

    for (int i = inventory_first_slot_index(); i < MAX_EQUIPMENT_SLOTS; ++i) {
        c->equipment_slots[i].slot_type = EQUIP_SLOT_NONE;
        c->equipment_slots[i].item.type = ITEM_TYPE_NONE;
        c->equipment_slots[i].is_container_slot = 0;
    }

    c->inventory_slot_count = INVENTORY_SIZE;
    c->equipment_slot_count = inventory_first_slot_index() + c->inventory_slot_count;
    if(c->equipment_slot_count > MAX_EQUIPMENT_SLOTS)
        c->equipment_slot_count = MAX_EQUIPMENT_SLOTS;

    update_dynamic_container_slots(c);
    return 1;
}

// Recalculate the carried inventory region separately from fixed equipment slots.
void update_dynamic_container_slots(Character* c) {
    Item stored_inventory[MAX_EQUIPMENT_SLOTS];
    int stored_count = 0;
    int inventory_start;
    int total_slots;

    if (!c)
        return;

    inventory_start = inventory_first_slot_index();

    for (int i = 0; i < c->equipment_slot_count && i < MAX_EQUIPMENT_SLOTS; ++i) {
        if (c->equipment_slots[i].slot_type == EQUIP_SLOT_NONE &&
            c->equipment_slots[i].item.type != ITEM_TYPE_NONE) {
            stored_inventory[stored_count++] = c->equipment_slots[i].item;
        }
    }

    for (int i = 0; i < EQUIP_SLOT_COUNT && i < MAX_EQUIPMENT_SLOTS; ++i) {
        c->equipment_slots[i].is_container_slot =
            (c->equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_BACKPACK ||
             c->equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_POUCH ||
             c->equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_QUIVER) ? 1 : 0;
    }

    c->inventory_slot_count = inventory_capacity_from_equipped_containers(c);
    total_slots = inventory_start + c->inventory_slot_count;
    if(total_slots > MAX_EQUIPMENT_SLOTS)
        total_slots = MAX_EQUIPMENT_SLOTS;
    c->equipment_slot_count = total_slots;

    for (int i = inventory_start; i < MAX_EQUIPMENT_SLOTS; ++i) {
        memset(&c->equipment_slots[i], 0, sizeof(c->equipment_slots[i]));
        c->equipment_slots[i].slot_type = EQUIP_SLOT_NONE;
        c->equipment_slots[i].item.type = ITEM_TYPE_NONE;
        c->equipment_slots[i].is_container_slot = 0;
    }

    for (int i = 0; i < stored_count && i < c->inventory_slot_count; ++i)
        c->equipment_slots[inventory_start + i].item = stored_inventory[i];
}

int inventory_equip(Character* c, int inv_slot, int equip_slot)
{
    if (!c) return 0;
    if (inv_slot < 0 || inv_slot >= c->equipment_slot_count) return 0;
    if (equip_slot < 0 || equip_slot >= c->equipment_slot_count) return 0;

    Item* inv_item = &c->equipment_slots[inv_slot].item;
    EquipmentSlot* dst_slot = &c->equipment_slots[equip_slot];

    if (inv_item->type == ITEM_TYPE_NONE)
        return 0;

    if (dst_slot->item.type != ITEM_TYPE_NONE)
        return 0;

    if (!item_type_fits_slot(inv_item->type, dst_slot->slot_type))
        return 0;

    dst_slot->item = *inv_item;
    dst_slot->item.slot_type = dst_slot->slot_type;
    inventory_apply_equipped_item_stats(c, &dst_slot->item, 1);
    clear_slot_item(&c->equipment_slots[inv_slot]);

    update_dynamic_container_slots(c);
    return 1;
}

int inventory_equip_to_slot(Character* c, int inv_slot, EquipmentSlotType slot_type)
{
    if (!c) return 0;

    int equip_slot = -1;
    for (int i = 0; i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].slot_type == slot_type && c->equipment_slots[i].item.type == ITEM_TYPE_NONE) {
            equip_slot = i;
            break;
        }
    }

    if (equip_slot < 0)
        return 0;

    return inventory_equip(c, inv_slot, equip_slot);
}

int inventory_auto_equip(Character* c, int inv_slot)
{
    int equip_slot;

    if(!c || inv_slot < 0 || inv_slot >= c->equipment_slot_count)
        return 0;
    if(c->equipment_slots[inv_slot].slot_type != EQUIP_SLOT_NONE)
        return 0;
    if(c->equipment_slots[inv_slot].item.type == ITEM_TYPE_NONE)
        return 0;

    equip_slot = find_first_empty_equip_slot(c, c->equipment_slots[inv_slot].item.type);
    if(equip_slot < 0)
        return 0;

    return inventory_equip(c, inv_slot, equip_slot);
}

int inventory_unequip_slot(Character* c, EquipmentSlotType slot_type)
{
    if (!c) return 0;

    int equipped_index = -1;
    for (int i = 0; i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].slot_type == slot_type && c->equipment_slots[i].item.type != ITEM_TYPE_NONE) {
            equipped_index = i;
            break;
        }
    }

    if (equipped_index < 0)
        return 0;

    int inventory_index = -1;
    for (int i = inventory_first_slot_index(); i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].slot_type == EQUIP_SLOT_NONE && c->equipment_slots[i].item.type == ITEM_TYPE_NONE) {
            inventory_index = i;
            break;
        }
    }

    if (inventory_index < 0)
        return 0;

    inventory_apply_equipped_item_stats(c, &c->equipment_slots[equipped_index].item, -1);
    c->equipment_slots[inventory_index].item = c->equipment_slots[equipped_index].item;
    c->equipment_slots[inventory_index].item.slot_type = EQUIP_SLOT_NONE;
    clear_slot_item(&c->equipment_slots[equipped_index]);

    update_dynamic_container_slots(c);
    return 1;
}

int inventory_unequip(Character* c, ItemType type)
{
    if (!c) return 0;

    int slot_to_unequip = -1;
    for (int i = 0; i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].item.type == type) {
            slot_to_unequip = c->equipment_slots[i].slot_type;
            break;
        }
    }

    if (slot_to_unequip < 0) return 0;

    return inventory_unequip_slot(c, (EquipmentSlotType)slot_to_unequip);
}

// Print inventory/equipment summary using slot-based system
void inventory_print(const Character* c)
{
    if (!c) return;
    printf("Equipped items:\n");
    for (int i = 0; i < c->equipment_slot_count; ++i) {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if (slot->item.type != ITEM_TYPE_NONE) {
            printf("  Slot %d (%d): %s x%d\n", i, slot->slot_type, slot->item.name, slot->item.quantity);
        }
    }

}

// Run full inventory interaction overlay loop.
void inventory_menu(Character* c)
{
    if(!c) return;

    update_dynamic_container_slots(c);

    char status[192];
    int scroll_offset = 0;
    int selected = 0;
    int total_slots = 0;
    int slot_indices[256]; // Map visible row to (section, index)
    int slot_types[256];   // 0=equipment, 1=inventory
    memset(slot_indices, 0, sizeof(slot_indices));
    memset(slot_types, 0, sizeof(slot_types));

    // Build flat slot list for navigation
    // Equipment (non-container)
    for (int i = 0; i < c->equipment_slot_count; ++i) {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if (slot->slot_type == EQUIP_SLOT_NONE) continue;
        if (slot->slot_type == EQUIP_SLOT_CONTAINER_BACKPACK || slot->is_container_slot) continue; // skip containers for now
        slot_indices[total_slots] = i;
        slot_types[total_slots] = 0;
        total_slots++;
    }
    // Containers (backpack and dynamic)
    for (int i = 0; i < c->equipment_slot_count; ++i) {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if (slot->slot_type == EQUIP_SLOT_CONTAINER_BACKPACK || slot->is_container_slot) {
            slot_indices[total_slots] = i;
            slot_types[total_slots] = 2; // container
            total_slots++;
        }
    }
    // Inventory
    for (int i = inventory_first_slot_index(); i < c->equipment_slot_count; ++i) {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if (slot->slot_type != EQUIP_SLOT_NONE) continue;
        slot_indices[total_slots] = i;
        slot_types[total_slots] = 1;
        total_slots++;
    }


    snprintf(status, sizeof(status), "Enter: Action | Q: Exit | W/S: Move | N: Unequip");

    while (1) {
        // Render overlay frame and inventory/equipment list
        ui_overlay_draw_frame("Inventory");
        int overlay_content_lines = ui_overlay_content_lines();
        int visible_rows = (overlay_content_lines > 2) ? (overlay_content_lines - 2) : 0;
        int max_scroll = total_slots - visible_rows;
        if (max_scroll < 0) max_scroll = 0;
        int row = 0;
        for (int i = 0; i < visible_rows && (i + scroll_offset) < total_slots; ++i) {
            int idx = slot_indices[i + scroll_offset];
            int stype = slot_types[i + scroll_offset];
            const EquipmentSlot* slot = &c->equipment_slots[idx];
            char line[128];
            int shown_quantity = (slot->item.quantity > 0) ? slot->item.quantity : 1;
            if (slot->item.type == ITEM_TYPE_NONE || slot->item.name[0] == '\0') {
                if (stype == 0) {
                    snprintf(line, sizeof(line), "[E] (empty)");
                } else if (stype == 2) {
                    if (slot->slot_type == EQUIP_SLOT_CONTAINER_BACKPACK) {
                        snprintf(line, sizeof(line), "[B] (empty)");
                    } else {
                        snprintf(line, sizeof(line), "[C] (empty)");
                    }
                } else {
                    snprintf(line, sizeof(line), "[I] (empty)");
                }
            } else if (stype == 0) {
                snprintf(line, sizeof(line), "[E] %-16s x%d", slot->item.name, shown_quantity);
            } else if (stype == 2) {
                if (slot->slot_type == EQUIP_SLOT_CONTAINER_BACKPACK) {
                    snprintf(line, sizeof(line), "[B] %s x%d", slot->item.name, shown_quantity);
                } else {
                    snprintf(line, sizeof(line), "[C] %s x%d", slot->item.name, shown_quantity);
                }
            } else {
                snprintf(line, sizeof(line), "[I] %-16s x%d", slot->item.name, shown_quantity);
            }
            if ((i + scroll_offset) == selected) {
                char sel_line[132];
                snprintf(sel_line, sizeof(sel_line), "> %s", line);
                ui_overlay_draw_line(row++, sel_line);
            } else {
                ui_overlay_draw_line(row++, line);
            }
        }
        ui_overlay_draw_line(row++, status);

        int cmd = read_input_key();
        if (cmd == 'q' || cmd == 'Q') break;

        if (cmd == INPUT_KEY_UP || cmd == 'w' || cmd == 'W') {
            if (selected > 0) selected--;
            if (selected < scroll_offset) scroll_offset = selected;
            continue;
        }
        if (cmd == INPUT_KEY_DOWN || cmd == 's' || cmd == 'S') {
            if (selected < total_slots - 1) selected++;
            if (selected >= scroll_offset + visible_rows) scroll_offset = selected - visible_rows + 1;
            continue;
        }
        if (cmd == INPUT_KEY_PGUP) {
            selected -= visible_rows;
            if (selected < 0) selected = 0;
            scroll_offset = selected;
            continue;
        }
        if (cmd == INPUT_KEY_PGDN) {
            selected += visible_rows;
            if (selected >= total_slots) selected = total_slots - 1;
            scroll_offset = selected - visible_rows + 1;
            if (scroll_offset < 0) scroll_offset = 0;
            continue;
        }
        if (cmd == INPUT_KEY_HOME) {
            selected = 0;
            scroll_offset = 0;
            continue;
        }
        if (cmd == INPUT_KEY_END) {
            selected = total_slots - 1;
            scroll_offset = max_scroll;
            continue;
        }

        // Overlay switching
        {
            OverlayType next_overlay;
            if (overlay_type_from_key(cmd, &next_overlay) && next_overlay != OVERLAY_TYPE_INVENTORY) {
                overlay_request(next_overlay);
                break;
            }
        }

        // Contextual actions based on slot type
        int stype = slot_types[selected];
        int sidx = slot_indices[selected];
        if (cmd == 13) { // Enter: context action
            if (stype == 0) { // Equipment slot: unequip
                if (c->equipment_slots[sidx].item.type != ITEM_TYPE_NONE) {
                    if (inventory_unequip_slot(c, c->equipment_slots[sidx].slot_type)) {
                        snprintf(status, sizeof(status), "Unequipped %s.", c->equipment_slots[sidx].item.name);
                    } else {
                        snprintf(status, sizeof(status), "Failed to unequip.");
                    }
                }
            } else if (stype == 1) { // Inventory slot: use/equip/stash/drop
                if (c->equipment_slots[sidx].item.type != ITEM_TYPE_NONE) {
                    // Try use first, then equip
                    if (c->equipment_slots[sidx].item.type == ITEM_TYPE_CONSUMABLE) {
                        if (inventory_use(c, sidx)) {
                            snprintf(status, sizeof(status), "Used %s.", c->equipment_slots[sidx].item.name);
                        } else {
                            snprintf(status, sizeof(status), "Failed to use.");
                        }
                    } else {
                        char item_name[32];
                        snprintf(item_name, sizeof(item_name), "%s", c->equipment_slots[sidx].item.name);
                        if (inventory_auto_equip(c, sidx)) {
                            snprintf(status, sizeof(status), "Equipped %s.", item_name);
                        } else {
                            snprintf(status, sizeof(status), "Failed to equip %s.", item_name);
                        }
                    }
                }
            }
        }
        if (cmd == 'n' || cmd == 'N') {
            // Show a menu of equipped items to unequip
            int equipped_count = 0;
            int equipped_indices[64];
            for (int i = 0; i < c->equipment_slot_count; ++i) {
                if (c->equipment_slots[i].slot_type != EQUIP_SLOT_NONE && c->equipment_slots[i].item.type != ITEM_TYPE_NONE) {
                    equipped_indices[equipped_count++] = i;
                }
            }
            if (equipped_count == 0) {
                snprintf(status, sizeof(status), "No items equipped.");
                continue;
            }
            int unequip_selected = 0;
            while (1) {
                ui_overlay_draw_frame("Unequip");
                for (int i = 0; i < equipped_count; i++) {
                    char line[64];
                    const EquipmentSlot* eq = &c->equipment_slots[equipped_indices[i]];
                    snprintf(line, sizeof(line), "%c %s", (i == unequip_selected) ? '>' : ' ', (eq->item.type != ITEM_TYPE_NONE) ? eq->item.name : "(none)");
                    ui_overlay_draw_line(i, line);
                }
                int unequip_cmd = read_input_key();
                if (unequip_cmd == 'q' || unequip_cmd == 'Q') break;
                if (unequip_cmd == INPUT_KEY_UP || unequip_cmd == 'w' || unequip_cmd == 'W') {
                    if (unequip_selected > 0) unequip_selected--;
                    continue;
                }
                if (unequip_cmd == INPUT_KEY_DOWN || unequip_cmd == 's' || unequip_cmd == 'S') {
                    if (unequip_selected < equipped_count - 1) unequip_selected++;
                    continue;
                }
                if (unequip_cmd == 13) {
                    int eqidx = equipped_indices[unequip_selected];
                    if (inventory_unequip_slot(c, c->equipment_slots[eqidx].slot_type)) {
                        snprintf(status, sizeof(status), "Unequipped %s.", c->equipment_slots[eqidx].item.name);
                    } else {
                        snprintf(status, sizeof(status), "Failed to unequip.");
                    }
                    break;
                }
            }
            continue;
        }

        // Defensive: clamp cursor and scroll
        if (selected < 0) selected = 0;
        if (selected >= total_slots) selected = total_slots - 1;
        if (scroll_offset < 0) scroll_offset = 0;
        if (scroll_offset > max_scroll) scroll_offset = max_scroll;
    }
}
// END inventory_menu







