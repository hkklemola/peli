
#include "bestiary.h"
#include "entity.h"
#include "actor.h"
#include "character.h"
#include "player.h"
#include "spawn.h"
#include "atlas.h"
#include "collision.h"
#include "log.h"
#include <stdlib.h> // rand

/*
 * Purpose:
 *   Implements creature spawn placement with blocked-tile validation.
 *
 * Functions:
 *   - spawn_monster: spawns at fixed tile or random unblocked tile.
 */

// Spawn one creature at given coordinates or random valid tile.
Creature* spawn_monster(int x, int y, CreatureTemplate* template)
{
    int nx = x;
    int ny = y;
    int area_width;
    int area_height;

    if(!current_area)
        return NULL;

    area_width = current_area->width;
    area_height = current_area->height;

    if(x == -1 || y == -1)
    {
        int attempts = area_width * area_height;
        if(attempts < 200)
            attempts = 200;

        int found = 0;
        while(attempts--)
        {
            nx = rand() % area_width;
            ny = rand() % area_height;
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
actor_ensure_base_attributes(&c->actor);
c->actor.entity.x = nx;
c->actor.entity.y = ny;
c->actor.entity.symbol = template->symbol;
c->actor.entity.color = template->color;
c->actor.entity.blocks = 1;
c->actor.entity.layer = TILE_LAYER_UNIT;
c->actor.entity.hide_below = template->hide_below ? 1 : 0;
c->template = template;
c->move_state = CREATURE_STATE_WANDER;
c->state_turns = 0;
c->move_dx = 0;
c->move_dy = 0;

return c;
}

