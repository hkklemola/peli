
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/player.h"
#include "d:/projekti/peli/include/spawn.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/collision.h"
#include "d:/projekti/peli/include/log.h"
#include <stdlib.h> // rand

// Spawn a monster at specific or random location
Creature* spawn_monster(int x, int y, CreatureTemplate* template)
{
    int nx = x;
    int ny = y;

    if(x == -1 || y == -1)
    {
        int attempts = 100;
        int found = 0;
        while(attempts--)
        {
            nx = rand() % MAP_WIDTH;
            ny = rand() % MAP_HEIGHT;
            if(!is_blocked(nx, ny, 1))      // ignore creatures when checking for free tile
            {
                found = 1;
                break;
            }
        }
        if(!found)
        {
            log_add("Failed to find a free tile to spawn %s!", template->name);
            return NULL;
        }
    }
    else
    {
        if(is_blocked(nx, ny, 1))
        {
            log_add("Cannot spawn %s at blocked tile (%d,%d)!", template->name, nx, ny);
            return NULL;
        }
    }

    Creature* c = get_free_creature_slot();
if(!c)
{
    log_add("No free creature slot to spawn %s!", template->name);
    return NULL;
}

c->alive = 1;
c->actor = template->actor;
c->actor.entity.x = nx;
c->actor.entity.y = ny;
c->actor.entity.symbol = template->symbol;
c->actor.entity.blocks = 1;
c->template = template;

return c;
}