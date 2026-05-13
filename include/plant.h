#ifndef PLANT_H
#define PLANT_H

#include "entity.h"
#include "tile.h"

typedef struct Area Area;
typedef struct Player Player;

#define MAX_AREA_TREES 8192
#define MAX_AREA_BUSHES 4096
#define MAX_AREA_HERBS 2048
#define MAX_AREA_FLOWERS 2048
#define MAX_AREA_PLANTS (MAX_AREA_TREES + MAX_AREA_BUSHES + MAX_AREA_HERBS + MAX_AREA_FLOWERS)

typedef enum PlantType {
    PLANT_TYPE_NONE = 0,
    PLANT_TYPE_TREE,
    PLANT_TYPE_BUSH,
    PLANT_TYPE_HERB,
    PLANT_TYPE_FLOWER,
    PLANT_TYPE_COUNT
} PlantType;

typedef enum PlantState {
    PLANT_STATE_NONE = 0,
    PLANT_STATE_YOUNG,
    PLANT_STATE_MATURE,
    PLANT_STATE_HARVESTABLE,
    PLANT_STATE_STUMP,
    PLANT_STATE_DEAD,
} PlantState;

typedef struct PlantTemplate {
    PlantType type;
    const char* name;
    unsigned char symbol;
    unsigned char stump_symbol;
    int color;
    int stump_color;
    int blocks_movement;
    int blocks_sight;
    int blocks_projectile;
    int harvestable;
    int max_health;
    int min_height;
    int max_height;
    int max_growth_stage;
    int growth_turns_per_stage;
} PlantTemplate;

typedef struct Plant {
    int active;
    Entity entity;
    PlantType type;
    PlantState state;
    TreeSpecies species;
    const PlantTemplate* template_data;
    int health;
    int max_health;
    int height;
    int growth_stage;
    int growth_progress;
    int harvest_cooldown;
} Plant;

Plant* plant_at_3d(Area* area, int x, int y, int z);
Plant* plant_spawn(Area* area, PlantType type, TreeSpecies species, int x, int y, int z);
Plant* plant_find_free_slot(Area* area, PlantType type);
Plant* plant_init_at_3d(Area* area, Plant* plant, PlantType type, TreeSpecies species, int x, int y, int z);
void plant_clear_area(Area* area);
int plant_templates_load(const char* path);
void clear_plant_templates(void);
const char* plant_templates_last_error(void);
const PlantTemplate* plant_template_for_type(PlantType type);
const PlantTemplate* plant_template_for_species(TreeSpecies species);
void plants_take_turns(Player* p);
int plant_damage(Plant* plant, int damage);
void plant_transition_to_stump(Plant* plant);

#endif // PLANT_H
