#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "player.h"
#include "atlas.h"
#include "bestiary.h"
#include "log.h"
#include "tile.h"  

/*
 * Purpose:
 *   Declares player movement and movement-driven melee engagement behavior.
 */

// Attempt to move player by (dx, dy), including collision and melee handling.
void player_move(Player* p, int dx, int dy);

// Attempt a direct melee attack against one target creature using requested mode.
// Returns 1 when an attack exchange happened, 0 when blocked (for example out of range).
int player_attack_creature(Player* p, Creature* target, AttackMode requested_mode);

// Attempt melee attack in a direction from player position.
// Uses direct adjacency first, then reach attack if weapon supports it.
int player_attack_direction(Player* p, int dx, int dy, AttackMode requested_mode);

// Attempt a direct ranged attack against one target creature.
// Returns 1 when a ranged attack happened, 0 when blocked/invalid.
int player_ranged_attack_creature(Player* p, Creature* target, AttackMode requested_mode);

// Attempt a ranged attack toward one target tile.
// Returns 1 when the shot is consumed, 0 when blocked/invalid.
int player_ranged_attack_tile(Player* p, int target_x, int target_y, int target_z, AttackMode requested_mode);

// Attempt quickstep movement in direction (dx, dy) for multiple tiles.
// Quickstep spends action-point cost up front; if step 2 is blocked, 1 action point is refunded.
void player_quickstep(Player* p, int dx, int dy, int stamina_cost);

// Advance creature AI for one turn after player movement actions.
void creatures_take_turns(Player* p);

#endif

