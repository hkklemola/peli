#include "entity.h"
#include "actor.h"
#include "character.h"
#include "player.h"
#include "atlas.h"
#include "bestiary.h"
#include "log.h"
#include "tile.h"
#include "tileset.h"
#include "map.h"
#include "movement.h"
#include "collision.h"

/*
 * Purpose:
 *   Implements shared collision checks used by movement, spawn, and placement.
 *
 * Functions:
 *   - is_blocked: central movement blocking check.
 *   - creature_at: creature occupancy lookup.
 *   - player_at: player occupancy lookup.
 */

// Return whether movement into a map coordinate is blocked.
int is_blocked_3d(int x, int y, int z, int ignore_creatures)
{
    if(!current_area)
        return 1;

    // Map bounds
    if(x < 0 || x >= current_area->width || y < 0 || y >= current_area->height)
        return 1;

    // Tile movement blocking
    if(map_cell_blocks_movement(current_area, x, y))
        return 1;

    // Creature blocking
    Creature* c = creature_at_3d(x, y, z);
    if(c && !ignore_creatures)
        return 1;

    // Player blocking
    if(player_at_3d(x, y, z) && !ignore_creatures)
        return 1;

    return 0;
}

int is_blocked(int x, int y, int ignore_creatures)
{
    return is_blocked_3d(x, y, 0, ignore_creatures);
}

// Return alive creature at (x, y), or NULL.
Creature* creature_at_3d(int x, int y, int z)
{
    for(int i = 0; i < MAX_CREATURES; i++)
    {
        if(creatures[i].alive &&
           creatures[i].actor.entity.x == x &&
           creatures[i].actor.entity.y == y &&
           creatures[i].actor.entity.z == z)
            return &creatures[i];
    }
    return NULL;
}

Creature* creature_at(int x, int y)
{
    return creature_at_3d(x, y, 0);
}

// Return 1 when player occupies (x, y).
int player_at_3d(int x, int y, int z)
{
    return (player.character.actor.entity.x == x &&
            player.character.actor.entity.y == y &&
            player.character.actor.entity.z == z);
}

int player_at(int x, int y)
{
    return player_at_3d(x, y, 0);
}

