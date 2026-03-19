#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "d:/projekti/peli/include/player.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/tile.h"  

// Move the player by dx, dy if possible
void player_move(Player* p, int dx, int dy);

#endif