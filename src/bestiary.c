#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"
#include <string.h>
#include <stdio.h>

// Storage
Creature creatures[MAX_CREATURES];

// Make templates global
CreatureTemplate goblin_template = {
    "Goblin", 'g', 8, 3, 1
};

CreatureTemplate skeleton_template = {
    "Skeleton", 's', 10, 4, 2
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
           creatures[i].entity.x == x &&
           creatures[i].entity.y == y)
            return &creatures[i];
    return NULL;
}