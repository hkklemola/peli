#include "d:/projekti/peli/include/hud.h"
#include "d:/projekti/peli/include/log.h"
#include <stdio.h>

void hud_init()
{
    // Any HUD initialization (message log, UI buffers, etc.)
    log_init();
}

void draw_hud(Player* player)
{
    printf("HP: %d/%d  ATK: %d  DEF: %d\n",
           player->hp, player->max_hp,
           player->attack, player->defense);
}