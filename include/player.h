#ifndef PLAYER_H
#define PLAYER_H

#include "character.h" // REQUIRED
#include "item.h"
#include "inventory.h"

#define JOURNAL_MAX_ENTRIES 64
#define JOURNAL_ENTRY_LENGTH 128
#define JOURNAL_TIMESTAMP_LENGTH 20
#define TARGET_LOCK_AREA_LENGTH 32

typedef enum TargetLockKind {
    TARGET_LOCK_NONE = 0,
    TARGET_LOCK_CREATURE,
    TARGET_LOCK_WORLD_ITEM,
} TargetLockKind;

typedef struct TargetLockRecord {
    int active;
    TargetLockKind kind;
    int slot_index;
    int z;
    char area_name[TARGET_LOCK_AREA_LENGTH];
} TargetLockRecord;

typedef enum AttackAnimationType {
    ATTACK_ANIM_NONE = 0,
    ATTACK_ANIM_MELEE,
    ATTACK_ANIM_RANGED,
} AttackAnimationType;

typedef struct AttackAnimationState {
    int active;
    AttackAnimationType type;
    int origin_x;
    int origin_y;
    int origin_z;
    int target_x;
    int target_y;
    int target_z;
    int frame;
    int frame_max;
} AttackAnimationState;

/*
 * Purpose:
 *   Declares player state and player placement/initialization APIs.
 *
 * Functions:
 *   - player_create: initializes all player stats and starter loadout.
 *   - player_place: sets exact map position.
 *   - player_place_random: finds and places player on a free tile.
 *   - player_show_character_sheet: opens the stats/skills overlay view.
 */

// ===== Player structure =====
typedef struct Player {
    Character character;    // inherit from Character
    // Player-specific fields
    int experience;
    int level;
    int gold;
    AttackMode selected_attack_mode;
    TargetLockRecord target_lock;
    int journal_count;
    char journal_entries[JOURNAL_MAX_ENTRIES][JOURNAL_ENTRY_LENGTH];
    char journal_timestamps[JOURNAL_MAX_ENTRIES][JOURNAL_TIMESTAMP_LENGTH];
    unsigned long long playtime_seconds;
    char created_timestamp[JOURNAL_TIMESTAMP_LENGTH];
    char last_saved_timestamp[JOURNAL_TIMESTAMP_LENGTH];
    int godmode;  // Debug mode flag: 1 = enabled, 0 = disabled

    // Stamina recovery state
    int stamina_recovery_delay;
    int is_resting;
    int rest_turns_left;
    int is_sleeping;
    int sleep_turns_left;
    int skip_action_point_regen_turn;
    int exhaustion;
    int travelling;
    AttackAnimationState attack_animation;
} Player;

// Global player instance
extern Player player;

// Initialize player state and starter items.
void player_create(Player* p, const char* name);

// Stamina and action-point recovery helpers
void player_init_recovery(Player* p);
void player_apply_stamina_cost(Player* p, int cost);
void player_apply_action_point_cost(Player* p, int cost);
int player_action_point_regen_per_turn(const Player* p);
int player_recover_action_points(Player* p, int amount);
int player_recover_action_points_from_stamina(Player* p, int stamina_cost, int ap_gain);
void player_recover_tick(Player* p, int in_combat);
int player_start_rest(Player* p, int in_combat);
int player_start_sleep(Player* p, int in_combat);
int player_wait(Player* p, int in_combat);
void player_add_exhaustion(Player* p, int amount);
void player_reduce_exhaustion(Player* p, int amount);
void player_clear_exhaustion(Player* p);
int player_exhaustion_surcharge(const Player* p);
int player_try_push_through_exhaustion(Player* p);

// Attack animation state helpers
void player_attack_animation_clear(Player* p);
void player_attack_animation_start(Player* p,
                                  AttackAnimationType type,
                                  int origin_x,
                                  int origin_y,
                                  int origin_z,
                                  int target_x,
                                  int target_y,
                                  int target_z,
                                  int frame_max);
void player_attack_animation_advance(Player* p);
int player_attack_animation_active(const Player* p);

// Place player at exact coordinates.
void player_place(Player* p, int x, int y);

// Place player on a random unblocked tile; returns 1 on success.
int player_place_random(Player* p);

// Show character-sheet overlay with full stats and skill values.
void player_show_character_sheet(const Player* p);

// Recalculate all derived resource maximums from base attributes and clamp current values.
void player_apply_derived_maximums(Player* p);

#endif

