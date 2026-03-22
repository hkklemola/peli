#ifndef WORLD_MAP_OVERLAY_H
#define WORLD_MAP_OVERLAY_H

#include "player.h"

// Open world map exploration overlay. Returns 1 if a destination area was selected.
int world_map_show_overlay(Player* player);

// Consume and clear selected destination area index, or -1 when none.
int world_map_overlay_take_selected_area(void);

#endif
