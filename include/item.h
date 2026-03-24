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

/** @enum ItemType
 *  @brief Categorizes items by equipment slot, weapon family, or consumable type.
 */
typedef enum ItemType {
    /** No item / empty slot. */
    ITEM_TYPE_NONE,
    /** Single-use items with consumable effects (potions, scrolls, etc.). */
    ITEM_TYPE_CONSUMABLE,
    /** Weapon for main/primary hand. */
    ITEM_TYPE_WEAPON_MAIN_HAND,
    /** Weapon for off/secondary hand. */
    ITEM_TYPE_WEAPON_OFF_HAND,
    /** One-handed weapon usable in either hand. */
    ITEM_TYPE_WEAPON_ONE_HANDED,
    /** Weapon usable one-handed or in both hands. */
    ITEM_TYPE_WEAPON_VERSATILE,
    /** Weapon requiring both hands. */
    ITEM_TYPE_WEAPON_TWO_HANDED,
    /** Armor for head (helmets, crowns, etc.). */
    ITEM_TYPE_ARMOR_HEAD,
    /** Armor for face (masks, visors, goggles, etc.). */
    ITEM_TYPE_ARMOR_FACE,
    /** Armor for neck area (gorgets, collars, etc.). */
    ITEM_TYPE_ARMOR_NECK,
    /** Armor for shoulders (pauldrons, spaulders, etc.). */
    ITEM_TYPE_ARMOR_SHOULDERS,
    /** Armor as a cloak/cape providing protection. */
    ITEM_TYPE_ARMOR_CLOAK,
    /** Armor for torso (breastplate, cuirass, etc.). */
    ITEM_TYPE_ARMOR_CHEST,
    /** Armor for waist (belts, faulds, etc.). */
    ITEM_TYPE_ARMOR_WAIST,
    /** Armor for arms (vambraces, rerebraces, etc.). */
    ITEM_TYPE_ARMOR_ARMS,
    /** Armor for hands (gauntlets, gloves providing armor rating). */
    ITEM_TYPE_ARMOR_HANDS,
    /** Armor for legs (greaves, cuisses, etc.). */
    ITEM_TYPE_ARMOR_LEGS,
    /** Armor for feet (armored boots, greaves, etc.). */
    ITEM_TYPE_ARMOR_FEET,
    /** Armor-rated boots and heavy footwear. */
    ITEM_TYPE_ARMOR_BOOTS,
    /** Clothing for head (hats, hoods, etc., with no armor rating). */
    ITEM_TYPE_CLOTHING_HEAD,
    /** Clothing for face (scarves, bandanas, etc., with no armor rating). */
    ITEM_TYPE_CLOTHING_FACE,
    /** Clothing for shoulders (cloaks, mantles, with no armor rating). */
    ITEM_TYPE_CLOTHING_SHOULDERS,
    /** Clothing for chest (shirts, robes, with no armor rating). */
    ITEM_TYPE_CLOTHING_CHEST,
    /** Clothing for hands (gloves, bracers, with no armor rating). */
    ITEM_TYPE_CLOTHING_HANDS,
    /** Clothing for waist (skirts, belts, with no armor rating). */
    ITEM_TYPE_CLOTHING_WAIST,
    /** Clothing for legs (skirts, pants, with no armor rating). */
    ITEM_TYPE_CLOTHING_LEGS,
    /** Clothing for feet (socks, fashion shoes, with no armor rating). */
    ITEM_TYPE_CLOTHING_FEET,
    /** Accessory for neck (amulets, necklaces, scarves). */
    ITEM_TYPE_ACCESSORY_NECK,
    /** Accessory trinket slot (brooches, pendants, quivers, etc.). */
    ITEM_TYPE_ACCESSORY_TRINKET,
    /** Accessory for finger (rings, signet rings). */
    ITEM_TYPE_ACCESSORY_FINGER,
    /** Accessory bracelet for wrist. */
    ITEM_TYPE_ACCESSORY_BRACELET,
    /** Container: large backpack or satchel. */
    ITEM_TYPE_BAG_BACKPACK,
    /** Container: small belt pouch or satchel. */
    ITEM_TYPE_BAG_BELTPOUCH,
    /** Key item for doors, puzzles, or quest purposes. */
    ITEM_TYPE_KEY,
} ItemType;

/** @enum DamageType
 *  @brief Physical damage type categories (piercing, slashing, crushing).
 *  Uses bit flags so weapons can support multiple damage types.
 */
typedef enum DamageType {
    /** No damage type. */
    DAMAGE_TYPE_NONE = 0,
    /** Piercing damage (daggers, spears, arrows). */
    DAMAGE_TYPE_PIERCING = 1 << 0,
    /** Slashing damage (swords, axes). */
    DAMAGE_TYPE_SLASHING = 1 << 1,
    /** Crushing damage (maces, fists, blunt weapons). */
    DAMAGE_TYPE_CRUSHING = 1 << 2,
} DamageType;

/** @enum AttackMode
 *  @brief Combat technique or style used to attack with a weapon or unarmed.
 */
typedef enum AttackMode {
    /** No attack mode selected. */
    ATTACK_MODE_NONE = 0,
    /** Punch attack (unarmed, fist-based). */
    ATTACK_MODE_PUNCH,
    /** Kick attack (unarmed, foot/leg-based). */
    ATTACK_MODE_KICK,
    /** Stab attack (piercing weapons like spears, daggers). */
    ATTACK_MODE_STAB,
    /** Cut attack (slashing weapons like swords, axes). */
    ATTACK_MODE_CUT,
    /** Smash attack (crushing weapons like maces, hammers). */
    ATTACK_MODE_SMASH,
} AttackMode;

/** @enum AttackModeFlag
 *  @brief Bit flags representing supported attack modes for a weapon.
 *  Used to quickly check if a weapon supports certain attack techniques.
 */
typedef enum AttackModeFlag {
    /** No attack modes supported. */
    ATTACK_MODE_FLAG_NONE = 0,
    /** Weapon supports punch attacks. */
    ATTACK_MODE_FLAG_PUNCH = 1 << 0,
    /** Weapon supports kick attacks. */
    ATTACK_MODE_FLAG_KICK = 1 << 1,
    /** Weapon supports stab/pierce attacks. */
    ATTACK_MODE_FLAG_STAB = 1 << 2,
    /** Weapon supports cut/slash attacks. */
    ATTACK_MODE_FLAG_CUT = 1 << 3,
    /** Weapon supports smash/crush attacks. */
    ATTACK_MODE_FLAG_SMASH = 1 << 4,
} AttackModeFlag;

typedef enum RangedWeaponType {
    RANGED_WEAPON_NONE = 0,
    RANGED_WEAPON_THROWN,
    RANGED_WEAPON_BOW,
    RANGED_WEAPON_CROSSBOW,
} RangedWeaponType;

/** @struct Item
 *  @brief Runtime item instance with position, stats, and equipped state.
 */
typedef struct Item {
    /** @brief Position in the world or inventory (inherited from Entity). */
    Entity entity;
    /** @brief Display name of the item. */
    char name[32];
    /** @brief 1 if item can be stacked (quantity > 1), 0 if unique. */
    int stackable;
    /** @brief Maximum stack size for one slot (>=1). */
    int stack_max;
    /** @brief Number of items in this stack (usually 1 for unique items). */
    int quantity;
    /** @brief Item's category/equipment slot (ItemType). */
    ItemType type;
    /** @brief Weapon power (damage, armor rating, or effect magnitude). */
    int power;
    /** @brief Weapon skill type if this is a weapon, otherwise WEAPON_SKILL_COUNT or 0. */
    WeaponSkillType weapon_skill_type;
    /** @brief Bonus to hit chance (in percentage points). */
    int accuracy_bonus;
    /** @brief Bonus to critical hit chance (in percentage points). */
    int crit_bonus;
    /** @brief Bonus to parry/block chance (in percentage points). */
    int parry_bonus;
    /** @brief Bonus to block chance (in percentage points). */
    int block_bonus;
    /** @brief 1 if weapon/item can be used for parrying defense, 0 otherwise. */
    int can_parry;
    /** @brief Bitmask of DamageType values this weapon can inflict. */
    int damage_type_mask;
    /** @brief Bitmask of AttackModeFlag values this weapon supports. */
    int attack_mode_mask;
    /** @brief Extra melee reach in tiles beyond adjacent range. */
    int reach_bonus;
    /** @brief Flat armor ignored during damage calculation. */
    int armor_penetration;
    /** @brief Stamina cost modifier applied per attack action. */
    int stamina_cost_mod;
    /** @brief Percent chance to apply bleed rider on hit. */
    int status_bleed_chance;
    /** @brief Percent chance to apply stun rider on hit. */
    int status_stun_chance;
    /** @brief Percent chance to apply slow rider on hit. */
    int status_slow_chance;
    /** @brief 1 when item can be placed in Camp setup mode. */
    int camp_placeable;
    /** @brief 1 when item is designed to be thrown effectively. */
    int throwable;
    /** @brief Ranged category for this weapon (none/thrown/bow/crossbow). */
    RangedWeaponType ranged_type;
    /** @brief Maximum ranged attack distance in tiles. */
    int ranged_range;
    /** @brief Required ammo item display name when ranged weapon consumes ammo. */
    char ammo_item_name[32];
    /** @brief Ammo units consumed per ranged attack. */
    int ammo_per_shot;
} Item;

/**
 * @brief Initialize an item instance with position, type, and stack data.
 * @param item Pointer to the Item to initialize.
 * @param name Display name for the item (e.g., "Iron Sword").
 * @param symbol Character used to represent the item on the map.
 * @param x World x-coordinate.
 * @param y World y-coordinate.
 * @param type Item category/equipment slot (ItemType).
 * @param stackable 1 if item can stack, 0 if unique.
 * @param quantity Number of items in the stack.
 */
void item_init(Item* item, const char* name, char symbol, int x, int y, ItemType type, int stackable, int quantity);

/**
 * @brief Check if an item type is a weapon category.
 * @param type The ItemType to classify.
 * @return 1 if type is any weapon category, 0 otherwise.
 */
int item_type_is_weapon(ItemType type);

/**
 * @brief Check if an item instance is a weapon.
 * @param item The item to classify.
 * @return 1 if item is in a weapon category, 0 otherwise.
 */
int item_is_weapon(const Item* item);

/**
 * @brief Check if an item is a ranged weapon type.
 * @param item The item to classify.
 * @return 1 when item is a weapon with ranged type, 0 otherwise.
 */
int item_is_ranged_weapon(const Item* item);

#endif

