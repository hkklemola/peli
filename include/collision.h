#ifndef COLLISION_H
#define COLLISION_H

#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/player.h"

// Returns 1 if tile is blocked for movement, 0 otherwise
int is_blocked(int x, int y, int ignore_creatures);

// Returns 1 if a creature is at the given position
Creature* creature_at(int x, int y);

// Returns 1 if the player is at the given position
int player_at(int x, int y);

#endif