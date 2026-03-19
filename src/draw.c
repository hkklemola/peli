#include "d:/projekti/peli/include/draw.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/hud.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/player.h"
#include <stdio.h>
#include <stdlib.h> // for system()

void draw_world(Player* p)
{
    if(!current_area) return;

    system("cls");

    for(int y = 0; y < MAP_HEIGHT; y++)
    {
        for(int x = 0; x < MAP_WIDTH; x++)
        {
            // Start with the tile
            char symbol = current_area->map[y][x].symbol;

            // Check if a creature is on this tile
            Creature* c = bestiary_creature_at(x, y);
            if(c && c->alive)
                symbol = c->actor.entity.symbol;

            // Player overrides everything
            if(p->character.actor.entity.x == x && p->character.actor.entity.y == y)
                symbol = p->character.actor.entity.symbol;

            putchar(symbol);
        }
        putchar('\n');
    }

    // Draw HUD (player stats)
    draw_hud(p);

    // Draw message log
    log_draw();
}