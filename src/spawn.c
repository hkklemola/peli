#include "d:/projekti/peli/include/spawn.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/actor.h"
#include <string.h>

Creature* spawn_creature(int species, int x, int y)
{
    Creature* c = bestiary_get_free();
    if(!c) return NULL;

    CreatureTemplate* t = bestiary_get(species);

    c->alive = 1;

    c->entity.x = x;
    c->entity.y = y;
    c->entity.symbol = t->symbol;
    c->entity.blocks = 1;

    strcpy(c->name, t->name);

    c->actor.max_hp = t->max_hp;
    c->actor.hp = t->max_hp;
    c->actor.attack = t->attack;
    c->actor.defense = t->defense;
    c->actor.magic = t->magic;
    c->actor.speed = t->speed;
    c->actor.level = t->level;
    c->actor.experience = 0;

    return c;
}