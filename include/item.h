#ifndef ITEM_H
#define ITEM_H

#include "actor.h"
#include "entity.h"

/*
 * Purpose:
 *   Defines item types, item data, and item classification helpers.
 *
 * Functions:
 *   - item_init: initializes an item instance from provided values.
 *   - item_type_is_weapon: item-type classification helper.
 *   - item_is_weapon: item-instance classification helper.
 */

typedef enum ItemType {
    ITEM_TYPE_NONE,
    ITEM_TYPE_CONSUMABLE,
    ITEM_TYPE_WEAPON_MAIN_HAND,
    ITEM_TYPE_WEAPON_OFF_HAND,
    ITEM_TYPE_WEAPON_ONE_HANDED,
    ITEM_TYPE_WEAPON_VERSATILE,
    ITEM_TYPE_WEAPON_TWO_HANDED,
    ITEM_TYPE_ARMOR_HEAD,
    ITEM_TYPE_ARMOR_FACE,
    ITEM_TYPE_ARMOR_NECK,
    ITEM_TYPE_ARMOR_SHOULDERS,
    ITEM_TYPE_ARMOR_CLOAK,
    ITEM_TYPE_ARMOR_CHEST,
    ITEM_TYPE_ARMOR_WAIST,
    ITEM_TYPE_ARMOR_ARMS,
    ITEM_TYPE_ARMOR_HANDS,
    ITEM_TYPE_ARMOR_LEGS,
    ITEM_TYPE_ARMOR_FEET,
    ITEM_TYPE_ARMOR_BOOTS,
    ITEM_TYPE_CLOTHING_HEAD,
    ITEM_TYPE_CLOTHING_FACE,
    ITEM_TYPE_CLOTHING_SHOULDERS,
    ITEM_TYPE_CLOTHING_CHEST,
    ITEM_TYPE_CLOTHING_HANDS,
    ITEM_TYPE_CLOTHING_WAIST,
    ITEM_TYPE_CLOTHING_LEGS,
    ITEM_TYPE_CLOTHING_FEET,
    ITEM_TYPE_ACCESSORY_NECK,
    ITEM_TYPE_ACCESSORY_TRINKET,
    ITEM_TYPE_ACCESSORY_FINGER,
    ITEM_TYPE_ACCESSORY_BRACELET,
    ITEM_TYPE_ACCESSORY_BACKPACK,
    ITEM_TYPE_BAG_BACKPACK,
    ITEM_TYPE_BAG_BELTPOUCH,
    ITEM_TYPE_KEY,
} ItemType;

typedef enum DamageType {
    DAMAGE_TYPE_NONE = 0,
    DAMAGE_TYPE_PIERCING = 1 << 0,
    DAMAGE_TYPE_SLASHING = 1 << 1,
    DAMAGE_TYPE_CRUSHING = 1 << 2,
} DamageType;

typedef enum AttackMode {
    ATTACK_MODE_NONE = 0,
    ATTACK_MODE_PUNCH,
    ATTACK_MODE_KICK,
    ATTACK_MODE_STAB,
    ATTACK_MODE_CUT,
    ATTACK_MODE_SMASH,
} AttackMode;

typedef enum AttackModeFlag {
    ATTACK_MODE_FLAG_NONE = 0,
    ATTACK_MODE_FLAG_PUNCH = 1 << 0,
    ATTACK_MODE_FLAG_KICK = 1 << 1,
    ATTACK_MODE_FLAG_STAB = 1 << 2,
    ATTACK_MODE_FLAG_CUT = 1 << 3,
    ATTACK_MODE_FLAG_SMASH = 1 << 4,
} AttackModeFlag;

typedef struct Item {
    Entity entity;          // still an entity
    char name[32];
    int stackable;
    int quantity;
    ItemType type;
    int power;
    WeaponSkillType weapon_skill_type;
    int accuracy_bonus;
    int crit_bonus;
    int parry_bonus;
    int can_parry;
    int damage_type_mask;
    int attack_mode_mask;
} Item;

// Initialize one item instance with position, type, and stack data.
void item_init(Item* item, const char* name, char symbol, int x, int y, ItemType type, int stackable, int quantity);

// Return 1 when an item type is any weapon category.
int item_type_is_weapon(ItemType type);

// Return 1 when an item instance is in a weapon category.
int item_is_weapon(const Item* item);

#endif

