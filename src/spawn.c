
#include "bestiary.h"
#include "entity.h"
#include "actor.h"
#include "character.h"
#include "player.h"
#include "spawn.h"
#include "atlas.h"
#include "collision.h"
#include "log.h"
#include "map.h"
#include "npc.h"
#include "render_color.h"
#include <stdlib.h> // rand

static int g_next_entity_id = 1;

int spawn_next_entity_id(void)
{
    return g_next_entity_id++;
}

int spawn_peek_next_entity_id(void)
{
    return g_next_entity_id;
}

void spawn_set_next_entity_id(int next_id)
{
    if(next_id > g_next_entity_id)
        g_next_entity_id = next_id;
}

/*
 * Purpose:
 *   Implements creature spawn placement with blocked-tile validation.
 *
 * Functions:
 *   - spawn_monster: spawns at fixed tile or random unblocked tile.
 */

// Spawn one creature at given coordinates or random valid tile.
Creature* spawn_monster_3d(int x, int y, int z, CreatureTemplate* template)
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
            if(!is_blocked_3d(nx, ny, z, 1))      // ignore creatures when checking for free tile
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
        if(is_blocked_3d(nx, ny, z, 1))
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
c->actor.entity.z = z;
c->actor.entity.id = spawn_next_entity_id();
c->actor.entity.symbol = template->symbol;
c->actor.entity.color = template->color;
c->actor.entity.blocks = 1;
c->actor.entity.layer = TILE_LAYER_EFFECT;
c->actor.entity.hide_below = template->hide_below ? 1 : 0;
c->template = template;
c->move_state = CREATURE_STATE_WANDER;
c->state_turns = 0;
c->move_dx = 0;
c->move_dy = 0;
c->disposition = template->base_disposition;
c->taming_stage = TAMING_WILD;
actor_body_set_layout(&c->actor, ACTOR_BODY_LAYOUT_CREATURE_GENERIC);
(void)actor_body_distribute_health(&c->actor, c->actor.health, c->actor.max_health);

return c;
}

Creature* spawn_monster(int x, int y, CreatureTemplate* template)
{
    return spawn_monster_3d(x, y, AREA_GROUND_Z, template);
}

NPC* spawn_npc_3d(const char* name,
                  unsigned char symbol,
                  int color,
                  int x,
                  int y,
                  int z,
                  int home_x0,
                  int home_y0,
                  int home_x1,
                  int home_y1)
{
    return npc_spawn_wanderer(name, symbol, color, x, y, z, home_x0, home_y0, home_x1, home_y1);
}

NPC* spawn_old_hermit_npc(void)
{
    int tower_x;
    int tower_y;
    NPC* npc;

    if(!current_area || current_area->type != LOCATION_STARTER)
        return NULL;

    tower_x = (current_area->width / 2) + HERMIT_TOWER_OFFSET_X;
    tower_y = (current_area->height / 2) + HERMIT_TOWER_OFFSET_Y;

    npc = spawn_npc_3d("Old Hermit",
                       'H',
                       RENDER_COLOR_LIGHT_GRAY,
                       tower_x + 4,
                       tower_y + 4,
                       HERMIT_TOWER_BASE_Z,
                       tower_x + 1,
                       tower_y + 1,
                       tower_x + HERMIT_TOWER_WIDTH - 2,
                       tower_y + HERMIT_TOWER_HEIGHT - 2);
    if(npc)
        npc_set_dialogue_profile(npc, NPC_DIALOGUE_OLD_HERMIT);

    return npc;
}

