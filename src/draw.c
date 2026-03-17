#include "d:/projekti/peli/include/draw.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/hud.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/bestiary.h"
#include <stdio.h>
#include <stdlib.h> // for system()

void draw(Player* player)
{
    system("cls"); // clear console

    for(int y = 0; y < MAP_HEIGHT; y++)
    {
        for(int x = 0; x < MAP_WIDTH; x++)
        {
            Tile* t = &current_area->map[y][x];
            char tile_symbol = t->symbol;

            // Draw creatures
            Creature* c = bestiary_creature_at(x, y);
            if(c) tile_symbol = c->entity.symbol;

            // Player overrides everything
            if(player->entity.x == x && player->entity.y == y)
                tile_symbol = player->entity.symbol;

            putchar(tile_symbol);
        }
        putchar('\n');
    }

    draw_hud(player);
    log_draw();
}