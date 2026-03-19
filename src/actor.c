#include "d:/projekti/peli/include/actor.h"

void damage_actor(Actor* a, int dmg)
{
    a->hp -= dmg;

    if(a->hp < 0)
        a->hp = 0;
}