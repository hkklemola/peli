#ifndef ACTOR_H
#define ACTOR_H

#include "d:/projekti/peli/include/entity.h"   // Entity struct for position & symbol

typedef struct Actor {
    Entity entity;      // composition: every actor **has** an entity
    int hp;
    int max_hp;
    int attack;
    int defense;
} Actor;

#endif