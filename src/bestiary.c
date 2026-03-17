#include "d:/projekti/peli/include/bestiary.h"
#include <string.h>

CreatureTemplate bestiary[MAX_SPECIES];

void bestiary_init(void)
{
    // ---------- PLAYER ----------
    strcpy(bestiary[CREATURE_PLAYER].name, "Player");
    bestiary[CREATURE_PLAYER].symbol = '@';

    bestiary[CREATURE_PLAYER].max_hp = 20;
    bestiary[CREATURE_PLAYER].attack = 5;
    bestiary[CREATURE_PLAYER].defense = 3;
    bestiary[CREATURE_PLAYER].magic = 2;
    bestiary[CREATURE_PLAYER].speed = 10;

    bestiary[CREATURE_PLAYER].level = 1;
    bestiary[CREATURE_PLAYER].xp_value = 0;


    // ---------- GOBLIN ----------
    strcpy(bestiary[CREATURE_GOBLIN].name, "Goblin");
    bestiary[CREATURE_GOBLIN].symbol = 'g';

    bestiary[CREATURE_GOBLIN].max_hp = 8;
    bestiary[CREATURE_GOBLIN].attack = 3;
    bestiary[CREATURE_GOBLIN].defense = 1;
    bestiary[CREATURE_GOBLIN].magic = 0;
    bestiary[CREATURE_GOBLIN].speed = 7;

    bestiary[CREATURE_GOBLIN].level = 1;
    bestiary[CREATURE_GOBLIN].xp_value = 10;


    // ---------- ORC ----------
    strcpy(bestiary[CREATURE_ORC].name, "Orc");
    bestiary[CREATURE_ORC].symbol = 'o';

    bestiary[CREATURE_ORC].max_hp = 16;
    bestiary[CREATURE_ORC].attack = 6;
    bestiary[CREATURE_ORC].defense = 3;
    bestiary[CREATURE_ORC].magic = 0;
    bestiary[CREATURE_ORC].speed = 8;

    bestiary[CREATURE_ORC].level = 2;
    bestiary[CREATURE_ORC].xp_value = 25;
}

CreatureTemplate* bestiary_get(int id)
{
    return &bestiary[id];
}