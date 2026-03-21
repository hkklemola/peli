#include "interact.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "atlas.h"
#include "bestiary.h"
#include "log.h"
#include "map.h"
#include "movement.h"
#include "world_items.h"

// Return max inspect interaction range in tiles.
static int interact_max_range(void)
{
    return INTERACT_RANGE_DEFAULT;
}

// Return 1 when target is within interaction range.
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
    Tile* tile;
    WorldItem* world_item;

    if(creature && creature->alive && creature->template)
        return creature->template->name;

    if(!current_area || tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return NULL;

    tile = &current_area->map[ty][tx];
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

    if(tile->symbol == '+' || tile->symbol == '/')
        return 1;
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

// Try interacting with tile-level interactables (doors now, extensible for switches/containers).
static int interact_tile(Player* p, int tx, int ty)
{
    Tile* tile;

    if(!p || !current_area)
        return 0;
    if(tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return 0;

    tile = &current_area->map[ty][tx];

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
        log_add("You inspect the container, but it cannot be opened yet.");
        return 0;
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
