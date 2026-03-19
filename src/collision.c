#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/player.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/tile.h"
#include "d:/projekti/peli/include/tileset.h"
#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/movement.h"
#include "d:/projekti/peli/include/collision.h"

// Checks if the tile is blocked for movement
int is_blocked(int x, int y, int ignore_creatures)
{
    // Map bounds
    if(x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
        return 1;

    // Tile walkable
    Tile* t = &current_area->map[y][x];
    if(!t->walkable)
        return 1;

    // Creature blocking
    Creature* c = creature_at(x, y);
    if(c && !ignore_creatures)
        return 1;

    // Player blocking
    if(player_at(x, y) && !ignore_creatures)
        return 1;

    return 0;
}

// Returns pointer to creature at (x,y) or NULL
Creature* creature_at(int x, int y)
{
    for(int i = 0; i < MAX_CREATURES; i++)
    {
        if(creatures[i].alive &&
           creatures[i].actor.entity.x == x &&
           creatures[i].actor.entity.y == y)
            return &creatures[i];
    }
    return NULL;
}

// Returns 1 if player is at (x,y)
int player_at(int x, int y)
{
    return (player.character.actor.entity.x == x && player.character.actor.entity.y == y);
}