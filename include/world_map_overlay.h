#ifndef WORLD_MAP_OVERLAY_H
#define WORLD_MAP_OVERLAY_H

#include "player.h"

// Open world map exploration overlay from the player's current overworld position.
int world_map_show_overlay(Player* player);

// Open world map exploration overlay with camera centered on a chosen known location.
int world_map_show_overlay_centered(Player* player, int focus_x, int focus_y, const char* focus_label);

// Consume and clear selected destination area index, or -1 when none.
int world_map_overlay_take_selected_area(void);

#endif
