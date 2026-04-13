#ifndef ITEM_H
#define ITEM_H

#include <stddef.h>

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
    /** Readable book item (long-form text and knowledge). */
    ITEM_TYPE_BOOK,
    /** Readable scroll item (short-form text and knowledge). */
    ITEM_TYPE_SCROLL,
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
    /** Raw or refined crafting material, resource, or component. */
    ITEM_TYPE_MATERIAL,
} ItemType;

#define ITEM_BOOK_MAX_LOCATIONS 8
#define ITEM_BOOK_TEXT_LENGTH 256
#define ITEM_BOOK_HINT_LENGTH 128
#define ITEM_BOOK_RECIPE_ID_LENGTH 48

typedef enum BookContentType {
    BOOK_CONTENT_NONE = 0,
    BOOK_CONTENT_STORY,
    BOOK_CONTENT_LOCATION,
    BOOK_CONTENT_RECIPE,
    BOOK_CONTENT_SKILL_REFERENCE,
} BookContentType;

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
 *  Keep the original values for Punch/Kick/Stab/Cut/Smash stable for save compatibility.
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
    /** Stronger committed piercing follow-through. */
    ATTACK_MODE_THRUST,
    /** Wider follow-up slashing attack. */
    ATTACK_MODE_SLASH,
    /** Faster blunt follow-up strike. */
    ATTACK_MODE_BASH,
    /** Standard ranged shot. */
    ATTACK_MODE_SHOT,
    /** Careful ranged precision shot. */
    ATTACK_MODE_AIMED_SHOT,
    /** Unarmed special power blow. */
    ATTACK_MODE_HAYMAKER,
    /** Dagger special deceptive strike. */
    ATTACK_MODE_FEINT,
    /** Sword special extended attack. */
    ATTACK_MODE_LUNGE,
    /** Axe special heavy sweeping strike. */
    ATTACK_MODE_CLEAVE,
    /** Mace special armor-breaking hit. */
    ATTACK_MODE_SHATTER,
    /** Spear special penetrating attack. */
    ATTACK_MODE_IMPALE,
    /** Staff special controlling arc strike. */
    ATTACK_MODE_SWEEP,
    /** Polearm special hooking cut. */
    ATTACK_MODE_HOOK,
    /** Thrown-weapon special rapid release. */
    ATTACK_MODE_VOLLEY,
    /** Bow special precise disabling shot. */
    ATTACK_MODE_PIN_SHOT,
    /** Crossbow special ultra-precise shot. */
    ATTACK_MODE_DEADEYE,
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
    /** Weapon supports thrust attacks. */
    ATTACK_MODE_FLAG_THRUST = 1 << 5,
    /** Weapon supports wide slash attacks. */
    ATTACK_MODE_FLAG_SLASH = 1 << 6,
    /** Weapon supports bash attacks. */
    ATTACK_MODE_FLAG_BASH = 1 << 7,
    /** Weapon supports standard ranged shots. */
    ATTACK_MODE_FLAG_SHOT = 1 << 8,
    /** Weapon supports aimed ranged shots. */
    ATTACK_MODE_FLAG_AIMED_SHOT = 1 << 9,
    /** Weapon supports haymaker attacks. */
    ATTACK_MODE_FLAG_HAYMAKER = 1 << 10,
    /** Weapon supports feint attacks. */
    ATTACK_MODE_FLAG_FEINT = 1 << 11,
    /** Weapon supports lunge attacks. */
    ATTACK_MODE_FLAG_LUNGE = 1 << 12,
    /** Weapon supports cleave attacks. */
    ATTACK_MODE_FLAG_CLEAVE = 1 << 13,
    /** Weapon supports shatter attacks. */
    ATTACK_MODE_FLAG_SHATTER = 1 << 14,
    /** Weapon supports impale attacks. */
    ATTACK_MODE_FLAG_IMPALE = 1 << 15,
    /** Weapon supports sweep attacks. */
    ATTACK_MODE_FLAG_SWEEP = 1 << 16,
    /** Weapon supports hook attacks. */
    ATTACK_MODE_FLAG_HOOK = 1 << 17,
    /** Weapon supports volley attacks. */
    ATTACK_MODE_FLAG_VOLLEY = 1 << 18,
    /** Weapon supports pin-shot attacks. */
    ATTACK_MODE_FLAG_PIN_SHOT = 1 << 19,
    /** Weapon supports deadeye attacks. */
    ATTACK_MODE_FLAG_DEADEYE = 1 << 20,
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
    MATERIAL_TYPE_MINERAL,
} MaterialType;

typedef enum MaterialState {
    MATERIAL_STATE_NONE = 0,
    MATERIAL_STATE_UNREFINED,
    MATERIAL_STATE_REFINED,
} MaterialState;

typedef enum ItemQuality {
    ITEM_QUALITY_UNSPECIFIED = -1,
    ITEM_QUALITY_HORRIBLE = 0,
    ITEM_QUALITY_CRUDE,
    ITEM_QUALITY_REGULAR,
    ITEM_QUALITY_GOOD,
    ITEM_QUALITY_EXCEPTIONAL,
    ITEM_QUALITY_MASTERWORK,
    ITEM_QUALITY_COUNT
} ItemQuality;

typedef enum ItemHeatState {
    ITEM_HEAT_NONE = 0,
    ITEM_HEAT_HOT,
} ItemHeatState;

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
    ItemQuality quality;
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
    int two_hand_attack_mode_mask;
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
    ItemHeatState heat_state;
    int heat_turns_remaining;
    // New: for slot-based equipment/container logic
    int slot_type; // EquipmentSlotType, if equipped
    int is_container; // 1 if this item is a container
    int container_capacity; // if container
    int container_accepted_flags; // if container
    // New: pointer to container contents if this is a container (NULL if not)
    struct Item* container_contents;
    int container_count;
    int is_readable;
    BookContentType book_content_type;
    char book_flavor[ITEM_BOOK_TEXT_LENGTH];
    char book_content[ITEM_BOOK_TEXT_LENGTH];
    char recipe_unlock_id[ITEM_BOOK_RECIPE_ID_LENGTH];
    int book_location_count;
    int book_location_index[ITEM_BOOK_MAX_LOCATIONS];
    int book_location_knowledge[ITEM_BOOK_MAX_LOCATIONS];
    char book_location_hint[ITEM_BOOK_MAX_LOCATIONS][ITEM_BOOK_HINT_LENGTH];
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
 * @brief Roll the default quality tier for a newly created item instance.
 * @param item The item being initialized.
 * @return The selected quality tier.
 */
ItemQuality item_roll_quality(const Item* item);

/**
 * @brief Apply one quality tier's stat modifiers to an item instance.
 * @param item The item to modify in place.
 * @param quality The quality tier to assign and apply.
 */
void item_apply_quality(Item* item, ItemQuality quality);

/**
 * @brief Convert an item quality enum to a stable save-token string.
 * @param quality The quality tier to stringify.
 * @return Lowercase token like `regular` or `masterwork`.
 */
const char* item_quality_name(ItemQuality quality);

/**
 * @brief Parse a saved quality token into an enum value.
 * @param text The input text token.
 * @return Matching quality tier, or `ITEM_QUALITY_REGULAR` when unknown.
 */
ItemQuality item_quality_from_string(const char* text);

/**
 * @brief Format the user-facing display name for an item, including non-regular quality prefixes.
 * @param item The item to name.
 * @param out Destination buffer.
 * @param out_size Destination buffer size.
 */
void item_format_display_name(const Item* item, char* out, size_t out_size);

/**
 * @brief Return a temporary static display string for an item.
 * @param item The item to name.
 * @return Pointer to a static buffer containing the formatted display name.
 */
const char* item_display_name(const Item* item);

/**
 * @brief Check if an item type is a weapon category.
 * @param type The ItemType to classify.
 * @return 1 if type is any weapon category, 0 otherwise.
 */
int item_type_is_weapon(ItemType type);

/**
 * @brief Check whether an item contains a specific category tag.
 * @param item The item to inspect.
 * @param target The category string to look for.
 * @return 1 when the category exists on the item, 0 otherwise.
 */
int item_has_category(const Item* item, const char* target);

/**
 * @brief Check whether an item is tagged as a tool.
 * @param item The item to classify.
 * @return 1 when the item has the `tool` category, 0 otherwise.
 */
int item_is_tool(const Item* item);

/**
 * @brief Resolve the non-weapon skill associated with a tagged tool item.
 * @param item The item to classify.
 * @return Matching skill type, or `NON_WEAPON_SKILL_COUNT` if no tool role is tagged.
 */
NonWeaponSkillType item_tool_non_weapon_skill(const Item* item);

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

