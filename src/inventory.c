#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include "combat.h"
#include "atlas.h"
#include "inventory.h"
#include "item_data.h"
#include "input.h"
#include "log.h"
#include "map.h"
#include "overlay_nav.h"
#include "ui_overlay.h"
#include "world_items.h"

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

typedef enum InventorySource {
    INVENTORY_SOURCE_BASE,       /**< Main inventory (10 slots). */
    INVENTORY_SOURCE_BELTPOUCH, /**< Belt pouch container (6 slots). */
    INVENTORY_SOURCE_BACKPACK,  /**< Backpack container (12 slots). */
} InventorySource;

/**
 * @brief Get the display name of an inventory source.
 * @param source The inventory source type.
 * @return A human-readable string ("inventory", "belt pouch", "backpack", or "unknown").
 */
static const char* source_name(InventorySource source)
{
    switch(source)
    {
        case INVENTORY_SOURCE_BASE: return "inventory";
        case INVENTORY_SOURCE_BELTPOUCH: return "belt pouch";
        case INVENTORY_SOURCE_BACKPACK: return "backpack";
        default: return "unknown";
    }
}

/**
 * @brief Determine inventory source from keyboard input character.
 * @param key The input key ('b'/'B' for belt pouch, 'p'/'P' for backpack, others for base).
 * @return The source type (defaults to INVENTORY_SOURCE_BASE for unrecognized keys).
 */
static InventorySource source_from_key(int key)
{
    if(key == 'b' || key == 'B') return INVENTORY_SOURCE_BELTPOUCH;
    if(key == 'p' || key == 'P') return INVENTORY_SOURCE_BACKPACK;
    return INVENTORY_SOURCE_BASE;
}

/**
 * @brief Get the current item count in an inventory source.
 * @param c The character whose inventory is being queried.
 * @param source The inventory source to count items from.
 * @return Number of items in the source (0 if container not equipped or doesn't exist).
 */
static int source_count(const Character* c, InventorySource source)
{
    if(!c) return 0;

    switch(source)
    {
        case INVENTORY_SOURCE_BASE:
            return c->inventory_count;
        case INVENTORY_SOURCE_BELTPOUCH:
            return (c->equipped_bag_beltpouch.type == ITEM_TYPE_BAG_BELTPOUCH) ? c->beltpouch_count : 0;
        case INVENTORY_SOURCE_BACKPACK:
            return (c->equipped_bag_backpack.type == ITEM_TYPE_BAG_BACKPACK) ? c->backpack_count : 0;
        default:
            return 0;
    }
}

/**
 * @brief Get the storage capacity of an inventory source (not current usage).
 * @param source The inventory source type.
 * @return Maximum number of items that can fit in this source.
 *        Base=10, Backpack=12, Belt Pouch=6.
 */
static int source_capacity(InventorySource source)
{
    if(source == INVENTORY_SOURCE_BASE)
        return INVENTORY_SIZE;
    if(source == INVENTORY_SOURCE_BACKPACK)
        return BACKPACK_CAPACITY;
    return BELTPOUCH_CAPACITY;
}

/**
 * @brief Convert a numeric key input to an inventory slot index.
 * @param key The input key ('1'-'9' or '0').
 * @return Slot index 0-9 on success, -1 if key is not a digit.
 * @note '1' = slot 0, '2' = slot 1, ..., '9' = slot 8, '0' = slot 9.
 */
static int slot_from_key(int key)
{
    if(key >= '1' && key <= '9') return key - '1';
    if(key == '0') return 9;
    return -1;
}

/**
 * @brief Get a pointer to an item in an inventory source.
 * @param c The character owning the inventory.
 * @param source The inventory source to access.
 * @param slot The slot index within that source.
 * @return Pointer to the Item if valid, NULL if slot is out of bounds or container not equipped.
 * @note This provides abstract access to items across different container types.
 */
static Item* source_item(Character* c, InventorySource source, int slot)
{
    if(!c) return NULL;

    switch(source)
    {
        case INVENTORY_SOURCE_BASE:
            if(slot < 0 || slot >= c->inventory_count) return NULL;
            return &c->inventory[slot];
        case INVENTORY_SOURCE_BELTPOUCH:
            if(c->equipped_bag_beltpouch.type != ITEM_TYPE_BAG_BELTPOUCH) return NULL;
            if(slot < 0 || slot >= c->beltpouch_count) return NULL;
            return &c->beltpouch_contents[slot];
        case INVENTORY_SOURCE_BACKPACK:
            if(c->equipped_bag_backpack.type != ITEM_TYPE_BAG_BACKPACK) return NULL;
            if(slot < 0 || slot >= c->backpack_count) return NULL;
            return &c->backpack_contents[slot];
        default:
            return NULL;
    }
}

static int source_add_item(Character* c, InventorySource source, const Item* item)
{
    if(!c || !item || item->type == ITEM_TYPE_NONE)
        return 0;

    switch(source)
    {
        case INVENTORY_SOURCE_BASE:
            return inventory_add(c, item);
        case INVENTORY_SOURCE_BELTPOUCH:
            if(c->equipped_bag_beltpouch.type != ITEM_TYPE_BAG_BELTPOUCH)
            {
                log_add("No belt pouch equipped");
                return 0;
            }
            if(c->beltpouch_count >= BELTPOUCH_CAPACITY)
            {
                log_add("Belt pouch full");
                return 0;
            }
            c->beltpouch_contents[c->beltpouch_count++] = *item;
            return 1;
        case INVENTORY_SOURCE_BACKPACK:
            if(c->equipped_bag_backpack.type != ITEM_TYPE_BAG_BACKPACK)
            {
                log_add("No backpack equipped");
                return 0;
            }
            if(c->backpack_count >= BACKPACK_CAPACITY)
            {
                log_add("Backpack full");
                return 0;
            }
            c->backpack_contents[c->backpack_count++] = *item;
            return 1;
        default:
            return 0;
    }
}

static int source_remove_item(Character* c, InventorySource source, int slot)
{
    if(!c) return 0;
    int i;
    switch(source)
    {
        case INVENTORY_SOURCE_BASE:
            if(slot < 0 || slot >= c->inventory_count) return 0;
            for(i = slot; i < c->inventory_count - 1; i++)
                c->inventory[i] = c->inventory[i + 1];
            c->inventory_count--;
            return 1;
        case INVENTORY_SOURCE_BELTPOUCH:
            if(slot < 0 || slot >= c->beltpouch_count) return 0;
            for(i = slot; i < c->beltpouch_count - 1; i++)
                c->beltpouch_contents[i] = c->beltpouch_contents[i + 1];
            item_init(&c->beltpouch_contents[c->beltpouch_count-1], "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
            c->beltpouch_count--;
            return 1;
        case INVENTORY_SOURCE_BACKPACK:
            if(slot < 0 || slot >= c->backpack_count) return 0;
            for(i = slot; i < c->backpack_count - 1; i++)
                c->backpack_contents[i] = c->backpack_contents[i + 1];
            item_init(&c->backpack_contents[c->backpack_count-1], "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
            c->backpack_count--;
            return 1;
        default:
            return 0;
    }
}

// Format one inventory slot row for overlay display.
static void format_slot_text(const Character* c, int slot, char out[64])
{
    const Item* item;

    if(slot < c->inventory_count)
    {
        item = &c->inventory[slot];
        if(item_is_weapon(item))
        {
            snprintf(out, 64, "%2d) %-14.14s %s P%d H%+d C%+d", slot + 1, item->name, weapon_skill_short_name(item->weapon_skill_type), item->power, item->accuracy_bonus, item->crit_bonus);
            return;
        }

        snprintf(out, 64, "%2d) %-22.22s x%-2d", slot + 1, item->name, item->quantity);
        return;
    }

    snprintf(out, 64, "%2d) %-24.24s", slot + 1, "--");
}

static void format_container_slot_text(const Item* item, int slot, char out[64])
{
    if(!item || item->type == ITEM_TYPE_NONE)
    {
        snprintf(out, 64, "%2d) %-24.24s", slot + 1, "--");
        return;
    }

    if(item_is_weapon(item))
    {
        snprintf(out, 64, "%2d) %-14.14s %s P%d H%+d C%+d",
                 slot + 1,
                 item->name,
                 weapon_skill_short_name(item->weapon_skill_type),
                 item->power,
                 item->accuracy_bonus,
                 item->crit_bonus);
        return;
    }

    snprintf(out, 64, "%2d) %-22.22s x%-2d", slot + 1, item->name, item->quantity);
}

// Format equipped-weapon text including combat modifiers.
static void format_equipped_weapon_text(const Item* item, char out[128])
{
    if(!item || item->type == ITEM_TYPE_NONE)
    {
        snprintf(out, 128, "None");
        return;
    }

    if(item_is_weapon(item))
    {
        snprintf(out, 128, "%s %s P%d H%+d C%+d Pr%+d", item->name, weapon_skill_short_name(item->weapon_skill_type), item->power, item->accuracy_bonus, item->crit_bonus, item->parry_bonus);
        return;
    }

    snprintf(out, 128, "%s", item->name);
}

// Draw full inventory/equipment overlay with status line.
static void draw_inventory_overlay(const Character* c, const char* status)
{
    char line[128];
    char left[64];
    char right[64];
    char left_hand[128];
    char right_hand[128];
    const char* armor_head;
    const char* armor_face;
    const char* armor_shoulders;
    const char* armor_chest;
    const char* armor_arms;
    const char* armor_hands;
    const char* armor_waist;
    const char* armor_legs;
    const char* armor_feet;
    const char* clothing_head;
    const char* clothing_face;
    const char* clothing_shoulders;
    const char* clothing_chest;
    const char* clothing_hands;
    const char* clothing_waist;
    const char* clothing_legs;
    const char* clothing_feet;
    const char* accessory_neck;
    const char* bracelet_r;
    const char* bracelet_l;
    const char* finger_r;
    const char* finger_l;
    const char* trinket0;
    const char* trinket1;
    const char* bag_backpack;
    const char* bag_beltpouch;
    int overlay_text_width = ui_overlay_text_width();
    int overlay_content_lines = ui_overlay_content_lines();
    int status_line = (overlay_content_lines > 1) ? (overlay_content_lines - 2) : 0;
    int hand_width = (overlay_text_width - 9) / 2;
    int slot_width = (overlay_text_width - 1) / 2;

    if(hand_width < 12) hand_width = 12;
    if(slot_width < 20) slot_width = 20;

    ui_overlay_draw_frame("Inventory (u use, e equip, x stash, d drop, n unequip)");

    // Display equipped items
    format_equipped_weapon_text(&c->equipped_left_hand, left_hand);
    format_equipped_weapon_text(&c->equipped_right_hand, right_hand);

    armor_head = c->equipped_armor_head.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_head.name;
    armor_face = c->equipped_armor_face.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_face.name;
    armor_shoulders = c->equipped_armor_shoulders.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_shoulders.name;
    armor_chest = c->equipped_armor_chest.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_chest.name;
    armor_arms = c->equipped_armor_arms.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_arms.name;
    armor_hands = c->equipped_armor_hands.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_hands.name;
    armor_waist = c->equipped_armor_waist.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_waist.name;
    armor_legs = c->equipped_armor_legs.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_legs.name;
    armor_feet = c->equipped_armor_feet.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_feet.name;

    clothing_head = c->equipped_clothing_head.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_head.name;
    clothing_face = c->equipped_clothing_face.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_face.name;
    clothing_shoulders = c->equipped_clothing_shoulders.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_shoulders.name;
    clothing_chest = c->equipped_clothing_chest.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_chest.name;
    clothing_hands = c->equipped_clothing_hands.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_hands.name;
    clothing_waist = c->equipped_clothing_waist.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_waist.name;
    clothing_legs = c->equipped_clothing_legs.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_legs.name;
    clothing_feet = c->equipped_clothing_feet.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_feet.name;

    accessory_neck = c->equipped_accessory_neck.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_neck.name;
    bracelet_r = c->equipped_accessory_bracelet_right.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_bracelet_right.name;
    bracelet_l = c->equipped_accessory_bracelet_left.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_bracelet_left.name;
    finger_r = c->equipped_accessory_finger_right.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_finger_right.name;
    finger_l = c->equipped_accessory_finger_left.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_finger_left.name;
    trinket0 = c->equipped_accessory_trinket_0.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_trinket_0.name;
    trinket1 = c->equipped_accessory_trinket_1.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_trinket_1.name;

    bag_backpack = c->equipped_bag_backpack.type == ITEM_TYPE_NONE ? "None" : c->equipped_bag_backpack.name;
    bag_beltpouch = c->equipped_bag_beltpouch.type == ITEM_TYPE_NONE ? "None" : c->equipped_bag_beltpouch.name;

    snprintf(line, sizeof(line), "Weapons: LH %-*.*s RH %-*.*s", hand_width-5, hand_width-5, left_hand, hand_width-5, hand_width-5, right_hand);
    ui_overlay_draw_line(0, line);

    snprintf(line, sizeof(line), "--- Armor ---");
    ui_overlay_draw_line(1, line);
    snprintf(line, sizeof(line), "Head: %-10.10s Face: %-10.10s Shoulders: %-10.10s Chest: %-10.10s", armor_head, armor_face, armor_shoulders, armor_chest);
    ui_overlay_draw_line(2, line);
    snprintf(line, sizeof(line), "Arms: %-10.10s Hands: %-10.10s Waist: %-10.10s Legs: %-10.10s", armor_arms, armor_hands, armor_waist, armor_legs);
    ui_overlay_draw_line(3, line);
    snprintf(line, sizeof(line), "Feet: %-10.10s", armor_feet);
    ui_overlay_draw_line(4, line);

    snprintf(line, sizeof(line), "--- Clothing ---");
    ui_overlay_draw_line(5, line);
    snprintf(line, sizeof(line), "Head: %-10.10s Face: %-10.10s Shoulders: %-10.10s Chest: %-10.10s", clothing_head, clothing_face, clothing_shoulders, clothing_chest);
    ui_overlay_draw_line(6, line);
    snprintf(line, sizeof(line), "Hands: %-10.10s Waist: %-10.10s Legs: %-10.10s Feet: %-10.10s", clothing_hands, clothing_waist, clothing_legs, clothing_feet);
    ui_overlay_draw_line(7, line);

    snprintf(line, sizeof(line), "--- Accessories ---");
    ui_overlay_draw_line(8, line);
    snprintf(line, sizeof(line), "Neck: %-8.8s BracR: %-8.8s BracL: %-8.8s", accessory_neck, bracelet_r, bracelet_l);
    ui_overlay_draw_line(9, line);
    snprintf(line, sizeof(line), "RingR: %-8.8s RingL: %-8.8s Trinket0: %-9.9s Trinket1: %-9.9s", finger_r, finger_l, trinket0, trinket1);
    ui_overlay_draw_line(10, line);

    // Draw standard inventory header, then inventory slots. Bags are shown after inventory.
    ui_overlay_draw_line(11, "-------------------- Inventory --------------------");
    int inventory_start_line = 12;

    for(int row = 0; row < 5; row++)
    {
        int left_slot = row * 2;
        int right_slot = left_slot + 1;
        format_slot_text(c, left_slot, left);
        format_slot_text(c, right_slot, right);
        snprintf(line, sizeof(line), "%-*.*s %-*.*s", slot_width, slot_width, left, slot_width, slot_width, right);
        ui_overlay_draw_line(inventory_start_line + row, line);
    }

    // Bag section below the standard inventory
    int bag_line = inventory_start_line + 5;
    snprintf(line, sizeof(line), "Backpack: %-9.9s (%d/%d)", bag_backpack, c->backpack_count, BACKPACK_CAPACITY);
    ui_overlay_draw_line(bag_line++, line);
    if(c->equipped_bag_backpack.type == ITEM_TYPE_BAG_BACKPACK)
    {
        for(int i = 0; i < BACKPACK_CAPACITY; i++)
        {
            format_container_slot_text(i < c->backpack_count ? &c->backpack_contents[i] : NULL, i, line);
            ui_overlay_draw_line(bag_line++, line);
        }
    }

    snprintf(line, sizeof(line), "Beltpouch: %-9.9s (%d/%d)", bag_beltpouch, c->beltpouch_count, BELTPOUCH_CAPACITY);
    ui_overlay_draw_line(bag_line++, line);
    if(c->equipped_bag_beltpouch.type == ITEM_TYPE_BAG_BELTPOUCH)
    {
        for(int i = 0; i < BELTPOUCH_CAPACITY; i++)
        {
            format_container_slot_text(i < c->beltpouch_count ? &c->beltpouch_contents[i] : NULL, i, line);
            ui_overlay_draw_line(bag_line++, line);
        }
    }

    ui_overlay_draw_line(status_line, status ? status : "");
    ui_overlay_draw_global_hotkeys();
}

// Reset inventory and all equipment slots to empty defaults.
void inventory_init(Character* c)
{
    if(!c) return;
    c->inventory_count = 0;
    // Weapon slots
    item_init(&c->equipped_right_hand, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_left_hand, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);

    // Armor slots
    item_init(&c->equipped_armor_head, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_armor_face, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_armor_shoulders, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_armor_chest, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_armor_arms, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_armor_hands, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_armor_waist, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_armor_legs, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_armor_feet, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);

    // Clothing slots
    item_init(&c->equipped_clothing_head, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_clothing_face, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_clothing_shoulders, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_clothing_chest, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_clothing_hands, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_clothing_waist, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_clothing_legs, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_clothing_feet, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);

    // Accessories slots
    item_init(&c->equipped_accessory_neck, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_accessory_bracelet_right, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_accessory_bracelet_left, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_accessory_finger_right, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_accessory_finger_left, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_accessory_trinket_0, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_accessory_trinket_1, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);

    // Bag slots
    item_init(&c->equipped_bag_backpack, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    item_init(&c->equipped_bag_beltpouch, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    c->backpack_count = 0;
    c->beltpouch_count = 0;
    for(int i = 0; i < BACKPACK_CAPACITY; i++) {
        item_init(&c->backpack_contents[i], "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    }
    for(int i = 0; i < BELTPOUCH_CAPACITY; i++) {
        item_init(&c->beltpouch_contents[i], "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    }

    for(int i = 0; i < INVENTORY_SIZE; i++)
    {
        c->inventory[i].entity.blocks = 0;
        c->inventory[i].entity.x = -1;
        c->inventory[i].entity.y = -1;
        c->inventory[i].entity.symbol = '?';
        c->inventory[i].entity.color = RENDER_COLOR_DEFAULT;
        c->inventory[i].name[0] = '\0';
        c->inventory[i].stackable = 0;
        c->inventory[i].quantity = 0;
        c->inventory[i].type = ITEM_TYPE_NONE;
        c->inventory[i].power = 0;
    }
}

// Append an item to inventory when space is available.
int inventory_add(Character* c, const Item* item)
{
    if(!c || !item) return 0;

    if(c->inventory_count >= INVENTORY_SIZE)
    {
        log_add("Inventory full: cannot add %s", item->name);
        return 0;
    }

    c->inventory[c->inventory_count++] = *item;
    log_add("Added %s to inventory", item->name);
    return 1;
}

static int inventory_apply_heal_effect(Character* c, const Item* item)
{
    int heal;

    if(!c || !item)
        return 0;

    heal = item->power > 0 ? item->power : 5;
    c->actor.health += heal;
    if(c->actor.health > c->actor.max_health)
        c->actor.health = c->actor.max_health;

    log_add("Used %s and recovered %d Health", item->name, heal);
    return 1;
}

static int inventory_apply_map_knowledge_effect(const ItemTemplate* tmpl)
{
    int upgraded = 0;
    int unchanged = 0;
    char hint[ATLAS_LOCATION_HINT_LENGTH];

    if(!tmpl)
        return 0;

    for(int i = 0; i < tmpl->map_knowledge_count; i++)
    {
        int atlas_index = tmpl->map_location_index[i];
        int knowledge_raw = tmpl->map_location_knowledge[i];
        LocationKnowledge before;
        LocationKnowledge target;

        if(atlas_index < 0 || atlas_index >= MAX_AREAS)
            continue;

        target = (LocationKnowledge)knowledge_raw;
        if(target < LOCATION_KNOWLEDGE_AWARE)
            target = LOCATION_KNOWLEDGE_AWARE;
        if(target > LOCATION_KNOWLEDGE_LOCATED)
            target = LOCATION_KNOWLEDGE_LOCATED;

        before = atlas_get_knowledge(atlas_index);
        atlas_upgrade_knowledge(atlas_index, target);
        snprintf(hint, sizeof(hint), "Map note from %s.", tmpl->name ? tmpl->name : "unknown map");
        atlas_add_location_hint(atlas_index, hint);

        if(atlas_get_knowledge(atlas_index) > before)
            upgraded++;
        else
            unchanged++;
    }

    if(upgraded > 0)
    {
        log_add("You study the map and update your atlas.");
        if(unchanged > 0)
            log_add("%d locations improved, %d already known.", upgraded, unchanged);
        else
            log_add("%d locations improved.", upgraded);
        return 1;
    }

    if(unchanged > 0)
    {
        log_add("You study the map, but learn nothing new.");
        return 1;
    }

    log_add("This map has no usable location data.");
    return 0;
}

static int inventory_apply_consumable_effect(Character* c, const Item* item, int* out_consumed)
{
    const ItemTemplate* tmpl;

    if(!c || !item || !out_consumed)
        return 0;

    *out_consumed = 1;
    tmpl = item_template_by_name(item->name);

    if(tmpl && tmpl->effect_type == ITEM_EFFECT_MAP_KNOWLEDGE)
    {
        if(!inventory_apply_map_knowledge_effect(tmpl))
            return 0;
        *out_consumed = tmpl->consumable_reusable ? 0 : 1;
        return 1;
    }

    return inventory_apply_heal_effect(c, item);
}

int inventory_use_source(Character* c, InventorySource source, int slot)
{
    Item* item = source_item(c, source, slot);
    int consumed = 1;

    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;
    if(item->type != ITEM_TYPE_CONSUMABLE)
    {
        log_add("Cannot use %s from bag", item->name);
        return 0;
    }

    if(!inventory_apply_consumable_effect(c, item, &consumed))
        return 0;

    if(consumed)
    {
        item->quantity--;
        if(item->quantity <= 0)
            source_remove_item(c, source, slot);
    }

    return 1;
}

int inventory_equip_source(Character* c, InventorySource source, int slot)
{
    if(source == INVENTORY_SOURCE_BASE)
        return inventory_equip(c, slot);

    Item* item = source_item(c, source, slot);
    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;

    // Move from bag to temporary inventory so we can reuse existing logic.
    if(c->inventory_count >= INVENTORY_SIZE)
    {
        log_add("Inventory full: cannot equip from bag");
        return 0;
    }

    Item bag_item = *item;
    if(!inventory_add(c, &bag_item))
        return 0;

    int src_index = c->inventory_count - 1;
    if(!inventory_equip(c, src_index))
    {
        // restore to bag if equip failed
        inventory_remove(c, c->inventory_count - 1);
        return 0;
    }

    source_remove_item(c, source, slot);
    return 1;
}

static int inventory_transfer(Character* c, InventorySource from, InventorySource to, int slot)
{
    Item* item;
    Item moved_item;

    if(!c || from == to)
        return 0;

    item = source_item(c, from, slot);
    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;

    moved_item = *item;

    if(!source_add_item(c, to, &moved_item))
        return 0;

    source_remove_item(c, from, slot);
    log_add("Moved %s from %s to %s", moved_item.name, source_name(from), source_name(to));
    return 1;
}

static int inventory_drop(Character* c, InventorySource source, int slot)
{
    Item* item;
    Item dropped_item;

    if(!c || !current_area)
        return 0;

    item = source_item(c, source, slot);
    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;

    dropped_item = *item;
    if(!world_item_drop(&dropped_item, current_area->name, c->actor.entity.x, c->actor.entity.y))
    {
        log_add("Cannot drop %s here", dropped_item.name);
        return 0;
    }

    source_remove_item(c, source, slot);
    log_add("Dropped %s on the ground", dropped_item.name);
    return 1;
}

// Remove one inventory item by shifting subsequent slots.
int inventory_remove(Character* c, int slot)
{
    if(!c) return 0;
    if(slot < 0 || slot >= c->inventory_count) return 0;

    for(int i = slot; i < c->inventory_count - 1; i++)
        c->inventory[i] = c->inventory[i + 1];

    c->inventory_count--;
    return 1;
}

// Use one consumable item from a slot.
int inventory_use(Character* c, int slot)
{
    return inventory_use_source(c, INVENTORY_SOURCE_BASE, slot);
}

// Return whether a matching item is currently equipped.
static int inventory_is_equipped(const Character* c, const Item* item)
{
    if(!c || !item) return 0;
    Item* equipped[] = {
        // Weapons
        (Item*)&c->equipped_right_hand,
        (Item*)&c->equipped_left_hand,

        // Armor
        (Item*)&c->equipped_armor_head,
        (Item*)&c->equipped_armor_face,
        (Item*)&c->equipped_armor_shoulders,
        (Item*)&c->equipped_armor_chest,
        (Item*)&c->equipped_armor_arms,
        (Item*)&c->equipped_armor_hands,
        (Item*)&c->equipped_armor_waist,
        (Item*)&c->equipped_armor_legs,
        (Item*)&c->equipped_armor_feet,

        // Clothing
        (Item*)&c->equipped_clothing_head,
        (Item*)&c->equipped_clothing_face,
        (Item*)&c->equipped_clothing_shoulders,
        (Item*)&c->equipped_clothing_chest,
        (Item*)&c->equipped_clothing_hands,
        (Item*)&c->equipped_clothing_waist,
        (Item*)&c->equipped_clothing_legs,
        (Item*)&c->equipped_clothing_feet,

        // Accessory
        (Item*)&c->equipped_accessory_neck,
        (Item*)&c->equipped_accessory_bracelet_right,
        (Item*)&c->equipped_accessory_bracelet_left,
        (Item*)&c->equipped_accessory_finger_right,
        (Item*)&c->equipped_accessory_finger_left,
        (Item*)&c->equipped_accessory_trinket_0,
        (Item*)&c->equipped_accessory_trinket_1,
        (Item*)&c->equipped_bag_backpack,
        (Item*)&c->equipped_bag_beltpouch,
    };
    for(int i = 0; i < sizeof(equipped)/sizeof(equipped[0]); i++) {
        if(equipped[i]->type != ITEM_TYPE_NONE && strcmp(equipped[i]->name, item->name) == 0)
            return 1;
    }
    return 0;
}

// Equip one inventory item into its target equipment slot(s).
int inventory_equip(Character* c, int slot)
{
    if(!c) return 0;
    if(slot < 0 || slot >= c->inventory_count) return 0;

    Item item = c->inventory[slot];
    Item* target = NULL;
    Item this_old = {0};
    int two_handed = 0;

    switch(item.type)
    {
        case ITEM_TYPE_WEAPON_OFF_HAND:
            target = &c->equipped_left_hand;
            break;
        case ITEM_TYPE_WEAPON_MAIN_HAND:
            target = &c->equipped_right_hand;
            break;
        case ITEM_TYPE_WEAPON_ONE_HANDED:
            target = &c->equipped_right_hand;
            break;
        case ITEM_TYPE_WEAPON_VERSATILE:
            target = &c->equipped_right_hand;
            break;
        case ITEM_TYPE_WEAPON_TWO_HANDED:
            two_handed = 1;
            break;

        case ITEM_TYPE_ARMOR_HEAD:
            target = &c->equipped_armor_head;
            break;
        case ITEM_TYPE_CLOTHING_HEAD:
            target = &c->equipped_clothing_head;
            break;
        case ITEM_TYPE_ARMOR_FACE:
            target = &c->equipped_armor_face;
            break;
        case ITEM_TYPE_CLOTHING_FACE:
            target = &c->equipped_clothing_face;
            break;
        case ITEM_TYPE_ACCESSORY_NECK:
            target = &c->equipped_accessory_neck;
            break;
        case ITEM_TYPE_ARMOR_SHOULDERS:
            target = &c->equipped_armor_shoulders;
            break;
        case ITEM_TYPE_CLOTHING_SHOULDERS:
            target = &c->equipped_clothing_shoulders;
            break;
        case ITEM_TYPE_ARMOR_CHEST:
            target = &c->equipped_armor_chest;
            break;
        case ITEM_TYPE_CLOTHING_CHEST:
            target = &c->equipped_clothing_chest;
            break;
        case ITEM_TYPE_ARMOR_ARMS:
            target = &c->equipped_armor_arms;
            break;
        case ITEM_TYPE_ARMOR_HANDS:
            target = &c->equipped_armor_hands;
            break;
        case ITEM_TYPE_CLOTHING_HANDS:
            target = &c->equipped_clothing_hands;
            break;
        case ITEM_TYPE_ACCESSORY_BRACELET:
            if(c->equipped_accessory_bracelet_right.type == ITEM_TYPE_NONE)
                target = &c->equipped_accessory_bracelet_right;
            else
                target = &c->equipped_accessory_bracelet_left;
            break;
        case ITEM_TYPE_ACCESSORY_FINGER:
            if(c->equipped_accessory_finger_right.type == ITEM_TYPE_NONE)
                target = &c->equipped_accessory_finger_right;
            else
                target = &c->equipped_accessory_finger_left;
            break;
        case ITEM_TYPE_ARMOR_WAIST:
            target = &c->equipped_armor_waist;
            break;
        case ITEM_TYPE_CLOTHING_WAIST:
            target = &c->equipped_clothing_waist;
            break;
        case ITEM_TYPE_ARMOR_LEGS:
            target = &c->equipped_armor_legs;
            break;
        case ITEM_TYPE_CLOTHING_LEGS:
            target = &c->equipped_clothing_legs;
            break;
        case ITEM_TYPE_ARMOR_FEET:
            target = &c->equipped_armor_feet;
            break;
        case ITEM_TYPE_CLOTHING_FEET:
            target = &c->equipped_clothing_feet;
            break;
        case ITEM_TYPE_ACCESSORY_TRINKET:
            if(c->equipped_accessory_trinket_0.type == ITEM_TYPE_NONE)
                target = &c->equipped_accessory_trinket_0;
            else
                target = &c->equipped_accessory_trinket_1;
            break;
        case ITEM_TYPE_BAG_BACKPACK:
            target = &c->equipped_bag_backpack;
            break;
        case ITEM_TYPE_BAG_BELTPOUCH:
            target = &c->equipped_bag_beltpouch;
            break;
        default:
            log_add("Cannot equip %s", item.name);
            return 0;
    }

    // Remove from inventory by shifting down before equip to simplify
    for(int i = slot; i < c->inventory_count - 1; i++)
        c->inventory[i] = c->inventory[i + 1];
    c->inventory_count--;

    if(two_handed)
    {
        int has_dual_two_handed = c->equipped_left_hand.type == ITEM_TYPE_WEAPON_TWO_HANDED &&
                                  c->equipped_right_hand.type == ITEM_TYPE_WEAPON_TWO_HANDED &&
                                  strcmp(c->equipped_left_hand.name, c->equipped_right_hand.name) == 0;

        if(has_dual_two_handed)
        {
            if(c->inventory_count >= INVENTORY_SIZE) { log_add("Inventory full, cannot swap two-handed weapon"); return 0; }
            c->inventory[c->inventory_count++] = c->equipped_left_hand;
        }
        else
        {
            if(c->equipped_left_hand.type != ITEM_TYPE_NONE)
            {
                if(c->inventory_count >= INVENTORY_SIZE) { log_add("Inventory full, cannot swap two-handed weapon"); return 0; }
                c->inventory[c->inventory_count++] = c->equipped_left_hand;
            }
            if(c->equipped_right_hand.type != ITEM_TYPE_NONE)
            {
                if(c->inventory_count >= INVENTORY_SIZE) { log_add("Inventory full, cannot swap two-handed weapon"); return 0; }
                c->inventory[c->inventory_count++] = c->equipped_right_hand;
            }
        }
        c->equipped_left_hand = item;
        c->equipped_right_hand = item;
        log_add("Equipped two-handed %s", item.name);
        return 1;
    }

    this_old = *target;
    *target = item;

    if(this_old.type != ITEM_TYPE_NONE)
    {
        if(c->inventory_count >= INVENTORY_SIZE)
        {
            *target = this_old;
            log_add("Cannot swap equip %s: inventory full", item.name);
            // Add removed item back to inventory
            if(c->inventory_count < INVENTORY_SIZE)
                c->inventory[c->inventory_count++] = item;
            return 0;
        }
        c->inventory[c->inventory_count++] = this_old;
    }

    log_add("Equipped %s", item.name);
    return 1;
}

// Unequip one item type back into inventory.
int inventory_unequip(Character* c, ItemType type)
{
    if(!c) return 0;

    Item* equipped = NULL;
    switch(type) {
        case ITEM_TYPE_WEAPON_MAIN_HAND: equipped = &c->equipped_right_hand; break;
        case ITEM_TYPE_WEAPON_OFF_HAND: equipped = &c->equipped_left_hand; break;
        case ITEM_TYPE_WEAPON_TWO_HANDED:
            if(c->equipped_left_hand.type == ITEM_TYPE_NONE && c->equipped_right_hand.type == ITEM_TYPE_NONE) {
                log_add("No two-handed weapon equipped");
                return 0;
            }
            if(c->equipped_left_hand.type == ITEM_TYPE_WEAPON_TWO_HANDED &&
               c->equipped_right_hand.type == ITEM_TYPE_WEAPON_TWO_HANDED &&
               strcmp(c->equipped_left_hand.name, c->equipped_right_hand.name) == 0)
            {
                if(c->inventory_count >= INVENTORY_SIZE) {
                    log_add("Cannot unequip two-handed weapon: inventory full");
                    return 0;
                }
                c->inventory[c->inventory_count++] = c->equipped_left_hand;
                item_init(&c->equipped_left_hand, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
                item_init(&c->equipped_right_hand, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
                log_add("Unequipped two-handed weapon");
                return 1;
            }
            if(c->inventory_count + 2 > INVENTORY_SIZE) {
                log_add("Cannot unequip two-handed weapon: inventory full");
                return 0;
            }
            c->inventory[c->inventory_count++] = c->equipped_left_hand;
            c->inventory[c->inventory_count++] = c->equipped_right_hand;
            item_init(&c->equipped_left_hand, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
            item_init(&c->equipped_right_hand, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
            log_add("Unequipped two-handed weapon");
            return 1;
        case ITEM_TYPE_ARMOR_HEAD: equipped = &c->equipped_armor_head; break;
        case ITEM_TYPE_CLOTHING_HEAD: equipped = &c->equipped_clothing_head; break;
        case ITEM_TYPE_ARMOR_FACE: equipped = &c->equipped_armor_face; break;
        case ITEM_TYPE_CLOTHING_FACE: equipped = &c->equipped_clothing_face; break;
        case ITEM_TYPE_ACCESSORY_NECK: equipped = &c->equipped_accessory_neck; break;
        case ITEM_TYPE_ARMOR_SHOULDERS: equipped = &c->equipped_armor_shoulders; break;
        case ITEM_TYPE_CLOTHING_SHOULDERS: equipped = &c->equipped_clothing_shoulders; break;
        case ITEM_TYPE_ARMOR_CHEST: equipped = &c->equipped_armor_chest; break;
        case ITEM_TYPE_CLOTHING_CHEST: equipped = &c->equipped_clothing_chest; break;
        case ITEM_TYPE_ARMOR_ARMS: equipped = &c->equipped_armor_arms; break;
        case ITEM_TYPE_ARMOR_HANDS: equipped = &c->equipped_armor_hands; break;
        case ITEM_TYPE_CLOTHING_HANDS: equipped = &c->equipped_clothing_hands; break;
        case ITEM_TYPE_ACCESSORY_BRACELET:
            if(c->equipped_accessory_bracelet_right.type != ITEM_TYPE_NONE)
                equipped = &c->equipped_accessory_bracelet_right;
            else
                equipped = &c->equipped_accessory_bracelet_left;
            break;
        case ITEM_TYPE_ACCESSORY_FINGER:
            if(c->equipped_accessory_finger_right.type != ITEM_TYPE_NONE)
                equipped = &c->equipped_accessory_finger_right;
            else
                equipped = &c->equipped_accessory_finger_left;
            break;
        case ITEM_TYPE_ARMOR_WAIST: equipped = &c->equipped_armor_waist; break;
        case ITEM_TYPE_CLOTHING_WAIST: equipped = &c->equipped_clothing_waist; break;
        case ITEM_TYPE_ARMOR_LEGS: equipped = &c->equipped_armor_legs; break;
        case ITEM_TYPE_CLOTHING_LEGS: equipped = &c->equipped_clothing_legs; break;
        case ITEM_TYPE_ARMOR_FEET: equipped = &c->equipped_armor_feet; break;
        case ITEM_TYPE_CLOTHING_FEET: equipped = &c->equipped_clothing_feet; break;
        case ITEM_TYPE_ACCESSORY_TRINKET:
            if(c->equipped_accessory_trinket_0.type != ITEM_TYPE_NONE)
                equipped = &c->equipped_accessory_trinket_0;
            else
                equipped = &c->equipped_accessory_trinket_1;
            break;
        case ITEM_TYPE_BAG_BACKPACK: equipped = &c->equipped_bag_backpack; break;
        case ITEM_TYPE_BAG_BELTPOUCH: equipped = &c->equipped_bag_beltpouch; break;
        default:
            log_add("Cannot unequip this type");
            return 0;
    }

    if(!equipped) return 0;
    if(equipped->type == ITEM_TYPE_NONE) {
        log_add("No item equipped in that slot");
        return 0;
    }

    if(c->inventory_count >= INVENTORY_SIZE) {
        log_add("Cannot unequip %s: inventory full", equipped->name);
        return 0;
    }

    c->inventory[c->inventory_count++] = *equipped;
    log_add("Unequipped %s", equipped->name);
    item_init(equipped, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    return 1;
}

// Print inventory/equipment summary to stdout.
void inventory_print(const Character* c)
{
    if(!c) return;

    const char* left = c->equipped_left_hand.type == ITEM_TYPE_NONE ? "None" : c->equipped_left_hand.name;
    const char* right = c->equipped_right_hand.type == ITEM_TYPE_NONE ? "None" : c->equipped_right_hand.name;
    const char* armor_head = c->equipped_armor_head.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_head.name;
    const char* clothing_head = c->equipped_clothing_head.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_head.name;
    const char* armor_face = c->equipped_armor_face.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_face.name;
    const char* clothing_face = c->equipped_clothing_face.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_face.name;
    const char* accessory_neck = c->equipped_accessory_neck.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_neck.name;
    const char* armor_shoulders = c->equipped_armor_shoulders.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_shoulders.name;
    const char* clothing_shoulders = c->equipped_clothing_shoulders.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_shoulders.name;
    const char* armor_chest = c->equipped_armor_chest.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_chest.name;
    const char* clothing_chest = c->equipped_clothing_chest.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_chest.name;
    const char* armor_arms = c->equipped_armor_arms.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_arms.name;
    const char* armor_hands = c->equipped_armor_hands.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_hands.name;
    const char* clothing_hands = c->equipped_clothing_hands.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_hands.name;
    const char* accessory_bracelet_r = c->equipped_accessory_bracelet_right.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_bracelet_right.name;
    const char* accessory_bracelet_l = c->equipped_accessory_bracelet_left.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_bracelet_left.name;
    const char* accessory_finger_r = c->equipped_accessory_finger_right.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_finger_right.name;
    const char* accessory_finger_l = c->equipped_accessory_finger_left.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_finger_left.name;
    const char* armor_waist = c->equipped_armor_waist.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_waist.name;
    const char* clothing_waist = c->equipped_clothing_waist.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_waist.name;
    const char* armor_legs = c->equipped_armor_legs.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_legs.name;
    const char* clothing_legs = c->equipped_clothing_legs.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_legs.name;
    const char* armor_feet = c->equipped_armor_feet.type == ITEM_TYPE_NONE ? "None" : c->equipped_armor_feet.name;
    const char* clothing_feet = c->equipped_clothing_feet.type == ITEM_TYPE_NONE ? "None" : c->equipped_clothing_feet.name;
    const char* accessory_trinket_0 = c->equipped_accessory_trinket_0.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_trinket_0.name;
    const char* accessory_trinket_1 = c->equipped_accessory_trinket_1.type == ITEM_TYPE_NONE ? "None" : c->equipped_accessory_trinket_1.name;
    const char* bag_backpack = c->equipped_bag_backpack.type == ITEM_TYPE_NONE ? "None" : c->equipped_bag_backpack.name;
    const char* bag_beltpouch = c->equipped_bag_beltpouch.type == ITEM_TYPE_NONE ? "None" : c->equipped_bag_beltpouch.name;

    printf("Equipped: L-Hand: %s R-Hand: %s\n", left, right);
    printf("Head: Armor %s, Clothing %s, Face Armor %s, Face Clothing %s\n", armor_head, clothing_head, armor_face, clothing_face);
    printf("Neck: %s Shoulders Armor %s Shoulders Clothing %s\n", accessory_neck, armor_shoulders, clothing_shoulders);
    printf("Chest Armor %s, Chest Clothing %s, Arms %s, Hands Armor %s, Hands Clothing %s\n", armor_chest, clothing_chest, armor_arms, armor_hands, clothing_hands);
    printf("Bracelets: R=%s L=%s Fingers: R=%s L=%s\n", accessory_bracelet_r, accessory_bracelet_l, accessory_finger_r, accessory_finger_l);
    printf("Waist: Armor %s, Clothing %s Legs Armor %s, Legs Clothing %s\n", armor_waist, clothing_waist, armor_legs, clothing_legs);
    printf("Feet Armor %s, Feet Clothing %s\n", armor_feet, clothing_feet);
    printf("Trinkets: 0=%s 1=%s Bags: Backpack=%s Beltpouch=%s\n", accessory_trinket_0, accessory_trinket_1, bag_backpack, bag_beltpouch);
    printf("Inventory (%d/%d):\n", c->inventory_count, INVENTORY_SIZE);
    for(int i = 0; i < c->inventory_count; i++)
    {
        printf(" %d) %s x%d\n", i + 1, c->inventory[i].name, c->inventory[i].quantity);
    }
}

// Run full inventory interaction overlay loop.
void inventory_menu(Character* c)
{
    if(!c) return;

    char status[128];
    snprintf(status, sizeof(status), "Commands: u use, e equip, x stash, d drop, n unequip. Sources: i inv, b belt, p pack.");

    while(1)
    {
        draw_inventory_overlay(c, status);

        int cmd = read_input_key();
        if(cmd == 'q' || cmd == 'Q')
            break;

        {
            OverlayType next_overlay;
            if(overlay_type_from_key(cmd, &next_overlay) && next_overlay != OVERLAY_TYPE_INVENTORY)
            {
                overlay_request(next_overlay);
                break;
            }
        }

        if(cmd == 'u' || cmd == 'U' || cmd == 'e' || cmd == 'E' || cmd == 'x' || cmd == 'X' || cmd == 'd' || cmd == 'D')
        {
            InventorySource source = INVENTORY_SOURCE_BASE;
            int source_key;

            snprintf(status, sizeof(status), "Source: i inventory, b belt pouch, p backpack");
            draw_inventory_overlay(c, status);
            source_key = read_input_key();
            if(source_key == 'b' || source_key == 'B')
                source = INVENTORY_SOURCE_BELTPOUCH;
            else if(source_key == 'p' || source_key == 'P')
                source = INVENTORY_SOURCE_BACKPACK;
            else
                source = INVENTORY_SOURCE_BASE;

            int max_slots = (source == INVENTORY_SOURCE_BASE) ? c->inventory_count
                            : (source == INVENTORY_SOURCE_BELTPOUCH ? c->beltpouch_count : c->backpack_count);
            if(max_slots <= 0)
            {
                snprintf(status, sizeof(status), "No items in selected source.");
                continue;
            }

            snprintf(status, sizeof(status), "Select slot: 1-%d", max_slots);
            draw_inventory_overlay(c, status);
            int slot = slot_from_key(read_input_key());
            if(slot < 0 || slot >= max_slots)
            {
                snprintf(status, sizeof(status), "Invalid or empty slot.");
                continue;
            }

            if(cmd == 'u' || cmd == 'U')
            {
                if(!inventory_use_source(c, source, slot))
                    snprintf(status, sizeof(status), "Failed to use slot %d.", slot + 1);
                else
                    snprintf(status, sizeof(status), "Used slot %d.", slot + 1);
            }
            else if(cmd == 'e' || cmd == 'E')
            {
                if(!inventory_equip_source(c, source, slot))
                    snprintf(status, sizeof(status), "Failed to equip slot %d.", slot + 1);
                else
                    snprintf(status, sizeof(status), "Equipped slot %d.", slot + 1);
            }
            else if(cmd == 'x' || cmd == 'X')
            {
                InventorySource target_source;

                snprintf(status, sizeof(status), "Stash to: i inventory, b belt pouch, p backpack");
                draw_inventory_overlay(c, status);
                target_source = source_from_key(read_input_key());

                if(source == target_source)
                {
                    snprintf(status, sizeof(status), "Source and target are the same.");
                    continue;
                }

                if(!inventory_transfer(c, source, target_source, slot))
                    snprintf(status, sizeof(status), "Failed to move slot %d.", slot + 1);
                else
                    snprintf(status, sizeof(status), "Moved slot %d to %s.", slot + 1, source_name(target_source));
            }
            else
            {
                if(!inventory_drop(c, source, slot))
                    snprintf(status, sizeof(status), "Failed to drop slot %d.", slot + 1);
                else
                    snprintf(status, sizeof(status), "Dropped slot %d on the ground.", slot + 1);
            }
        }
        else if(cmd == 'n' || cmd == 'N')
        {
            snprintf(status, sizeof(status), "Unequip slot: l/r/2/h/f/p/k/s/c/a/u/w/q/z/v/b/m/t");
            draw_inventory_overlay(c, status);
            int sl = read_input_key();
            ItemType type = ITEM_TYPE_NONE;
            if(sl == 'l') type = ITEM_TYPE_WEAPON_OFF_HAND;
            else if(sl == 'r') type = ITEM_TYPE_WEAPON_MAIN_HAND;
            else if(sl == '2') type = ITEM_TYPE_WEAPON_TWO_HANDED;
            else if(sl == 'h') type = ITEM_TYPE_ARMOR_HEAD;
            else if(sl == 'f') type = ITEM_TYPE_ARMOR_FACE;
            else if(sl == 'p') type = ITEM_TYPE_CLOTHING_FACE;
            else if(sl == 'k') type = ITEM_TYPE_ACCESSORY_NECK;
            else if(sl == 's') type = ITEM_TYPE_ARMOR_SHOULDERS;
            else if(sl == 'c') type = ITEM_TYPE_ARMOR_CHEST;
            else if(sl == 'a') type = ITEM_TYPE_CLOTHING_CHEST;
            else if(sl == 'u') type = ITEM_TYPE_ARMOR_ARMS;
            else if(sl == 'w') type = ITEM_TYPE_ARMOR_WAIST;
            else if(sl == 'q') type = ITEM_TYPE_CLOTHING_WAIST;
            else if(sl == 'z') type = ITEM_TYPE_ARMOR_LEGS;
            else if(sl == 'v') type = ITEM_TYPE_CLOTHING_LEGS;
            else if(sl == 'b') type = ITEM_TYPE_ARMOR_BOOTS;
            else if(sl == 'm') type = ITEM_TYPE_CLOTHING_FEET;
            else if(sl == 't') type = ITEM_TYPE_ACCESSORY_TRINKET;
            if(type != ITEM_TYPE_NONE)
            {
                if(!inventory_unequip(c, type))
                    snprintf(status, sizeof(status), "Cannot unequip selected slot.");
                else
                    snprintf(status, sizeof(status), "Unequipped selected slot.");
            }
            else
            {
                snprintf(status, sizeof(status), "Invalid unequip slot key.");
            }
        }
        else
        {
            snprintf(status, sizeof(status), "Unknown command key.");
        }
    }
}

// Run quick-equip overlay loop for fast slot selection.
void inventory_quick_equip(Character* c)
{
    if(!c) return;

    char status[128];
    snprintf(status, sizeof(status), "Quick equip: press slot 1-9, 0 for slot 10, q to close.");

    while(1)
    {
        draw_inventory_overlay(c, status);

        int cmd = read_input_key();
        if(cmd == 'q' || cmd == 'Q')
            break;

        int slot = slot_from_key(cmd);
        if(slot >= 0 && slot < c->inventory_count)
        {
            if(inventory_equip(c, slot))
                snprintf(status, sizeof(status), "Equipped slot %d.", slot + 1);
            else
                snprintf(status, sizeof(status), "Failed to equip slot %d.", slot + 1);
        }
        else
        {
            snprintf(status, sizeof(status), "Invalid or empty slot.");
        }
    }
}

