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

#define INVENTORY_SIZE 10

#define BACKPACK_CAPACITY 12
#define BELTPOUCH_CAPACITY 6

typedef struct Character {
    Actor actor;               // base stats
    char name[32];
    Item inventory[INVENTORY_SIZE];
    int inventory_count;
    // Weapon slots
    Item equipped_right_hand;                   // main hand, one handed, versatile, or two handed weapon.
    Item equipped_left_hand;                    // off-hand, shield, or secondary weapon, versatile when wielded in two hands, and two-handed weapons' secondary attack.

    // Armor slots
    Item equipped_armor_head;                   // helmets, hoods, circlets, etc. that provide armor rating.
    Item equipped_armor_face;                   // masks, visors, goggles, etc. that provide armor rating and can be worn under helmets or alone for fashion.
    Item equipped_armor_shoulders;              // pauldrons, spaulders, etc.
    Item equipped_armor_chest;                  // breastplates, cuirasses, etc. that provide armor rating.
    Item equipped_armor_arms;                   // vambraces, rerebraces, etc. that provide armor rating.
    Item equipped_armor_hands;                  // gauntlets, gloves, etc. that provide armor rating.
    Item equipped_armor_waist;                  // belts, faulds, tassets, etc. that provide armor rating.
    Item equipped_armor_legs;                   // greaves, cuisses, etc. that provide armor rating.
    Item equipped_armor_feet;                   // sturdied footwear like boots, greaves, etc.

    // Clothing slots
    Item equipped_clothing_head;                // clothing layers can be worn under armor for extra stats or fashion.
    Item equipped_clothing_face;                // scarves, bandanas, etc. that can be worn under helmets or alone for fashion.
    Item equipped_clothing_shoulders;           // cloaks and mantles that can be worn over armor or alone for fashion.
    Item equipped_clothing_chest;               // shirts, robes, etc. that can be worn under armor or alone for fashion.
    Item equipped_clothing_hands;               // gloves, bracers, etc. that can be worn under armor or alone for fashion.
    Item equipped_clothing_waist;               // skirts, belts, etc.
    Item equipped_clothing_legs;                // skirts, pants, etc. that can be worn under armor or alone for fashion.
    Item equipped_clothing_feet;               // socks, footwraps, etc. and fashion-only shoes like boots with no armor rating.

    // Accessories slots
    Item equipped_accessory_neck;               // amulets, necklaces, scarves, etc.
    Item equipped_accessory_bracelet_right;     // bracelets, cuffs, etc.
    Item equipped_accessory_bracelet_left;      // bracelets, cuffs, etc.
    Item equipped_accessory_finger_right;       // rings, signet rings, etc.
    Item equipped_accessory_finger_left;        // rings, signet rings, etc.
    Item equipped_accessory_trinket_0;          // brooches, pendants, quivers, etc.
    Item equipped_accessory_trinket_1;          // brooches, pendants, quivers, etc.

    // Bag slots
    Item equipped_bag_backpack;                 // backpacks, satchels, other larger bags.
    Item equipped_bag_beltpouch;                // satchels, pouches, other small bags.

    // Container contents for equipped bags.
    Item backpack_contents[BACKPACK_CAPACITY];
    int backpack_count;
    Item beltpouch_contents[BELTPOUCH_CAPACITY];
    int beltpouch_count;
} Character;

typedef struct NPC {
    Character character;    // same as Character
    // AI or dialogue fields
    int hostile;            // 0 = neutral, 1 = hostile
} NPC;

// Initialize the global player character at the given position.
void character_create(const char* name, int x, int y);

// Return the global player's x-coordinate.
int character_x();

// Return the global player's y-coordinate.
int character_y();

#endif

