#include "world_items.h"

#include <stdio.h>
#include <string.h>

#include "atlas.h"

WorldItem world_items[MAX_WORLD_ITEMS];

void world_items_init(void)
{
    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        world_items[i].active = 0;
        world_items[i].area_name[0] = '\0';
        item_init(&world_items[i].item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    }
}

WorldItem* world_item_at(int x, int y)
{
    if(!current_area)
        return NULL;

    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        if(world_items[i].active &&
           strcmp(world_items[i].area_name, current_area->name) == 0 &&
           world_items[i].item.entity.x == x &&
           world_items[i].item.entity.y == y)
            return &world_items[i];
    }

    return NULL;
}

int world_item_drop(const Item* item, const char* area_name, int x, int y)
{
    if(!item || item->type == ITEM_TYPE_NONE || !area_name)
        return 0;

    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        if(world_items[i].active)
            continue;

        world_items[i].active = 1;
        world_items[i].item = *item;
        world_items[i].item.entity.x = x;
        world_items[i].item.entity.y = y;
        snprintf(world_items[i].area_name, sizeof(world_items[i].area_name), "%s", area_name);
        return 1;
    }

    return 0;
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