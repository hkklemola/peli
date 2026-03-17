#ifndef SPAWN_H
#define SPAWN_H

#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/player.h"

// Spawn the player at a given position
void spawn_player(Player* p, int x, int y, const char* name);

// Spawn a monster using a template
Creature* spawn_monster(int x, int y, CreatureTemplate* template);

#endif