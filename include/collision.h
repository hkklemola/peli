#ifndef COLLISION_H
#define COLLISION_H

#include "atlas.h"
#include "bestiary.h"
#include "player.h"

/*
 * Purpose:
 *   Declares shared occupancy and tile-collision queries.
 *
 * Functions:
 *   - is_blocked: central movement blocking check for map + actors.
 *   - creature_at: creature lookup by coordinates.
 *   - player_at: player occupancy check by coordinates.
 */

// Return 1 when movement into (x, y) is blocked.
int is_blocked(int x, int y, int ignore_creatures);

// Return the creature at (x, y), or NULL.
Creature* creature_at(int x, int y);

// Return 1 when the player occupies (x, y).
int player_at(int x, int y);

#endif

