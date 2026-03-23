#include "target_lock.h"

#include <stdio.h>
#include <string.h>

#include "atlas.h"
#include "bestiary.h"
#include "log.h"
#include "map.h"
#include "world_items.h"

/**
 * @file target_lock.c
 * @brief Implementation of target lock management for quick-access hotkey bindings.
 *
 * Provides functions for setting, validating, and resolving target locks that persist
 * across game state changes, enabling players to quick-access frequently-used targets
 * (creatures, items) via hotkeys even after area transitions.
 */

const char* target_lock_kind_name(TargetLockKind kind)
{
    switch(kind)
    {
        case TARGET_LOCK_CREATURE: return "creature";
        case TARGET_LOCK_WORLD_ITEM: return "item";
        default: return "none";
    }
}

void target_lock_describe(const TargetLockResolved* lock, char* out, int out_size)
{
    if(!out || out_size <= 0)
        return;

    if(!lock || !lock->name)
    {
        snprintf(out, (size_t)out_size, "none");
        return;
    }

    snprintf(out, (size_t)out_size, "%s %s (%d,%d,%d)", target_lock_kind_name(lock->kind), lock->name, lock->x, lock->y, lock->z);
}

static int target_lock_area_matches_current(const char* area_name)
{
    if(!current_area || !area_name)
        return 0;

    return strcmp(area_name, current_area->name) == 0;
}

void target_lock_clear(Player* p)
{
    if(!p)
        return;

    p->target_lock.active = 0;
    p->target_lock.kind = TARGET_LOCK_NONE;
    p->target_lock.slot_index = -1;
    p->target_lock.z = AREA_GROUND_Z;
    p->target_lock.area_name[0] = '\0';
}

int target_lock_set_creature(Player* p, int slot_index, const char* area_name)
{
    if(!p || !area_name || slot_index < 0 || slot_index >= MAX_CREATURES)
        return 0;

    p->target_lock.active = 1;
    p->target_lock.kind = TARGET_LOCK_CREATURE;
    p->target_lock.slot_index = slot_index;
    p->target_lock.z = creatures[slot_index].actor.entity.z;
    snprintf(p->target_lock.area_name, sizeof(p->target_lock.area_name), "%s", area_name);
    return 1;
}

int target_lock_set_world_item(Player* p, int slot_index, const char* area_name)
{
    if(!p || !area_name || slot_index < 0 || slot_index >= MAX_WORLD_ITEMS)
        return 0;

    p->target_lock.active = 1;
    p->target_lock.kind = TARGET_LOCK_WORLD_ITEM;
    p->target_lock.slot_index = slot_index;
    p->target_lock.z = world_items[slot_index].item.entity.z;
    snprintf(p->target_lock.area_name, sizeof(p->target_lock.area_name), "%s", area_name);
    return 1;
}

int target_lock_matches_creature(const Player* p, int slot_index, const char* area_name)
{
    if(!p || !p->target_lock.active || !area_name)
        return 0;

    return p->target_lock.kind == TARGET_LOCK_CREATURE &&
           p->target_lock.slot_index == slot_index &&
           strcmp(p->target_lock.area_name, area_name) == 0;
}

int target_lock_matches_world_item(const Player* p, int slot_index, const char* area_name)
{
    if(!p || !p->target_lock.active || !area_name)
        return 0;

    return p->target_lock.kind == TARGET_LOCK_WORLD_ITEM &&
           p->target_lock.slot_index == slot_index &&
           strcmp(p->target_lock.area_name, area_name) == 0;
}

int target_lock_resolve(Player* p, TargetLockResolved* out, int clear_invalid)
{
    int lock_invalid = 0;

    if(!p || !p->target_lock.active)
        return 0;

    if(!target_lock_area_matches_current(p->target_lock.area_name))
        return 0;

    if(p->target_lock.kind == TARGET_LOCK_CREATURE)
    {
        int index = p->target_lock.slot_index;
        if(index < 0 || index >= MAX_CREATURES)
        {
            lock_invalid = 1;
        }
        else
        {
            Creature* creature = &creatures[index];
            if(!creature->alive || !creature->template)
            {
                lock_invalid = 1;
            }
            else if(out)
            {
                out->kind = TARGET_LOCK_CREATURE;
                out->slot_index = index;
                out->name = creature->template->name;
                out->x = creature->actor.entity.x;
                out->y = creature->actor.entity.y;
                out->z = creature->actor.entity.z;
            }
        }
    }
    else if(p->target_lock.kind == TARGET_LOCK_WORLD_ITEM)
    {
        int index = p->target_lock.slot_index;
        if(index < 0 || index >= MAX_WORLD_ITEMS)
        {
            lock_invalid = 1;
        }
        else
        {
            WorldItem* world_item = &world_items[index];
            if(!world_item->active ||
               strcmp(world_item->area_name, p->target_lock.area_name) != 0 ||
               world_item->item.type == ITEM_TYPE_NONE)
            {
                lock_invalid = 1;
            }
            else if(out)
            {
                out->kind = TARGET_LOCK_WORLD_ITEM;
                out->slot_index = index;
                out->name = world_item->item.name;
                out->x = world_item->item.entity.x;
                out->y = world_item->item.entity.y;
                out->z = world_item->item.entity.z;
            }
        }
    }
    else
    {
        lock_invalid = 1;
    }

    if(lock_invalid && clear_invalid)
        target_lock_clear(p);

    return lock_invalid ? 0 : 1;
}

int target_lock_resolve_live(Player* p, TargetLockResolved* out, int log_on_invalidate)
{
    char reason[96] = "";

    if(!p || !p->target_lock.active)
        return 0;

    if(!target_lock_area_matches_current(p->target_lock.area_name))
    {
        snprintf(reason, sizeof(reason), "left area %s", p->target_lock.area_name);
    }
    else if(p->target_lock.kind == TARGET_LOCK_CREATURE)
    {
        int index = p->target_lock.slot_index;
        if(index < 0 || index >= MAX_CREATURES)
            snprintf(reason, sizeof(reason), "invalid creature target slot");
        else if(!creatures[index].alive || !creatures[index].template)
            snprintf(reason, sizeof(reason), "creature is gone");
    }
    else if(p->target_lock.kind == TARGET_LOCK_WORLD_ITEM)
    {
        int index = p->target_lock.slot_index;
        if(index < 0 || index >= MAX_WORLD_ITEMS)
            snprintf(reason, sizeof(reason), "invalid item target slot");
        else if(!world_items[index].active ||
                strcmp(world_items[index].area_name, p->target_lock.area_name) != 0 ||
                world_items[index].item.type == ITEM_TYPE_NONE)
            snprintf(reason, sizeof(reason), "item is gone");
    }
    else
    {
        snprintf(reason, sizeof(reason), "invalid target kind");
    }

    if(reason[0])
    {
        if(log_on_invalidate)
            log_add("Target lock cleared: %s.", reason);
        target_lock_clear(p);
        return 0;
    }

    return target_lock_resolve(p, out, 1);
}