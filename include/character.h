#ifndef CHARACTER_H
#define CHARACTER_H

#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"

typedef struct Character {
    Actor actor;        // base stats
    char name[32];
} Character;

typedef struct NPC {
    Character character;    // same as Character
    // AI or dialogue fields
    int hostile;            // 0 = neutral, 1 = hostile
} NPC;
// Creation
void character_create(const char* name, int x, int y);

// Access helpers
int character_x();
int character_y();

#endif