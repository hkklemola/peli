#include "world_items.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "inventory.h"
#include "item_data.h"
#include "log.h"

/**
 * @file world_items.c
 * @brief Implementation of game-world item management and persistence.
 *
 * Manages the global array of items dropped in the game world across all areas.
 * Provides spatial lookup, add/remove operations, and serialization support.
 */

WorldItem world_items[MAX_WORLD_ITEMS];
WorldContainer world_containers[MAX_WORLD_CONTAINERS];
WorldCorpse world_corpses[MAX_WORLD_CORPSES];

static int world_container_item_limit(const WorldContainer* container)
{
    if(!container)
        return WORLD_CONTAINER_CAPACITY;

    if(strncmp(container->label, "Bookshelf Shelf ", 16) == 0)
        return 20;

    return WORLD_CONTAINER_CAPACITY;
}

static void world_container_clear_slot(int i)
{
    world_containers[i].active = 0;
    world_containers[i].area_name[0] = '\0';
    world_containers[i].x = -1;
    world_containers[i].y = -1;
    world_containers[i].z = 0;
    world_containers[i].label[0] = '\0';
    world_containers[i].item_count = 0;

    for(int j = 0; j < WORLD_CONTAINER_CAPACITY; j++)
        item_init(&world_containers[i].items[j], "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
}

static void world_corpse_clear_slot(int i)
{
    memset(&world_corpses[i], 0, sizeof(world_corpses[i]));
    world_corpses[i].world_container_index = -1;
}

void world_containers_init(void)
{
    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
        world_container_clear_slot(i);
}

void world_corpses_init(void)
{
    for(int i = 0; i < MAX_WORLD_CORPSES; i++)
        world_corpse_clear_slot(i);
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
    world_corpses_init();
}

static WorldContainer* world_container_find(const char* area_name, int x, int y, int z)
{
    if(!area_name || area_name[0] == '\0')
        return NULL;

    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        if(!world_containers[i].active)
            continue;
        if(strcmp(world_containers[i].area_name, area_name) != 0)
            continue;
        if(world_containers[i].x != x || world_containers[i].y != y || world_containers[i].z != z)
            continue;
        return &world_containers[i];
    }

    return NULL;
}

static int random_range_inclusive(int min_value, int max_value)
{
    int temp;

    if(max_value < min_value)
    {
        temp = min_value;
        min_value = max_value;
        max_value = temp;
    }

    if(max_value <= min_value)
        return min_value;

    return min_value + (rand() % (max_value - min_value + 1));
}

static const char* world_corpse_marker_item_name(WorldCorpseType type)
{
    return (type == WORLD_CORPSE_CHARACTER) ? "Corpse" : "Animal Carcass";
}

void world_corpse_refresh_label(WorldCorpse* corpse)
{
    const char* source_name;

    if(!corpse)
        return;

    source_name = corpse->source_name[0] ? corpse->source_name : "Unknown";

    if(corpse->type == WORLD_CORPSE_CHARACTER)
    {
        snprintf(corpse->label, sizeof(corpse->label), "Corpse of %s", source_name);
    }
    else if(corpse->skinned && corpse->butchered)
    {
        snprintf(corpse->label, sizeof(corpse->label), "%s remains", source_name);
    }
    else if(corpse->skinned)
    {
        snprintf(corpse->label, sizeof(corpse->label), "%s carcass (skinned)", source_name);
    }
    else if(corpse->butchered)
    {
        snprintf(corpse->label, sizeof(corpse->label), "%s carcass (butchered)", source_name);
    }
    else
    {
        snprintf(corpse->label, sizeof(corpse->label), "%s carcass", source_name);
    }
}

static void world_corpse_remove_marker(const WorldCorpse* corpse)
{
    const char* marker_name;

    if(!corpse || !corpse->active)
        return;

    marker_name = world_corpse_marker_item_name(corpse->type);
    for(int i = 0; i < MAX_WORLD_ITEMS; i++)
    {
        if(!world_items[i].active)
            continue;
        if(strcmp(world_items[i].area_name, corpse->area_name) != 0)
            continue;
        if(world_items[i].item.object.base.x != corpse->x ||
           world_items[i].item.object.base.y != corpse->y ||
           world_items[i].item.object.base.z != corpse->z)
            continue;
        if(strcmp(world_items[i].item.name, marker_name) != 0)
            continue;

        (void)world_item_remove(i);
        break;
    }
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

WorldContainer* world_container_at_3d(int x, int y, int z)
{
    if(!current_area)
        return NULL;

    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        if(!world_containers[i].active)
            continue;
        if(strcmp(world_containers[i].area_name, current_area->name) != 0)
            continue;
        if(world_containers[i].x == x && world_containers[i].y == y && world_containers[i].z == z)
            return &world_containers[i];
    }

    return NULL;
}

int world_container_spawn(const char* area_name, int x, int y, const char* label)
{
    return world_container_spawn_3d(area_name, x, y, 0, label);
}

int world_container_spawn_3d(const char* area_name, int x, int y, int z, const char* label)
{
    if(!area_name || area_name[0] == '\0' || !label)
        return -1;

    // Return existing matching container at exact position and label.
    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        if(!world_containers[i].active)
            continue;
        if(strcmp(world_containers[i].area_name, area_name) != 0)
            continue;
        if(world_containers[i].x == x && world_containers[i].y == y && world_containers[i].z == z &&
           strcmp(world_containers[i].label, label) == 0)
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
        world_containers[i].z = z;
        snprintf(world_containers[i].label, sizeof(world_containers[i].label), "%s", label);
        world_containers[i].item_count = 0;
        for(int j = 0; j < WORLD_CONTAINER_CAPACITY; j++)
            item_init(&world_containers[i].items[j], "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
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
    if(container->item_count >= world_container_item_limit(container))
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

WorldCorpse* world_corpse_at(int x, int y)
{
    return world_corpse_at_3d(x, y, 0);
}

WorldCorpse* world_corpse_at_3d(int x, int y, int z)
{
    if(!current_area)
        return NULL;

    for(int i = 0; i < MAX_WORLD_CORPSES; i++)
    {
        if(!world_corpses[i].active)
            continue;
        if(strcmp(world_corpses[i].area_name, current_area->name) != 0)
            continue;
        if(world_corpses[i].x == x && world_corpses[i].y == y && world_corpses[i].z == z)
            return &world_corpses[i];
    }

    return NULL;
}

WorldCorpse* world_corpse_by_container_index(int container_index)
{
    if(container_index < 0 || container_index >= MAX_WORLD_CONTAINERS)
        return NULL;

    for(int i = 0; i < MAX_WORLD_CORPSES; i++)
    {
        if(world_corpses[i].active && world_corpses[i].world_container_index == container_index)
            return &world_corpses[i];
    }

    return NULL;
}

int world_corpse_spawn(const WorldCorpse* corpse)
{
    Item marker;
    const ItemTemplate* marker_template;
    const char* marker_name;

    if(!corpse || !corpse->area_name[0])
        return -1;

    for(int i = 0; i < MAX_WORLD_CORPSES; i++)
    {
        if(world_corpses[i].active &&
           strcmp(world_corpses[i].area_name, corpse->area_name) == 0 &&
           world_corpses[i].x == corpse->x &&
           world_corpses[i].y == corpse->y &&
           world_corpses[i].z == corpse->z &&
           world_corpses[i].type == corpse->type &&
           strcmp(world_corpses[i].source_name, corpse->source_name) == 0)
        {
            world_corpses[i] = *corpse;
            world_corpses[i].active = 1;
            world_corpse_refresh_label(&world_corpses[i]);
            return i;
        }
    }

    for(int i = 0; i < MAX_WORLD_CORPSES; i++)
    {
        if(world_corpses[i].active)
            continue;

        world_corpses[i] = *corpse;
        world_corpses[i].active = 1;
        if(world_corpses[i].world_container_index < -1)
            world_corpses[i].world_container_index = -1;
        if(world_corpses[i].skinning_loot_count < 0)
            world_corpses[i].skinning_loot_count = 0;
        if(world_corpses[i].skinning_loot_count > MAX_WORLD_CORPSE_LOOT_ENTRIES)
            world_corpses[i].skinning_loot_count = MAX_WORLD_CORPSE_LOOT_ENTRIES;
        if(world_corpses[i].butchering_loot_count < 0)
            world_corpses[i].butchering_loot_count = 0;
        if(world_corpses[i].butchering_loot_count > MAX_WORLD_CORPSE_LOOT_ENTRIES)
            world_corpses[i].butchering_loot_count = MAX_WORLD_CORPSE_LOOT_ENTRIES;
        world_corpse_refresh_label(&world_corpses[i]);

        marker_name = world_corpse_marker_item_name(world_corpses[i].type);
        marker_template = item_template_by_name(marker_name);
        if(marker_template)
        {
            item_init_from_template(&marker, marker_template, world_corpses[i].x, world_corpses[i].y);
        }
        else
        {
            item_init(&marker,
                      marker_name,
                      (world_corpses[i].type == WORLD_CORPSE_CHARACTER) ? '%' : '&',
                      world_corpses[i].x,
                      world_corpses[i].y,
                      ITEM_TYPE_MATERIAL,
                      0,
                      1);
        }
        marker.object.base.z = world_corpses[i].z;
        marker.quantity = 1;
        (void)world_item_drop_3d(&marker,
                                 world_corpses[i].area_name,
                                 world_corpses[i].x,
                                 world_corpses[i].y,
                                 world_corpses[i].z);
        return i;
    }

    return -1;
}

int world_corpse_drop_loot(WorldCorpse* corpse,
                           Character* collector,
                           int skinning_phase,
                           int* out_added_to_inventory,
                           int* out_dropped_to_ground)
{
    const WorldCorpseLootEntry* entries;
    int entry_count;
    int total_harvested = 0;
    int inventory_added = 0;
    int ground_dropped = 0;

    if(out_added_to_inventory)
        *out_added_to_inventory = 0;
    if(out_dropped_to_ground)
        *out_dropped_to_ground = 0;

    if(!corpse || !corpse->active)
        return 0;

    entries = skinning_phase ? corpse->skinning_loot : corpse->butchering_loot;
    entry_count = skinning_phase ? corpse->skinning_loot_count : corpse->butchering_loot_count;

    for(int i = 0; i < entry_count; i++)
    {
        const ItemTemplate* item_tmpl;
        int total_quantity;

        if(entries[i].item_name[0] == '\0' || entries[i].chance_percent <= 0)
            continue;
        if((rand() % 100) + 1 > entries[i].chance_percent)
            continue;

        item_tmpl = item_template_by_name(entries[i].item_name);
        if(!item_tmpl)
        {
            log_add("Missing corpse loot item template: %s", entries[i].item_name);
            continue;
        }

        total_quantity = random_range_inclusive(entries[i].min_quantity, entries[i].max_quantity);
        while(total_quantity > 0)
        {
            Item harvested_item;
            int chunk = 1;

            item_init_from_template(&harvested_item, item_tmpl, corpse->x, corpse->y);
            if(harvested_item.type == ITEM_TYPE_NONE)
                break;

            if(harvested_item.stackable)
            {
                chunk = total_quantity;
                if(harvested_item.stack_max > 0 && chunk > harvested_item.stack_max)
                    chunk = harvested_item.stack_max;
            }

            harvested_item.quantity = chunk;
            if(collector && inventory_add(collector, &harvested_item))
            {
                inventory_added += chunk;
            }
            else if(world_item_drop_3d(&harvested_item, corpse->area_name, corpse->x, corpse->y, corpse->z))
            {
                ground_dropped += chunk;
            }
            else
            {
                log_add("No room to store or drop %s from %s.", harvested_item.name, corpse->label);
                break;
            }

            total_harvested += chunk;
            total_quantity -= chunk;
        }
    }

    if(out_added_to_inventory)
        *out_added_to_inventory = inventory_added;
    if(out_dropped_to_ground)
        *out_dropped_to_ground = ground_dropped;

    return total_harvested;
}

int world_corpse_remove(int index)
{
    if(index < 0 || index >= MAX_WORLD_CORPSES)
        return 0;
    if(!world_corpses[index].active)
        return 0;

    world_corpse_remove_marker(&world_corpses[index]);
    world_corpse_clear_slot(index);
    return 1;
}

void world_corpse_remove_by_container_index(int container_index)
{
    for(int i = 0; i < MAX_WORLD_CORPSES; i++)
    {
        if(world_corpses[i].active && world_corpses[i].world_container_index == container_index)
            (void)world_corpse_remove(i);
    }
}

int world_corpse_index_of(const WorldCorpse* corpse)
{
    if(!corpse)
        return -1;

    for(int i = 0; i < MAX_WORLD_CORPSES; i++)
    {
        if(&world_corpses[i] == corpse)
            return i;
    }

    return -1;
}