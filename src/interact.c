#include "interact.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "atlas.h"
#include "bestiary.h"
#include "draw.h"
#include "input.h"
#include "inventory.h"
#include "log.h"
#include "map.h"
#include "movement.h"
#include "ui_overlay.h"
#include "world_items.h"

/**
 * @file interact.c
 * @brief Implementation of world object interaction system (doors, items, creatures).
 *
 * Handles interaction range checking, priority ordering of interactive targets,
 * and execution of interactions like opening doors, picking up items, or examining creatures.
 */

/**
 * @brief Get the maximum interaction range in tiles.
 * @return The default interaction range (currently INTERACT_RANGE_DEFAULT from header).
 */
static int interact_max_range(void)
{
    return INTERACT_RANGE_DEFAULT;
}

/**
 * @brief Check if a target is within interaction range using Chebyshev distance.
 * @param px The player's x-coordinate.
 * @param py The player's y-coordinate.
 * @param tx The target's x-coordinate.
 * @param ty The target's y-coordinate.
 * @param range The maximum interaction range in tiles.
 * @return 1 if target is within range, 0 otherwise.
 * @note Uses Chebyshev distance (max of absolute differences), allowing diagonal interactions.
 */
static int interact_in_range(int px, int py, int tx, int ty, int range)
{
    int dx = abs(tx - px);
    int dy = abs(ty - py);
    return dx <= range && dy <= range;
}

// Resolve user-facing target name in interaction priority order.
static const char* interact_target_name_at(int tx, int ty)
{
    Creature* creature = bestiary_creature_at(tx, ty);
    const Tile* tile;
    WorldItem* world_item;

    if(creature && creature->alive && creature->template)
        return creature->template->name;

    if(!current_area || tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return NULL;

    tile = map_top_visible_tile(current_area, tx, ty, NULL);
    if(tile->interactable && tile->name[0])
        return tile->name;

    world_item = world_item_at(tx, ty);
    if(world_item && world_item->active)
        return world_item->item.name;

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

// Try interacting with a creature first, per inspect interaction priority.
static int interact_creature(Player* p, Creature* creature)
{
    if(!p || !creature || !creature->alive || !creature->template)
        return 0;

    if(creature->template->is_hostile)
    {
        log_add("%s is hostile. Maybe use attack instead.", creature->template->name);
        return 0;
    }

    log_add("You pet the %s.", creature->template->name);
    creatures_take_turns(p);
    return 1;
}

// Open one world container and allow item pickup via overlay.
static int interact_open_container(Player* p, WorldContainer* container)
{
    int selected = 0;
    int took_any = 0;

    if(!p || !container || !container->active)
        return 0;

    while(1)
    {
        char title[96];
        int content_lines;
        int status_line;
        int line_i = 0;

        draw_world(p);
        snprintf(title, sizeof(title), "Dev Hut - %s", container->label);
        ui_overlay_draw_frame(title);

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;

        if(container->item_count <= 0)
        {
            if(line_i < status_line) ui_overlay_draw_line(line_i++, "This chest is empty.");
            while(line_i < status_line) ui_overlay_draw_line(line_i++, "");
            ui_overlay_draw_line(status_line, "Esc/Q close | Enter take selected | W/S move");
            ui_overlay_draw_global_hotkeys();
        }
        else
        {
            if(selected < 0) selected = 0;
            if(selected >= container->item_count) selected = container->item_count - 1;

            for(int i = 0; i < container->item_count && line_i < status_line; i++)
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

            ui_overlay_draw_line(status_line, "Esc/Q close | Enter take selected | W/S move");
            ui_overlay_draw_global_hotkeys();
        }

        {
            int key = read_input_key();

            if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
                break;

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

            if(key == 13)
            {
                Item picked_item;
                int container_index = world_container_index_of(container);

                if(container_index < 0)
                    continue;

                if(world_container_remove_item(container_index, selected, &picked_item))
                {
                    if(inventory_add(&p->character, &picked_item))
                    {
                        log_add("You take %s from %s.", picked_item.name, container->label);
                        took_any = 1;

                        if(selected >= container->item_count)
                            selected = container->item_count - 1;
                        if(selected < 0)
                            selected = 0;
                    }
                    else
                    {
                        log_add("No space in inventory for %s.", picked_item.name);
                        (void)world_container_add_item(container_index, &picked_item);
                    }
                }
            }
        }
    }

    return took_any;
}

// Try interacting with tile-level interactables (doors now, extensible for switches/containers).
static int interact_tile(Player* p, int tx, int ty)
{
    Tile* tile;

    if(!p || !current_area)
        return 0;
    if(tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return 0;

    tile = map_tile_at_layer(current_area, tx, ty, TILE_LAYER_STRUCTURE);
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

    if(strstr(tile->name, "Switch"))
    {
        log_add("You inspect the switch, but it is not wired yet.");
        return 0;
    }

    if(strstr(tile->name, "Chest") || strstr(tile->name, "Container"))
    {
        WorldContainer* container = world_container_at(tx, ty);
        if(!container)
        {
            log_add("This chest is empty.");
            return 1;
        }

        if(interact_open_container(p, container))
            creatures_take_turns(p);
        return 1;
    }

    if(strcmp(tile->name, "Signpost") == 0)
    {
        int learned_new_location = 0;

        if(atlas_get_knowledge(4) < LOCATION_KNOWLEDGE_AWARE)
            learned_new_location = 1;
        if(atlas_get_knowledge(5) < LOCATION_KNOWLEDGE_AWARE)
            learned_new_location = 1;
        if(atlas_get_knowledge(6) < LOCATION_KNOWLEDGE_AWARE)
            learned_new_location = 1;
        if(atlas_get_knowledge(7) < LOCATION_KNOWLEDGE_AWARE)
            learned_new_location = 1;

        atlas_upgrade_knowledge(4, LOCATION_KNOWLEDGE_AWARE);
        atlas_upgrade_knowledge(5, LOCATION_KNOWLEDGE_AWARE);
        atlas_upgrade_knowledge(6, LOCATION_KNOWLEDGE_AWARE);
        atlas_upgrade_knowledge(7, LOCATION_KNOWLEDGE_AWARE);

        atlas_add_location_hint(4, "Signpost: North route points to Old Mine.");
        atlas_add_location_hint(5, "Signpost: East route points to Castle Ruins.");
        atlas_add_location_hint(6, "Signpost: South route points to Village.");
        atlas_add_location_hint(7, "Signpost: West route points to Forest Lake.");

        log_add("North - Old Mine");
        log_add("East - Castle Ruins");
        log_add("South - Village");
        log_add("West - Forest Lake");
        if(learned_new_location)
            log_add("You mark the signposted locations on your atlas.");
        creatures_take_turns(p);
        return 1;
    }

    log_add("Nothing obvious happens when you interact with %s.", tile->name);
    return 0;
}

// Try interacting with world item at tile (placeholder behavior).
static int interact_world_item(Player* p, WorldItem* world_item)
{
    if(!p || !world_item || !world_item->active)
        return 0;

    log_add("You examine %s.", world_item->item.name);
    creatures_take_turns(p);
    return 1;
}

int inspect_interact_at(Player* p, int tx, int ty)
{
    int px;
    int py;
    int max_range;
    const char* target_name;
    Creature* creature;
    WorldItem* world_item;

    if(!p || !current_area)
        return 0;

    px = p->character.actor.entity.x;
    py = p->character.actor.entity.y;

    if(!map_has_line_of_sight(px, py, tx, ty))
    {
        log_add("Cannot interact at %d,%d: out of sight.", tx, ty);
        return 0;
    }

    max_range = interact_max_range();
    target_name = interact_target_name_at(tx, ty);
    if(target_name && !interact_in_range(px, py, tx, ty, max_range))
    {
        log_add("You are too far away to interact with %s", target_name);
        return 0;
    }

    creature = bestiary_creature_at(tx, ty);
    if(interact_creature(p, creature))
        return 1;

    if(interact_tile(p, tx, ty))
        return 1;

    world_item = world_item_at(tx, ty);
    if(interact_world_item(p, world_item))
        return 1;

    log_add("Nothing to interact with at %d,%d.", tx, ty);
    return 0;
}

// Prompt player for direction and attempt interaction in that direction, or same tile if space/enter.
void quick_interact(Player* p)
{
    int dx = 0;
    int dy = 0;
    int target_x;
    int target_y;
    int key;

    if(!p)
        return;

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
    (void)inspect_interact_at(p, target_x, target_y);
}
