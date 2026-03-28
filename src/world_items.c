#include "world_items.h"

#include <stdio.h>
#include <string.h>

#include "atlas.h"

/**
 * @file world_items.c
 * @brief Implementation of game-world item management and persistence.
 *
 * Manages the global array of items dropped in the game world across all areas.
 * Provides spatial lookup, add/remove operations, and serialization support.
 */

WorldItem world_items[MAX_WORLD_ITEMS];
WorldContainer world_containers[MAX_WORLD_CONTAINERS];

static void world_container_clear_slot(int i)
{
    world_containers[i].active = 0;
    world_containers[i].area_name[0] = '\0';
    world_containers[i].x = -1;
    world_containers[i].y = -1;
    world_containers[i].label[0] = '\0';
    world_containers[i].item_count = 0;

    for(int j = 0; j < WORLD_CONTAINER_CAPACITY; j++)
        item_init(&world_containers[i].items[j], "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
}

void world_containers_init(void)
{
    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
        world_container_clear_slot(i);
}

void world_items_init(void)
{
    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        world_items[i].active = 0;
        world_items[i].area_name[0] = '\0';
        item_init(&world_items[i].item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    }

    world_containers_init();
}

WorldItem* world_item_at_3d(int x, int y, int z)
{
    return world_item_at_ordinal_3d(x, y, z, 0);
}

WorldItem* world_item_at(int x, int y)
{
    return world_item_at_3d(x, y, 0);
}

int world_item_count_at_3d(int x, int y, int z)
{
    int count = 0;

    if(!current_area)
        return 0;

    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        if(world_items[i].active &&
           strcmp(world_items[i].area_name, current_area->name) == 0 &&
           world_items[i].item.object.base.x == x &&
           world_items[i].item.object.base.y == y &&
           world_items[i].item.object.base.z == z)
            count++;
    }

    return count;
}

int world_item_count_at(int x, int y)
{
    return world_item_count_at_3d(x, y, 0);
}

WorldItem* world_item_at_ordinal_3d(int x, int y, int z, int ordinal)
{
    int match_index = 0;

    if(!current_area || ordinal < 0)
        return NULL;

    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        if(!(world_items[i].active &&
             strcmp(world_items[i].area_name, current_area->name) == 0 &&
                         world_items[i].item.object.base.x == x &&
                             world_items[i].item.object.base.y == y &&
                             world_items[i].item.object.base.z == z))
            continue;

        if(match_index == ordinal)
            return &world_items[i];

        match_index++;
    }

    return NULL;
}

WorldItem* world_item_at_ordinal(int x, int y, int ordinal)
{
    return world_item_at_ordinal_3d(x, y, 0, ordinal);
}

WorldItem* world_item_next_at_3d(int x, int y, int z, const WorldItem* current_item)
{
    int count;
    int current_ordinal = -1;
    int next_ordinal;

    count = world_item_count_at_3d(x, y, z);
    if(count <= 0)
        return NULL;

    if(current_item)
    {
        for(int i = 0; i < count; i++)
        {
            WorldItem* candidate = world_item_at_ordinal_3d(x, y, z, i);
            if(candidate == current_item)
            {
                current_ordinal = i;
                break;
            }
        }
    }

    next_ordinal = (current_ordinal + 1) % count;
    return world_item_at_ordinal_3d(x, y, z, next_ordinal);
}

WorldItem* world_item_next_at(int x, int y, const WorldItem* current_item)
{
    return world_item_next_at_3d(x, y, 0, current_item);
}

int world_item_drop_3d(const Item* item, const char* area_name, int x, int y, int z)
{
    if(!item || item->type == ITEM_TYPE_NONE || !area_name)
        return 0;

    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        if(world_items[i].active)
            continue;

        world_items[i].active = 1;
        world_items[i].item = *item;
        world_items[i].item.object.base.x = x;
        world_items[i].item.object.base.y = y;
        world_items[i].item.object.base.z = z;
        snprintf(world_items[i].area_name, sizeof(world_items[i].area_name), "%s", area_name);
        return 1;
    }

    return 0;
}

int world_item_drop(const Item* item, const char* area_name, int x, int y)
{
    return world_item_drop_3d(item, area_name, x, y, 0);
}

int world_item_remove(int index)
{
    if(index < 0 || index >= MAX_WORLD_ITEMS)
        return 0;

    world_items[index].active = 0;
    world_items[index].area_name[0] = '\0';
    item_init(&world_items[index].item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    return 1;
}

int world_item_index_of(const WorldItem* world_item)
{
    if(!world_item)
        return -1;

    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        if(&world_items[i] == world_item)
            return i;
    }

    return -1;
}

WorldContainer* world_container_at(int x, int y)
{
    if(!current_area)
        return NULL;

    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        if(!world_containers[i].active)
            continue;
        if(strcmp(world_containers[i].area_name, current_area->name) != 0)
            continue;
        if(world_containers[i].x == x && world_containers[i].y == y)
            return &world_containers[i];
    }

    return NULL;
}

int world_container_spawn(const char* area_name, int x, int y, const char* label)
{
    if(!area_name || area_name[0] == '\0' || !label)
        return -1;

    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        if(!world_containers[i].active)
            continue;
        if(strcmp(world_containers[i].area_name, area_name) != 0)
            continue;
        if(world_containers[i].x == x && world_containers[i].y == y)
            return i;
    }

    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        if(world_containers[i].active)
            continue;

        world_containers[i].active = 1;
        snprintf(world_containers[i].area_name, sizeof(world_containers[i].area_name), "%s", area_name);
        world_containers[i].x = x;
        world_containers[i].y = y;
        snprintf(world_containers[i].label, sizeof(world_containers[i].label), "%s", label);
        world_containers[i].item_count = 0;
        return i;
    }

    return -1;
}

int world_container_add_item(int container_index, const Item* item)
{
    WorldContainer* container;

    if(container_index < 0 || container_index >= MAX_WORLD_CONTAINERS || !item || item->type == ITEM_TYPE_NONE)
        return 0;

    container = &world_containers[container_index];
    if(!container->active)
        return 0;
    if(container->item_count >= WORLD_CONTAINER_CAPACITY)
        return 0;

    container->items[container->item_count] = *item;
    container->items[container->item_count].object.base.x = container->x;
    container->items[container->item_count].object.base.y = container->y;
    container->item_count++;
    return 1;
}

int world_container_remove_item(int container_index, int item_slot, Item* out_item)
{
    WorldContainer* container;

    if(container_index < 0 || container_index >= MAX_WORLD_CONTAINERS || !out_item)
        return 0;

    container = &world_containers[container_index];
    if(!container->active)
        return 0;
    if(item_slot < 0 || item_slot >= container->item_count)
        return 0;

    *out_item = container->items[item_slot];

    for(int i = item_slot; i < container->item_count - 1; i++)
        container->items[i] = container->items[i + 1];

    container->item_count--;
    item_init(&container->items[container->item_count], "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    return 1;
}

int world_container_remove(int container_index)
{
    if(container_index < 0 || container_index >= MAX_WORLD_CONTAINERS)
        return 0;

    if(!world_containers[container_index].active)
        return 0;

    world_container_clear_slot(container_index);
    return 1;
}

int world_container_index_of(const WorldContainer* container)
{
    if(!container)
        return -1;

    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        if(&world_containers[i] == container)
            return i;
    }

    return -1;
}