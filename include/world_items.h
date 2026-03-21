#ifndef WORLD_ITEMS_H
#define WORLD_ITEMS_H

#include "item.h"

#define MAX_WORLD_ITEMS 64

typedef struct WorldItem {
    Item item;
    char area_name[32];
    int active;
} WorldItem;

extern WorldItem world_items[MAX_WORLD_ITEMS];

void world_items_init(void);
WorldItem* world_item_at(int x, int y);
int world_item_drop(const Item* item, const char* area_name, int x, int y);
int world_item_remove(int index);
int world_item_index_of(const WorldItem* world_item);

#endif