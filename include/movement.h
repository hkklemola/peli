#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "player.h"
#include "atlas.h"
#include "bestiary.h"
#include "item_data.h"
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

// Attempt a one-tile quickstep in direction (dx, dy) without advancing turns.
void player_quickstep(Player* p, int dx, int dy);

// Attempt sprint movement in direction (dx, dy) for multiple tiles.
// Sprint spends action points and advances turns after execution.
void player_sprint(Player* p, int dx, int dy, int action_point_cost, int step_count);

typedef struct MovementTreeTarget {
    int found;
    int x;
    int y;
    int z;
    Tile* tile;
    TreeDurabilityState* tree_state;
    TreeSpecies species;
    const TreeSpeciesInfo* species_info;
} MovementTreeTarget;

typedef struct MovementTreeDamageResult {
    int damage_dealt;
    int levels_gained;
    int felled;
    int mutation_failed;
    int trunk_count;
    int placed_trunks;
    int remaining_points;
    int max_points;
} MovementTreeDamageResult;

int movement_is_tree_tile(const Tile* tile);
int movement_resolve_tree_target(Area* area,
                                 int x,
                                 int y,
                                 int preferred_z,
                                 int create_state,
                                 MovementTreeTarget* out_target);
int movement_apply_tree_hit(Player* p,
                            MovementTreeTarget* target,
                            int raw_damage,
                            int animation_frames,
                            int play_animation,
                            NonWeaponSkillType skill_type,
                            MovementTreeDamageResult* out_result);
TreeDurabilityState* movement_tree_state_at(Area* area,
                                            int x,
                                            int y,
                                            int z,
                                            TreeSpecies species,
                                            int create);
int movement_drop_tree_trunk_line(const ItemTemplate* trunk_template,
                                  int trunk_count,
                                  int origin_x,
                                  int origin_y,
                                  int z,
                                  int dx,
                                  int dy);

// Advance creature AI for one turn after player movement actions.
void creatures_take_turns(Player* p);

#endif

