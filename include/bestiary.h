#ifndef BESTIARY_H
#define BESTIARY_H

#include "d:/projekti/peli/include/entity.h" 
#include "d:/projekti/peli/include/actor.h"   // Actor struct for stats
 // Entity struct for position & symbol

#define MAX_CREATURES 16

// Template for creature stats
typedef struct CreatureTemplate {
    const char* name;
    char symbol;
    Actor actor;
} CreatureTemplate;

// Actual creature instance
typedef struct Creature {
    Actor actor;            // inherit stats
    struct CreatureTemplate* template;  // pointer to base stats template
    int alive;
} Creature;

// Storage for all creatures
extern Creature creatures[MAX_CREATURES];

Creature* get_free_creature_slot(void);

// Initialize the bestiary (marks all slots as free)
void bestiary_init();

// Return pointer to creature at a given coordinate
Creature* bestiary_creature_at(int x, int y);

// Predefined creature templates
extern CreatureTemplate goblin_template;
extern CreatureTemplate skeleton_template;

#endif