#ifndef ITEM_H
#define ITEM_H

#include "actor.h"
#include "object.h"

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
    /** Armor for eyes (goggles, visors, blindfold helms, etc.). */
    ITEM_TYPE_ARMOR_EYES,
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
    /** Clothing for eyes (veils, spectacles, blindfolds, etc., with no armor rating). */
    ITEM_TYPE_CLOTHING_EYES,
    /** Clothing for face (scarves, bandanas, etc., with no armor rating). */
    ITEM_TYPE_CLOTHING_FACE,
    /** Clothing for neck (scarves, cravats, collars, etc., with no armor rating). */
    ITEM_TYPE_CLOTHING_NECK,
    /** Clothing for shoulders (cloaks, mantles, with no armor rating). */
    ITEM_TYPE_CLOTHING_SHOULDERS,
    /** Clothing for chest (shirts, robes, with no armor rating). */
    ITEM_TYPE_CLOTHING_CHEST,
    /** Clothing for arms (sleeves, wraps, bracers, with no armor rating). */
    ITEM_TYPE_CLOTHING_ARMS,
    /** Clothing for hands (gloves, bracers, with no armor rating). */
    ITEM_TYPE_CLOTHING_HANDS,
    /** Clothing for waist (skirts, belts, with no armor rating). */
    ITEM_TYPE_CLOTHING_WAIST,
    /** Clothing for legs (skirts, pants, with no armor rating). */
    ITEM_TYPE_CLOTHING_LEGS,
    /** Clothing for feet (socks, fashion shoes, with no armor rating). */
    ITEM_TYPE_CLOTHING_FEET,
    /** Accessory for head (circlets, tiaras, hairpins). */
    ITEM_TYPE_ACCESSORY_HEAD,
    /** Accessory for eyes (monocles, spectacles, charms). */
    ITEM_TYPE_ACCESSORY_EYES,
    /** Accessory for face (nose rings, piercings, charms). */
    ITEM_TYPE_ACCESSORY_FACE,
    /** Accessory for neck (amulets, necklaces, scarves). */
    ITEM_TYPE_ACCESSORY_NECK,
    /** Accessory trinket slot (brooches, pendants, keepsakes, etc.). */
    ITEM_TYPE_ACCESSORY_TRINKET,
    /** Accessory for finger (rings, signet rings). */
    ITEM_TYPE_ACCESSORY_FINGER,
    /** Accessory for the wrist (bands, bracelets, cuffs). */
    ITEM_TYPE_ACCESSORY_WRIST,
    /** Container: large backpack or satchel. */
    ITEM_TYPE_CONTAINER_BACKPACK,
    /** Container: small pouch or satchel. */
    ITEM_TYPE_CONTAINER_POUCH,
    /** Container: quiver for arrows and bolts only. */
    ITEM_TYPE_CONTAINER_QUIVER,
    /** Key item for doors, puzzles, or quest purposes. */
    ITEM_TYPE_KEY,
} ItemType;

/** @enum DamageType
 *  @brief Damage type categories and delivery families used by combat.
 *  Uses bit flags so weapons/ammo can support multiple types.
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
    /** Ranged delivery family (bows/crossbows); actual projectile type can come from ammo. */
    DAMAGE_TYPE_RANGED = 1 << 3,
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

typedef enum MaterialType {
    MATERIAL_TYPE_NONE = 0,
    MATERIAL_TYPE_METAL,
    MATERIAL_TYPE_WOOD,
    MATERIAL_TYPE_GEMSTONE,
    MATERIAL_TYPE_LEATHER,
    MATERIAL_TYPE_CLOTH,
} MaterialType;

typedef enum MaterialState {
    MATERIAL_STATE_NONE = 0,
    MATERIAL_STATE_UNREFINED,
    MATERIAL_STATE_REFINED,
} MaterialState;

/** @struct Item
 *  @brief Runtime item instance with position, stats, and equipped state.
 *
 * Hierarchy: Entity -> Object -> Item
 */
typedef struct Item {
    object_t object;
    char name[32];
    int stackable;
    int stack_max;
    int quantity;
    ItemType type; // Deprecated: use categories for new logic
    char categories[4][24]; // Up to 4 categories, 24 chars each (e.g., {"equipable", "weapon"})
    int power;
    int damage_min;
    int damage_max;
    int stab_damage_min;
    int stab_damage_max;
    int cut_damage_min;
    int cut_damage_max;
    int smash_damage_min;
    int smash_damage_max;
    int punch_damage_min;
    int punch_damage_max;
    int kick_damage_min;
    int kick_damage_max;
    WeaponSkillType weapon_skill_type;
    int accuracy_bonus;
    int crit_bonus;
    int parry_bonus;
    int block_bonus;
    int can_parry;
    int damage_type_mask;
    int attack_mode_mask;
    int reach_bonus;
    int armor_penetration;
    int stamina_cost_mod;
    int status_bleed_chance;
    int status_stun_chance;
    int status_slow_chance;
    int camp_placeable;
    int throwable;
    RangedWeaponType ranged_type;
    int ranged_range;
    char ammo_item_name[32];
    int ammo_per_shot;
    int is_ammo;
    int is_material;
    MaterialType material_type;
    MaterialState material_state;
    // New: for slot-based equipment/container logic
    int slot_type; // EquipmentSlotType, if equipped
    int is_container; // 1 if this item is a container
    int container_capacity; // if container
    int container_accepted_flags; // if container
    // New: pointer to container contents if this is a container (NULL if not)
    struct Item* container_contents;
    int container_count;
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

/**
 * @brief Check if an item instance is tagged as a material resource.
 * @param item The item to classify.
 * @return 1 when item belongs to the material family, 0 otherwise.
 */
int item_is_material(const Item* item);

#endif

