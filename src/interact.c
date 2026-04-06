#include "inventory.h"

// Forward declaration for slot mapping utility
EquipmentSlotType equipment_slot_for_item_type(ItemType type);


// --- STUBS FOR MISSING FUNCTIONS (implementations) ---
#include "tile.h"


#include "bestiary.h"
#include "atlas.h"
#include "furniture.h"
#include "map.h"
#include "input.h"
#include "keybind_helpers.h"
#include "item_data.h"
#include "log.h"
#include "movement.h"
#include "draw.h"


// Forward declarations for types used before their definition
struct Creature;
struct Tile;
struct InteractionAction;
#include "world_items.h"
#include "interact.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>



#include "atlas.h"
#include "inventory.h"

#include "world_items.h"
#include "bestiary.h"
#include "map.h"
#include "interact.h"
#include "target_lock.h"
#include "ui_overlay.h"
#include "world_items.h"


// --- STUB IMPLEMENTATIONS FOR MISSING FUNCTIONS ---
// TODO: Replace with real implementations if needed.


static const char* interact_target_name_at(int tx, int ty)
{
    int pz = player.character.actor.entity.z;
    Creature* creature = bestiary_creature_at_3d(tx, ty, pz);
    const Tile* tile;
    WorldItem* world_item;
    WorldContainer* world_container;

    if(creature && creature->alive && creature->template)
        return creature->template->name;

    if(!current_area || tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return NULL;

    {
        Furniture* furn = furniture_at(current_area, tx, ty);
        if(furn && furn->type != FURNITURE_NONE)
        {
            if(furniture_uses_container_type(furn->type))
            {
                WorldContainer* wc = world_container_at_3d(tx, ty, pz);
                if(wc && wc->active && wc->label[0])
                    return wc->label;
            }
            return furniture_display_name(furn);
        }
    }

    tile = map_top_visible_tile(current_area, tx, ty, NULL);
    if(tile->interactable && tile->name[0])
        return tile->name;

    world_item = world_item_at_3d(tx, ty, pz);
    if(world_item && world_item->active)
        return world_item->item.name;

    world_container = world_container_at_3d(tx, ty, pz);
    if(world_container && world_container->active)
        return world_container->label;

    return NULL;
}

// Return 1 when tile behaves like a door in current tile schema.
static int tile_is_door(const Tile* tile)
{
    if(!tile)
        return 0;

    if(strcmp(tile->name, "Door") == 0 || strcmp(tile->name, "Open Door") == 0)
        return 1;

    return 0;
}

static int tile_is_staircase(const Tile* tile)
{
    if(!tile)
        return 0;

    if(strcmp(tile->name, "Staircase") == 0 ||
       strcmp(tile->name, "Stairs Up") == 0 ||
       strcmp(tile->name, "Stairs Down") == 0)
        return 1;

    return tile->symbol == '<' || tile->symbol == '>';
}

static int stairs_find_connected_step(const Area* area,
                                      int stair_x,
                                      int stair_y,
                                      int current_z,
                                      int dz,
                                      int* out_x,
                                      int* out_y)
{
    int next_z;
    int best_score = 9999;
    int best_x = stair_x;
    int best_y = stair_y;

    if(!area || dz == 0)
        return 0;

    next_z = current_z + ((dz > 0) ? 1 : -1);
    if(next_z < AREA_GROUND_Z || next_z > map_max_view_floor(area))
        return 0;

    for(int search_radius = 0; search_radius <= 2; ++search_radius)
    {
        for(int dy = -search_radius; dy <= search_radius; ++dy)
        {
            for(int dx = -search_radius; dx <= search_radius; ++dx)
            {
                int nx = stair_x + dx;
                int ny = stair_y + dy;
                const Tile* candidate;
                int score;

                if(nx < 0 || ny < 0 || nx >= area->width || ny >= area->height)
                    continue;

                candidate = map_tile_at_layer_z((Area*)area, nx, ny, next_z, TILE_LAYER_WALL);
                if(!tile_is_staircase(candidate))
                    continue;

                score = abs(dx) + abs(dy);
                if(score < best_score)
                {
                    best_score = score;
                    best_x = nx;
                    best_y = ny;
                }
            }
        }
    }

    if(best_score == 9999)
        return 0;

    if(out_x)
        *out_x = best_x;
    if(out_y)
        *out_y = best_y;
    return 1;
}

static int stairs_can_move(const Player* p, int stair_x, int stair_y, int dz)
{
    if(!p || !current_area || dz == 0)
        return 0;

    return stairs_find_connected_step(current_area,
                                      stair_x,
                                      stair_y,
                                      p->character.actor.entity.z,
                                      dz,
                                      NULL,
                                      NULL);
}

static int stairs_floor_number_for_z(int z)
{
    if(z <= HERMIT_TOWER_BASE_Z)
        return 1;

    return ((z - HERMIT_TOWER_BASE_Z) / HERMIT_TOWER_FLOOR_Z_STEP) + 1;
}

static int stairs_is_floor_landing_z(int z)
{
    if(z < HERMIT_TOWER_BASE_Z)
        return 0;

    return ((z - HERMIT_TOWER_BASE_Z) % HERMIT_TOWER_FLOOR_Z_STEP) == 0;
}

static int interact_use_stairs(Player* p, int stair_x, int stair_y, int dz)
{
    int next_z;
    int next_x = stair_x;
    int next_y = stair_y;

    if(!p || !current_area || dz == 0)
        return 0;

    dz = (dz > 0) ? 1 : -1;
    if(!stairs_find_connected_step(current_area,
                                   stair_x,
                                   stair_y,
                                   p->character.actor.entity.z,
                                   dz,
                                   &next_x,
                                   &next_y))
    {
        if(dz > 0)
            log_add("This staircase does not continue upward from here.");
        else
            log_add("This staircase does not continue downward from here.");
        return 0;
    }

    next_z = p->character.actor.entity.z + dz;
    p->character.actor.entity.x = next_x;
    p->character.actor.entity.y = next_y;
    p->character.actor.entity.z = next_z;

    if(stairs_is_floor_landing_z(next_z))
    {
        log_add("You %s the staircase to Hermit Tower floor %d (z=%d).",
                (dz > 0) ? "climb" : "descend",
                stairs_floor_number_for_z(next_z),
                next_z);
    }
    else
    {
        log_add("You %s the staircase to stair level z=%d.",
                (dz > 0) ? "climb" : "descend",
                next_z);
    }

    creatures_take_turns(p);

    {
        const Tile* landed_tile = map_top_visible_tile(current_area, next_x, next_y, NULL);
        if(tile_is_staircase(landed_tile))
            (void)interact_at(p, next_x, next_y);
    }

    return 1;
}

static int interact_current_area_index(void)
{
    if(!current_area || !current_area->name || current_area->name[0] == '\0')
        return -1;

    return atlas_find_location(current_area->name);
}

static int interact_max_range(void)
{
    return INTERACT_RANGE_DEFAULT;
}

static int interact_in_range(int px, int py, int tx, int ty, int max_range)
{
    int dx = abs(px - tx);
    int dy = abs(py - ty);
    return (dx > dy ? dx : dy) <= max_range;
}

static int interact_furniture_owns_world_container(const Furniture* furn, const WorldContainer* world_container)
{
    int world_container_index;

    if(!furn || !world_container || !world_container->active)
        return 0;

    if(furniture_interaction_type(furn) != FURNITURE_INTERACTION_OPEN_CONTAINER)
        return 0;

    world_container_index = world_container_index_of(world_container);
    if(furn->world_container_index >= 0 && world_container_index == furn->world_container_index)
        return 1;

    return current_area &&
           strcmp(world_container->area_name, current_area->name) == 0 &&
           world_container->x == furn->base.base.x &&
           world_container->y == furn->base.base.y &&
           world_container->z == furn->base.base.z;
}

typedef enum InteractionActionType {
    INTERACTION_ACTION_OPEN_CONTAINER = 0,
    INTERACTION_ACTION_PICK_UP_ITEM,
    INTERACTION_ACTION_EQUIP_FROM_GROUND,
    INTERACTION_ACTION_EXAMINE_ITEM,
    INTERACTION_ACTION_PET,
    INTERACTION_ACTION_FEED,
    INTERACTION_ACTION_TREAT_INJURY,
    INTERACTION_ACTION_TALK,
    INTERACTION_ACTION_GIVE_ITEM,
    INTERACTION_ACTION_TILE_USE,
    INTERACTION_ACTION_EXTINGUISH_FORGE,
} InteractionActionType;

typedef struct InteractionAction {
    InteractionActionType type;
    int enabled;
    char label[80];
    char disabled_reason[80];
    Creature* creature;
    WorldItem* world_item;
    WorldContainer* world_container;
    Furniture* furniture; // NEW: direct pointer to furniture entity
    int tx;
    int ty;
    int stair_delta_z;
    // New fields for slot-based system
    int inventory_slot; // Index in inventory, if relevant
    int equipment_slot; // Index in equipment, if relevant
    int container_slot; // Index in container, if relevant
    int source_type;    // 0=none, 1=inventory, 2=equipment, 3=ground, 4=container
    int dest_type;      // 0=none, 1=inventory, 2=equipment, 3=ground, 4=container
} InteractionAction;

#define INTERACTION_ACTIONS_MAX 24

static int interact_item_type_is_container(ItemType type)
{
    return type == ITEM_TYPE_CONTAINER_BACKPACK || type == ITEM_TYPE_CONTAINER_POUCH || type == ITEM_TYPE_CONTAINER_QUIVER;
}

static int interact_item_available_quantity(const Item* item)
{
    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;

    if(item->stackable)
        return (item->quantity > 0) ? item->quantity : 0;

    return 1;
}

static int interact_count_carried_item_quantity(const Player* p, const char* item_name)
{
    int total = 0;

    if(!p || !item_name || !item_name[0])
        return 0;

    for(int i = 0; i < p->character.equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &p->character.equipment_slots[i];
        const Item* item = &slot->item;
        int available;

        if(slot->slot_type != EQUIP_SLOT_NONE || item->type == ITEM_TYPE_NONE)
            continue;
        if(strcmp(item->name, item_name) != 0)
            continue;

        available = interact_item_available_quantity(item);
        if(available <= 0)
            continue;

        total += available;
    }

    return total;
}

static int interact_consume_carried_item_quantity(Player* p, const char* item_name, int amount)
{
    if(!p || !item_name || !item_name[0] || amount <= 0)
        return 0;

    for(int i = 0; i < p->character.equipment_slot_count && amount > 0; ++i)
    {
        EquipmentSlot* slot = &p->character.equipment_slots[i];
        Item* item = &slot->item;
        int available;
        int consume_amount;

        if(slot->slot_type != EQUIP_SLOT_NONE || item->type == ITEM_TYPE_NONE)
            continue;
        if(strcmp(item->name, item_name) != 0)
            continue;

        available = interact_item_available_quantity(item);
        if(available <= 0)
            continue;

        consume_amount = (available < amount) ? available : amount;
        available -= consume_amount;
        amount -= consume_amount;

        if(item->stackable)
        {
            if(available > 0)
                item->quantity = available;
            else
                item_init(item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
        }
        else
        {
            item_init(item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
        }
    }

    return amount == 0;
}

static int interact_add_template_item_to_inventory(Player* p, const char* template_name, int quantity)
{
    const ItemTemplate* tmpl;

    if(!p || !template_name || !template_name[0] || quantity <= 0)
        return 0;

    tmpl = item_template_by_name(template_name);
    if(!tmpl)
        return 0;

    while(quantity > 0)
    {
        Item produced;
        int chunk = 1;

        item_init_from_template(&produced, tmpl, -1, -1);
        if(produced.type == ITEM_TYPE_NONE)
            return 0;

        if(produced.stackable)
        {
            int stack_max = produced.stack_max > 0 ? produced.stack_max : 99;
            chunk = (quantity < stack_max) ? quantity : stack_max;
            produced.quantity = chunk;
        }
        else
        {
            produced.quantity = 1;
        }

        if(!inventory_add(&p->character, &produced))
            return 0;

        quantity -= chunk;
    }

    return 1;
}

static int interact_use_forge(Player* p, Furniture* furn)
{
    static const struct {
        const char* item_name;
        int fuel_units;
    } fuels[] = {
        { "Wood Log", 3 },
        { "Wood Plank", 1 }
    };
    static const struct {
        const char* input_name;
        const char* output_name;
    } recipes[] = {
        { "Iron Ore", "Iron Ingot" },
        { "Copper Ore", "Copper Ingot" }
    };
    int smelted_any = 0;

    if(!p || !furn)
        return 0;

    if(furn->fuel_units <= 0)
    {
        furn->fuel_units = 0;
        furn->is_ignited = 0;

        for(int i = 0; i < (int)(sizeof(fuels) / sizeof(fuels[0])); ++i)
        {
            if(interact_count_carried_item_quantity(p, fuels[i].item_name) <= 0)
                continue;
            if(!interact_consume_carried_item_quantity(p, fuels[i].item_name, 1))
                continue;

            furn->fuel_units += fuels[i].fuel_units;
            log_add("You add %s to the forge.", fuels[i].item_name);
            log_add("The forge now has %d fuel.", furn->fuel_units);
            creatures_take_turns(p);
            return 1;
        }

        log_add("The forge is cold. Add a Wood Log or Wood Plank as fuel first.");
        return 1;
    }

    if(!furn->is_ignited)
    {
        furn->is_ignited = 1;
        log_add("You ignite the forge. The coals flare to life.");
        creatures_take_turns(p);
        return 1;
    }

    for(int i = 0; i < (int)(sizeof(recipes) / sizeof(recipes[0])); ++i)
    {
        int available = interact_count_carried_item_quantity(p, recipes[i].input_name);

        if(available <= 0)
            continue;

        if(!item_template_by_name(recipes[i].output_name))
        {
            log_add("The forge lacks a valid recipe output for %s.", recipes[i].input_name);
            continue;
        }

        if(!interact_consume_carried_item_quantity(p, recipes[i].input_name, available))
            continue;

        if(!interact_add_template_item_to_inventory(p, recipes[i].output_name, available))
        {
            (void)interact_add_template_item_to_inventory(p, recipes[i].input_name, available);
            log_add("You do not have enough room to collect the smelted %s.", recipes[i].output_name);
            return 1;
        }

        log_add("You smelt %d %s into %d %s.",
                available,
                recipes[i].input_name,
                available,
                recipes[i].output_name);
        smelted_any = 1;
    }

    if(smelted_any)
    {
        furn->fuel_units--;
        if(furn->fuel_units <= 0)
        {
            furn->fuel_units = 0;
            furn->is_ignited = 0;
            log_add("The forge burns through its fuel and goes dark.");
        }
        else
        {
            log_add("The forge remains lit with %d fuel left.", furn->fuel_units);
        }

        creatures_take_turns(p);
        return 1;
    }

    log_add("The forge is lit, but you need metal ore in your pack to smelt.");
    return 1;
}

static void interaction_action_add(InteractionAction* actions,
                                   int* count,
                                   InteractionActionType type,
                                   int enabled,
                                   const char* label,
                                   const char* disabled_reason,
                                   Creature* creature,
                                   WorldItem* world_item,
                                   WorldContainer* world_container,
                                   int tx,
                                   int ty)
{
    InteractionAction* action;

    if(!actions || !count || *count < 0 || *count >= INTERACTION_ACTIONS_MAX)
        return;
    action = &actions[*count];
    action->type = type;
    action->enabled = enabled;
    action->creature = creature;
    action->world_item = world_item;
    action->world_container = world_container;
    action->furniture = NULL; // default, set by caller if needed
    action->tx = tx;
    action->ty = ty;
    // Slot-based fields are set by caller if needed

    snprintf(action->label, sizeof(action->label), "%s", label ? label : "Action");
    if(disabled_reason && disabled_reason[0] != '\0')
        snprintf(action->disabled_reason, sizeof(action->disabled_reason), "%s", disabled_reason);
    else
        action->disabled_reason[0] = '\0';

    (*count)++;
}

// Try interacting with a creature first, per inspect interaction priority.
static int interact_inventory_visible_count(const Character* c)
{
    int count = 0;

    if(!c)
        return 0;

    for(int i = EQUIP_SLOT_COUNT; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->slot_type != EQUIP_SLOT_NONE || slot->item.type == ITEM_TYPE_NONE)
            continue;
        count++;
    }

    return count;
}

static int interact_inventory_slot_from_visible_index(const Character* c, int visible_index)
{
    int count = 0;

    if(!c || visible_index < 0)
        return -1;

    for(int i = EQUIP_SLOT_COUNT; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->slot_type != EQUIP_SLOT_NONE || slot->item.type == ITEM_TYPE_NONE)
            continue;
        if(count == visible_index)
            return i;
        count++;
    }

    return -1;
}

static int interact_deposit_to_container(Player* p, WorldContainer* container)
{
    int selected = 0;
    int scroll_offset = 0;
    char title[96];

    if(!p || !container || !container->active)
        return 0;

    snprintf(title, sizeof(title), "Deposit - %s", container->label);

    while(1)
    {
        int item_count = interact_inventory_visible_count(&p->character);
        int content_lines;
        int status_line;
        int visible_rows;
        int max_scroll;
        int line_i = 0;
        int key;

        draw_world(p);
        ui_overlay_draw_frame(title);
        ui_overlay_invalidate_cache();

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        visible_rows = status_line;
        max_scroll = item_count - visible_rows;
        if(max_scroll < 0)
            max_scroll = 0;

        if(item_count <= 0)
        {
            selected = 0;
            scroll_offset = 0;
            if(line_i < status_line) ui_overlay_draw_line(line_i++, "You have no carried inventory items to deposit.");
            while(line_i < status_line) ui_overlay_draw_line(line_i++, "");
            ui_overlay_draw_line(status_line, "Esc/Q back | Enter none");
            ui_overlay_draw_global_hotkeys();
        }
        else
        {
            if(selected < 0) selected = 0;
            if(selected >= item_count) selected = item_count - 1;
            if(selected < scroll_offset)
                scroll_offset = selected;
            if(visible_rows > 0 && selected >= scroll_offset + visible_rows)
                scroll_offset = selected - visible_rows + 1;
            if(scroll_offset < 0)
                scroll_offset = 0;
            if(scroll_offset > max_scroll)
                scroll_offset = max_scroll;

            for(int visible_i = scroll_offset; visible_i < item_count && line_i < status_line; ++visible_i)
            {
                int slot_index = interact_inventory_slot_from_visible_index(&p->character, visible_i);
                char line[128];
                const Item* item;
                int shown_quantity;

                if(slot_index < 0)
                    continue;

                item = &p->character.equipment_slots[slot_index].item;
                shown_quantity = (item->quantity > 0) ? item->quantity : 1;
                snprintf(line,
                         sizeof(line),
                         "%c %2d. %-28s x%d",
                         (visible_i == selected) ? '>' : ' ',
                         visible_i + 1,
                         item->name,
                         shown_quantity);
                ui_overlay_draw_line(line_i++, line);
            }

            while(line_i < status_line)
                ui_overlay_draw_line(line_i++, "");

            ui_overlay_draw_line(status_line, "Enter deposit | W/S move | PgUp/PgDn jump | Home/End | Esc/Q back");
            ui_overlay_draw_global_hotkeys();
        }

        key = read_input_key();

        if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
            return 0;

        if(item_count <= 0)
            continue;

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            if(selected > 0) selected--;
            continue;
        }

        if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
        {
            if(selected < item_count - 1) selected++;
            continue;
        }

        if(key == INPUT_KEY_PGUP)
        {
            selected -= (visible_rows > 0) ? visible_rows : 1;
            if(selected < 0)
                selected = 0;
            continue;
        }

        if(key == INPUT_KEY_PGDN)
        {
            selected += (visible_rows > 0) ? visible_rows : 1;
            if(selected >= item_count)
                selected = item_count - 1;
            continue;
        }

        if(key == INPUT_KEY_HOME)
        {
            selected = 0;
            continue;
        }

        if(key == INPUT_KEY_END)
        {
            selected = item_count - 1;
            continue;
        }

        if(key == 13)
        {
            int container_index = world_container_index_of(container);
            int slot_index = interact_inventory_slot_from_visible_index(&p->character, selected);
            Item moved_item;

            if(container_index < 0 || slot_index < 0)
                continue;

            moved_item = p->character.equipment_slots[slot_index].item;
            if(moved_item.type == ITEM_TYPE_NONE)
                continue;

            if(!world_container_add_item(container_index, &moved_item))
            {
                log_add("%s cannot hold any more items.", container->label);
                continue;
            }

            if(!inventory_remove(&p->character, slot_index))
            {
                Item rollback_item;
                (void)world_container_remove_item(container_index, container->item_count - 1, &rollback_item);
                log_add("Failed to move %s into %s.", moved_item.name, container->label);
                continue;
            }

            log_add("You place %s into %s.", moved_item.name, container->label);
            return 1;
        }
    }
}

static int interact_creature(Player* p, Creature* creature)
{
    if(!p || !creature || !creature->alive || !creature->template)
        return 0;

    if(creature_is_hostile(creature))
    {
        log_add("%s is hostile. Maybe use attack instead.", creature->template->name);
        return 0;
    }

    {
        int animal_handling = p->character.actor.animal_handling_skill;
        creature_apply_pet_event(creature, animal_handling);

        if(creature->template->tamable)
        {
            log_add("You pet the %s. [%s]", creature->template->name, taming_stage_name(creature->taming_stage));
            /* Grant animal handling XP: 5 XP per pet, next level at 100 * (current_level + 1) */
            p->character.actor.animal_handling_skill_xp += 5;
            if(p->character.actor.animal_handling_skill_xp >= 100 * (p->character.actor.animal_handling_skill + 1))
            {
                p->character.actor.animal_handling_skill_xp = 0;
                p->character.actor.animal_handling_skill++;
                log_add("Your animal handling skill improved to %d!", p->character.actor.animal_handling_skill);
            }
        }
        else
        {
            log_add("You pet the %s.", creature->template->name);
        }
    }

    creatures_take_turns(p);
    return 1;
}

// Open one world container and allow item pickup via overlay.
int interact_open_container(Player* p, WorldContainer* container)
{
    int selected = 0;
    int scroll_offset = 0;
    int took_any = 0;
    int need_world_redraw = 1;
    char title[96];

    if(!p || !container || !container->active)
        return 0;

    snprintf(title, sizeof(title), "Container - %s", container->label);

    while(1)
    {
        int content_lines;
        int status_line;
        int visible_rows;
        int max_scroll;
        int line_i = 0;

        if(need_world_redraw)
        {
            draw_world(p);
                ui_overlay_draw_frame(title);
            ui_overlay_invalidate_cache();
            need_world_redraw = 0;
        }

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        visible_rows = status_line;
        max_scroll = container->item_count - visible_rows;
        if(max_scroll < 0)
            max_scroll = 0;

        if(container->item_count <= 0)
        {
            selected = 0;
            scroll_offset = 0;
            if(line_i < status_line) ui_overlay_draw_line(line_i++, "This container is empty.");
            while(line_i < status_line) ui_overlay_draw_line(line_i++, "");
            ui_overlay_draw_line(status_line, "Esc/Q close | D deposit item | W/S move");
            ui_overlay_draw_global_hotkeys();
        }
        else
        {
            if(selected < 0) selected = 0;
            if(selected >= container->item_count) selected = container->item_count - 1;
            if(selected < scroll_offset)
                scroll_offset = selected;
            if(visible_rows > 0 && selected >= scroll_offset + visible_rows)
                scroll_offset = selected - visible_rows + 1;
            if(scroll_offset < 0)
                scroll_offset = 0;
            if(scroll_offset > max_scroll)
                scroll_offset = max_scroll;

            for(int i = scroll_offset; i < container->item_count && line_i < status_line; i++)
            {
                char line[128];
                snprintf(line, sizeof(line), "%c %2d. %-28s x%d",
                         (i == selected) ? '>' : ' ',
                         i + 1,
                         container->items[i].name,
                         container->items[i].quantity > 0 ? container->items[i].quantity : 1);
                ui_overlay_draw_line(line_i++, line);
            }

            while(line_i < status_line)
                ui_overlay_draw_line(line_i++, "");

            ui_overlay_draw_line(status_line, "Esc/Q close | Enter take | D deposit | W/S move | PgUp/PgDn jump | Home/End");
            ui_overlay_draw_global_hotkeys();
        }

        {
            int key = read_input_key();

            if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
                break;

            if(key == 'd' || key == 'D')
            {
                if(interact_deposit_to_container(p, container))
                {
                    took_any = 1;
                    need_world_redraw = 1;
                }
                continue;
            }

            if(container->item_count <= 0)
                continue;

            if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
            {
                if(selected > 0) selected--;
                continue;
            }

            if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
            {
                if(selected < container->item_count - 1) selected++;
                continue;
            }

            if(key == INPUT_KEY_PGUP)
            {
                selected -= (visible_rows > 0) ? visible_rows : 1;
                if(selected < 0)
                    selected = 0;
                continue;
            }

            if(key == INPUT_KEY_PGDN)
            {
                selected += (visible_rows > 0) ? visible_rows : 1;
                if(selected >= container->item_count)
                    selected = container->item_count - 1;
                continue;
            }

            if(key == INPUT_KEY_HOME)
            {
                selected = 0;
                continue;
            }

            if(key == INPUT_KEY_END)
            {
                selected = container->item_count - 1;
                continue;
            }

            if(key == 13)
            {
                Item picked_item;
                int container_index = world_container_index_of(container);

                if(container_index < 0)
                    continue;

                if(world_container_remove_item(container_index, selected, &picked_item))
                {
                    int handled_pickup = 0;

                    if(strcmp(picked_item.name, "Gold Coins") == 0)
                    {
                        int gold_amount = picked_item.quantity > 0 ? picked_item.quantity : 1;
                        p->gold += gold_amount;
                        log_add("You take %d gold from %s.", gold_amount, container->label);
                        handled_pickup = 1;
                    }
                    else if(inventory_add(&p->character, &picked_item))
                    {
                        log_add("You take %s from %s.", picked_item.name, container->label);
                        handled_pickup = 1;
                    }
                    else
                    {
                        log_add("No space in inventory for %s.", picked_item.name);
                        (void)world_container_add_item(container_index, &picked_item);
                        need_world_redraw = 1;
                    }

                    if(handled_pickup)
                    {
                        took_any = 1;
                        need_world_redraw = 1;

                        if(container->item_count <= 0)
                        {
                            (void)world_container_remove(container_index);
                            break;
                        }

                        if(selected >= container->item_count)
                            selected = container->item_count - 1;
                        if(selected < 0)
                            selected = 0;
                        max_scroll = container->item_count - visible_rows;
                        if(max_scroll < 0)
                            max_scroll = 0;
                        if(scroll_offset > max_scroll)
                            scroll_offset = max_scroll;
                    }
                }
            }
        }
    }

    return took_any;
}

static int interact_tile(Player* p, int tx, int ty);

static char g_interaction_last_target[96] = "";
static char g_interaction_last_label[80] = "";

static int interaction_initial_selection(const char* target_name, InteractionAction* actions, int action_count)
{
    if(!actions || action_count <= 0)
        return 0;

    if(g_interaction_last_label[0] != '\0')
    {
        for(int i = 0; i < action_count; ++i)
        {
            if(strcmp(actions[i].label, g_interaction_last_label) != 0)
                continue;

            if((target_name && target_name[0] && strcmp(g_interaction_last_target, target_name) == 0) ||
               strcmp(actions[i].label, "Go up") == 0 ||
               strcmp(actions[i].label, "Go down") == 0)
                return i;
        }
    }

    return 0;
}

static int interaction_show_menu(Player* p, const char* target_name, InteractionAction* actions, int action_count)
{
    int selected;
    int scroll_offset = 0;
    char title[96];

    if(!p || !actions || action_count <= 0)
        return -1;

    selected = interaction_initial_selection(target_name, actions, action_count);

    snprintf(title, sizeof(title), "Interact - %s", (target_name && target_name[0]) ? target_name : "Target");

    while(1)
    {
        int content_lines;
        int status_line;
        int visible_rows;
        int max_scroll;
        int line_i = 0;
        int key;

        draw_world(p);
        ui_overlay_draw_frame(title);
        ui_overlay_invalidate_cache();

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        visible_rows = status_line;
        max_scroll = action_count - visible_rows;
        if(max_scroll < 0)
            max_scroll = 0;

        if(selected < 0)
            selected = 0;
        if(selected >= action_count)
            selected = action_count - 1;

        if(selected < scroll_offset)
            scroll_offset = selected;
        if(visible_rows > 0 && selected >= scroll_offset + visible_rows)
            scroll_offset = selected - visible_rows + 1;
        if(scroll_offset < 0)
            scroll_offset = 0;
        if(scroll_offset > max_scroll)
            scroll_offset = max_scroll;

        for(int i = scroll_offset; i < action_count && line_i < status_line; i++)
        {
            char line[128];
            const char* state_tag = actions[i].enabled ? "" : " [disabled]";

            snprintf(line, sizeof(line), "%c %s%s",
                     (i == selected) ? '>' : ' ',
                     actions[i].label,
                     state_tag);
            ui_overlay_draw_line(line_i++, line);
        }

        while(line_i < status_line)
            ui_overlay_draw_line(line_i++, "");

        if(actions[selected].enabled)
            ui_overlay_draw_line(status_line, "Enter confirm | W/S move | Q/Esc cancel");
        else
        {
            char status[128];
            snprintf(status, sizeof(status), "Unavailable: %s | W/S move | Q/Esc cancel",
                     actions[selected].disabled_reason[0] ? actions[selected].disabled_reason : "Not implemented yet");
            ui_overlay_draw_line(status_line, status);
        }

        ui_overlay_draw_global_hotkeys();

        key = read_input_key();

        if(KEYBIND_CANCEL(key) || key == 'e' || key == 'E')
            return -1;

        if(KEYBIND_UP(key))
        {
            if(selected > 0)
                selected--;
            continue;
        }

        if(KEYBIND_DOWN(key))
        {
            if(selected < action_count - 1)
                selected++;
            continue;
        }

        if(KEYBIND_CONFIRM(key))
        {
            if(!actions[selected].enabled)
            {
                log_add("%s", actions[selected].disabled_reason[0] ? actions[selected].disabled_reason : "Not implemented yet.");
                continue;
            }

            snprintf(g_interaction_last_target,
                     sizeof(g_interaction_last_target),
                     "%s",
                     (target_name && target_name[0]) ? target_name : "Target");
            snprintf(g_interaction_last_label,
                     sizeof(g_interaction_last_label),
                     "%s",
                     actions[selected].label);
            return selected;
        }
    }
}

int interact_pick_up_world_item(Player* p, WorldItem* world_item)
{
    int world_index;

    if(!p || !world_item || !world_item->active)
        return 0;

    if(!inventory_add(&p->character, &world_item->item))
    {
        log_add("No space in inventory for %s.", world_item->item.name);
        return 0;
    }

    world_index = world_item_index_of(world_item);
    if(world_index >= 0)
        (void)world_item_remove(world_index);

    log_add("Picked up %s.", world_item->item.name);
    return 1;
}

static int interact_equipped_container_slot(Character* c,
                                            const int* before_empty,
                                            ItemType expected_type,
                                            const char* expected_name)
{
    if(!c || !before_empty)
        return -1;


}

int interact_transfer_world_container_to_equipped(Character* c, WorldContainer* world_container, int equipped_ci)
{
    int world_container_index;
    int moved = 0;

    if(!c || !world_container || !world_container->active)
        return 0;
    // Removed MAX_ATTACHED_CONTAINERS check; slot-based system handles bounds







    return moved;
}

static int interact_container_label_is_item_container(const char* label)
{
    const ItemTemplate* tmpl;

    if(!label || label[0] == '\0')
        return 0;

    tmpl = item_template_by_name(label);
    if(tmpl)
        return interact_item_type_is_container(tmpl->type);

    if(strstr(label, "Pouch") || strstr(label, "pouch"))
        return 1;
    if(strstr(label, "Backpack") || strstr(label, "backpack"))
        return 1;
    if(strstr(label, "Quiver") || strstr(label, "quiver"))
        return 1;

    return 0;
}

static int interact_container_item_from_label(const char* label, Item* out_item, int x, int y)
{
    const ItemTemplate* tmpl;

    if(!label || !out_item)
        return 0;

    tmpl = item_template_by_name(label);
    if(tmpl && interact_item_type_is_container(tmpl->type))
    {
        item_init_from_template(out_item, tmpl, x, y);
        return 1;
    }

    if(strstr(label, "Pouch") || strstr(label, "pouch"))
    {
        item_init(out_item, label, 'p', x, y, ITEM_TYPE_CONTAINER_POUCH, 0, 1);
        return 1;
    }

    if(strstr(label, "Quiver") || strstr(label, "quiver"))
    {
        item_init(out_item, label, '}', x, y, ITEM_TYPE_CONTAINER_QUIVER, 0, 1);
        return 1;
    }

    if(strstr(label, "Backpack") || strstr(label, "backpack"))
    {
        item_init(out_item, label, 'B', x, y, ITEM_TYPE_CONTAINER_BACKPACK, 0, 1);
        return 1;
    }

    return 0;
}

int interact_equip_container_from_ground(Player* p, WorldItem* world_item, WorldContainer* world_container)
{
    Item equip_item;
    int inventory_slot;
    int equipped_ci;
    int world_item_index = -1;
    int equip_slot = -1;

    if(!p)
        return 0;


    if(world_item && world_item->active)
    {
        equip_item = world_item->item;
        world_item_index = world_item_index_of(world_item);
    }
    else if(world_container && world_container->active)
    {
        if(!interact_container_item_from_label(world_container->label,
                                               &equip_item,
                                               p->character.actor.entity.x,
                                               p->character.actor.entity.y))
        {
            log_add("Cannot equip this container.");
            return 0;
        }
    }
    else
    {
        return 0;
    }

    if(!interact_item_type_is_container(equip_item.type))
    {
        log_add("Only container items can be equipped from ground.");
        return 0;
    }

    if(!inventory_add(&p->character, &equip_item))
    {
        log_add("Inventory full: cannot equip %s.", equip_item.name);
        return 0;
    }

    inventory_slot = -1;
    // Find the slot where the item was added
    for(int i = 0; i < p->character.equipment_slot_count; ++i) {
        if(p->character.equipment_slots[i].slot_type == EQUIP_SLOT_NONE &&
           p->character.equipment_slots[i].item.type == equip_item.type &&
           strcmp(p->character.equipment_slots[i].item.name, equip_item.name) == 0) {
            inventory_slot = i;
            break;
        }
    }
    // Find a suitable container equipment slot (dynamic or backpack).
    for(int i = 0; i < p->character.equipment_slot_count; ++i) {
        if(p->character.equipment_slots[i].item.type == ITEM_TYPE_NONE &&
           (p->character.equipment_slots[i].is_container_slot || p->character.equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_BACKPACK)) {
            equip_slot = i;
            break;
        }
    }
    if(equip_slot < 0 || !inventory_equip(&p->character, inventory_slot, equip_slot))
    {
        if(inventory_slot >= 0) (void)inventory_remove(&p->character, inventory_slot);
        log_add("Cannot equip %s right now.", equip_item.name);
        return 0;
    }

    if(world_item_index >= 0)
    {
        (void)world_item_remove(world_item_index);
    }
    else if(world_container && world_container->active)
    {
        world_container->active = 0;
    }

    return 1;
}

static int interaction_run_action(Player* p, const InteractionAction* action)
{
    if(!p || !action)
        return 0;

    switch(action->type)
    {
        case INTERACTION_ACTION_OPEN_CONTAINER:
            if(!action->world_container)
            {
                log_add("This container is empty.");
                return 0;
            }
            if(interact_open_container(p, action->world_container))
                creatures_take_turns(p);
            return 1;

        case INTERACTION_ACTION_PICK_UP_ITEM:
            // Pick up world item to inventory
            if(action->world_item) {
                if(inventory_add(&p->character, &action->world_item->item)) {
                    world_item_remove(world_item_index_of(action->world_item));
                    log_add("Picked up %s.", action->world_item->item.name);
                    creatures_take_turns(p);
                    return 1;
                } else {
                    log_add("No space in inventory for %s.", action->world_item->item.name);
                }
            }
            return 0;

        case INTERACTION_ACTION_EQUIP_FROM_GROUND:
            // Equip item from ground directly to equipment slot
            if(action->world_item) {
                int inv_slot = -1;
                char item_name[32];
                snprintf(item_name, sizeof(item_name), "%s", action->world_item->item.name);
                // Add to inventory first
                if(inventory_add(&p->character, &action->world_item->item)) {
                    // Find the slot where it was added
                    for(int i = 0; i < p->character.equipment_slot_count; ++i) {
                        if(p->character.equipment_slots[i].slot_type == EQUIP_SLOT_NONE &&
                           p->character.equipment_slots[i].item.type == action->world_item->item.type &&
                           strcmp(p->character.equipment_slots[i].item.name, item_name) == 0) {
                            inv_slot = i;
                            break;
                        }
                    }
                    if(inv_slot >= 0 && inventory_auto_equip(&p->character, inv_slot)) {
                        world_item_remove(world_item_index_of(action->world_item));
                        log_add("Equipped %s from ground.", item_name);
                        creatures_take_turns(p);
                        return 1;
                    } else {
                        log_add("Cannot equip %s right now.", item_name);
                    }
                } else {
                    log_add("No space in inventory for %s.", item_name);
                }
            }
            return 0;

        case INTERACTION_ACTION_EXAMINE_ITEM:
            if (action->world_item) {
                log_add("You examine %s.", action->world_item->item.name);
            } else if (action->world_container) {
                log_add("You examine %s.", action->world_container->label);
            } else if (action->inventory_slot >= 0) {
                log_add("You examine %s.", p->character.equipment_slots[action->inventory_slot].item.name);
            } else if (action->equipment_slot >= 0) {
                log_add("You examine equipped %s.", p->character.equipment_slots[action->equipment_slot].item.name);
            } else {
                log_add("You examine the target.");
            }
            creatures_take_turns(p);
            return 1;

        // Inventory/equipment slot-based actions (use, equip, unequip)
        // Use item (consumable)
        // NOTE: Could add INTERACTION_ACTION_USE_ITEM for clarity
        case INTERACTION_ACTION_PET:
            return interact_creature(p, action->creature);

        case INTERACTION_ACTION_FEED:
        case INTERACTION_ACTION_TREAT_INJURY:
        case INTERACTION_ACTION_TALK:
        case INTERACTION_ACTION_GIVE_ITEM:
            log_add("Not implemented yet.");
            return 0;

        case INTERACTION_ACTION_EXTINGUISH_FORGE:
            if(action->furniture && action->furniture->type == FURNITURE_FORGE && action->furniture->is_ignited)
            {
                action->furniture->is_ignited = 0;
                log_add("You extinguish the forge. %d fuel remains.", action->furniture->fuel_units);
                creatures_take_turns(p);
                return 1;
            }
            return 0;

        case INTERACTION_ACTION_TILE_USE:
            if(action->stair_delta_z != 0)
                return interact_use_stairs(p, action->tx, action->ty, action->stair_delta_z);

            if(action->furniture) {
                Furniture* furn = action->furniture;
                switch(furniture_interaction_type(furn)) {
                    case FURNITURE_INTERACTION_TOGGLE_DOOR:
                        if(furniture_toggle_door(current_area, furn->base.base.x, furn->base.base.y)) {
                            log_add(furn->is_open ? "You open the door." : "You close the door.");
                            creatures_take_turns(p);
                            return 1;
                        }
                        return 0;
                    case FURNITURE_INTERACTION_OPEN_CONTAINER:
                        {
                            WorldContainer* container = NULL;
                            if(furn->world_container_index >= 0 && furn->world_container_index < MAX_WORLD_CONTAINERS) {
                                WorldContainer* cand = &world_containers[furn->world_container_index];
                                if(cand->active && strcmp(cand->area_name, current_area->name) == 0 &&
                                   cand->x == furn->base.base.x && cand->y == furn->base.base.y &&
                                   cand->z == furn->base.base.z) {
                                    container = cand;
                                }
                            }
                            if(!container) {
                                container = world_container_at_3d(furn->base.base.x, furn->base.base.y, furn->base.base.z);
                            }
                            if(container && container->active) {
                                if(interact_open_container(p, container)) {
                                    creatures_take_turns(p);
                                    return 1;
                                }
                            }
                            return 0;
                        }
                    case FURNITURE_INTERACTION_INSPECT:
                        if(furn->type == FURNITURE_FORGE)
                            return interact_use_forge(p, furn);

                        if(furniture_is_destructible(furn))
                        {
                            log_add("You inspect %s. Hardness %d, structure %d/%d.",
                                    furniture_display_name(furn),
                                    furniture_hardness(furn),
                                    furniture_current_structure_points(furn),
                                    furniture_max_structure_points(furn));
                        }
                        else
                        {
                            log_add("You inspect %s.", furniture_display_name(furn));
                        }
                        creatures_take_turns(p);
                        return 1;
                    case FURNITURE_INTERACTION_SIT:
                        log_add("You sit on the chair.");
                        creatures_take_turns(p);
                        return 1;
                    case FURNITURE_INTERACTION_READ_SIGN: {
                        int area_index = interact_current_area_index();
                        int pz = p->character.actor.entity.z;
                        SignpostInstance* signpost;
                        int learned_new_location = 0;
                        int read_count = 0;
                        if(area_index < 0) {
                            log_add("This signpost cannot be read right now.");
                            return 0;
                        }
                        signpost = world_map_signpost_at_mut(area_index, furn->base.base.x, furn->base.base.y, pz);
                        if(!signpost || signpost->sign_count <= 0) {
                            log_add("The signpost is weathered and unreadable.");
                            creatures_take_turns(p);
                            return 1;
                        }
                        for(int i = 0; i < signpost->sign_count; i++) {
                            int destination = signpost->signs[i].destination_index;
                            const char* destination_name;
                            if(destination < 0 || destination >= atlas_location_count)
                                continue;
                            if(atlas_get_knowledge(destination) < LOCATION_KNOWLEDGE_AWARE)
                                learned_new_location = 1;
                            atlas_upgrade_knowledge(destination, LOCATION_KNOWLEDGE_AWARE);
                            atlas_add_location_hint(destination, signpost->signs[i].hint_text);
                            destination_name = atlas[destination].name;
                            log_add("%s - %s",
                                    signpost->signs[i].direction[0] ? signpost->signs[i].direction : "Route",
                                    (destination_name && destination_name[0]) ? destination_name : "Unknown");
                            read_count++;
                        }
                        signpost->visited = 1;
                        if(read_count <= 0) {
                            log_add("The signpost has no useful destination markings.");
                            creatures_take_turns(p);
                            return 1;
                        }
                        if(learned_new_location)
                            log_add("You mark the signposted locations on your atlas.");
                        creatures_take_turns(p);
                        return 1;
                    }
                    case FURNITURE_INTERACTION_REST:
                        log_add("You rest on the bed.");
                        creatures_take_turns(p);
                        return 1;
                    case FURNITURE_INTERACTION_NONE:
                    default:
                        break;
                }
            }
            return interact_tile(p, action->tx, action->ty);

        default:
            // Handle unequip/equip/use for slot-based actions
            if(action->inventory_slot >= 0) {
                // Use if consumable (but not ammo)
                Item* item = &p->character.equipment_slots[action->inventory_slot].item;
                if(item->type == ITEM_TYPE_CONSUMABLE && !item->is_ammo) {
                    if(inventory_use(&p->character, action->inventory_slot)) {
                        log_add("Used %s.", item->name);
                        creatures_take_turns(p);
                        return 1;
                    } else {
                        log_add("Failed to use %s.", item->name);
                    }
                } else if(item->is_ammo) {
                    log_add("%s is ammo for ranged weapons.", item->name);
                } else {
                    char item_name[32];
                    snprintf(item_name, sizeof(item_name), "%s", item->name);
                    if(inventory_auto_equip(&p->character, action->inventory_slot)) {
                        log_add("Equipped %s.", item_name);
                        creatures_take_turns(p);
                        return 1;
                    } else {
                        log_add("Failed to equip %s.", item_name);
                    }
                }
            } else if(action->equipment_slot >= 0) {
                // Unequip
                EquipmentSlot* eq = &p->character.equipment_slots[action->equipment_slot];
                if(eq->item.type != ITEM_TYPE_NONE) {
                    if(inventory_unequip_slot(&p->character, eq->slot_type)) {
                        log_add("Unequipped %s.", eq->item.name);
                        creatures_take_turns(p);
                        return 1;
                    } else {
                        log_add("Failed to unequip %s.", eq->item.name);
                    }
                }
            }
            return 0;
    }
}

static void interaction_collect_actions(Player* p,
                                        int tx,
                                        int ty,
                                        Creature* creature,
                                        Tile* tile,
                                        WorldItem* world_item,
                                        WorldContainer* world_container,
                                        InteractionAction* actions,
                                        int* action_count)
{
    if(!p || !actions || !action_count)
        return;

    // --- Creature interactions (unchanged for now) ---
    if(creature && creature->alive && creature->template)
    {
        int hostile = creature_is_hostile(creature);
        int is_animal = creature->template->tamable ? 1 : 0;
        int is_character = (!hostile && !is_animal) ? 1 : 0;
        int is_injured_neutral = (!hostile && creature->actor.health < creature->actor.max_health) ? 1 : 0;

        if(is_animal)
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_PET,
                                   !hostile,
                                   "Pet",
                                   hostile ? "Too dangerous while hostile" : "",
                                   creature, NULL, NULL, tx, ty);
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_FEED,
                                   0,
                                   "Feed",
                                   "Not implemented yet",
                                   creature, NULL, NULL, tx, ty);
        }
        if(is_injured_neutral)
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_TREAT_INJURY,
                                   0,
                                   "Treat injury",
                                   "Not implemented yet",
                                   creature, NULL, NULL, tx, ty);
        }
        if(is_character)
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_TALK,
                                   0,
                                   "Talk",
                                   "Not implemented yet",
                                   creature, NULL, NULL, tx, ty);
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_GIVE_ITEM,
                                   0,
                                   "Give item",
                                   "Not implemented yet",
                                   creature, NULL, NULL, tx, ty);
        }
    }

    Furniture* furn = furniture_at(current_area, tx, ty);

    // --- Ground/container/furniture interactions only ---
    // Inventory and equipment actions belong in the inventory UI, not the world interaction menu.

    // Ground/world item
    if(world_item && world_item->active) {
        InteractionAction a = {0};
        a.type = INTERACTION_ACTION_PICK_UP_ITEM;
        a.enabled = 1;
        snprintf(a.label, sizeof(a.label), "Pick up %s", world_item->item.name);
        a.world_item = world_item;
        a.source_type = 3; // ground
        actions[(*action_count)++] = a;

        // Equip directly from ground if possible
        a.type = INTERACTION_ACTION_EQUIP_FROM_GROUND;
        a.enabled = 1;
        snprintf(a.label, sizeof(a.label), "Equip %s from ground", world_item->item.name);
        actions[(*action_count)++] = a;

        // Examine
        a.type = INTERACTION_ACTION_EXAMINE_ITEM;
        a.enabled = 1;
        snprintf(a.label, sizeof(a.label), "Examine %s", world_item->item.name);
        actions[(*action_count)++] = a;
    }

    // Standalone world containers (skip duplicates when a furniture entity already owns this container)
    if(world_container && world_container->active && !interact_furniture_owns_world_container(furn, world_container)) {
        InteractionAction a = {0};
        a.type = INTERACTION_ACTION_OPEN_CONTAINER;
        a.enabled = 1;
        snprintf(a.label, sizeof(a.label), "Open %s", world_container->label);
        a.world_container = world_container;
        a.source_type = 4; // container
        actions[(*action_count)++] = a;

        if(interact_container_label_is_item_container(world_container->label))
        {
            a.type = INTERACTION_ACTION_EQUIP_FROM_GROUND;
            a.enabled = 1;
            snprintf(a.label, sizeof(a.label), "Equip %s", world_container->label);
            actions[(*action_count)++] = a;
        }

        // Examine
        a.type = INTERACTION_ACTION_EXAMINE_ITEM;
        a.enabled = 1;
        snprintf(a.label, sizeof(a.label), "Examine %s", world_container->label);
        actions[(*action_count)++] = a;
    }

    // Furniture interactions (new entity layer)
    if(furn && furn->type != FURNITURE_NONE)
    {
        InteractionAction a = {0};
        a.type = INTERACTION_ACTION_TILE_USE;
        a.enabled = (furn->interactable ? 1 : 0);
        a.furniture = furn;
        a.tx = tx;
        a.ty = ty;

        switch(furniture_interaction_type(furn))
        {
            case FURNITURE_INTERACTION_TOGGLE_DOOR:
            case FURNITURE_INTERACTION_READ_SIGN:
            case FURNITURE_INTERACTION_REST:
            case FURNITURE_INTERACTION_INSPECT:
            case FURNITURE_INTERACTION_SIT:
                furniture_get_interaction_label(furn, a.label, sizeof(a.label));
                actions[(*action_count)++] = a;
                if(furn->type == FURNITURE_FORGE && furn->is_ignited && *action_count < INTERACTION_ACTIONS_MAX)
                {
                    InteractionAction forge_action = {0};
                    forge_action.type = INTERACTION_ACTION_EXTINGUISH_FORGE;
                    forge_action.enabled = 1;
                    forge_action.furniture = furn;
                    forge_action.tx = tx;
                    forge_action.ty = ty;
                    snprintf(forge_action.label, sizeof(forge_action.label), "Extinguish forge");
                    actions[(*action_count)++] = forge_action;
                }
                break;
            case FURNITURE_INTERACTION_OPEN_CONTAINER:
            {
                const char* container_name = furniture_container_label_for_type(furn->type);
                WorldContainer* wc = NULL;
                if(furn->world_container_index >= 0 && furn->world_container_index < MAX_WORLD_CONTAINERS) {
                    WorldContainer* cand = &world_containers[furn->world_container_index];
                    if(cand->active && strcmp(cand->area_name, current_area->name) == 0 &&
                       cand->x == furn->base.base.x && cand->y == furn->base.base.y &&
                       cand->z == furn->base.base.z)
                    {
                        wc = cand;
                    }
                }
                if(!wc) {
                    wc = world_container_at_3d(furn->base.base.x, furn->base.base.y, furn->base.base.z);
                }

                if(wc && wc->active) {
                    a.enabled = 1;
                    if(wc->label[0])
                        container_name = wc->label;
                } else {
                    a.enabled = 0;
                    snprintf(a.disabled_reason, sizeof(a.disabled_reason), "No container present");
                }
                snprintf(a.label, sizeof(a.label), "Open %s", container_name);
                actions[(*action_count)++] = a;
                break;
            }
            case FURNITURE_INTERACTION_NONE:
            default:
                break;
        }
    }

    // Tile-based interactions (doors, stairs, etc.)
    if(tile) {
        if(tile_is_door(tile)) {
            InteractionAction a = {0};
            a.type = INTERACTION_ACTION_TILE_USE;
            a.enabled = 1;
            a.tx = tx;
            a.ty = ty;
            snprintf(a.label, sizeof(a.label), tile->blocks_movement ? "Open door" : "Close door");
            actions[(*action_count)++] = a;
        } else if(tile_is_staircase(tile)) {
            if(*action_count < INTERACTION_ACTIONS_MAX)
            {
                InteractionAction a = {0};
                a.type = INTERACTION_ACTION_TILE_USE;
                a.enabled = stairs_can_move(p, tx, ty, 1);
                a.tx = tx;
                a.ty = ty;
                a.stair_delta_z = 1;
                if(!a.enabled)
                    snprintf(a.disabled_reason, sizeof(a.disabled_reason), "Already at top floor");
                snprintf(a.label, sizeof(a.label), "Go up");
                actions[(*action_count)++] = a;
            }

            if(*action_count < INTERACTION_ACTIONS_MAX)
            {
                InteractionAction a = {0};
                a.type = INTERACTION_ACTION_TILE_USE;
                a.enabled = stairs_can_move(p, tx, ty, -1);
                a.tx = tx;
                a.ty = ty;
                a.stair_delta_z = -1;
                if(!a.enabled)
                    snprintf(a.disabled_reason, sizeof(a.disabled_reason), "Already at ground floor");
                snprintf(a.label, sizeof(a.label), "Go down");
                actions[(*action_count)++] = a;
            }
        } else if(strcmp(tile->name, "Signpost") == 0) {
            InteractionAction a = {0};
            a.type = INTERACTION_ACTION_TILE_USE;
            a.enabled = 1;
            a.tx = tx;
            a.ty = ty;
            snprintf(a.label, sizeof(a.label), "Read signpost");
            actions[(*action_count)++] = a;
        } else if(strstr(tile->name, "Switch")) {
            InteractionAction a = {0};
            a.type = INTERACTION_ACTION_TILE_USE;
            a.enabled = 1;
            a.tx = tx;
            a.ty = ty;
            snprintf(a.label, sizeof(a.label), "Inspect switch");
            actions[(*action_count)++] = a;
        }
    }
}

// Try interacting with tile-level interactables (doors now, extensible for switches/containers).
static int interact_tile(Player* p, int tx, int ty)
{
    Tile* tile;
    WorldContainer* any_container;

    if(!p || !current_area)
        return 0;
    if(tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return 0;

    tile = map_tile_at_layer(current_area, tx, ty, TILE_LAYER_WALL);

    any_container = world_container_at_3d(tx, ty, p->character.actor.entity.z);
    if(any_container)
    {
        if(interact_open_container(p, any_container))
        {
            creatures_take_turns(p);
            return 1;
        }
        return 0;
    }

    {
        Furniture* furn = furniture_at(current_area, tx, ty);
        if(furn)
        {
            switch(furniture_interaction_type(furn))
            {
                case FURNITURE_INTERACTION_TOGGLE_DOOR:
                    if(furniture_toggle_door(current_area, tx, ty))
                    {
                        log_add(furn->is_open ? "You open the door." : "You close the door.");
                        creatures_take_turns(p);
                        return 1;
                    }
                    break;
                case FURNITURE_INTERACTION_READ_SIGN:
                {
                    int area_index = interact_current_area_index();
                    int pz = p->character.actor.entity.z;
                    SignpostInstance* signpost;
                    int learned_new_location = 0;
                    int read_count = 0;

                    if(area_index < 0)
                    {
                        log_add("This signpost cannot be read right now.");
                        return 0;
                    }

                    signpost = world_map_signpost_at_mut(area_index, tx, ty, pz);
                    if(!signpost || signpost->sign_count <= 0)
                    {
                        log_add("The signpost is weathered and unreadable.");
                        creatures_take_turns(p);
                        return 1;
                    }

                    for(int i = 0; i < signpost->sign_count; i++)
                    {
                        int destination = signpost->signs[i].destination_index;
                        const char* destination_name;

                        if(destination < 0 || destination >= atlas_location_count)
                            continue;

                        if(atlas_get_knowledge(destination) < LOCATION_KNOWLEDGE_AWARE)
                            learned_new_location = 1;

                        atlas_upgrade_knowledge(destination, LOCATION_KNOWLEDGE_AWARE);
                        atlas_add_location_hint(destination, signpost->signs[i].hint_text);

                        destination_name = atlas[destination].name;
                        log_add("%s - %s",
                                signpost->signs[i].direction[0] ? signpost->signs[i].direction : "Route",
                                (destination_name && destination_name[0]) ? destination_name : "Unknown");
                        read_count++;
                    }

                    signpost->visited = 1;

                    if(read_count <= 0)
                    {
                        log_add("The signpost has no useful destination markings.");
                        creatures_take_turns(p);
                        return 1;
                    }
                    if(learned_new_location)
                        log_add("You mark the signposted locations on your atlas.");

                    creatures_take_turns(p);
                    return 1;
                }
                case FURNITURE_INTERACTION_INSPECT:
                    if(furn->type == FURNITURE_FORGE)
                        return interact_use_forge(p, furn);

                    if(furniture_is_destructible(furn))
                    {
                        log_add("You inspect %s. Hardness %d, structure %d/%d.",
                                furniture_display_name(furn),
                                furniture_hardness(furn),
                                furniture_current_structure_points(furn),
                                furniture_max_structure_points(furn));
                    }
                    else
                    {
                        log_add("You inspect %s.", furniture_display_name(furn));
                    }
                    creatures_take_turns(p);
                    return 1;
                case FURNITURE_INTERACTION_SIT:
                    log_add("You sit on the chair.");
                    creatures_take_turns(p);
                    return 1;
                case FURNITURE_INTERACTION_REST:
                    log_add("You rest on the bed.");
                    creatures_take_turns(p);
                    return 1;
                case FURNITURE_INTERACTION_OPEN_CONTAINER:
                case FURNITURE_INTERACTION_NONE:
                default:
                    break;
            }
        }
    }

    if(!tile)
        return 0;

    if(!tile->interactable)
        return 0;

    if(tile_is_door(tile))
    {
        TileMutationState next_state = tile->blocks_movement ? TILE_MUTATION_STATE_DOOR_OPEN : TILE_MUTATION_STATE_DOOR_CLOSED;

        if(!atlas_set_tile_mutation(current_area, tx, ty, next_state))
        {
            log_add("Failed to toggle door at %d,%d.", tx, ty);
            return 0;
        }

        if(next_state == TILE_MUTATION_STATE_DOOR_OPEN)
            log_add("You open the door.");
        else
            log_add("You close the door.");

        creatures_take_turns(p);
        return 1;
    }

    if(tile_is_staircase(tile))
    {
        int preferred_dz = (tile->symbol == '>') ? -1 : 1;

        if(stairs_can_move(p, tx, ty, preferred_dz))
            return interact_use_stairs(p, tx, ty, preferred_dz);
        if(stairs_can_move(p, tx, ty, -preferred_dz))
            return interact_use_stairs(p, tx, ty, -preferred_dz);

        log_add("The staircase does not lead anywhere from here.");
        return 0;
    }

    if(strstr(tile->name, "Switch"))
    {
        log_add("You inspect the switch, but it is not wired yet.");
        return 0;
    }

    if(strstr(tile->name, "Chest") || strstr(tile->name, "Container"))
    {
        int pz = p->character.actor.entity.z;
        WorldContainer* container = world_container_at_3d(tx, ty, pz);
        if(!container)
        {
            log_add("This container is empty.");
            return 1;
        }

        if(interact_open_container(p, container))
            creatures_take_turns(p);
        return 1;
    }

    if(strcmp(tile->name, "Signpost") == 0)
    {
        int area_index = interact_current_area_index();
        int pz = p->character.actor.entity.z;
        SignpostInstance* signpost;
        int learned_new_location = 0;
        int read_count = 0;

        if(area_index < 0)
        {
            log_add("This signpost cannot be read right now.");
            return 0;
        }

        signpost = world_map_signpost_at_mut(area_index, tx, ty, pz);
        if(!signpost || signpost->sign_count <= 0)
        {
            log_add("The signpost is weathered and unreadable.");
            creatures_take_turns(p);
            return 1;
        }

        for(int i = 0; i < signpost->sign_count; i++)
        {
            int destination = signpost->signs[i].destination_index;
            const char* destination_name;

            if(destination < 0 || destination >= atlas_location_count)
                continue;

            if(atlas_get_knowledge(destination) < LOCATION_KNOWLEDGE_AWARE)
                learned_new_location = 1;

            atlas_upgrade_knowledge(destination, LOCATION_KNOWLEDGE_AWARE);
            atlas_add_location_hint(destination, signpost->signs[i].hint_text);

            destination_name = atlas[destination].name;
            log_add("%s - %s",
                    signpost->signs[i].direction[0] ? signpost->signs[i].direction : "Route",
                    (destination_name && destination_name[0]) ? destination_name : "Unknown");
            read_count++;
        }

        signpost->visited = 1;

        if(read_count <= 0)
        {
            log_add("The signpost has no useful destination markings.");
            creatures_take_turns(p);
            return 1;
        }
        if(learned_new_location)
            log_add("You mark the signposted locations on your atlas.");
        creatures_take_turns(p);
        return 1;
    }

    log_add("Nothing obvious happens when you interact with %s.", tile->name);
    return 0;
}

int interact_at(Player* p, int tx, int ty)
{
    // int px, py; // Removed, use p->character.actor.entity.x/y directly
    int max_range;
    const char* target_name;
    Creature* creature;
    Tile* tile;
    WorldItem* world_item;
    WorldContainer* world_container;
    InteractionAction actions[INTERACTION_ACTIONS_MAX];
    int action_count = 0;
    int selected_action;

    if(!p || !current_area)
        return 0;


    if(!map_has_line_of_sight(p->character.actor.entity.x, p->character.actor.entity.y, tx, ty))
    {
        log_add("Cannot interact at %d,%d: out of sight.", tx, ty);
        return 0;
    }

    max_range = interact_max_range();
    target_name = interact_target_name_at(tx, ty);
    if(target_name && !interact_in_range(p->character.actor.entity.x, p->character.actor.entity.y, tx, ty, max_range))
    {
        log_add("You are too far away to interact with %s", target_name);
        return 0;
    }

    creature = bestiary_creature_at_3d(tx, ty, p->character.actor.entity.z);
    tile = map_tile_at_layer(current_area, tx, ty, TILE_LAYER_WALL);
    world_item = world_item_at_3d(tx, ty, p->character.actor.entity.z);
    world_container = world_container_at_3d(tx, ty, p->character.actor.entity.z);

    interaction_collect_actions(p, tx, ty, creature, tile, world_item, world_container, actions, &action_count);

    if(action_count <= 0)
    {
        log_add("Nothing to interact with at %d,%d.", tx, ty);
        return 0;
    }

    selected_action = interaction_show_menu(p, target_name, actions, action_count);
    if(selected_action < 0)
    {
        log_add("Interaction canceled.");
        return 0;
    }

    if(interaction_run_action(p, &actions[selected_action]))
        return 1;

    log_add("Nothing to interact with at %d,%d.", tx, ty);
    return 0;
}

// Prompt player for direction and attempt interaction in that direction, or same tile if space/enter.
// If player has a valid target lock, auto-interact with that target instead of prompting.
void quick_interact(Player* p)
{
    TargetLockResolved resolved;
    int dx = 0;
    int dy = 0;
    int target_x;
    int target_y;
    int key;

    if(!p)
        return;

    // Check if player has a valid target lock
    if(p->target_lock.active && target_lock_resolve_live(p, &resolved, 0))
    {
        // Valid lock: auto-interact with the locked target
        (void)interact_at(p, resolved.x, resolved.y);
        return;
    }

    // Target lock is invalid: determine reason and handle accordingly
    if(p->target_lock.active)
    {
        int is_permanent = 0;
        
        // Check if lock is in a different area (temporary condition - preserve lock)
        if(!current_area || strcmp(p->target_lock.area_name, current_area->name) != 0)
        {
            log_add("Your locked target has left the area.");
        }
        // Check if lock points to a dead creature (permanent condition - clear lock)
        else if(p->target_lock.kind == TARGET_LOCK_CREATURE)
        {
            int index = p->target_lock.slot_index;
            if(index >= 0 && index < MAX_CREATURES && (!creatures[index].alive || !creatures[index].template))
            {
                log_add("Your target is no longer alive.");
                is_permanent = 1;
            }
        }
        // Check if lock points to a despawned item (permanent condition - clear lock)
        else if(p->target_lock.kind == TARGET_LOCK_WORLD_ITEM)
        {
            int index = p->target_lock.slot_index;
            if(index >= 0 && index < MAX_WORLD_ITEMS && (!world_items[index].active || world_items[index].item.type == ITEM_TYPE_NONE))
            {
                log_add("Your target item no longer exists.");
                is_permanent = 1;
            }
        }
        
        // Clear lock only for permanent conditions
        if(is_permanent)
            target_lock_clear(p);
        
        return;
    }

    // No target lock: show direction prompt as before
    log_add("Interact: w/up=up, s/down=down, a/left=left, d/right=right, space/enter=here, q/esc=cancel");
    key = read_input_key();

    switch(key)
    {
        case 'w': case 'W': case INPUT_KEY_UP:
            dy = -1;
            break;
        case 's': case 'S': case INPUT_KEY_DOWN:
            dy = +1;
            break;
        case 'a': case 'A': case INPUT_KEY_LEFT:
            dx = -1;
            break;
        case 'd': case 'D': case INPUT_KEY_RIGHT:
            dx = +1;
            break;
        case ' ':
        case 13:  // space or enter - interact with same tile
            dx = 0;
            dy = 0;
            break;
        case 'q': case 'Q': case 27:  // escape
            log_add("Interaction canceled.");
            return;
        default:
            log_add("Invalid direction.");
            return;
    }

    target_x = p->character.actor.entity.x + dx;
    target_y = p->character.actor.entity.y + dy;
    (void)interact_at(p, target_x, target_y);
}
