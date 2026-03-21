#ifndef HUD_H
#define HUD_H

#include "player.h"

/*
 * Purpose:
 *   Declares helpers that build HUD text output for the renderer.
 *
 * Functions:
 *   - hud_init: initializes HUD-adjacent systems.
 *   - hud_get_lines: fills printable HUD rows for current player state.
 */

#define HUD_LINE_LENGTH 256

// Initialize HUD-related state.
void hud_init(void);

// Return preferred HUD box height based on current HUD content design.
int hud_preferred_height(void);

// Fill HUD output rows and return the number of rows produced.
int hud_get_lines(Player* player, char out_lines[][HUD_LINE_LENGTH], int max_lines);

#endif

