#ifndef ITEM_DATA_H
#define ITEM_DATA_H

#include "item.h"

#define ITEM_TEMPLATE_MAX_MAP_LOCATIONS 16

/** @defgroup ContainerAcceptFlags  Content-filter flags for containers.
 *  Zero means no restriction (accepts all item types).
 */

// Container accepted flags (bitmask)
#define CONTAINER_ACCEPTS_ALL        0
#define CONTAINER_ACCEPTS_AMMO       (1 << 0)
#define CONTAINER_ACCEPTS_CONSUMABLE (1 << 1)
#define CONTAINER_ACCEPTS_EQUIPMENT  (1 << 2)
#define CONTAINER_ACCEPTS_KEY        (1 << 3)
#define CONTAINER_ACCEPTS_QUEST      (1 << 4)
#define CONTAINER_ACCEPTS_MISC       (1 << 5)
#define CONTAINER_ACCEPTS_MATERIAL   (1 << 6)
// Add more as needed

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
    const char* name;
    char symbol;
    ItemType type; // Deprecated: use categories for new logic
    char categories[4][24]; // Up to 4 categories, 24 chars each (e.g., {"equipable", "weapon"})
    int stackable;
    int stack_max;
    int quantity;
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
    int hide_below;
    ItemEffectType effect_type;
    int consumable_reusable;
    int map_knowledge_count;
    int map_location_index[ITEM_TEMPLATE_MAX_MAP_LOCATIONS];
    int map_location_knowledge[ITEM_TEMPLATE_MAX_MAP_LOCATIONS];
    int is_ammo;
    int is_material;
    MaterialType material_type;
    MaterialState material_state;
    // New: for slot-based equipment/container logic
    int slot_type; // EquipmentSlotType, if equippable
    int is_container;
    int container_capacity;
    int container_accepted_flags;
    int is_attachment_host;
    int host_attachment_slots;
} ItemTemplate;

extern const ItemTemplate* item_templates;
extern int item_template_count;


// Load item templates from an external text file.
int item_templates_load(const char* path);
const char* item_templates_last_error(void);

// Clear all loaded item templates (frees storage, resets count)
void clear_item_templates(void);

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
