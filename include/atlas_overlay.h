#ifndef ATLAS_OVERLAY_H
#define ATLAS_OVERLAY_H

#include "player.h"

typedef enum AtlasOverlayMode {
	ATLAS_OVERLAY_MODE_VIEW = 0,
	ATLAS_OVERLAY_MODE_TRAVEL_SELECT
} AtlasOverlayMode;

// Display the Atlas overlay, showing discovered world locations.
void atlas_show_overlay(Player* player);

// Display Atlas overlay with explicit mode. Returns 1 when user selected travel destination.
int atlas_show_overlay_mode(Player* player, AtlasOverlayMode mode);

// Consume and clear pending travel destination selected in Atlas overlay.
int atlas_overlay_take_selected_travel(void);

#endif
