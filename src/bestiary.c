#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/player.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/tile.h"
#include "d:/projekti/peli/include/tileset.h"
#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/movement.h"
#include "d:/projekti/peli/include/collision.h" 


#include <string.h>
#include <stdio.h>

// Storage
Creature creatures[MAX_CREATURES];

Creature* get_free_creature_slot(void)
{
    for(int i = 0; i < MAX_CREATURES; i++)
    {
        if(!creatures[i].alive)
            return &creatures[i];
    }
    return NULL;
}

// Make templates global
CreatureTemplate goblin_template = {
    .name = "Goblin",
    .symbol = 'g',
    .actor = {
        .hp = 8,
        .max_hp = 8,
        .attack = 3,
        .defense = 1
    }
};

CreatureTemplate skeleton_template = {
    .name = "Skeleton",
    .symbol = 's',
    .actor = {
        .hp = 10,
        .max_hp = 10,
        .attack = 4,
        .defense = 2
    }
};

void bestiary_init()
{
    for(int i=0; i<MAX_CREATURES; i++)
        creatures[i].alive = 0;
}

Creature* bestiary_creature_at(int x, int y)
{
    for(int i=0; i<MAX_CREATURES; i++)
        if(creatures[i].alive &&
           creatures[i].actor.entity.x == x &&
           creatures[i].actor.entity.y == y)
            return &creatures[i];
    return NULL;
}