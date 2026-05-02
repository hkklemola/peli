#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "combat.h"
#include "character.h"
#include "inventory.h"
#include "input.h"
#include "keybind_helpers.h"
#include "item_data.h"
#include "item.h"
#include "log.h"
#include "map.h"
#include "overlay_nav.h"
#include "player.h"
#include "ui_overlay.h"
#include "world_items.h"
#include "crafting_compendium.h"

#define INVENTORY_ROW_KIND_EQUIP 0
#define INVENTORY_ROW_KIND_INVENTORY 1
#define INVENTORY_ROW_KIND_CONTAINER 2
#define INVENTORY_ROW_KIND_SHEATHED 3

// Forward declarations
void update_dynamic_container_slots(Character* c);
EquipmentSlotType equipment_slot_for_item_type(ItemType type);
static int inventory_row_is_header(int slot_type);
static int item_type_is_armour_piece(ItemType type);
static int inventory_slot_is_scabbard_type(EquipmentSlotType slot_type);
static int inventory_slot_is_container_type(EquipmentSlotType slot_type);
static int inventory_add_to_equipped_containers(Character* c, const Item* item);
static int inventory_take_one_item_from_container(WorldContainer* container, int container_slot, Item* out_item);
static int inventory_use_item_directly(Character* c, const Item* item);
static int inventory_item_is_directly_usable(const Item* item);
static int inventory_read_item(Character* c, const Item* item);
static int inventory_collect_equipped_container_deposit_candidates(const Character* c,
                                                                  const WorldContainer* target_container,
                                                                  const Item* target_item,
                                                                  WorldContainer** out_sources,
                                                                  int* out_source_slots,
                                                                  int max_candidates);
static int inventory_item_accepted_by_container(const Item* container, const Item* item);

// --- Inspect panel helpers ---

// Returns pointer to EquipmentSlot for the selected row, or NULL if not an item row
static const EquipmentSlot* inventory_selected_slot(const Character* c, int selected, int* slot_indices, int* slot_types, int total_slots) {
    if (!c || selected < 0 || selected >= total_slots) return NULL;
    int idx = slot_indices[selected];
    int stype = slot_types[selected];
    if (inventory_row_is_header(stype) || idx < 0 || idx >= c->equipment_slot_count) return NULL;
    return &c->equipment_slots[idx];
}

static const char* inventory_item_type_label(ItemType type)
{
    switch(type)
    {
        case ITEM_TYPE_CONSUMABLE: return "Consumable";
        case ITEM_TYPE_BOOK: return "Book";
        case ITEM_TYPE_SCROLL: return "Scroll";
        case ITEM_TYPE_WEAPON_MAIN_HAND:
        case ITEM_TYPE_WEAPON_OFF_HAND:
        case ITEM_TYPE_WEAPON_ONE_HANDED:
        case ITEM_TYPE_WEAPON_VERSATILE:
        case ITEM_TYPE_WEAPON_TWO_HANDED:
            return "Weapon";
        case ITEM_TYPE_TOOL_ONE_HANDED:
        case ITEM_TYPE_TOOL_TWO_HANDED:
            return "Tool";
        case ITEM_TYPE_ARMOUR_HEAD:
        case ITEM_TYPE_ARMOUR_EYES:
        case ITEM_TYPE_ARMOUR_FACE:
        case ITEM_TYPE_ARMOUR_NECK:
        case ITEM_TYPE_ARMOUR_SHOULDERS:
        case ITEM_TYPE_ARMOUR_CLOAK:
        case ITEM_TYPE_ARMOUR_CHEST:
        case ITEM_TYPE_ARMOUR_WAIST:
        case ITEM_TYPE_ARMOUR_ARMS:
        case ITEM_TYPE_ARMOUR_HANDS:
        case ITEM_TYPE_ARMOUR_LEGS:
        case ITEM_TYPE_ARMOUR_FEET:
        case ITEM_TYPE_ARMOUR_BOOTS:
            return "Armour";
        case ITEM_TYPE_CLOTHING_HEAD:
        case ITEM_TYPE_CLOTHING_EYES:
        case ITEM_TYPE_CLOTHING_FACE:
        case ITEM_TYPE_CLOTHING_NECK:
        case ITEM_TYPE_CLOTHING_SHOULDERS:
        case ITEM_TYPE_CLOTHING_CHEST:
        case ITEM_TYPE_CLOTHING_ARMS:
        case ITEM_TYPE_CLOTHING_HANDS:
        case ITEM_TYPE_CLOTHING_WAIST:
        case ITEM_TYPE_CLOTHING_LEGS:
        case ITEM_TYPE_CLOTHING_FEET:
            return "Clothing";
        case ITEM_TYPE_ACCESSORY_HEAD:
        case ITEM_TYPE_ACCESSORY_EYES:
        case ITEM_TYPE_ACCESSORY_FACE:
        case ITEM_TYPE_ACCESSORY_NECK:
        case ITEM_TYPE_ACCESSORY_TRINKET:
        case ITEM_TYPE_ACCESSORY_FINGER:
        case ITEM_TYPE_ACCESSORY_WRIST:
            return "Accessory";
        case ITEM_TYPE_CONTAINER_BACKPACK:
        case ITEM_TYPE_CONTAINER_POUCH:
        case ITEM_TYPE_CONTAINER_QUIVER:
            return "Container";
        case ITEM_TYPE_KEY: return "Key Item";
        case ITEM_TYPE_MATERIAL: return "Material";
        default: return "Misc";
    }
}

static const char* inventory_material_type_name(MaterialType material)
{
    switch(material)
    {
        case MATERIAL_TYPE_METAL: return "Metal";
        case MATERIAL_TYPE_WOOD: return "Wood";
        case MATERIAL_TYPE_GEMSTONE: return "Gemstone";
        case MATERIAL_TYPE_LEATHER: return "Leather";
        case MATERIAL_TYPE_CLOTH: return "Cloth";
        case MATERIAL_TYPE_MINERAL: return "Mineral";
        default: return "Unknown";
    }
}

// Fills out an array of detail lines for the inspect panel for the given item
#define INSPECT_PANEL_LINES 8
static void inventory_format_inspect_panel(const Item* item, char lines[INSPECT_PANEL_LINES][64]) {
    for (int i = 0; i < INSPECT_PANEL_LINES; ++i) lines[i][0] = '\0';
    if (!item || item->type == ITEM_TYPE_NONE || item->name[0] == '\0') {
        snprintf(lines[0], 64, "No item selected.");
        return;
    }

    int l = 0;
    char buf[64];

    snprintf(lines[l++], 64, "Item Info:");
    item_format_display_name(item, buf, sizeof(buf));
    snprintf(lines[l++], 64, "%s", buf);
    snprintf(lines[l++], 64, "Type: %s", inventory_item_type_label(item->type));
    snprintf(lines[l++], 64, "Quality: %s", item_quality_name(item->quality));
    snprintf(lines[l++], 64, "Quantity: %d", item->stackable ? item->quantity : 1);

    if (item->is_material) {
        snprintf(lines[l++], 64, "Material: %s", inventory_material_type_name(item->material_type));
    }

    if (item_type_is_weapon(item->type) || item_is_ranged_weapon(item)) {
        if (item->damage_min > 0 || item->damage_max > 0)
            snprintf(lines[l++], 64, "Weapon Dmg: %d-%d", item->damage_min, item->damage_max);
        else
            snprintf(lines[l++], 64, "Weapon Power: %d", item->power);
    } else if (item_type_is_armour_piece(item->type)) {
        snprintf(lines[l++], 64, "Armour: %d", item->power);
    } else if (item_is_tool(item)) {
        snprintf(lines[l++], 64, "Tool: %s", item->tool_type[0] ? item->tool_type : "Generic");
    } else if (item->is_readable) {
        snprintf(lines[l++], 64, "Readable: %s", item->book_flavor[0] ? item->book_flavor : "Text" );
    }

    if (item->is_container) {
        snprintf(lines[l++], 64, "Capacity: %d", item->container_capacity);
        if (item->container_world_index >= 0) {
            WorldContainer* container = world_container_for_item(item);
            if (container)
                snprintf(lines[l++], 64, "Stored: %d/%d", container->item_count, item->container_capacity);
        }
    }

    if (item->scabbard_capacity > 0) {
        snprintf(lines[l++], 64, "Scabbard: %d", item->scabbard_capacity);
    }

    if (item->is_ammo) {
        if (item->ammo_item_name[0])
            snprintf(lines[l++], 64, "Ammo for: %s", item->ammo_item_name);
        else
            snprintf(lines[l++], 64, "Ammo stack: %d", item->quantity);
    }
}

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

static int item_type_is_armour_piece(ItemType type)
{
    switch(type)
    {
        case ITEM_TYPE_ARMOUR_HEAD:
        case ITEM_TYPE_ARMOUR_EYES:
        case ITEM_TYPE_ARMOUR_FACE:
        case ITEM_TYPE_ARMOUR_NECK:
        case ITEM_TYPE_ARMOUR_SHOULDERS:
        case ITEM_TYPE_ARMOUR_CLOAK:
        case ITEM_TYPE_ARMOUR_CHEST:
        case ITEM_TYPE_ARMOUR_WAIST:
        case ITEM_TYPE_ARMOUR_ARMS:
        case ITEM_TYPE_ARMOUR_HANDS:
        case ITEM_TYPE_ARMOUR_LEGS:
        case ITEM_TYPE_ARMOUR_FEET:
        case ITEM_TYPE_ARMOUR_BOOTS:
            return 1;
        default:
            return 0;
    }
}

static int item_type_is_clothing_piece(ItemType type)
{
    switch(type)
    {
        case ITEM_TYPE_CLOTHING_HEAD:
        case ITEM_TYPE_CLOTHING_EYES:
        case ITEM_TYPE_CLOTHING_FACE:
        case ITEM_TYPE_CLOTHING_NECK:
        case ITEM_TYPE_CLOTHING_SHOULDERS:
        case ITEM_TYPE_CLOTHING_CHEST:
        case ITEM_TYPE_CLOTHING_WAIST:
        case ITEM_TYPE_CLOTHING_ARMS:
        case ITEM_TYPE_CLOTHING_HANDS:
        case ITEM_TYPE_CLOTHING_LEGS:
        case ITEM_TYPE_CLOTHING_FEET:
            return 1;
        default:
            return 0;
    }
}

static int inventory_get_item_body_parts(ItemType type, ActorBodyPart parts[4])
{
    int count = 0;
    if(!parts)
        return 0;

    switch(type)
    {
        case ITEM_TYPE_ARMOUR_HEAD:
        case ITEM_TYPE_CLOTHING_HEAD:
            parts[count++] = ACTOR_BODY_PART_HEAD;
            break;
        case ITEM_TYPE_ARMOUR_EYES:
        case ITEM_TYPE_CLOTHING_EYES:
            parts[count++] = ACTOR_BODY_PART_LEFT_EYE;
            parts[count++] = ACTOR_BODY_PART_RIGHT_EYE;
            break;
        case ITEM_TYPE_ARMOUR_FACE:
        case ITEM_TYPE_CLOTHING_FACE:
            parts[count++] = ACTOR_BODY_PART_FACE;
            break;
        case ITEM_TYPE_ARMOUR_NECK:
        case ITEM_TYPE_CLOTHING_NECK:
            parts[count++] = ACTOR_BODY_PART_NECK;
            break;
        case ITEM_TYPE_ARMOUR_SHOULDERS:
        case ITEM_TYPE_ARMOUR_CLOAK:
        case ITEM_TYPE_CLOTHING_SHOULDERS:
            parts[count++] = ACTOR_BODY_PART_TORSO;
            parts[count++] = ACTOR_BODY_PART_LEFT_ARM;
            parts[count++] = ACTOR_BODY_PART_RIGHT_ARM;
            break;
        case ITEM_TYPE_ARMOUR_CHEST:
        case ITEM_TYPE_CLOTHING_CHEST:
            parts[count++] = ACTOR_BODY_PART_TORSO;
            break;
        case ITEM_TYPE_ARMOUR_WAIST:
        case ITEM_TYPE_CLOTHING_WAIST:
            parts[count++] = ACTOR_BODY_PART_TORSO;
            parts[count++] = ACTOR_BODY_PART_LEFT_LEG;
            parts[count++] = ACTOR_BODY_PART_RIGHT_LEG;
            break;
        case ITEM_TYPE_ARMOUR_ARMS:
        case ITEM_TYPE_CLOTHING_ARMS:
            parts[count++] = ACTOR_BODY_PART_LEFT_ARM;
            parts[count++] = ACTOR_BODY_PART_RIGHT_ARM;
            break;
        case ITEM_TYPE_ARMOUR_HANDS:
        case ITEM_TYPE_CLOTHING_HANDS:
            parts[count++] = ACTOR_BODY_PART_LEFT_HAND;
            parts[count++] = ACTOR_BODY_PART_RIGHT_HAND;
            break;
        case ITEM_TYPE_ARMOUR_LEGS:
        case ITEM_TYPE_CLOTHING_LEGS:
            parts[count++] = ACTOR_BODY_PART_LEFT_LEG;
            parts[count++] = ACTOR_BODY_PART_RIGHT_LEG;
            break;
        case ITEM_TYPE_ARMOUR_FEET:
        case ITEM_TYPE_ARMOUR_BOOTS:
        case ITEM_TYPE_CLOTHING_FEET:
            parts[count++] = ACTOR_BODY_PART_LEFT_FOOT;
            parts[count++] = ACTOR_BODY_PART_RIGHT_FOOT;
            break;
        default:
            break;
    }

    return count;
}

static void inventory_apply_equipped_item_stats(Character* c, const Item* item, int direction)
{
    int armour_delta;
    int hard_delta;
    int soft_delta;

    if(!c || !item || direction == 0)
        return;

    armour_delta = item_type_is_armour_piece(item->type) ? item->power : 0;
    hard_delta = item_type_is_armour_piece(item->type)
        ? ((item->hard_damage_reduction > 0) ? item->hard_damage_reduction : armour_delta)
        : 0;
    soft_delta = 0;
    if(item_type_is_armour_piece(item->type))
    {
        soft_delta = (item->soft_damage_reduction > 0) ? item->soft_damage_reduction : armour_delta;
    }
    else if(item_type_is_clothing_piece(item->type))
    {
        soft_delta = (item->soft_damage_reduction > 0) ? item->soft_damage_reduction : item->power;
    }

    if(armour_delta > 0)
    {
        c->actor.armour_rating += (armour_delta * direction);
        if(c->actor.armour_rating < 0)
            c->actor.armour_rating = 0;
    }

    if(hard_delta > 0)
    {
        c->actor.hard_damage_reduction += (hard_delta * direction);
        if(c->actor.hard_damage_reduction < 0)
            c->actor.hard_damage_reduction = 0;
    }

    if(soft_delta > 0)
    {
        c->actor.soft_damage_reduction += (soft_delta * direction);
        if(c->actor.soft_damage_reduction < 0)
            c->actor.soft_damage_reduction = 0;
    }

    {
        ActorBodyPart parts[4];
        int part_count = inventory_get_item_body_parts(item->type, parts);
        if(part_count > 0)
        {
            if(item_type_is_armour_piece(item->type))
            {
                if(hard_delta > 0)
                {
                    for(int i = 0; i < part_count; ++i)
                    {
                        ActorBodyPart part = parts[i];
                        c->actor.body_part_hard_damage_reduction[part] += (hard_delta * direction);
                        if(c->actor.body_part_hard_damage_reduction[part] < 0)
                            c->actor.body_part_hard_damage_reduction[part] = 0;
                    }
                }
            }
            else if(item_type_is_clothing_piece(item->type))
            {
                if(soft_delta > 0)
                {
                    for(int i = 0; i < part_count; ++i)
                    {
                        ActorBodyPart part = parts[i];
                        c->actor.body_part_soft_damage_reduction[part] += (soft_delta * direction);
                        if(c->actor.body_part_soft_damage_reduction[part] < 0)
                            c->actor.body_part_soft_damage_reduction[part] = 0;
                    }
                }
            }
        }
    }
}

void inventory_recompute_equipped_item_stats(Character* c)
{
    if(!c)
        return;

    c->actor.armour_rating = 0;
    c->actor.hard_damage_reduction = 0;
    c->actor.soft_damage_reduction = 0;
    memset(c->actor.body_part_hard_damage_reduction, 0, sizeof(c->actor.body_part_hard_damage_reduction));
    memset(c->actor.body_part_soft_damage_reduction, 0, sizeof(c->actor.body_part_soft_damage_reduction));

    for(int i = 0; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];

        if(slot->slot_type == EQUIP_SLOT_NONE)
            continue;
        if(slot->item.type == ITEM_TYPE_NONE)
            continue;

        inventory_apply_equipped_item_stats(c, &slot->item, 1);
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
        case ITEM_TYPE_TOOL_ONE_HANDED:
        case ITEM_TYPE_TOOL_TWO_HANDED:
            return slot_type == EQUIP_SLOT_MAIN_HAND || slot_type == EQUIP_SLOT_OFF_HAND;
        case ITEM_TYPE_ARMOUR_HEAD:
            return slot_type == EQUIP_SLOT_ARMOUR_HEAD;
        case ITEM_TYPE_ARMOUR_EYES:
            return slot_type == EQUIP_SLOT_ARMOUR_EYES;
        case ITEM_TYPE_ARMOUR_FACE:
            return slot_type == EQUIP_SLOT_ARMOUR_FACE;
        case ITEM_TYPE_ARMOUR_NECK:
            return slot_type == EQUIP_SLOT_ARMOUR_NECK;
        case ITEM_TYPE_ARMOUR_SHOULDERS:
        case ITEM_TYPE_ARMOUR_CLOAK:
            return slot_type == EQUIP_SLOT_ARMOUR_SHOULDERS;
        case ITEM_TYPE_ARMOUR_CHEST:
            return slot_type == EQUIP_SLOT_ARMOUR_CHEST;
        case ITEM_TYPE_ARMOUR_WAIST:
            return slot_type == EQUIP_SLOT_ARMOUR_WAIST;
        case ITEM_TYPE_ARMOUR_ARMS:
            return slot_type == EQUIP_SLOT_ARMOUR_ARMS;
        case ITEM_TYPE_ARMOUR_HANDS:
            return slot_type == EQUIP_SLOT_ARMOUR_HANDS;
        case ITEM_TYPE_ARMOUR_LEGS:
            return slot_type == EQUIP_SLOT_ARMOUR_LEGS;
        case ITEM_TYPE_ARMOUR_FEET:
        case ITEM_TYPE_ARMOUR_BOOTS:
            return slot_type == EQUIP_SLOT_ARMOUR_FEET;
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
        case ITEM_TYPE_CONTAINER_SCABBARD:
            return inventory_slot_is_scabbard_type(slot_type);
        default:
            return 0;
    }
}

static int slot_request_matches_candidate(EquipmentSlotType requested, EquipmentSlotType candidate)
{
    if(requested == candidate)
        return 1;

    if(inventory_slot_is_scabbard_type(requested))
    {
        return inventory_slot_is_scabbard_type(candidate);
    }

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

static int inventory_equipped_container_capacity(const Character* c, int slot_index);
static int inventory_equipped_scabbard_slot_count(const Character* c)
{
    int extra_slots = 0;

    if(!c)
        return 0;

    for(int i = 0; i < EQUIP_SLOT_COUNT && i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];

        if(slot->item.type == ITEM_TYPE_NONE)
            continue;
        if(inventory_slot_is_scabbard_type(slot->slot_type))
            continue;
        if(slot->item.scabbard_capacity > 0)
            extra_slots += slot->item.scabbard_capacity;
    }

    return extra_slots;
}

static int inventory_active_scabbard_slot_count(const Character* c)
{
    int extra_slots = inventory_equipped_scabbard_slot_count(c);
    return 1 + (extra_slots > 0 ? extra_slots : 0);
}

static int inventory_first_dynamic_inventory_slot_index(const Character* c)
{
    return inventory_first_slot_index();
}

static int inventory_capacity_from_equipped_containers(const Character* c)
{
    (void)c;
    return 0;
}

static int inventory_add_to_equipped_containers(Character* c, const Item* item)
{
    if(!c || !item || item->type == ITEM_TYPE_NONE)
        return 0;

    for(int i = 0; i < c->equipment_slot_count; ++i)
    {
        EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->item.type == ITEM_TYPE_NONE || !slot->item.is_container || slot->item.container_capacity <= 0)
            continue;
        if(!inventory_item_accepted_by_container(&slot->item, item))
            continue;

        if(slot->item.container_world_index < 0 || !world_container_for_item(&slot->item))
        {
            int index = world_container_spawn_personal(slot->item.name);
            if(index < 0)
                continue;
            slot->item.container_world_index = index;
        }

        WorldContainer* container = world_container_for_item(&slot->item);
        if(!container)
            continue;
        if(container->item_count >= slot->item.container_capacity)
            continue;

        Item stored_item = *item;
        stored_item.slot_type = EQUIP_SLOT_NONE;
        return world_container_add_item(world_container_index_of(container), &stored_item);
    }

    return 0;
}

static int inventory_equip_item_from_container(Character* c, WorldContainer* container, int container_slot)
{
    Item item;
    int equip_slot;

    if(!c || !container)
        return 0;
    if(container_slot < 0 || container_slot >= container->item_count)
        return 0;

    item = container->items[container_slot];
    if(item.type == ITEM_TYPE_NONE)
        return 0;

    equip_slot = find_first_empty_equip_slot(c, item.type);
    if(equip_slot < 0)
        return 0;

    if(!world_container_remove_item(world_container_index_of(container), container_slot, &item))
        return 0;

    c->equipment_slots[equip_slot].item = item;
    c->equipment_slots[equip_slot].item.slot_type = c->equipment_slots[equip_slot].slot_type;
    inventory_apply_equipped_item_stats(c, &c->equipment_slots[equip_slot].item, 1);
    return 1;
}

static int inventory_take_one_item_from_container(WorldContainer* container, int container_slot, Item* out_item)
{
    if(!container || !out_item)
        return 0;
    if(container_slot < 0 || container_slot >= container->item_count)
        return 0;

    *out_item = container->items[container_slot];
    if(out_item->stackable && out_item->quantity > 1)
    {
        container->items[container_slot].quantity -= 1;
        out_item->quantity = 1;
        return 1;
    }

    return world_container_remove_item(world_container_index_of(container), container_slot, out_item);
}

static int inventory_use_item_directly(Character* c, const Item* item)
{
    if(!c || !item)
        return 0;
    if(!inventory_item_is_directly_usable(item))
        return 0;

    if(item->type == ITEM_TYPE_BOOK || item->type == ITEM_TYPE_SCROLL)
        return inventory_read_item(c, item);

    char item_name[96];
    item_format_display_name(item, item_name, sizeof(item_name));
    log_add("You use %s.", item_name);
    return 1;
}

static int inventory_drop_item_from_container(Character* c, WorldContainer* container, int container_slot)
{
    Item item;

    if(!c || !container)
        return 0;
    if(container_slot < 0 || container_slot >= container->item_count)
        return 0;

    if(!world_container_remove_item(world_container_index_of(container), container_slot, &item))
        return 0;

    item.slot_type = EQUIP_SLOT_NONE;
    if(!current_area || !world_item_drop_3d(&item, current_area->name, player.character.actor.entity.x, player.character.actor.entity.y, player.character.actor.entity.z))
    {
        (void)world_container_add_item(world_container_index_of(container), &item);
        return 0;
    }

    return 1;
}

static int inventory_item_storage_flags(const Item* item)
{
    if(!item || item->type == ITEM_TYPE_NONE)
        return CONTAINER_ACCEPTS_MISC;

    if(item->is_ammo)
        return CONTAINER_ACCEPTS_AMMO;

    if(item_is_material(item))
        return CONTAINER_ACCEPTS_MATERIAL;

    if(item->type == ITEM_TYPE_CONSUMABLE)
        return CONTAINER_ACCEPTS_CONSUMABLE;

    if(item->type == ITEM_TYPE_KEY)
        return CONTAINER_ACCEPTS_KEY;

    if(item_type_is_weapon(item->type) || item_is_tool(item))
        return CONTAINER_ACCEPTS_WEAPON_TOOL;

    if(item->type >= ITEM_TYPE_ARMOUR_HEAD && item->type <= ITEM_TYPE_CONTAINER_SCABBARD)
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

static int inventory_item_is_weapon_or_tool(const Item* item)
{
    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;

    return item_type_is_weapon(item->type) || item_is_tool(item) || item_is_ranged_weapon(item);
}

static int inventory_item_accepted_by_container(const Item* container, const Item* item)
{
    if(!container || !item)
        return 0;
    if(!container->is_container || container->container_capacity <= 0)
        return 0;

    if(container->type == ITEM_TYPE_CONTAINER_SCABBARD)
        return inventory_item_is_weapon_or_tool(item);

    if(container->container_accepted_flags == CONTAINER_ACCEPTS_ALL)
        return 1;

    return (inventory_item_storage_flags(item) & container->container_accepted_flags) != 0;
}

static int inventory_find_empty_slot_in_range(Character* c, int start, int count)
{
    if(!c || count <= 0)
        return -1;

    for(int i = 0; i < count; ++i)
    {
        int slot_index = start + i;
        if(slot_index < inventory_first_dynamic_inventory_slot_index(c) || slot_index >= c->equipment_slot_count)
            break;

        if(c->equipment_slots[slot_index].slot_type == EQUIP_SLOT_NONE &&
           c->equipment_slots[slot_index].item.type == ITEM_TYPE_NONE)
            return slot_index;
    }

    return -1;
}

static int inventory_find_preferred_container_slot(Character* c, const Item* item, int require_specific_match)
{
    int inventory_start;

    (void)require_specific_match;

    if(!c || !item)
        return -1;

    inventory_start = inventory_first_dynamic_inventory_slot_index(c);
    for(int i = inventory_start; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->slot_type != EQUIP_SLOT_NONE)
            continue;
        if(slot->item.type != ITEM_TYPE_NONE)
            continue;
        return i;
    }

    return -1;
}

static int inventory_visible_carried_count(const Character* c)
{
    int count = 0;
    if(!c)
        return 0;

    for(int i = inventory_first_dynamic_inventory_slot_index(c); i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->slot_type != EQUIP_SLOT_NONE || slot->item.type == ITEM_TYPE_NONE)
            continue;
        count++;
    }

    return count;
}

static int inventory_slot_from_visible_carried_index(const Character* c, int visible_index)
{
    int count = 0;

    if(!c || visible_index < 0)
        return -1;

    for(int i = inventory_first_dynamic_inventory_slot_index(c); i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->slot_type != EQUIP_SLOT_NONE || slot->item.type == ITEM_TYPE_NONE)
            continue;
        if(count == visible_index)
            return i;
        count++;
    }

    return -1;
}

static int inventory_collect_equipped_container_deposit_candidates(const Character* c,
                                                                  const WorldContainer* target_container,
                                                                  const Item* target_item,
                                                                  WorldContainer** out_sources,
                                                                  int* out_source_slots,
                                                                  int max_candidates)
{
    int count = 0;

    if(!c || !target_container || !target_item || !out_sources || !out_source_slots || max_candidates <= 0)
        return 0;

    for(int i = 0; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->item.type == ITEM_TYPE_NONE || !slot->item.is_container || slot->item.container_capacity <= 0)
            continue;

        WorldContainer* source = world_container_for_item(&slot->item);
        if(!source || source == target_container)
            continue;

        for(int j = 0; j < source->item_count && count < max_candidates; ++j)
        {
            const Item* item = &source->items[j];
            if(item->type == ITEM_TYPE_NONE)
                continue;
            if(inventory_item_accepted_by_container(target_item, item))
            {
                out_sources[count] = source;
                out_source_slots[count] = j;
                count++;
            }
        }
    }

    return count;
}

static int inventory_deposit_to_personal_container(Character* c, WorldContainer* container, const Item* container_item)
{
    int selected = 0;
    int scroll_offset = 0;
    int deposited_any = 0;
    char title[96];

    if(!c || !container || !container->active || !container_item)
        return 0;

    if(container_item->container_capacity <= 0)
        return 0;

    snprintf(title, sizeof(title), "Deposit - %s", container->label);

    while(1)
    {
        WorldContainer* source_containers[256];
        int source_slots[256];
        int item_count = inventory_collect_equipped_container_deposit_candidates(c, container, container_item, source_containers, source_slots, 256);
        int content_lines;
        int status_line;
        int visible_rows;
        int max_scroll;
        int line_i = 0;
        int key;

        ui_overlay_draw_frame(title);
        ui_overlay_invalidate_cache();

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        visible_rows = status_line;
        max_scroll = item_count - visible_rows;
        if(max_scroll < 0)
            max_scroll = 0;

        if(item_count <= 0)
        {
            selected = 0;
            scroll_offset = 0;
            if(line_i < status_line) ui_overlay_draw_line(line_i++, "You have no items in equipped containers to deposit.");
            while(line_i < status_line) ui_overlay_draw_line(line_i++, "");
            ui_overlay_draw_line(status_line, "Esc/Q back | Enter none");
            ui_overlay_draw_global_hotkeys();
        }
        else
        {
            if(selected < 0) selected = 0;
            if(selected >= item_count) selected = item_count - 1;
            if(selected < scroll_offset)
                scroll_offset = selected;
            if(visible_rows > 0 && selected >= scroll_offset + visible_rows)
                scroll_offset = selected - visible_rows + 1;
            if(scroll_offset < 0)
                scroll_offset = 0;
            if(scroll_offset > max_scroll)
                scroll_offset = max_scroll;

            for(int visible_i = scroll_offset; visible_i < item_count && line_i < status_line; ++visible_i)
            {
                WorldContainer* source = source_containers[visible_i];
                int source_slot = source_slots[visible_i];
                const Item* item = &source->items[source_slot];
                char line[128];
                char display_name[96];
                int shown_quantity = (item->quantity > 0) ? item->quantity : 1;

                item_format_display_name(item, display_name, sizeof(display_name));
                snprintf(line,
                         sizeof(line),
                         "%c %2d. %-24s x%-3d (%s)",
                         (visible_i == selected) ? '>' : ' ',
                         visible_i + 1,
                         display_name,
                         shown_quantity,
                         source->label);
                ui_overlay_draw_line(line_i++, line);
            }

            while(line_i < status_line)
                ui_overlay_draw_line(line_i++, "");

            ui_overlay_draw_line(status_line, "Enter deposit | W/X move | PgUp/PgDn jump | Home/End | Esc/Q back");
            ui_overlay_draw_global_hotkeys();
        }

        key = read_input_key();

        if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
            return deposited_any;

        if(item_count <= 0)
            continue;

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            if(selected > 0) selected--;
            continue;
        }

        if(KEYBIND_DOWN(key))
        {
            if(selected < item_count - 1) selected++;
            continue;
        }

        if(key == INPUT_KEY_PGUP)
        {
            selected -= (visible_rows > 0) ? visible_rows : 1;
            if(selected < 0)
                selected = 0;
            continue;
        }

        if(key == INPUT_KEY_PGDN)
        {
            selected += (visible_rows > 0) ? visible_rows : 1;
            if(selected >= item_count)
                selected = item_count - 1;
            continue;
        }

        if(key == INPUT_KEY_HOME)
        {
            selected = 0;
            continue;
        }

        if(key == INPUT_KEY_END)
        {
            selected = item_count - 1;
            continue;
        }

        if(KEYBIND_SELECT(key))
        {
            if(selected < 0 || selected >= item_count)
                continue;

            WorldContainer* source = source_containers[selected];
            int source_slot = source_slots[selected];
            Item moved_item = source->items[source_slot];
            char moved_name[96];
            int accepted_quantity = moved_item.quantity > 0 ? moved_item.quantity : 1;
            Item rollback_item;

            if(moved_item.type == ITEM_TYPE_NONE)
                continue;

            item_format_display_name(&moved_item, moved_name, sizeof(moved_name));

            if(!inventory_item_accepted_by_container(container_item, &moved_item))
            {
                log_add("%s cannot be stored in %s.", moved_name, container->label);
                continue;
            }

            if(container->item_count + 1 > container_item->container_capacity)
            {
                log_add("%s cannot hold any more items.", container->label);
                continue;
            }

            if(!world_container_add_item(world_container_index_of(container), &moved_item))
            {
                log_add("%s cannot hold any more items.", container->label);
                continue;
            }

            if(source->items[source_slot].stackable && source->items[source_slot].quantity > accepted_quantity)
            {
                source->items[source_slot].quantity -= accepted_quantity;
            }
            else if(!world_container_remove_item(world_container_index_of(source), source_slot, &rollback_item))
            {
                Item remove_failure;
                (void)world_container_remove_item(world_container_index_of(container), container->item_count - 1, &remove_failure);
                log_add("Failed to move %s into %s.", moved_name, container->label);
                continue;
            }

            deposited_any = 1;
            log_add("You place %s into %s.", moved_name, container->label);
            continue;
        }
    }
}

static int inventory_open_personal_container(Character* c, WorldContainer* container, const Item* container_item)
{
    int selected = 0;
    int scroll_offset = 0;
    int took_any = 0;
    int need_redraw = 1;
    char title[96];

    if(!c || !container || !container->active || !container_item)
        return 0;

    snprintf(title, sizeof(title), "Container - %s", container->label);

    while(1)
    {
        int content_lines;
        int status_line;
        int visible_rows;
        int max_scroll;
        int line_i = 0;

        if(need_redraw)
        {
            ui_overlay_draw_frame(title);
            ui_overlay_invalidate_cache();
            need_redraw = 0;
        }

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        visible_rows = status_line;
        max_scroll = container->item_count - visible_rows;
        if(max_scroll < 0)
            max_scroll = 0;

        if(container->item_count <= 0)
        {
            selected = 0;
            scroll_offset = 0;
            if(line_i < status_line) ui_overlay_draw_line(line_i++, "This container is empty.");
            while(line_i < status_line) ui_overlay_draw_line(line_i++, "");
            ui_overlay_draw_line(status_line, "Esc/Q close | D deposit item | W/X move");
            ui_overlay_draw_global_hotkeys();
        }
        else
        {
            if(selected < 0) selected = 0;
            if(selected >= container->item_count) selected = container->item_count - 1;
            if(selected < scroll_offset)
                scroll_offset = selected;
            if(visible_rows > 0 && selected >= scroll_offset + visible_rows)
                scroll_offset = selected - visible_rows + 1;
            if(scroll_offset < 0)
                scroll_offset = 0;
            if(scroll_offset > max_scroll)
                scroll_offset = max_scroll;

            for(int i = scroll_offset; i < container->item_count && line_i < status_line; i++)
            {
                char line[128];
                char display_name[96];
                const Item* item = &container->items[i];
                int shown_quantity = (item->quantity > 0) ? item->quantity : 1;

                item_format_display_name(item, display_name, sizeof(display_name));
                snprintf(line, sizeof(line), "%c %2d. %-28s x%d",
                         (i == selected) ? '>' : ' ',
                         i + 1,
                         display_name,
                         shown_quantity);
                ui_overlay_draw_line(line_i++, line);
            }

            while(line_i < status_line)
                ui_overlay_draw_line(line_i++, "");

            ui_overlay_draw_line(status_line, "Esc/Q close | Enter/S equip/use | E equip | G drop | D deposit | W/X move | PgUp/PgDn jump | Home/End");
            ui_overlay_draw_global_hotkeys();
        }

        {
            int key = read_input_key();

            if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
                break;

            if(key == 'd' || key == 'D')
            {
                if(inventory_deposit_to_personal_container(c, container, container_item))
                {
                    took_any = 1;
                    need_redraw = 1;
                }
                continue;
            }

            if(key == 'e' || key == 'E')
            {
                if(container->item_count <= 0)
                    continue;

                Item equip_item = container->items[selected];
                char item_name[96];
                item_format_display_name(&equip_item, item_name, sizeof(item_name));
                if(inventory_equip_item_from_container(c, container, selected))
                {
                    log_add("You equip %s from %s.", item_name, container->label);
                    took_any = 1;
                    need_redraw = 1;
                    if(selected >= container->item_count)
                        selected = container->item_count - 1;
                    if(selected < 0)
                        selected = 0;
                }
                else
                {
                    log_add("You cannot equip %s.", item_name);
                }
                continue;
            }

            if(key == 'g' || key == 'G')
            {
                if(container->item_count <= 0)
                    continue;

                Item drop_item = container->items[selected];
                char item_name[96];
                item_format_display_name(&drop_item, item_name, sizeof(item_name));
                if(inventory_drop_item_from_container(c, container, selected))
                {
                    log_add("You drop %s from %s.", item_name, container->label);
                    took_any = 1;
                    need_redraw = 1;
                    if(selected >= container->item_count)
                        selected = container->item_count - 1;
                    if(selected < 0)
                        selected = 0;
                }
                else
                {
                    log_add("Unable to drop %s.", item_name);
                }
                continue;
            }

            if(container->item_count <= 0)
                continue;

            if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
            {
                if(selected > 0) selected--;
                continue;
            }

            if(KEYBIND_DOWN(key))
            {
                if(selected < container->item_count - 1) selected++;
                continue;
            }

            if(key == INPUT_KEY_PGUP)
            {
                selected -= (visible_rows > 0) ? visible_rows : 1;
                if(selected < 0)
                    selected = 0;
                continue;
            }

            if(key == INPUT_KEY_PGDN)
            {
                selected += (visible_rows > 0) ? visible_rows : 1;
                if(selected >= container->item_count)
                    selected = container->item_count - 1;
                continue;
            }

            if(key == INPUT_KEY_HOME)
            {
                selected = 0;
                continue;
            }

            if(key == INPUT_KEY_END)
            {
                selected = container->item_count - 1;
                continue;
            }

            if(KEYBIND_SELECT(key) || key == 's' || key == 'S')
            {
                Item container_item;
                Item picked_item;
                int handled_action = 0;

                if(container->item_count <= 0)
                    continue;

                container_item = container->items[selected];
                char item_name[96];
                item_format_display_name(&container_item, item_name, sizeof(item_name));

                if(inventory_item_is_directly_usable(&container_item))
                {
                    if(inventory_take_one_item_from_container(container, selected, &picked_item) &&
                       inventory_use_item_directly(c, &picked_item))
                    {
                        log_add("You use %s from %s.", item_name, container->label);
                        handled_action = 1;
                    }
                    else if(!world_container_add_item(world_container_index_of(container), &picked_item))
                    {
                        log_add("Unable to use %s from %s.", item_name, container->label);
                    }
                }
                else if(find_first_empty_equip_slot(c, container_item.type) >= 0)
                {
                    if(inventory_equip_item_from_container(c, container, selected))
                    {
                        log_add("You equip %s from %s.", item_name, container->label);
                        handled_action = 1;
                    }
                    else
                    {
                        log_add("You cannot equip %s.", item_name);
                    }
                }

                if(!handled_action)
                {
                    if(!world_container_remove_item(world_container_index_of(container), selected, &picked_item))
                        continue;

                    if(inventory_add(c, &picked_item))
                    {
                        log_add("You take %s from %s.", item_name, container->label);
                        handled_action = 1;
                    }
                    else
                    {
                        log_add("No space in inventory for %s.", item_name);
                        (void)world_container_add_item(world_container_index_of(container), &picked_item);
                        need_redraw = 1;
                    }
                }

                if(handled_action)
                {
                    took_any = 1;
                    need_redraw = 1;

                    if(selected >= container->item_count)
                        selected = container->item_count - 1;
                    if(selected < 0)
                        selected = 0;
                }
            }
        }
    }

    return took_any;
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
                                                        inventory_first_dynamic_inventory_slot_index(c),
                                                        c->equipment_slot_count - inventory_first_dynamic_inventory_slot_index(c));

    if(slot_index < 0)
        return 0;

    c->equipment_slots[slot_index].item = *item;
    c->equipment_slots[slot_index].item.slot_type = EQUIP_SLOT_NONE;
    return 1;
}

static int inventory_items_can_stack_together(const Item* existing, const Item* incoming)
{
    if(!existing || !incoming)
        return 0;
    if(!existing->stackable || !incoming->stackable)
        return 0;
    if(existing->type != incoming->type)
        return 0;
    if(existing->quality != incoming->quality)
        return 0;
    if(strcmp(existing->name, incoming->name) != 0)
        return 0;
    return 1;
}

static int inventory_matching_stack_space(const Character* c, const Item* incoming)
{
    int total_space = 0;

    if(!c || !incoming)
        return 0;

    for(int i = inventory_first_dynamic_inventory_slot_index(c); i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        const Item* existing = &slot->item;
        int stack_max;

        if(slot->slot_type != EQUIP_SLOT_NONE)
            continue;
        if(existing->type == ITEM_TYPE_NONE)
            continue;
        if(!inventory_items_can_stack_together(existing, incoming))
            continue;

        stack_max = existing->stack_max > 0 ? existing->stack_max : 99;
        if(existing->quantity < stack_max)
            total_space += stack_max - existing->quantity;
    }

    return total_space;
}

static int inventory_merge_into_matching_stacks(Character* c, const Item* incoming, int quantity)
{
    if(!c || !incoming || quantity <= 0)
        return quantity;

    for(int i = inventory_first_dynamic_inventory_slot_index(c); i < c->equipment_slot_count && quantity > 0; ++i)
    {
        EquipmentSlot* slot = &c->equipment_slots[i];
        Item* existing = &slot->item;
        int stack_max;
        int free_space;
        int moved;

        if(slot->slot_type != EQUIP_SLOT_NONE)
            continue;
        if(existing->type == ITEM_TYPE_NONE)
            continue;
        if(!inventory_items_can_stack_together(existing, incoming))
            continue;

        stack_max = existing->stack_max > 0 ? existing->stack_max : 99;
        free_space = stack_max - existing->quantity;
        if(free_space <= 0)
            continue;

        moved = (quantity < free_space) ? quantity : free_space;
        existing->quantity += moved;
        quantity -= moved;
    }

    return quantity;
}

// Add one item instance to inventory (slot_type == EQUIP_SLOT_NONE); returns 1 on success.
int inventory_add(Character* c, const Item* item) {
    int quantity;
    int stack_max;
    Item partial;

    if (!c || !item || item->type == ITEM_TYPE_NONE) return 0;

    if(c->inventory_slot_count == 0)
        return inventory_add_to_equipped_containers(c, item);

    quantity = item->quantity > 0 ? item->quantity : 1;
    if(item->stackable)
    {
        stack_max = item->stack_max > 0 ? item->stack_max : 99;
        quantity = inventory_merge_into_matching_stacks(c, item, quantity);

        while(quantity > 0)
        {
            partial = *item;
            partial.quantity = (quantity < stack_max) ? quantity : stack_max;
            if(!inventory_place_item_in_carried_slots(c, &partial))
                return 0;
            quantity -= partial.quantity;
        }

        return 1;
    }

    if(item->quantity > 1)
    {
        partial = *item;
        partial.quantity = 1;
        for(int i = 0; i < quantity; ++i)
        {
            if(!inventory_place_item_in_carried_slots(c, &partial))
                return 0;
        }
        return 1;
    }

    return inventory_place_item_in_carried_slots(c, item);
}

// Remove an item at slot index; returns 1 on success.
int inventory_remove(Character* c, int slot) {
    if (!c || slot < inventory_first_slot_index() || slot >= c->equipment_slot_count) return 0;
    if (c->equipment_slots[slot].item.type == ITEM_TYPE_NONE) return 0;
    clear_slot_item(&c->equipment_slots[slot]);
    return 1;
}

static int inventory_drop_inventory_item_to_world(Character* c,
                                                  int slot,
                                                  const char* area_name,
                                                  int x,
                                                  int y,
                                                  int z)
{
    Item dropped_item;

    if(!c || !area_name)
        return 0;
    if(slot < inventory_first_dynamic_inventory_slot_index(c) || slot >= c->equipment_slot_count)
        return 0;
    if(c->equipment_slots[slot].slot_type != EQUIP_SLOT_NONE)
        return 0;
    if(c->equipment_slots[slot].item.type == ITEM_TYPE_NONE)
        return 0;

    dropped_item = c->equipment_slots[slot].item;
    dropped_item.slot_type = EQUIP_SLOT_NONE;

    if(!world_item_drop_3d(&dropped_item, area_name, x, y, z))
        return 0;

    clear_slot_item(&c->equipment_slots[slot]);
    return 1;
}

static int inventory_item_is_directly_usable(const Item* item)
{
    if(!item)
        return 0;

    if(item->is_ammo)
        return 0;

    return item->type == ITEM_TYPE_CONSUMABLE ||
           item->type == ITEM_TYPE_BOOK ||
           item->type == ITEM_TYPE_SCROLL;
}

static int inventory_read_item(Character* c, const Item* item)
{
    int discovered_count = 0;
    int unlocked_recipe = 0;

    if(!c || !item || !item->is_readable)
        return 0;

    ui_overlay_show_mini_prompt(item->name,
                                item->book_flavor[0] ? item->book_flavor : "You read carefully.",
                                item->book_content[0] ? item->book_content : "The text is hard to interpret.");

    switch(item->book_content_type)
    {
        case BOOK_CONTENT_STORY:
            log_add("You read %s.", item->name);
            break;
        case BOOK_CONTENT_LOCATION:
            for(int i = 0; i < item->book_location_count; i++)
            {
                int destination;
                int knowledge;

                if(i >= ITEM_BOOK_MAX_LOCATIONS)
                    break;

                destination = item->book_location_index[i];
                if(destination < 0 || destination >= atlas_location_count)
                    continue;

                knowledge = item->book_location_knowledge[i];
                if(knowledge < LOCATION_KNOWLEDGE_AWARE || knowledge > LOCATION_KNOWLEDGE_VISITED)
                    knowledge = LOCATION_KNOWLEDGE_AWARE;

                if(atlas_get_knowledge(destination) < knowledge)
                    discovered_count++;

                atlas_upgrade_knowledge(destination, (LocationKnowledge)knowledge);
                if(item->book_location_hint[i][0] != '\0')
                    atlas_add_location_hint(destination, item->book_location_hint[i]);
            }

            if(discovered_count > 0)
                log_add("%s reveals %d new location clue%s.", item->name, discovered_count, discovered_count == 1 ? "" : "s");
            else
                log_add("%s contains familiar travel notes.", item->name);
            break;
        case BOOK_CONTENT_RECIPE:
            if(item->recipe_unlock_id[0] != '\0')
            {
                (void)crafting_compendium_register_recipe(item->recipe_unlock_id,
                                                          "Unknown",
                                                          NON_WEAPON_SKILL_COUNT,
                                                          0);
                (void)crafting_compendium_upgrade_tier(item->recipe_unlock_id, CRAFTING_DISCOVERY_RECORDED);
                if(item->book_content[0] != '\0')
                    (void)crafting_compendium_add_hint(item->recipe_unlock_id, item->book_content);

                unlocked_recipe = character_add_recipe_unlock(c, item->recipe_unlock_id);
                if(unlocked_recipe)
                    log_add("You learn the recipe: %s.", item->recipe_unlock_id);
                else
                    log_add("You study %s, but cannot remember more recipes right now.", item->name);
            }
            else
            {
                log_add("%s references techniques, but no concrete recipe is recorded.", item->name);
            }
            break;
        case BOOK_CONTENT_SKILL_REFERENCE:
            log_add("You review %s for practical guidance.", item->name);
            break;
        case BOOK_CONTENT_NONE:
        default:
            log_add("You read %s.", item->name);
            break;
    }

    return 1;
}

// Use item at slot index (consumables); returns 1 on success.
int inventory_use(Character* c, int slot) {
    if (!c || slot < inventory_first_slot_index() || slot >= c->equipment_slot_count) return 0;
    Item* item = &c->equipment_slots[slot].item;
    if (!inventory_item_is_directly_usable(item)) return 0;

    if(item->type == ITEM_TYPE_BOOK || item->type == ITEM_TYPE_SCROLL)
        return inventory_read_item(c, item);

    // TODO: Implement consumable effect logic (e.g., heal, buffs)
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
        case ITEM_TYPE_TOOL_ONE_HANDED:
        case ITEM_TYPE_TOOL_TWO_HANDED:
            return EQUIP_SLOT_MAIN_HAND;
        case ITEM_TYPE_WEAPON_OFF_HAND:
            return EQUIP_SLOT_OFF_HAND;
        case ITEM_TYPE_ARMOUR_HEAD:
            return EQUIP_SLOT_ARMOUR_HEAD;
        case ITEM_TYPE_ARMOUR_EYES:
            return EQUIP_SLOT_ARMOUR_EYES;
        case ITEM_TYPE_ARMOUR_FACE:
            return EQUIP_SLOT_ARMOUR_FACE;
        case ITEM_TYPE_ARMOUR_NECK:
            return EQUIP_SLOT_ARMOUR_NECK;
        case ITEM_TYPE_ARMOUR_SHOULDERS:
        case ITEM_TYPE_ARMOUR_CLOAK:
            return EQUIP_SLOT_ARMOUR_SHOULDERS;
        case ITEM_TYPE_ARMOUR_CHEST:
            return EQUIP_SLOT_ARMOUR_CHEST;
        case ITEM_TYPE_ARMOUR_ARMS:
            return EQUIP_SLOT_ARMOUR_ARMS;
        case ITEM_TYPE_ARMOUR_HANDS:
            return EQUIP_SLOT_ARMOUR_HANDS;
        case ITEM_TYPE_ARMOUR_WAIST:
            return EQUIP_SLOT_ARMOUR_WAIST;
        case ITEM_TYPE_ARMOUR_LEGS:
            return EQUIP_SLOT_ARMOUR_LEGS;
        case ITEM_TYPE_ARMOUR_FEET:
        case ITEM_TYPE_ARMOUR_BOOTS:
            return EQUIP_SLOT_ARMOUR_FEET;
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
            (c->equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_BACKPACK ||
             c->equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_POUCH ||
             c->equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_QUIVER ||
             inventory_slot_is_scabbard_type(c->equipment_slots[i].slot_type)) ? 1 : 0;
    }

    for (int i = inventory_first_slot_index(); i < MAX_EQUIPMENT_SLOTS; ++i) {
        c->equipment_slots[i].slot_type = EQUIP_SLOT_NONE;
        c->equipment_slots[i].item.type = ITEM_TYPE_NONE;
        c->equipment_slots[i].is_container_slot = 0;
    }

    c->inventory_slot_count = INVENTORY_SIZE;
    c->equipment_slot_count = inventory_first_dynamic_inventory_slot_index(c) + c->inventory_slot_count;
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

    inventory_start = inventory_first_dynamic_inventory_slot_index(c);

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

    int active_scabbards = inventory_active_scabbard_slot_count(c);

    for (int i = 0; i < EQUIP_SLOT_COUNT && i < MAX_EQUIPMENT_SLOTS; ++i) {
        if (c->equipment_slots[i].slot_type == EQUIP_SLOT_NONE)
            c->equipment_slots[i].slot_type = (EquipmentSlotType)i;
    }

    for (int i = 0; i < EQUIP_SLOT_COUNT && i < MAX_EQUIPMENT_SLOTS; ++i) {
        if(inventory_slot_is_scabbard_type(c->equipment_slots[i].slot_type))
        {
            int scabbard_index = c->equipment_slots[i].slot_type - EQUIP_SLOT_CONTAINER_SCABBARD + 1;
            if(scabbard_index > active_scabbards)
            {
                c->equipment_slots[i].slot_type = EQUIP_SLOT_NONE;
                c->equipment_slots[i].item.type = ITEM_TYPE_NONE;
            }
        }
        c->equipment_slots[i].is_container_slot =
            (c->equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_BACKPACK ||
             c->equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_POUCH ||
             c->equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_QUIVER ||
             inventory_slot_is_scabbard_type(c->equipment_slots[i].slot_type)) ? 1 : 0;
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

    for (int i = inventory_first_dynamic_inventory_slot_index(c); i < c->equipment_slot_count; ++i) {
        if (c->equipment_slots[i].slot_type == EQUIP_SLOT_NONE && c->equipment_slots[i].item.type == ITEM_TYPE_NONE) {
            inventory_index = i;
            break;
        }
    }

    Item unequipped_item = c->equipment_slots[equipped_index].item;
    inventory_apply_equipped_item_stats(c, &unequipped_item, -1);

    if (inventory_add_to_equipped_containers(c, &unequipped_item))
    {
        clear_slot_item(&c->equipment_slots[equipped_index]);
        update_dynamic_container_slots(c);
        return 1;
    }

    if (inventory_index >= 0)
    {
        c->equipment_slots[inventory_index].item = unequipped_item;
        c->equipment_slots[inventory_index].item.slot_type = EQUIP_SLOT_NONE;
        clear_slot_item(&c->equipment_slots[equipped_index]);
        update_dynamic_container_slots(c);
        return 1;
    }

    if(area_name && area_name[0])
    {
        Item dropped_item = unequipped_item;
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
    INVENTORY_TAB_ARMOUR,
    INVENTORY_TAB_ACCESSORIES,
    INVENTORY_TAB_CONTAINERS,
    INVENTORY_TAB_COUNT
} InventoryOverlayTab;

static const char* inventory_tab_name(InventoryOverlayTab tab)
{
    switch(tab)
    {
        case INVENTORY_TAB_CLOTHING:
            return "Clothing";
        case INVENTORY_TAB_ARMOUR:
            return "Armour";
        case INVENTORY_TAB_ACCESSORIES:
            return "Accessories";
        case INVENTORY_TAB_CONTAINERS:
            return "Containers";
        case INVENTORY_TAB_LOADOUT:
        default:
            return "Loadout";
    }
}

static int inventory_slot_is_scabbard_type(EquipmentSlotType slot_type)
{
    return slot_type == EQUIP_SLOT_CONTAINER_SCABBARD ||
           slot_type == EQUIP_SLOT_CONTAINER_SCABBARD_2 ||
           slot_type == EQUIP_SLOT_CONTAINER_SCABBARD_3 ||
           slot_type == EQUIP_SLOT_CONTAINER_SCABBARD_4 ||
           slot_type == EQUIP_SLOT_CONTAINER_SCABBARD_5 ||
           slot_type == EQUIP_SLOT_CONTAINER_SCABBARD_6;
}

static int inventory_slot_is_container_type(EquipmentSlotType slot_type)
{
    return slot_type == EQUIP_SLOT_CONTAINER_BACKPACK ||
           slot_type == EQUIP_SLOT_CONTAINER_POUCH ||
           slot_type == EQUIP_SLOT_CONTAINER_QUIVER ||
           inventory_slot_is_scabbard_type(slot_type);
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

static int inventory_slot_is_armour(EquipmentSlotType slot_type)
{
    switch(slot_type)
    {
        case EQUIP_SLOT_ARMOUR_HEAD:
        case EQUIP_SLOT_ARMOUR_EYES:
        case EQUIP_SLOT_ARMOUR_FACE:
        case EQUIP_SLOT_ARMOUR_NECK:
        case EQUIP_SLOT_ARMOUR_SHOULDERS:
        case EQUIP_SLOT_ARMOUR_CHEST:
        case EQUIP_SLOT_ARMOUR_ARMS:
        case EQUIP_SLOT_ARMOUR_HANDS:
        case EQUIP_SLOT_ARMOUR_WAIST:
        case EQUIP_SLOT_ARMOUR_LEGS:
        case EQUIP_SLOT_ARMOUR_FEET:
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
        case EQUIP_SLOT_ARMOUR_HEAD:
            snprintf(out, out_size, "Armour Head");
            return;
        case EQUIP_SLOT_ARMOUR_EYES:
            snprintf(out, out_size, "Armour Eyes");
            return;
        case EQUIP_SLOT_ARMOUR_FACE:
            snprintf(out, out_size, "Armour Face");
            return;
        case EQUIP_SLOT_ARMOUR_NECK:
            snprintf(out, out_size, "Armour Neck");
            return;
        case EQUIP_SLOT_ARMOUR_SHOULDERS:
            snprintf(out, out_size, "Armour Shoulders");
            return;
        case EQUIP_SLOT_ARMOUR_CHEST:
            snprintf(out, out_size, "Armour Chest");
            return;
        case EQUIP_SLOT_ARMOUR_ARMS:
            snprintf(out, out_size, "Armour Arms");
            return;
        case EQUIP_SLOT_ARMOUR_HANDS:
            snprintf(out, out_size, "Armour Hands");
            return;
        case EQUIP_SLOT_ARMOUR_WAIST:
            snprintf(out, out_size, "Armour Waist");
            return;
        case EQUIP_SLOT_ARMOUR_LEGS:
            snprintf(out, out_size, "Armour Legs");
            return;
        case EQUIP_SLOT_ARMOUR_FEET:
            snprintf(out, out_size, "Armour Feet");
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
        case EQUIP_SLOT_CONTAINER_SCABBARD:
            snprintf(out, out_size, "Scabbard 1");
            return;
        case EQUIP_SLOT_CONTAINER_SCABBARD_2:
            snprintf(out, out_size, "Scabbard 2");
            return;
        case EQUIP_SLOT_CONTAINER_SCABBARD_3:
            snprintf(out, out_size, "Scabbard 3");
            return;
        case EQUIP_SLOT_CONTAINER_SCABBARD_4:
            snprintf(out, out_size, "Scabbard 4");
            return;
        case EQUIP_SLOT_CONTAINER_SCABBARD_5:
            snprintf(out, out_size, "Scabbard 5");
            return;
        case EQUIP_SLOT_CONTAINER_SCABBARD_6:
            snprintf(out, out_size, "Scabbard 6");
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
        case INVENTORY_ROW_KIND_INVENTORY:
            return 'I';
        case INVENTORY_ROW_KIND_CONTAINER:
            return 'B';
        case INVENTORY_ROW_KIND_SHEATHED:
            return 'S';
        case INVENTORY_ROW_KIND_EQUIP:
        default:
            return 'E';
    }
}

static void inventory_format_row_text(char* out, size_t out_size, const EquipmentSlot* slot, int slot_index, int slot_kind)
{
    char label[32];
    char display_name[96];
    char prefix;
    const char* lead;
    int shown_quantity;

    if(!out || out_size == 0)
        return;

    if(slot_kind == INVENTORY_ROW_KIND_SHEATHED && slot && inventory_slot_is_scabbard_type(slot->slot_type))
    {
        int scabbard_index = slot->slot_type - EQUIP_SLOT_CONTAINER_SCABBARD + 1;
        snprintf(label, sizeof(label), "Sheathed %d", scabbard_index);
    }
    else
    {
        inventory_format_slot_label(label, sizeof(label), slot, slot_index);
    }

    prefix = inventory_row_prefix(slot_kind);
    lead = (slot_kind == INVENTORY_ROW_KIND_INVENTORY) ? "  " : "";

    if(!slot)
    {
        snprintf(out, out_size, "%s[%c] %-18s: (invalid)", lead, prefix, label);
        return;
    }

    const Item* display_item = &slot->item;
    Item temp_item;
    if(slot_kind == INVENTORY_ROW_KIND_SHEATHED && inventory_slot_is_scabbard_type(slot->slot_type))
    {
        WorldContainer* container = world_container_for_item(&slot->item);
        if(container && container->item_count > 0)
            display_item = &container->items[0];
        else
            display_item = NULL;
    }

    if(!display_item || display_item->type == ITEM_TYPE_NONE || display_item->name[0] == '\0')
        snprintf(out, out_size, "%s[%c] %-18s: (empty)", lead, prefix, label);
    else
    {
        shown_quantity = (display_item->quantity > 0) ? display_item->quantity : 1;
        item_format_display_name(display_item, display_name, sizeof(display_name));
        snprintf(out, out_size, "%s[%c] %-18s: %-20s x%d", lead, prefix, label, display_name, shown_quantity);
    }
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
    int capacity;

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
    int inventory_start = inventory_first_dynamic_inventory_slot_index(c);

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
        int sheathed_total = 0;
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
            else if(inventory_slot_is_scabbard_type(slot->slot_type))
            {
                if(slot->item.type != ITEM_TYPE_NONE) {
                    WorldContainer* container = world_container_for_item(&slot->item);
                    if(container && container->item_count > 0)
                        sheathed_total++;
                }
            }
        }

        snprintf(header, sizeof(header), "-- Hands (%d/%d ready) --", loadout_filled, loadout_total);
        total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            EquipmentSlotType slot_type = c->equipment_slots[i].slot_type;
            if(slot_type == EQUIP_SLOT_MAIN_HAND || slot_type == EQUIP_SLOT_OFF_HAND)
                total_slots = inventory_append_slot_row(total_slots, slot_indices, slot_types, i, INVENTORY_ROW_KIND_EQUIP);
        }

        if(sheathed_total > 0)
        {
            snprintf(header, sizeof(header), "-- Sheathed Weapons (%d) --", sheathed_total);
            total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
            for(int i = 0; i < c->equipment_slot_count; ++i)
            {
                const EquipmentSlot* slot = &c->equipment_slots[i];
                if(!inventory_slot_is_scabbard_type(slot->slot_type))
                    continue;
                if(slot->item.type == ITEM_TYPE_NONE)
                    continue;
                WorldContainer* container = world_container_for_item(&slot->item);
                if(!container || container->item_count <= 0)
                    continue;
                total_slots = inventory_append_slot_row(total_slots, slot_indices, slot_types, i, INVENTORY_ROW_KIND_SHEATHED);
            }
        }

        if(inventory_offset < c->inventory_slot_count)
        {
            int loose_total = c->inventory_slot_count - inventory_offset;
            int loose_filled = 0;
            int inventory_start = inventory_first_dynamic_inventory_slot_index(c) + inventory_offset;

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

    if(current_tab == INVENTORY_TAB_CONTAINERS)
    {
        int container_total = 0;
        int container_filled = 0;

        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            const EquipmentSlot* slot = &c->equipment_slots[i];
            if(!inventory_slot_is_container_type(slot->slot_type))
                continue;
            container_total++;
            if(slot->item.type != ITEM_TYPE_NONE)
                container_filled++;
        }

        snprintf(header, sizeof(header), "-- Containers (%d/%d equipped) --", container_filled, container_total);
        total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            const EquipmentSlot* slot = &c->equipment_slots[i];
            if(!inventory_slot_is_container_type(slot->slot_type))
                continue;

            total_slots = inventory_append_slot_row(total_slots, slot_indices, slot_types, i, INVENTORY_ROW_KIND_CONTAINER);
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

    if(current_tab == INVENTORY_TAB_ARMOUR)
    {
        int armour_total = 0;
        int armour_filled = 0;

        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            const EquipmentSlot* slot = &c->equipment_slots[i];
            if(!inventory_slot_is_armour(slot->slot_type))
                continue;
            armour_total++;
            if(slot->item.type != ITEM_TYPE_NONE)
                armour_filled++;
        }

        snprintf(header, sizeof(header), "-- Armour (%d/%d equipped) --", armour_filled, armour_total);
        total_slots = inventory_append_header_row(total_slots, slot_indices, slot_types, row_labels, header);
        for(int i = 0; i < c->equipment_slot_count; ++i)
        {
            if(inventory_slot_is_armour(c->equipment_slots[i].slot_type))
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

    snprintf(status, sizeof(status), "Enter: Action | A/D or 1-4: Tabs | W/S: Move | N: Unequip/Drop | X: Drop | Q: Exit");

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
                 "Tabs: %c1.Loadout  %c2.Clothing  %c3.Armour  %c4.Accessories  %c5.Containers",
                 (current_tab == INVENTORY_TAB_LOADOUT) ? '*' : ' ',
                 (current_tab == INVENTORY_TAB_CLOTHING) ? '*' : ' ',
                 (current_tab == INVENTORY_TAB_ARMOUR) ? '*' : ' ',
                 (current_tab == INVENTORY_TAB_ACCESSORIES) ? '*' : ' ',
                 (current_tab == INVENTORY_TAB_CONTAINERS) ? '*' : ' ');
        ui_overlay_draw_line(0, tab_line);


        // Inspect panel integration
        int overlay_width = ui_overlay_text_width();
        int min_left_width = 16; // Minimum width for inventory list column
        int min_right_width = 12; // Minimum width for item info panel
        int right_panel_width = overlay_width / 3;
        if (right_panel_width > 30)
            right_panel_width = 30;
        if (right_panel_width < min_right_width)
            right_panel_width = min_right_width;
        int left_panel_width = overlay_width - right_panel_width - 2; // 2 for gap
        if (overlay_width < right_panel_width + min_left_width + 2 || left_panel_width < min_left_width) {
            // Fallback: draw only inventory list as before
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
        } else {
            // Draw split panel: left inventory, right inspect
            // Precompute inspect panel lines for selected item
            char inspect_lines[INSPECT_PANEL_LINES][64];
            const EquipmentSlot* sel_slot = inventory_selected_slot(c, selected, slot_indices, slot_types, total_slots);
            if (sel_slot)
                inventory_format_inspect_panel(&sel_slot->item, inspect_lines);
            else
                for (int i = 0; i < INSPECT_PANEL_LINES; ++i) inspect_lines[i][0] = '\0';

            row = 1;
            for (int i = 0; i < visible_rows && (i + scroll_offset) < total_slots; ++i) {
                int list_index = i + scroll_offset;
                int idx = slot_indices[list_index];
                int stype = slot_types[list_index];
                char left[256];
                char right[128];
                char combined[512];
                int inspect_line_idx = i;

                if(inventory_row_is_header(stype)) {
                    snprintf(left, sizeof(left), "%s", row_labels[list_index]);
                    if (inspect_line_idx >= 0 && inspect_line_idx < INSPECT_PANEL_LINES)
                        snprintf(right, sizeof(right), "%s", inspect_lines[inspect_line_idx]);
                    else
                        right[0] = '\0';
                    snprintf(combined, sizeof(combined), "%-*.*s  %-*.*s", left_panel_width, left_panel_width, left, right_panel_width, right_panel_width, right);
                    ui_overlay_draw_line(row++, combined);
                    continue;
                }

                inventory_format_row_text(left, sizeof(left), &c->equipment_slots[idx], idx, stype);
                if (list_index == selected) {
                    char left_prefixed[256];
                    snprintf(left_prefixed, sizeof(left_prefixed), "> %s", left);
                    snprintf(left, sizeof(left), "%s", left_prefixed);
                }

                if (inspect_line_idx >= 0 && inspect_line_idx < INSPECT_PANEL_LINES) {
                    snprintf(right, sizeof(right), "%s", inspect_lines[inspect_line_idx]);
                } else {
                    right[0] = '\0';
                }
                snprintf(combined, sizeof(combined), "%-*.*s  %-*.*s", left_panel_width, left_panel_width, left, right_panel_width, right_panel_width, right);
                ui_overlay_draw_line(row++, combined);
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
            if (cmd >= '1' && cmd <= '0' + INVENTORY_TAB_COUNT) {
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
            if (cmd == INPUT_KEY_DOWN || KEYBIND_DOWN(cmd)) {
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
                    } else if (stype == 2) {
                        if (c->equipment_slots[sidx].item.type != ITEM_TYPE_NONE &&
                            c->equipment_slots[sidx].item.is_container &&
                            c->equipment_slots[sidx].item.container_capacity > 0) {
                            Item* item = &c->equipment_slots[sidx].item;
                            WorldContainer* container = world_container_for_item(item);
                            if(!container)
                            {
                                int index = world_container_spawn_personal(item->name);
                                if(index >= 0)
                                    item->container_world_index = index;
                                container = world_container_for_item(item);
                            }

                            if(container)
                                inventory_open_personal_container(c, container, item);
                            snprintf(status, sizeof(status), "Opened %s.", item->name);
                        } else {
                            snprintf(status, sizeof(status), "Nothing to open here.");
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
                    } else if (stype == 1 && c->equipment_slots[sidx].item.type != ITEM_TYPE_NONE) {
                        char item_name[32];
                        snprintf(item_name, sizeof(item_name), "%s", c->equipment_slots[sidx].item.name);

                        if (!current_area) {
                            snprintf(status, sizeof(status), "Cannot drop %s here.", item_name);
                        } else if (inventory_drop_inventory_item_to_world(c,
                                                                         sidx,
                                                                         current_area->name,
                                                                         c->actor.entity.x,
                                                                         c->actor.entity.y,
                                                                         c->actor.entity.z)) {
                            snprintf(status, sizeof(status), "Dropped %s.", item_name);
                        } else {
                            snprintf(status, sizeof(status), "Failed to drop %s.", item_name);
                        }
                    } else {
                        snprintf(status, sizeof(status), "No item on this row.");
                    }
                    continue;
                }

                if (cmd == 'x' || cmd == 'X') {
                    if (stype == 1 && c->equipment_slots[sidx].item.type != ITEM_TYPE_NONE) {
                        char item_name[32];
                        snprintf(item_name, sizeof(item_name), "%s", c->equipment_slots[sidx].item.name);

                        if (!current_area) {
                            snprintf(status, sizeof(status), "Cannot drop %s here.", item_name);
                        } else if (inventory_drop_inventory_item_to_world(c,
                                                                         sidx,
                                                                         current_area->name,
                                                                         c->actor.entity.x,
                                                                         c->actor.entity.y,
                                                                         c->actor.entity.z)) {
                            snprintf(status, sizeof(status), "Dropped %s.", item_name);
                        } else {
                            snprintf(status, sizeof(status), "Failed to drop %s.", item_name);
                        }
                    } else {
                        snprintf(status, sizeof(status), "Select a carried inventory item to drop.");
                    }
                    continue;
                }
            }
        }
    }
}
// END inventory_menu







