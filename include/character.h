#ifndef CHARACTER_H
#define CHARACTER_H

#include "entity.h"
#include "actor.h"
#include "item.h"

/*
 * Purpose:
 *   Defines Character/NPC data containers and compatibility access helpers.
 *
 * Functions:
 *   - character_create: initializes the global player character wrapper.
 *   - character_x / character_y: return current global player position.
 */



#define INVENTORY_SIZE 4
/*
 * Keep fixed equipment slots and carried inventory slots distinct while leaving
 * room for the starter Traveler's Backpack and future container upgrades.
 */
#define MAX_EQUIPMENT_SLOTS 64



// Equipment slot types are now extensible; can be data-driven in the future
typedef enum EquipmentSlotType {
    EQUIP_SLOT_NONE = 0,
    EQUIP_SLOT_MAIN_HAND,
    EQUIP_SLOT_OFF_HAND,
    EQUIP_SLOT_ARMOR_HEAD,
    EQUIP_SLOT_ARMOR_EYES,
    EQUIP_SLOT_ARMOR_FACE,
    EQUIP_SLOT_ARMOR_NECK,
    EQUIP_SLOT_ARMOR_SHOULDERS,
    EQUIP_SLOT_ARMOR_CHEST,
    EQUIP_SLOT_ARMOR_ARMS,
    EQUIP_SLOT_ARMOR_HANDS,
    EQUIP_SLOT_ARMOR_WAIST,
    EQUIP_SLOT_ARMOR_LEGS,
    EQUIP_SLOT_ARMOR_FEET,
    EQUIP_SLOT_CLOTHING_HEAD,
    EQUIP_SLOT_CLOTHING_EYES,
    EQUIP_SLOT_CLOTHING_FACE,
    EQUIP_SLOT_CLOTHING_NECK,
    EQUIP_SLOT_CLOTHING_SHOULDERS,
    EQUIP_SLOT_CLOTHING_CHEST,
    EQUIP_SLOT_CLOTHING_ARMS,
    EQUIP_SLOT_CLOTHING_HANDS,
    EQUIP_SLOT_CLOTHING_WAIST,
    EQUIP_SLOT_CLOTHING_LEGS,
    EQUIP_SLOT_CLOTHING_FEET,
    EQUIP_SLOT_ACCESSORY_HEAD,
    EQUIP_SLOT_ACCESSORY_EYES,
    EQUIP_SLOT_ACCESSORY_FACE,
    EQUIP_SLOT_ACCESSORY_NECK,
    EQUIP_SLOT_ACCESSORY_WRIST_RIGHT,
    EQUIP_SLOT_ACCESSORY_WRIST_LEFT,
    EQUIP_SLOT_ACCESSORY_WRIST = EQUIP_SLOT_ACCESSORY_WRIST_RIGHT,
    EQUIP_SLOT_ACCESSORY_FINGER_RIGHT = EQUIP_SLOT_ACCESSORY_WRIST_LEFT + 1,
    EQUIP_SLOT_ACCESSORY_FINGER_LEFT,
    EQUIP_SLOT_ACCESSORY_FINGER = EQUIP_SLOT_ACCESSORY_FINGER_RIGHT,
    EQUIP_SLOT_ACCESSORY_TRINKET_1 = EQUIP_SLOT_ACCESSORY_FINGER_LEFT + 1,
    EQUIP_SLOT_ACCESSORY_TRINKET_2,
    EQUIP_SLOT_ACCESSORY_TRINKET = EQUIP_SLOT_ACCESSORY_TRINKET_1,
    EQUIP_SLOT_CONTAINER_BACKPACK = EQUIP_SLOT_ACCESSORY_TRINKET_2 + 1,
    EQUIP_SLOT_CONTAINER_POUCH,
    EQUIP_SLOT_CONTAINER_QUIVER,
    EQUIP_SLOT_COUNT
} EquipmentSlotType;

typedef struct EquipmentSlot {
    EquipmentSlotType slot_type;
    Item item;
    // Optionally: constraints, e.g. accepted item types, exclusivity, etc.
    int accepts_type_mask; // Bitmask for allowed item types
    int is_container_slot; // 1 if this slot is for a container
} EquipmentSlot;

typedef enum WeaponGripMode {
    WEAPON_GRIP_ONE_HANDED = 0,
    WEAPON_GRIP_TWO_HANDED,
} WeaponGripMode;

typedef struct Character {
    Actor actor;
    char name[32];
    WeaponGripMode versatile_grip_mode;
    /*
     * Fixed equipment slots live in the low enum-indexed range, while the
     * trailing EQUIP_SLOT_NONE entries act as distinct carried inventory slots.
     */
    EquipmentSlot equipment_slots[MAX_EQUIPMENT_SLOTS];
    int equipment_slot_count;
    int inventory_slot_count;
} Character;

typedef struct NPC {
    Character character;
    int active;
    int hostile;            // 0 = neutral, 1 = hostile
    int move_state;
    int state_turns;
    int move_dx;
    int move_dy;
    char area_name[32];
    int home_x0;
    int home_y0;
    int home_x1;
    int home_y1;
    int home_z;
    int dialogue_profile;
    int greeted_this_session;
    int last_gossip_index;
} NPC;

// Initialize the global player character at the given position.
void character_create(const char* name, int x, int y);

// Return the global player's x-coordinate.
int character_x();

// Return the global player's y-coordinate.
int character_y();

// Return the global player's z-coordinate.
int character_z();

#endif

