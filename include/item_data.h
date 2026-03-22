#ifndef ITEM_DATA_H
#define ITEM_DATA_H

#include "item.h"

#define ITEM_TEMPLATE_MAX_MAP_LOCATIONS 16

typedef enum ItemEffectType {
    ITEM_EFFECT_HEAL = 0,
    ITEM_EFFECT_MAP_KNOWLEDGE,
} ItemEffectType;

/*
 * Purpose:
 *   Declares item templates and lookup/init helpers for data-driven items.
 *
 * Functions:
 *   - item_template_by_name: finds a template by item name.
 *   - item_init_from_template: creates an item instance from template data.
 */

/*
 * Purpose:
 *   Declares item templates and lookup/init helpers for data-driven items.
 *
 * Functions:
 *   - item_template_by_name: finds a template by item name.
 *   - item_init_from_template: creates an item instance from template data.
 */

/** @struct ItemTemplate
 *  @brief Defines a data-driven item template used to spawn item instances.
 */
typedef struct ItemTemplate {
    /** @brief Item display name (used for lookup and display). */
    const char* name;
    /** @brief Map representation character (e.g., '/', 'T' for weapons). */
    char symbol;
    /** @brief Item category/equipment slot (ItemType). */
    ItemType type;
    /** @brief 1 if items of this template can stack, 0 if unique. */
    int stackable;
    /** @brief Default stack quantity when spawned. */
    int quantity;
    /** @brief Base damage or armor rating value. */
    int power;
    /** @brief Weapon skill category if this is a weapon, e.g. WEAPON_SKILL_SWORD. */
    WeaponSkillType weapon_skill_type;
    /** @brief Bonus to hit chance (in percentage points). */
    int accuracy_bonus;
    /** @brief Bonus to critical hit chance (in percentage points). */
    int crit_bonus;
    /** @brief Bonus to parry defense chance (in percentage points). */
    int parry_bonus;
    /** @brief Bonus to block defense chance (in percentage points). */
    int block_bonus;
    /** @brief 1 if can be used for parrying, 0 otherwise. */
    int can_parry;
    /** @brief Bitmask of DamageType values this weapon inflicts. */
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
    /** @brief 1 hides lower layers during inspection, 0 allows transparency. */
    int hide_below;
    /** @brief Consumable effect behavior. */
    ItemEffectType effect_type;
    /** @brief 1 keeps item after use, 0 consumes quantity normally. */
    int consumable_reusable;
    /** @brief Number of configured map-knowledge entries. */
    int map_knowledge_count;
    /** @brief Atlas area indices for map-knowledge entries. */
    int map_location_index[ITEM_TEMPLATE_MAX_MAP_LOCATIONS];
    /** @brief Target knowledge tier per map entry (LocationKnowledge as int). */
    int map_location_knowledge[ITEM_TEMPLATE_MAX_MAP_LOCATIONS];
} ItemTemplate;

extern const ItemTemplate* item_templates;
extern int item_template_count;

// Load item templates from an external text file.
int item_templates_load(const char* path);

/**
 * @brief Find item template by exact display name.
 * @param name The item name to search for (case-sensitive).
 * @return Pointer to the ItemTemplate if found, NULL otherwise.
 */
const ItemTemplate* item_template_by_name(const char* name);

/**
 * @brief Initialize an item instance from a template and world coordinates.
 * @param item Pointer to the Item to initialize (will be overwritten).
 * @param tmpl Pointer to the ItemTemplate to use as source data.
 * @param x World x-coordinate for the new item.
 * @param y World y-coordinate for the new item.
 */
void item_init_from_template(Item* item, const ItemTemplate* tmpl, int x, int y);

#endif
