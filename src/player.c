#include "d:/projekti/peli/include/player.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/tile.h"
#include "d:/projekti/peli/include/tileset.h"
#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/movement.h"
#include "d:/projekti/peli/include/collision.h"
#include <string.h>
#include <conio.h> // for _getch()
#include <stdlib.h> // optional, for exit()

void player_handle_input()
{
    int nx = player.entity.x;
    int ny = player.entity.y;

    char c = _getch();
    switch(c)
    {
        case 'w': ny--; break;
        case 's': ny++; break;
        case 'a': nx--; break;
        case 'd': nx++; break;
        case 'q': exit(0); break;
    }

    Creature* target = creature_at(nx, ny);
    if(target)
    {
        log_add("You attack the %s!", target->template->name);
        return;
    }

    if(is_blocked(nx, ny, 0))
    {
        log_add("You cannot move there!");
        return;
    }

    player.entity.x = nx;
    player.entity.y = ny;
}