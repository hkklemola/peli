#ifndef TARGET_LOCK_H
#define TARGET_LOCK_H

#include "player.h"

typedef struct TargetLockResolved
{
    TargetLockKind kind;
    int slot_index;
    const char* name;
    int x;
    int y;
} TargetLockResolved;

const char* target_lock_kind_name(TargetLockKind kind);
void target_lock_describe(const TargetLockResolved* lock, char* out, int out_size);
void target_lock_clear(Player* p);
int target_lock_set_creature(Player* p, int slot_index, const char* area_name);
int target_lock_set_world_item(Player* p, int slot_index, const char* area_name);
int target_lock_matches_creature(const Player* p, int slot_index, const char* area_name);
int target_lock_matches_world_item(const Player* p, int slot_index, const char* area_name);
int target_lock_resolve(Player* p, TargetLockResolved* out, int clear_invalid);
int target_lock_resolve_live(Player* p, TargetLockResolved* out, int log_on_invalidate);

#endif