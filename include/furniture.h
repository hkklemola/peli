#ifndef FURNITURE_H
#define FURNITURE_H


#include "object.h"

// Forward declaration to avoid circular dependency with atlas.h
typedef struct Area Area;

#define MAX_AREA_FURNITURE 128

typedef enum FurnitureType {
    FURNITURE_NONE = 0,
    FURNITURE_CHEST,
    FURNITURE_BARREL,
    FURNITURE_CHAIR,
    FURNITURE_TABLE,
    FURNITURE_DOOR,
    FURNITURE_SIGNPOST,
    FURNITURE_BED
} FurnitureType;

typedef struct Furniture {
    Object base;
    FurnitureType type;
    int interactable;
    int blocks_movement;
    int blocks_sight;
    int blocks_projectile;
    int is_open;
    int world_container_index; // for chests
} Furniture;

void furniture_init(Furniture* f, FurnitureType type, int x, int y);
Furniture* furniture_at(Area* area, int x, int y);
int furniture_spawn(Area* area, FurnitureType type, int x, int y);
void furniture_clear(Area* area);
int furniture_toggle_door(Area* area, int x, int y);

#endif // FURNITURE_H