#ifndef FURNITURE_H
#define FURNITURE_H

#include <stddef.h>

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
    FURNITURE_BED,
    FURNITURE_WARDROBE,
    FURNITURE_WEAPON_RACK,
    FURNITURE_TARGET_DUMMY,
    FURNITURE_TYPE_COUNT
} FurnitureType;

typedef enum FurnitureInteractionType {
    FURNITURE_INTERACTION_NONE = 0,
    FURNITURE_INTERACTION_OPEN_CONTAINER,
    FURNITURE_INTERACTION_TOGGLE_DOOR,
    FURNITURE_INTERACTION_READ_SIGN,
    FURNITURE_INTERACTION_REST,
    FURNITURE_INTERACTION_INSPECT,
    FURNITURE_INTERACTION_SIT
} FurnitureInteractionType;

typedef struct FurnitureTemplate {
    FurnitureType type;
    char id[32];
    char name[64];
    char open_name[64];
    char interaction_label[64];
    char interaction_label_open[64];
    char container_label[64];
    char symbol;
    char symbol_open;
    int color;
    int interactable;
    int blocks_movement;
    int blocks_sight;
    int blocks_projectile;
    int open_blocks_movement;
    int open_blocks_sight;
    int open_blocks_projectile;
    int uses_container;
    int hardness;
    int structure_points;
    FurnitureInteractionType interaction_type;
} FurnitureTemplate;

typedef struct Furniture {
    Object base;
    FurnitureType type;
    const FurnitureTemplate* template_data;
    int interactable;
    int blocks_movement;
    int blocks_sight;
    int blocks_projectile;
    int is_open;
    int world_container_index; // for chests and similar storage furniture
    int hardness;
    int structure_points;
    int max_structure_points;
} Furniture;

void furniture_init(Furniture* f, FurnitureType type, int x, int y);
void furniture_init_at_z(Furniture* f, FurnitureType type, int x, int y, int z);
Furniture* furniture_at(const Area* area, int x, int y);
Furniture* furniture_at_3d(const Area* area, int x, int y, int z);
int furniture_spawn(Area* area, FurnitureType type, int x, int y);
int furniture_spawn_at_z(Area* area, FurnitureType type, int x, int y, int z);
void furniture_clear(Area* area);
int furniture_toggle_door(Area* area, int x, int y);

const FurnitureTemplate* furniture_template_by_type(FurnitureType type);
int furniture_templates_load(const char* path);
void clear_furniture_templates(void);
const char* furniture_templates_last_error(void);
int furniture_uses_container_type(FurnitureType type);
FurnitureInteractionType furniture_interaction_type(const Furniture* furniture);
const char* furniture_display_name(const Furniture* furniture);
const char* furniture_container_label_for_type(FurnitureType type);
void furniture_get_interaction_label(const Furniture* furniture, char* out, size_t out_size);
int furniture_is_destructible(const Furniture* furniture);
int furniture_hardness(const Furniture* furniture);
int furniture_current_structure_points(const Furniture* furniture);
int furniture_max_structure_points(const Furniture* furniture);
int furniture_apply_damage(Furniture* furniture, int raw_damage, int* out_damage_dealt, int* out_destroyed);

#endif // FURNITURE_H