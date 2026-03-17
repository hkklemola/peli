#include <conio.h>
#include <stdlib.h>

#include "d:/projekti/peli/include/input.h"
#include "d:/projekti/peli/include/movement.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/bestiary.h"

void input_handle(Creature* player, int* running) {
    char c = _getch();
    int nx = player->entity.x;
    int ny = player->entity.y;

    switch(c) {
        case 'w': ny--; break;
        case 's': ny++; break;
        case 'a': nx--; break;
        case 'd': nx++; break;
        case 'q': *running = 0; return; // quit game
    }

    // Try to move if tile is walkable
    Tile* t = &current_area->map[ny][nx];
    if(t->walkable)
        player->entity.x = nx, player->entity.y = ny;
}