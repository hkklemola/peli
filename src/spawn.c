#include "d:/projekti/peli/include/spawn.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/actor.h"
#include "d:/projekti/peli/include/tile.h"
#include "d:/projekti/peli/include/tileset.h"
#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/collision.h"
#include "d:/projekti/peli/include/player.h"
#include <string.h>
#include <stdlib.h>

void spawn_player(Player* p, int x, int y, const char* name)
{
    if(!p) return;

    if(x<0 || x>=MAP_WIDTH || y<0 || y>=MAP_HEIGHT)
    {
        x = MAP_WIDTH/2;
        y = MAP_HEIGHT/2;
    }

    p->entity.x = x;
    p->entity.y = y;
    p->entity.symbol = '@';
    p->entity.blocks = 1;

    strncpy(p->name, name, sizeof(p->name)-1);
    p->name[sizeof(p->name)-1] = '\0';

    p->hp = p->max_hp = 20;
    p->attack = 5;
    p->defense = 2;

    log_add("Player %s enters the dungeon.", p->name);
}

Creature* spawn_monster(int x, int y, CreatureTemplate* template)
{
    if(!template) return NULL;

    Creature* c = NULL;
    for(int i=0; i<MAX_CREATURES; i++)
        if(!creatures[i].alive)
        {
            c = &creatures[i];
            break;
        }

    if(!c) return NULL;

    // Only place on walkable tiles
    if(!current_area->map[y][x].walkable)
        return NULL;

    c->alive = 1;
    c->entity.x = x;
    c->entity.y = y;
    c->entity.symbol = template->symbol;
    c->entity.blocks = 1;
    c->template = template;

    c->actor.hp = c->actor.max_hp = template->hp;
    c->actor.attack = template->attack;
    c->actor.defense = template->defense;

    log_add("A %s appears!", template->name);
    return c;
}