#include "d:/projekti/peli/include/movement.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/collision.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/tile.h"
#include "d:/projekti/peli/include/tileset.h"
#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/player.h"
#include "d:/projekti/peli/include/character.h"
#include <stdio.h>

// Move the player by dx, dy
void player_move(Player* p, int dx, int dy)
{
    int nx = p->character.actor.entity.x + dx;
    int ny = p->character.actor.entity.y + dy;

    // Check map bounds
    if(nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT)
        return;

    // Check for tile blocking
    if(!current_area)
        return;

    if(current_area->map[ny][nx].blocks)
        return;

    // Check for creatures
    if(bestiary_creature_at(nx, ny))
    {
        log_add("There is a creature in the way!");
        return;
    }

    // Tile is free → move player
    p->character.actor.entity.x = nx;
    p->character.actor.entity.y = ny;
}