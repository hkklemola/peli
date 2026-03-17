#ifndef BESTIARY_H
#define BESTIARY_H

#include "d:/projekti/peli/include/entity.h"  // Entity struct for position & symbol
#include "d:/projekti/peli/include/actor.h"   // Actor struct for stats
#define MAX_CREATURES 16

// Template for creature stats
typedef struct {
    const char* name;
    char symbol;
    int hp;
    int attack;
    int defense;
} CreatureTemplate;

// Actual creature instance
typedef struct {
    Entity entity;
    int alive;
    CreatureTemplate* template;
    struct {
        int hp;
        int max_hp;
        int attack;
        int defense;
    } actor;
} Creature;

// Storage for all creatures
extern Creature creatures[MAX_CREATURES];

// Initialize the bestiary (marks all slots as free)
void bestiary_init();

// Return pointer to creature at a given coordinate
Creature* bestiary_creature_at(int x, int y);

// Predefined creature templates
extern CreatureTemplate goblin_template;
extern CreatureTemplate skeleton_template;

#endif