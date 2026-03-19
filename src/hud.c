#include "d:/projekti/peli/include/hud.h"
#include "d:/projekti/peli/include/log.h"
#include <stdio.h>

void hud_init()
{
    // Any HUD initialization (message log, UI buffers, etc.)
    log_init();
}

void draw_hud(Player* p)
{
    printf("Name: %s  HP: %d/%d  ATK: %d  DEF: %d  Level: %d  XP: %d\n",
        p->character.name,
        p->character.actor.hp,
        p->character.actor.max_hp,
        p->character.actor.attack,
        p->character.actor.defense,
        p->level,
        p->experience);
}