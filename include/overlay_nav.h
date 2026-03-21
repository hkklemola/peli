#ifndef OVERLAY_NAV_H
#define OVERLAY_NAV_H

#include "player.h"

typedef enum OverlayType {
    OVERLAY_TYPE_NONE = 0,
    OVERLAY_TYPE_INVENTORY,
    OVERLAY_TYPE_CHARACTER,
    OVERLAY_TYPE_LOG,
    OVERLAY_TYPE_JOURNAL,
    OVERLAY_TYPE_ATLAS
} OverlayType;

// Parse a global overlay hotkey and return 1 when it maps to an overlay.
int overlay_type_from_key(int key, OverlayType* out_type);

// Request opening another overlay when current overlay loop exits.
void overlay_request(OverlayType next_overlay);

// Read and clear pending overlay request.
OverlayType overlay_take_request(void);

// Open one overlay and keep switching while requests are queued.
void overlay_open(OverlayType initial_overlay, Player* player);

#endif
