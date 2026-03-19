#ifndef SPAWN_H
#define SPAWN_H

#include "player.h"
#include "bestiary.h"

// Spawn player randomly on a free tile
int player_place_random(Player* p);

// Spawn a monster (creature) at a given tile, or randomly if x/y == -1
Creature* spawn_monster(int x, int y, CreatureTemplate* template);

#endif