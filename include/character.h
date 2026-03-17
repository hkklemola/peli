#ifndef CHARACTER_H
#define CHARACTER_H

#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"

typedef struct {

    // Core identity
    char name[32];

    // Physical presence in world
    Entity entity;
    Actor actor;

    // Progression
    int level;
    int xp;
    int gold;

    // Future: inventory, equipment

} Character;

// Creation
void character_create(const char* name, int x, int y);

// Access helpers
int character_x();
int character_y();

#endif