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
    char area_name[TARGET_LOCK_AREA_LENGTH];
} TargetLockRecord;

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
    int godmode;  // Debug mode flag: 1 = enabled, 0 = disabled
} Player;

// Global player instance
extern Player player;

// Initialize player state and starter items.
void player_create(Player* p, const char* name);

// Place player at exact coordinates.
void player_place(Player* p, int x, int y);

// Place player on a random unblocked tile; returns 1 on success.
int player_place_random(Player* p);

// Show character-sheet overlay with full stats and skill values.
void player_show_character_sheet(const Player* p);

// Recalculate all derived resource maximums from base attributes and clamp current values.
void player_apply_derived_maximums(Player* p);

#endif

