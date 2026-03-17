#include "d:/projekti/peli/include/draw.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/hud.h"
#include "d:/projekti/peli/include/log.h"
#include <stdio.h>
#include <stdlib.h> // for system()

void draw(Creature* player) {
    system("cls"); // clear console

    // Draw the map
    for(int y = 0; y < MAP_HEIGHT; y++) {
        for(int x = 0; x < MAP_WIDTH; x++) {
            Tile* t = &current_area->map[y][x];
            char tile_symbol = t->symbol;

            // Overlay creatures
            Creature* c = bestiary_creature_at(x, y);
            if(c)
                tile_symbol = c->entity.symbol;

            putchar(tile_symbol);
        }
        putchar('\n');
    }

    // Draw HUD (player stats + tile info)
    draw_hud(player);

    // Draw the message log
    log_draw();
}