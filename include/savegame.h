#ifndef SAVEGAME_H
#define SAVEGAME_H

#include "player.h"

#define SAVEGAME_FILE "savegame.ini"

// Return 1 when a save file exists at the given path.
int savegame_exists(const char* path);

// Save the current player/game state to disk. Returns 1 on success.
int savegame_save(const char* path, const Player* player);

// Load player/game state from disk. Returns 1 on success.
int savegame_load(const char* path, Player* player);

#endif