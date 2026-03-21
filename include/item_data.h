#ifndef ITEM_DATA_H
#define ITEM_DATA_H

#include "item.h"

/*
 * Purpose:
 *   Declares item templates and lookup/init helpers for data-driven items.
 *
 * Functions:
 *   - item_template_by_name: finds a template by item name.
 *   - item_init_from_template: creates an item instance from template data.
 */

typedef struct ItemTemplate {
    const char* name;
    char symbol;
    ItemType type;
    int stackable;
    int quantity;
    int power;
    WeaponSkillType weapon_skill_type;
    int accuracy_bonus;
    int crit_bonus;
    int parry_bonus;
    int can_parry;
    int damage_type_mask;
    int attack_mode_mask;
} ItemTemplate;

extern const ItemTemplate item_templates[];
extern const int item_template_count;

// Find item template by exact display name.
const ItemTemplate* item_template_by_name(const char* name);

// Initialize an item instance from a template and world coordinates.
void item_init_from_template(Item* item, const ItemTemplate* tmpl, int x, int y);

#endif
