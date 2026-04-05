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
        case ITEM_TYPE_ARMOR_EYES:
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
        case ITEM_TYPE_ARMOR_EYES:
            return slot_type == EQUIP_SLOT_ARMOR_EYES;
        case ITEM_TYPE_ARMOR_FACE:
            return slot_type == EQUIP_SLOT_ARMOR_FACE;
        case ITEM_TYPE_ARMOR_NECK:
            return slot_type == EQUIP_SLOT_ARMOR_NECK;
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
        case ITEM_TYPE_CLOTHING_EYES:
            return slot_type == EQUIP_SLOT_CLOTHING_EYES;
        case ITEM_TYPE_CLOTHING_FACE:
            return slot_type == EQUIP_SLOT_CLOTHING_FACE;
        case ITEM_TYPE_CLOTHING_NECK:
            return slot_type == EQUIP_SLOT_CLOTHING_NECK;
        case ITEM_TYPE_CLOTHING_SHOULDERS:
            return slot_type == EQUIP_SLOT_CLOTHING_SHOULDERS;
        case ITEM_TYPE_CLOTHING_CHEST:
            return slot_type == EQUIP_SLOT_CLOTHING_CHEST;
        case ITEM_TYPE_CLOTHING_ARMS:
            return slot_type == EQUIP_SLOT_CLOTHING_ARMS;
        case ITEM_TYPE_CLOTHING_HANDS:
            return slot_type == EQUIP_SLOT_CLOTHING_HANDS;
        case ITEM_TYPE_CLOTHING_WAIST:
            return slot_type == EQUIP_SLOT_CLOTHING_WAIST;
        case ITEM_TYPE_CLOTHING_LEGS:
            return slot_type == EQUIP_SLOT_CLOTHING_LEGS;
        case ITEM_TYPE_CLOTHING_FEET:
            return slot_type == EQUIP_SLOT_CLOTHING_FEET;
        case ITEM_TYPE_ACCESSORY_HEAD:
            return slot_type == EQUIP_SLOT_ACCESSORY_HEAD;
        case ITEM_TYPE_ACCESSORY_EYES:
            return slot_type == EQUIP_SLOT_ACCESSORY_EYES;
        case ITEM_TYPE_ACCESSORY_FACE:
            return slot_type == EQUIP_SLOT_ACCESSORY_FACE;
        case ITEM_TYPE_ACCESSORY_NECK:
            return slot_type == EQUIP_SLOT_ACCESSORY_NECK;
        case ITEM_TYPE_ACCESSORY_WRIST:
            return slot_type == EQUIP_SLOT_ACCESSORY_WRIST_RIGHT || slot_type == EQUIP_SLOT_ACCESSORY_WRIST_LEFT;
        case ITEM_TYPE_ACCESSORY_FINGER:
            return slot_type == EQUIP_SLOT_ACCESSORY_FINGER_RIGHT || slot_type == EQUIP_SLOT_ACCESSORY_FINGER_LEFT;
        case ITEM_TYPE_ACCESSORY_TRINKET:
            return slot_type == EQUIP_SLOT_ACCESSORY_TRINKET_1 || slot_type == EQUIP_SLOT_ACCESSORY_TRINKET_2;
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

static int slot_request_matches_candidate(EquipmentSlotType requested, EquipmentSlotType candidate)
{
    if(requested == candidate)
        return 1;

    if(requested == EQUIP_SLOT_ACCESSORY_WRIST ||
       requested == EQUIP_SLOT_ACCESSORY_WRIST_RIGHT ||
       requested == EQUIP_SLOT_ACCESSORY_WRIST_LEFT)
    {
        return candidate == EQUIP_SLOT_ACCESSORY_WRIST_RIGHT ||
               candidate == EQUIP_SLOT_ACCESSORY_WRIST_LEFT;
    }

    if(requested == EQUIP_SLOT_ACCESSORY_FINGER ||
       requested == EQUIP_SLOT_ACCESSORY_FINGER_RIGHT ||
       requested == EQUIP_SLOT_ACCESSORY_FINGER_LEFT)
    {
        return candidate == EQUIP_SLOT_ACCESSORY_FINGER_RIGHT ||
               candidate == EQUIP_SLOT_ACCESSORY_FINGER_LEFT;
    }

    if(requested == EQUIP_SLOT_ACCESSORY_TRINKET ||
       requested == EQUIP_SLOT_ACCESSORY_TRINKET_1 ||
       requested == EQUIP_SLOT_ACCESSORY_TRINKET_2)
    {
        return candidate == EQUIP_SLOT_ACCESSORY_TRINKET_1 ||
               candidate == EQUIP_SLOT_ACCESSORY_TRINKET_2;
    }

    return 0;
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

static int inventory_item_storage_flags(const Item* item)
{
    if(!item || item->type == ITEM_TYPE_NONE)
        return CONTAINER_ACCEPTS_MISC;

    if(item->is_ammo)
        return CONTAINER_ACCEPTS_AMMO;

    if(item->type == ITEM_TYPE_CONSUMABLE)
        return CONTAINER_ACCEPTS_CONSUMABLE;

    if(item->type == ITEM_TYPE_KEY)
        return CONTAINER_ACCEPTS_KEY;

    if(item->type >= ITEM_TYPE_WEAPON_MAIN_HAND && item->type <= ITEM_TYPE_CONTAINER_QUIVER)
        return CONTAINER_ACCEPTS_EQUIPMENT;

    return CONTAINER_ACCEPTS_MISC;
}

static int inventory_container_accepts_item(const EquipmentSlot* slot, const Item* item, int require_specific_match)
{
    int accepted_flags;
    int item_flags;

    if(!slot || !item)
        return 0;

    if(slot->item.type == ITEM_TYPE_NONE || !slot->item.is_container || slot->item.container_capacity <= 0)
        return 0;

    accepted_flags = slot->item.container_accepted_flags;
    if(accepted_flags == CONTAINER_ACCEPTS_ALL)
        return require_specific_match ? 0 : 1;

    item_flags = inventory_item_storage_flags(item);
    return (accepted_flags & item_flags) != 0;
}

static int inventory_find_empty_slot_in_range(Character* c, int start, int count)
{
    if(!c || count <= 0)
        return -1;

    for(int i = 0; i < count; ++i)
    {
        int slot_index = start + i;
        if(slot_index < inventory_first_slot_index() || slot_index >= c->equipment_slot_count)
            break;

        if(c->equipment_slots[slot_index].slot_type == EQUIP_SLOT_NONE &&
           c->equipment_slots[slot_index].item.type == ITEM_TYPE_NONE)
            return slot_index;
    }

    return -1;
}

static int inventory_find_preferred_container_slot(Character* c, const Item* item, int require_specific_match)
{
    int inventory_offset = 0;
    int inventory_start = inventory_first_slot_index();

    if(!c || !item)
        return -1;

    for(int i = 0; i < EQUIP_SLOT_COUNT && i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        int capacity;
        int segment_start;
        int empty_slot;

        if(slot->slot_type != EQUIP_SLOT_CONTAINER_BACKPACK &&
           slot->slot_type != EQUIP_SLOT_CONTAINER_POUCH &&
           slot->slot_type != EQUIP_SLOT_CONTAINER_QUIVER)
            continue;

        capacity = slot->item.container_capacity;
        if(capacity < 0)
            capacity = 0;
        if(inventory_offset >= c->inventory_slot_count)
            break;
        if(capacity > c->inventory_slot_count - inventory_offset)
            capacity = c->inventory_slot_count - inventory_offset;

        segment_start = inventory_start + inventory_offset;
        inventory_offset += capacity;

        if(!inventory_container_accepts_item(slot, item, require_specific_match))
            continue;

        empty_slot = inventory_find_empty_slot_in_range(c, segment_start, capacity);
        if(empty_slot >= 0)
            return empty_slot;
    }

    return -1;
}

static int inventory_place_item_in_carried_slots(Character* c, const Item* item)
{
    int slot_index;

    if(!c || !item || item->type == ITEM_TYPE_NONE)
        return 0;

    slot_index = inventory_find_preferred_container_slot(c, item, 1);
    if(slot_index < 0)
        slot_index = inventory_find_preferred_container_slot(c, item, 0);
    if(slot_index < 0)
        slot_index = inventory_find_empty_slot_in_range(c,
                                                        inventory_first_slot_index(),
                                                        c->equipment_slot_count - inventory_first_slot_index());

    if(slot_index < 0)
        return 0;

    c->equipment_slots[slot_index].item = *item;
    c->equipment_slots[slot_index].item.slot_type = EQUIP_SLOT_NONE;
    return 1;
}

// Add one item instance to inventory (slot_type == EQUIP_SLOT_NONE); returns 1 on success.
int inventory_add(Character* c, const Item* item) {
    if (!c || !item || item->type == ITEM_TYPE_NONE) return 0;

    return inventory_place_item_in_carried_slots(c, item);
}

// Remove an item at slot index; returns 1 on success.
int inventory_remove(Character* c, int slot) {
    if (!c || slot < inventory_first_slot_index() || slot >= c->equipment_slot_count) return 0;
    if (c->equipment_slots[slot].item.type == ITEM_TYPE_NONE) return 0;
    clear_slot_item(&c->equipment_slots[slot]);
    return 1;
}

static int inventory_item_is_directly_usable(const Item* item)
{
    if(!item)
        return 0;

    return item->type == ITEM_TYPE_CONSUMABLE && !item->is_ammo;
}

// Use item at slot index (consumables); returns 1 on success.
int inventory_use(Character* c, int slot) {
    if (!c || slot < inventory_first_slot_index() || slot >= c->equipment_slot_count) return 0;
    Item* item = &c->equipment_slots[slot].item;
    if (!inventory_item_is_directly_usable(item)) return 0;
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
        case ITEM_TYPE_ARMOR_EYES:
            return EQUIP_SLOT_ARMOR_EYES;
        case ITEM_TYPE_ARMOR_FACE:
            return EQUIP_SLOT_ARMOR_FACE;
        case ITEM_TYPE_ARMOR_NECK:
            return EQUIP_SLOT_ARMOR_NECK;
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
        case ITEM_TYPE_CLOTHING_EYES:
            return EQUIP_SLOT_CLOTHING_EYES;
        case ITEM_TYPE_CLOTHING_FACE:
            return EQUIP_SLOT_CLOTHING_FACE;
        case ITEM_TYPE_CLOTHING_NECK:
            return EQUIP_SLOT_CLOTHING_NECK;
        case ITEM_TYPE_CLOTHING_SHOULDERS:
            return EQUIP_SLOT_CLOTHING_SHOULDERS;
        case ITEM_TYPE_CLOTHING_CHEST:
            return EQUIP_SLOT_CLOTHING_CHEST;
        case ITEM_TYPE_CLOTHING_ARMS:
            return EQUIP_SLOT_CLOTHING_ARMS;
        case ITEM_TYPE_CLOTHING_HANDS:
            return EQUIP_SLOT_CLOTHING_HANDS;
        case ITEM_TYPE_CLOTHING_WAIST:
            return EQUIP_SLOT_CLOTHING_WAIST;
        case ITEM_TYPE_CLOTHING_LEGS:
            return EQUIP_SLOT_CLOTHING_LEGS;
        case ITEM_TYPE_CLOTHING_FEET:
            return EQUIP_SLOT_CLOTHING_FEET;
        case ITEM_TYPE_ACCESSORY_HEAD:
            return EQUIP_SLOT_ACCESSORY_HEAD;
        case ITEM_TYPE_ACCESSORY_EYES:
            return EQUIP_SLOT_ACCESSORY_EYES;
        case ITEM_TYPE_ACCESSORY_FACE:
            return EQUIP_SLOT_ACCESSORY_FACE;
        case ITEM_TYPE_ACCESSORY_NECK:
            return EQUIP_SLOT_ACCESSORY_NECK;
        case ITEM_TYPE_ACCESSORY_WRIST:
            return EQUIP_SLOT_ACCESSORY_WRIST_RIGHT;
        case ITEM_TYPE_ACCESSORY_FINGER:
            return EQUIP_SLOT_ACCESSORY_FINGER_RIGHT;
        case ITEM_TYPE_ACCESSORY_TRINKET:
            return EQUIP_SLOT_ACCESSORY_TRINKET_1;
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

    /* Scan the full slot array here, not only equipment_slot_count.
     * During save/load, carried items can be restored into high inventory indices
     * before capacity is recalculated from equipped containers. Restricting the
     * staging pass to the old count can silently drop ammo or other carried items.
     */
    for (int i = 0; i < MAX_EQUIPMENT_SLOTS; ++i) {
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
    {
        if(!inventory_place_item_in_carried_slots(c, &stored_inventory[i]))
        {
            int slot_index = inventory_start + i;
            if(slot_index < c->equipment_slot_count)
                c->equipment_slots[slot_index].item = stored_inventory[i];
        }
    }
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
    int equip_slot = -1;
    Item* inv_item;

    if (!c) return 0;
    if (inv_slot < 0 || inv_slot >= c->equipment_slot_count) return 0;

    inv_item = &c->equipment_slots[inv_slot].item;
    if (c->equipment_slots[inv_slot].slot_type != EQUIP_SLOT_NONE || inv_item->type == ITEM_TYPE_NONE)
        return 0;

    for (int i = 0; i < c->equipment_slot_count; ++i) {
        const EquipmentSlot* slot = &c->equipment_slots[i];

        if (slot->item.type != ITEM_TYPE_NONE)
            continue;
        if (!slot_request_matches_candidate(slot_type, slot->slot_type))
            continue;
        if (!item_type_fits_slot(inv_item->type, slot->slot_type))
            continue;

        equip_slot = i;
        break;
    }

    if (equip_slot < 0 && item_type_fits_slot(inv_item->type, slot_type))
        equip_slot = find_first_empty_equip_slot(c, inv_item->type);

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
    return inventory_unequip_slot_or_drop(c, slot_type, NULL, 0, 0, 0, NULL);
}

int inventory_unequip_slot_or_drop(Character* c,
                                   EquipmentSlotType slot_type,
                                   const char* area_name,
                                   int x,
                                   int y,
                                   int z,
                                   int* dropped_to_world)
{
    int equipped_index = -1;
    int inventory_index = -1;

    if(dropped_to_world)
        *dropped_to_world = 0;

    if (!c)
        return 0;

    for (int i = 0; i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].slot_type == slot_type && c->equipment_slots[i].item.type != ITEM_TYPE_NONE) {
            equipped_index = i;
            break;
        }
    }

    if (equipped_index < 0)
        return 0;

    for (int i = inventory_first_slot_index(); i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].slot_type == EQUIP_SLOT_NONE && c->equipment_slots[i].item.type == ITEM_TYPE_NONE) {
            inventory_index = i;
            break;
        }
    }

    inventory_apply_equipped_item_stats(c, &c->equipment_slots[equipped_index].item, -1);

    if (inventory_index >= 0)
    {
        c->equipment_slots[inventory_index].item = c->equipment_slots[equipped_index].item;
        c->equipment_slots[inventory_index].item.slot_type = EQUIP_SLOT_NONE;
        clear_slot_item(&c->equipment_slots[equipped_index]);
        update_dynamic_container_slots(c);
        return 1;
    }

    if(area_name && area_name[0])
    {
        Item dropped_item = c->equipment_slots[equipped_index].item;
        dropped_item.slot_type = EQUIP_SLOT_NONE;
        if(world_item_drop_3d(&dropped_item, area_name, x, y, z))
        {
            clear_slot_item(&c->equipment_slots[equipped_index]);
            update_dynamic_container_slots(c);
            if(dropped_to_world)
                *dropped_to_world = 1;
            return 1;
        }
    }

    inventory_apply_equipped_item_stats(c, &c->equipment_slots[equipped_index].item, 1);
    return 0;
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

static int inventory_row_is_header(int slot_type)
{
    return slot_type < 0;
}

static int inventory_adjust_selected_row(const int* slot_types, int total_rows, int selected, int direction)
{
    if(!slot_types || total_rows <= 0)
        return 0;

    if(direction == 0)
        direction = 1;

    if(selected < 0)
        selected = 0;
    if(selected >= total_rows)
        selected = total_rows - 1;

    while(selected >= 0 && selected < total_rows && inventory_row_is_header(slot_types[selected]))
        selected += (direction > 0) ? 1 : -1;

    if(selected < 0)
    {
        selected = 0;
        while(selected < total_rows && inventory_row_is_header(slot_types[selected]))
            selected++;
    }
    else if(selected >= total_rows)
    {
        selected = total_rows - 1;
        while(selected > 0 && inventory_row_is_header(slot_types[selected]))
            selected--;
    }

    if(selected < 0)
        selected = 0;
    if(selected >= total_rows)
        selected = total_rows - 1;
    return selected;
}

typedef enum InventoryOverlayTab {
    INVENTORY_TAB_LOADOUT = 0,
    INVENTORY_TAB_CLOTHING,
    INVENTORY_TAB_ARMOR,
    INVENTORY_TAB_ACCESSORIES,
    INVENTORY_TAB_COUNT
} InventoryOverlayTab;

static const char* inventory_tab_name(InventoryOverlayTab tab)
{
    switch(tab)
    {
        case INVENTORY_TAB_CLOTHING:
            return "Clothing";
        case INVENTORY_TAB_ARMOR:
            return "Armor";
        case INVENTORY_TAB_ACCESSORIES:
            return "Accessories";
        case INVENTORY_TAB_LOADOUT:
        default:
            return "Loadout";
    }
}

static int inventory_slot_is_container_type(EquipmentSlotType slot_type)
{
    return slot_type == EQUIP_SLOT_CONTAINER_BACKPACK ||
           slot_type == EQUIP_SLOT_CONTAINER_POUCH ||
           slot_type == EQUIP_SLOT_CONTAINER_QUIVER;
}

static int inventory_slot_is_clothing(EquipmentSlotType slot_type)
{
    switch(slot_type)
    {
        case EQUIP_SLOT_CLOTHING_HEAD:
        case EQUIP_SLOT_CLOTHING_EYES:
        case EQUIP_SLOT_CLOTHING_FACE:
        case EQUIP_SLOT_CLOTHING_NECK:
        case EQUIP_SLOT_CLOTHING_SHOULDERS:
        case EQUIP_SLOT_CLOTHING_CHEST:
        case EQUIP_SLOT_CLOTHING_ARMS:
        case EQUIP_SLOT_CLOTHING_HANDS:
        case EQUIP_SLOT_CLOTHING_WAIST:
        case EQUIP_SLOT_CLOTHING_LEGS:
        case EQUIP_SLOT_CLOTHING_FEET:
            return 1;
        default:
            return 0;
    }
}

static int inventory_slot_is_armor(EquipmentSlotType slot_type)
{
    switch(slot_type)
    {
        case EQUIP_SLOT_ARMOR_HEAD:
        case EQUIP_SLOT_ARMOR_EYES:
        case EQUIP_SLOT_ARMOR_FACE:
        case EQUIP_SLOT_ARMOR_NECK:
        case EQUIP_SLOT_ARMOR_SHOULDERS:
        case EQUIP_SLOT_ARMOR_CHEST:
        case EQUIP_SLOT_ARMOR_ARMS:
        case EQUIP_SLOT_ARMOR_HANDS:
        case EQUIP_SLOT_ARMOR_WAIST:
        case EQUIP_SLOT_ARMOR_LEGS:
        case EQUIP_SLOT_ARMOR_FEET:
            return 1;
        default:
            return 0;
    }
}

static int inventory_slot_is_accessory(EquipmentSlotType slot_type)
{
    switch(slot_type)
    {
        case EQUIP_SLOT_ACCESSORY_HEAD:
        case EQUIP_SLOT_ACCESSORY_EYES:
        case EQUIP_SLOT_ACCESSORY_FACE:
        case EQUIP_SLOT_ACCESSORY_NECK:
        case EQUIP_SLOT_ACCESSORY_WRIST_RIGHT:
        case EQUIP_SLOT_ACCESSORY_WRIST_LEFT:
        case EQUIP_SLOT_ACCESSORY_FINGER_RIGHT:
        case EQUIP_SLOT_ACCESSORY_FINGER_LEFT:
        case EQUIP_SLOT_ACCESSORY_TRINKET_1:
        case EQUIP_SLOT_ACCESSORY_TRINKET_2:
            return 1;
        default:
            return 0;
    }
}

static void inventory_format_slot_label(char* out, size_t out_size, const EquipmentSlot* slot, int slot_index)
{
    if(!out || out_size == 0)
        return;

    out[0] = '\0';

    if(!slot)
    {
        snprintf(out, out_size, "Slot");
        return;
    }

    switch(slot->slot_type)
    {
        case EQUIP_SLOT_MAIN_HAND:
            snprintf(out, out_size, "Main Hand");
            return;
        case EQUIP_SLOT_OFF_HAND:
            snprintf(out, out_size, "Off Hand");
            return;
        case EQUIP_SLOT_ARMOR_HEAD:
            snprintf(out, out_size, "Armor Head");
            return;
        case EQUIP_SLOT_ARMOR_EYES:
            snprintf(out, out_size, "Armor Eyes");
            return;
        case EQUIP_SLOT_ARMOR_FACE:
            snprintf(out, out_size, "Armor Face");
            return;
        case EQUIP_SLOT_ARMOR_NECK:
            snprintf(out, out_size, "Armor Neck");
            return;
        case EQUIP_SLOT_ARMOR_SHOULDERS:
            snprintf(out, out_size, "Armor Shoulders");
            return;
        case EQUIP_SLOT_ARMOR_CHEST:
            snprintf(out, out_size, "Armor Chest");
            return;
        case EQUIP_SLOT_ARMOR_ARMS:
            snprintf(out, out_size, "Armor Arms");
            return;
        case EQUIP_SLOT_ARMOR_HANDS:
            snprintf(out, out_size, "Armor Hands");
            return;
        case EQUIP_SLOT_ARMOR_WAIST:
            snprintf(out, out_size, "Armor Waist");
            return;
        case EQUIP_SLOT_ARMOR_LEGS:
            snprintf(out, out_size, "Armor Legs");
            return;
        case EQUIP_SLOT_ARMOR_FEET:
            snprintf(out, out_size, "Armor Feet");
            return;
        case EQUIP_SLOT_CLOTHING_HEAD:
            snprintf(out, out_size, "Cloth Head");
            return;
        case EQUIP_SLOT_CLOTHING_EYES:
            snprintf(out, out_size, "Cloth Eyes");
            return;
        case EQUIP_SLOT_CLOTHING_FACE:
            snprintf(out, out_size, "Cloth Face");
            return;
        case EQUIP_SLOT_CLOTHING_NECK:
            snprintf(out, out_size, "Cloth Neck");
            return;
        case EQUIP_SLOT_CLOTHING_SHOULDERS:
            snprintf(out, out_size, "Cloth Shoulders");
            return;
        case EQUIP_SLOT_CLOTHING_CHEST:
            snprintf(out, out_size, "Cloth Chest");
            return;
        case EQUIP_SLOT_CLOTHING_ARMS:
            snprintf(out, out_size, "Cloth Arms");
            return;
        case EQUIP_SLOT_CLOTHING_HANDS:
            snprintf(out, out_size, "Cloth Hands");
            return;
        case EQUIP_SLOT_CLOTHING_WAIST:
            snprintf(out, out_size, "Cloth Waist");
            return;
        case EQUIP_SLOT_CLOTHING_LEGS:
            snprintf(out, out_size, "Cloth Legs");
            return;
        case EQUIP_SLOT_CLOTHING_FEET:
            snprintf(out, out_size, "Cloth Feet");
            return;
        case EQUIP_SLOT_ACCESSORY_HEAD:
            snprintf(out, out_size, "Accessory Head");
            return;
        case EQUIP_SLOT_ACCESSORY_EYES:
            snprintf(out, out_size, "Accessory Eyes");
            return;
        case EQUIP_SLOT_ACCESSORY_FACE:
            snprintf(out, out_size, "Accessory Face");
            return;
        case EQUIP_SLOT_ACCESSORY_NECK:
            snprintf(out, out_size, "Accessory Neck");
            return;
        case EQUIP_SLOT_ACCESSORY_WRIST_RIGHT:
            snprintf(out, out_size, "Right Wrist");
            return;
        case EQUIP_SLOT_ACCESSORY_WRIST_LEFT:
            snprintf(out, out_size, "Left Wrist");
            return;
        case EQUIP_SLOT_ACCESSORY_FINGER_RIGHT:
            snprintf(out, out_size, "Right Finger");
            return;
        case EQUIP_SLOT_ACCESSORY_FINGER_LEFT:
            snprintf(out, out_size, "Left Finger");
            return;
        case EQUIP_SLOT_ACCESSORY_TRINKET_1:
            snprintf(out, out_size, "Trinket 1");
            return;
        case EQUIP_SLOT_ACCESSORY_TRINKET_2:
            snprintf(out, out_size, "Trinket 2");
            return;
        case EQUIP_SLOT_CONTAINER_BACKPACK:
            snprintf(out, out_size, "Backpack");
            return;
        case EQUIP_SLOT_CONTAINER_POUCH:
            snprintf(out, out_size, "Pouch");
            return;
        case EQUIP_SLOT_CONTAINER_QUIVER:
            snprintf(out, out_size, "Quiver");
            return;
        case EQUIP_SLOT_NONE:
            if(slot_index >= inventory_first_slot_index())
                snprintf(out, out_size, "Slot %02d", (slot_index - inventory_first_slot_index()) + 1);
            else
                snprintf(out, out_size, "Inventory");
            return;
        default:
            snprintf(out, out_size, "Slot %d", slot_index);
            return;
    }
}

static char inventory_row_prefix(int slot_kind)
{
    switch(slot_kind)
    {
        case 1:
            return 'I';
        case 2:
            return 'B';
        case 0:
        default:
            return 'E';
    }
}

static void inventory_format_row_text(char* out, size_t out_size, const EquipmentSlot* slot, int slot_index, int slot_kind)
{
    char label[32];
    char prefix;
    const char* lead;
    int shown_quantity;

    if(!out || out_size == 0)
        return;

    inventory_format_slot_label(label, sizeof(label), slot, slot_index);
    prefix = inventory_row_prefix(slot_kind);
    lead = (slot_kind == 1) ? "  " : "";

    if(!slot)
    {
        snprintf(out, out_size, "%s[%c] %-18s: (invalid)", lead, prefix, label);
        return;
    }

    shown_quantity = (slot->item.quantity > 0) ? slot->item.quantity : 1;

    if(slot->item.type == ITEM_TYPE_NONE || slot->item.name[0] == '\0')
        snprintf(out, out_size, "%s[%c] %-18s: (empty)", lead, prefix, label);
    else
        snprintf(out, out_size, "%s[%c] %-18s: %-20s x%d", lead, prefix, label, slot->item.name, shown_quantity);
}

static int inventory_append_header_row(int total_slots, int* slot_indices, int* slot_types, char row_labels[256][96], const char* label)
{
    if(total_slots < 0 || total_slots >= 256)
        return total_slots;

    slot_indices[total_slots] = -1;
    slot_types[total_slots] = -1;
    snprintf(row_labels[total_slots], sizeof(row_labels[total_slots]), "%s", label ? label : "");
    return total_slots + 1;
}

static int inventory_append_slot_row(int total_slots, int* slot_indices, int* slot_types, int slot_index, int slot_kind)
{
    if(total_slots < 0 || total_slots >= 256)
        return total_slots;

    slot_indices[total_slots] = slot_index;
    slot_types[total_slots] = slot_kind;
    return total_slots + 1;
}

static int inventory_equipped_container_capacity(const Character* c, int slot_index)
{
    const EquipmentSlot* slot;

    if(!c || slot_index < 0 || slot_index >= c->equipment_slot_count)
        return 0;

    slot = &c->equipment_slots[slot_index];
    if(!inventory_slot_is_container_type(slot->slot_type))
        return 0;
    if(slot->item.type == ITEM_TYPE_NONE || !slot->item.is_container || slot->item.container_capacity <= 0)
        return 0;

    return slot->item.container_capacity;
}

static int inventory_append_inventory_rows(Character* c,
                                           int total_slots,
                                           int* slot_indices,
                                           int* slot_types,
                                           int* next_inventory_offset,
                                           int row_count)
{
    int inventory_start = inventory_first_slot_index();

    if(!c || !slot_indices || !slot_types || !next_inventory_offset || row_count <= 0)
        return total_slots;

    for(int i = 0; i < row_count; ++i)
    {
        int inventory_index = inventory_start + *next_inventory_offset;
        if(inventory_index >= c->equipment_slot_count)
            break;

        total_slots = inventory_append_slot_row(total_slots, slot_indices, slot_types, inventory_index, 1);
        (*next_inventory_offset)++;
    }

    return total_slots;
}

static int inventory_build_rows_for_tab(const Character* c,
                                        InventoryOverlayTab current_tab,
                                        int* slot_indices,
                                        int* slot_types,
                                        char row_labels[256][96])
{
    int total_slots = 0;
    char header[96];

    if(!c || !slot_indices || !slot_types || !row_labels)
        return 0;

    if(current_tab == INVENTORY_TAB_LOADOUT)
    {
        int loadout_total = 0;
        int loadout_filled = 0;
        int bag_total = 0;
        int bag_filled = 0;
        int inventory_offset = 0;

        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            const EquipmentSlot* slot = &c->equipment_slots[i];
            if(slot->slot_type == EQUIP_SLOT_MAIN_HAND || slot->slot_type == EQUIP_SLOT_OFF_HAND)
            {
                loadout_total++;
                if(slot->item.type != ITEM_TYPE_NONE)
                    loadout_filled++;
            }
            else if(inventory_slot_is_container_type(slot->slot_type))
            {
                bag_total++;
                if(slot->item.type != ITEM_TYPE_NONE)
                    bag_filled++;
            }
        }

        snprintf(header, sizeof(header), "-- Hands (%d/%d ready) --", loadout_filled, loadout_total);
        total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            EquipmentSlotType slot_type = c->equipment_slots[i].slot_type;
            if(slot_type == EQUIP_SLOT_MAIN_HAND || slot_type == EQUIP_SLOT_OFF_HAND)
                total_slots = inventory_append_slot_row(total_slots, slot_indices, slot_types, i, 0);
        }

        snprintf(header, sizeof(header), "-- Bags (%d/%d equipped) --", bag_filled, bag_total);
        total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            int bag_capacity;
            int remaining_slots;
            const EquipmentSlot* slot = &c->equipment_slots[i];

            if(!inventory_slot_is_container_type(slot->slot_type))
                continue;

            total_slots = inventory_append_slot_row(total_slots, slot_indices, slot_types, i, 2);

            bag_capacity = inventory_equipped_container_capacity(c, i);
            remaining_slots = c->inventory_slot_count - inventory_offset;
            if(bag_capacity > remaining_slots)
                bag_capacity = remaining_slots;
            total_slots = inventory_append_inventory_rows((Character*)c,
                                                         total_slots,
                                                         slot_indices,
                                                         slot_types,
                                                         &inventory_offset,
                                                         bag_capacity);
        }

        if(inventory_offset < c->inventory_slot_count)
        {
            int loose_total = c->inventory_slot_count - inventory_offset;
            int loose_filled = 0;
            int inventory_start = inventory_first_slot_index() + inventory_offset;

            for(int i = inventory_start; i < c->equipment_slot_count; ++i)
            {
                const EquipmentSlot* slot = &c->equipment_slots[i];
                if(slot->slot_type == EQUIP_SLOT_NONE && slot->item.type != ITEM_TYPE_NONE)
                    loose_filled++;
            }

            snprintf(header, sizeof(header), "-- On Person (%d/%d used) --", loose_filled, loose_total);
            total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
            total_slots = inventory_append_inventory_rows((Character*)c,
                                                         total_slots,
                                                         slot_indices,
                                                         slot_types,
                                                         &inventory_offset,
                                                         loose_total);
        }

        return total_slots;
    }

    if(current_tab == INVENTORY_TAB_CLOTHING)
    {
        int clothing_total = 0;
        int clothing_filled = 0;

        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            const EquipmentSlot* slot = &c->equipment_slots[i];
            if(!inventory_slot_is_clothing(slot->slot_type))
                continue;
            clothing_total++;
            if(slot->item.type != ITEM_TYPE_NONE)
                clothing_filled++;
        }

        snprintf(header, sizeof(header), "-- Clothing (%d/%d worn) --", clothing_filled, clothing_total);
        total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            if(inventory_slot_is_clothing(c->equipment_slots[i].slot_type))
                total_slots = inventory_append_slot_row(total_slots, slot_indices, slot_types, i, 0);
        }
        return total_slots;
    }

    if(current_tab == INVENTORY_TAB_ARMOR)
    {
        int armor_total = 0;
        int armor_filled = 0;

        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            const EquipmentSlot* slot = &c->equipment_slots[i];
            if(!inventory_slot_is_armor(slot->slot_type))
                continue;
            armor_total++;
            if(slot->item.type != ITEM_TYPE_NONE)
                armor_filled++;
        }

        snprintf(header, sizeof(header), "-- Armor (%d/%d equipped) --", armor_filled, armor_total);
        total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            if(inventory_slot_is_armor(c->equipment_slots[i].slot_type))
                total_slots = inventory_append_slot_row(total_slots, slot_indices, slot_types, i, 0);
        }
        return total_slots;
    }

    {
        int accessory_total = 0;
        int accessory_filled = 0;

        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            const EquipmentSlot* slot = &c->equipment_slots[i];
            if(!inventory_slot_is_accessory(slot->slot_type))
                continue;
            accessory_total++;
            if(slot->item.type != ITEM_TYPE_NONE)
                accessory_filled++;
        }

        snprintf(header, sizeof(header), "-- Accessories (%d/%d equipped) --", accessory_filled, accessory_total);
        total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            if(inventory_slot_is_accessory(c->equipment_slots[i].slot_type))
                total_slots = inventory_append_slot_row(total_slots, slot_indices, slot_types, i, 0);
        }
    }

    return total_slots;
}

// Run full inventory interaction overlay loop.
void inventory_menu(Character* c)
{
    if(!c) return;

    update_dynamic_container_slots(c);

    char status[192];
    int tab_scroll[INVENTORY_TAB_COUNT] = { 0 };
    int tab_selected[INVENTORY_TAB_COUNT] = { 0 };
    InventoryOverlayTab current_tab = INVENTORY_TAB_LOADOUT;

    snprintf(status, sizeof(status), "Enter: Action | A/D or 1-4: Tabs | W/S: Move | N: Unequip | Q: Exit");

    while (1) {
        int total_slots;
        int slot_indices[256];
        int slot_types[256];
        char row_labels[256][96];
        int overlay_content_lines;
        int visible_rows;
        int max_scroll;
        int selected;
        int scroll_offset;
        int row;
        char title[64];
        char tab_line[160];

        memset(slot_indices, 0, sizeof(slot_indices));
        memset(slot_types, 0, sizeof(slot_types));
        memset(row_labels, 0, sizeof(row_labels));

        total_slots = inventory_build_rows_for_tab(c, current_tab, slot_indices, slot_types, row_labels);
        if(total_slots <= 0)
        {
            total_slots = inventory_append_header_row(0, slot_indices, slot_types, row_labels, "-- Empty --");
        }

        selected = tab_selected[current_tab];
        scroll_offset = tab_scroll[current_tab];

        overlay_content_lines = ui_overlay_content_lines();
        visible_rows = (overlay_content_lines > 3) ? (overlay_content_lines - 3) : 1;
        max_scroll = total_slots - visible_rows;
        if (max_scroll < 0) max_scroll = 0;

        selected = inventory_adjust_selected_row(slot_types, total_slots, selected, 1);
        if (selected < 0) selected = 0;
        if (selected >= total_slots) selected = total_slots - 1;
        if (scroll_offset < 0) scroll_offset = 0;
        if (scroll_offset > max_scroll) scroll_offset = max_scroll;
        if (selected < scroll_offset) scroll_offset = selected;
        if (selected >= scroll_offset + visible_rows) scroll_offset = selected - visible_rows + 1;

        snprintf(title, sizeof(title), "Inventory - %s", inventory_tab_name(current_tab));
        ui_overlay_draw_frame(title);

        snprintf(tab_line,
                 sizeof(tab_line),
                 "Tabs: %c1.Loadout  %c2.Clothing  %c3.Armor  %c4.Accessories",
                 (current_tab == INVENTORY_TAB_LOADOUT) ? '*' : ' ',
                 (current_tab == INVENTORY_TAB_CLOTHING) ? '*' : ' ',
                 (current_tab == INVENTORY_TAB_ARMOR) ? '*' : ' ',
                 (current_tab == INVENTORY_TAB_ACCESSORIES) ? '*' : ' ');
        ui_overlay_draw_line(0, tab_line);

        row = 1;
        for (int i = 0; i < visible_rows && (i + scroll_offset) < total_slots; ++i) {
            int list_index = i + scroll_offset;
            int idx = slot_indices[list_index];
            int stype = slot_types[list_index];
            char line[128];

            if(inventory_row_is_header(stype)) {
                snprintf(line, sizeof(line), "%s", row_labels[list_index]);
                ui_overlay_draw_line(row++, line);
                continue;
            }

            inventory_format_row_text(line, sizeof(line), &c->equipment_slots[idx], idx, stype);

            if (list_index == selected) {
                char sel_line[132];
                snprintf(sel_line, sizeof(sel_line), "> %s", line);
                ui_overlay_draw_line(row++, sel_line);
            } else {
                ui_overlay_draw_line(row++, line);
            }
        }

        for(; row < overlay_content_lines - 1; ++row)
            ui_overlay_draw_line(row, "");
        ui_overlay_draw_line(row, status);

        tab_selected[current_tab] = selected;
        tab_scroll[current_tab] = scroll_offset;

        {
            int cmd = read_input_key();
            if (cmd == 'q' || cmd == 'Q') break;

            if (cmd == INPUT_KEY_LEFT || cmd == 'a' || cmd == 'A') {
                current_tab = (InventoryOverlayTab)((current_tab + INVENTORY_TAB_COUNT - 1) % INVENTORY_TAB_COUNT);
                continue;
            }
            if (cmd == INPUT_KEY_RIGHT || cmd == 'd' || cmd == 'D') {
                current_tab = (InventoryOverlayTab)((current_tab + 1) % INVENTORY_TAB_COUNT);
                continue;
            }
            if (cmd >= '1' && cmd <= '4') {
                current_tab = (InventoryOverlayTab)(cmd - '1');
                continue;
            }

            if (cmd == INPUT_KEY_UP || cmd == 'w' || cmd == 'W') {
                selected = inventory_adjust_selected_row(slot_types, total_slots, selected - 1, -1);
                if (selected < scroll_offset) scroll_offset = selected;
                tab_selected[current_tab] = selected;
                tab_scroll[current_tab] = scroll_offset;
                continue;
            }
            if (cmd == INPUT_KEY_DOWN || cmd == 's' || cmd == 'S') {
                selected = inventory_adjust_selected_row(slot_types, total_slots, selected + 1, 1);
                if (selected >= scroll_offset + visible_rows) scroll_offset = selected - visible_rows + 1;
                tab_selected[current_tab] = selected;
                tab_scroll[current_tab] = scroll_offset;
                continue;
            }
            if (cmd == INPUT_KEY_PGUP) {
                selected = inventory_adjust_selected_row(slot_types, total_slots, selected - visible_rows, -1);
                scroll_offset = selected;
                tab_selected[current_tab] = selected;
                tab_scroll[current_tab] = scroll_offset;
                continue;
            }
            if (cmd == INPUT_KEY_PGDN) {
                selected = inventory_adjust_selected_row(slot_types, total_slots, selected + visible_rows, 1);
                scroll_offset = selected - visible_rows + 1;
                if (scroll_offset < 0) scroll_offset = 0;
                tab_selected[current_tab] = selected;
                tab_scroll[current_tab] = scroll_offset;
                continue;
            }
            if (cmd == INPUT_KEY_HOME) {
                selected = inventory_adjust_selected_row(slot_types, total_slots, 0, 1);
                scroll_offset = 0;
                tab_selected[current_tab] = selected;
                tab_scroll[current_tab] = scroll_offset;
                continue;
            }
            if (cmd == INPUT_KEY_END) {
                selected = inventory_adjust_selected_row(slot_types, total_slots, total_slots - 1, -1);
                scroll_offset = max_scroll;
                tab_selected[current_tab] = selected;
                tab_scroll[current_tab] = scroll_offset;
                continue;
            }

            {
                OverlayType next_overlay;
                if (overlay_type_from_key(cmd, &next_overlay) && next_overlay != OVERLAY_TYPE_INVENTORY) {
                    overlay_request(next_overlay);
                    break;
                }
            }

            if(selected < 0 || selected >= total_slots || inventory_row_is_header(slot_types[selected]))
                continue;

            {
                int stype = slot_types[selected];
                int sidx = slot_indices[selected];
                if (cmd == 13) {
                    if (stype == 1) {
                        if (c->equipment_slots[sidx].item.type != ITEM_TYPE_NONE) {
                            char item_name[32];
                            Item* item = &c->equipment_slots[sidx].item;
                            snprintf(item_name, sizeof(item_name), "%s", item->name);

                            if (inventory_item_is_directly_usable(item)) {
                                if (inventory_use(c, sidx)) {
                                    snprintf(status, sizeof(status), "Used %s.", item_name);
                                } else {
                                    snprintf(status, sizeof(status), "Failed to use %s.", item_name);
                                }
                            } else if (item->is_ammo) {
                                snprintf(status, sizeof(status), "%s is ammo for ranged weapons.", item_name);
                            } else {
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
                    if ((stype == 0 || stype == 2) && c->equipment_slots[sidx].item.type != ITEM_TYPE_NONE) {
                        char item_name[32];
                        int dropped_to_world = 0;
                        snprintf(item_name, sizeof(item_name), "%s", c->equipment_slots[sidx].item.name);
                        if (inventory_unequip_slot_or_drop(c,
                                                           c->equipment_slots[sidx].slot_type,
                                                           current_area ? current_area->name : NULL,
                                                           c->actor.entity.x,
                                                           c->actor.entity.y,
                                                           c->actor.entity.z,
                                                           &dropped_to_world)) {
                            if(dropped_to_world)
                                snprintf(status, sizeof(status), "Inventory full - dropped %s on the ground.", item_name);
                            else
                                snprintf(status, sizeof(status), "Unequipped %s.", item_name);
                        } else {
                            snprintf(status, sizeof(status), "Failed to unequip %s.", item_name);
                        }
                    } else {
                        snprintf(status, sizeof(status), "Nothing equipped on this row.");
                    }
                    continue;
                }
            }
        }
    }
}
// END inventory_menu







