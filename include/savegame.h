#ifndef SAVEGAME_H
#define SAVEGAME_H

#include <stddef.h>

#include "player.h"

#define SAVEGAME_FILE "savegame.ini"
#define SAVEGAME_SLOT_COUNT 5
#define SAVEGAME_SLOT_PATH_LENGTH 128

typedef struct SavegameSlotInfo
{
	int slot_index;
	int occupied;
	int from_legacy_file;
	char player_name[64];
	int level;
	char area_name[64];
	unsigned long long playtime_seconds;
	char created_timestamp[JOURNAL_TIMESTAMP_LENGTH];
	char last_saved_timestamp[JOURNAL_TIMESTAMP_LENGTH];
} SavegameSlotInfo;

// Build canonical save path for one slot index (1..SAVEGAME_SLOT_COUNT).
void savegame_build_slot_path(int slot_index, char* out_path, size_t out_size);

// Resolve slot path for loading/listing. Slot 1 falls back to legacy SAVEGAME_FILE when needed.
void savegame_resolve_slot_path(int slot_index, char* out_path, size_t out_size);

// Return 1 when one slot has save data available.
int savegame_slot_exists(int slot_index);

// Populate one slot metadata record for startup listing. Returns 1 when occupied.
int savegame_read_slot_info(int slot_index, SavegameSlotInfo* out_info);

// Populate up to max_slots records; returns number of occupied slots found.
int savegame_list_slots(SavegameSlotInfo* out_infos, int max_slots);

// Return 1 when a save file exists at the given path.
int savegame_exists(const char* path);

// Save the current player/game state to disk. Returns 1 on success.
int savegame_save(const char* path, const Player* player);

// Load player/game state from disk. Returns 1 on success.
int savegame_load(const char* path, Player* player);

// Delete the save file for one slot. Returns 1 on success.
int savegame_delete_slot(int slot_index);

#endif