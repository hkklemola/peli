#ifndef SPAWN_H
#define SPAWN_H

#include "player.h"
#include "bestiary.h"

/*
 * Purpose:
 *   Declares spawn helpers for player placement and creature spawning.
 */

// Place player on a random unblocked tile.
int player_place_random(Player* p);

// Spawn a creature at fixed tile or random free tile when x/y are -1.
Creature* spawn_monster(int x, int y, CreatureTemplate* template);

#endif
