#ifndef INTERACT_H
#define INTERACT_H

#include "player.h"

// Default inspect interaction range in tiles (Chebyshev distance).
// Keep as a named constant so range can be made dynamic later.
#define INTERACT_RANGE_DEFAULT 1

// Attempt one context interaction at inspect cursor coordinates.
// Returns 1 when interaction succeeded and consumed a turn, otherwise 0.
int inspect_interact_at(Player* p, int tx, int ty);

// Prompt player for direction and attempt quick interaction.
// Accepts: w/up=up, s/down=down, a/left=left, d/right=right, space/enter=current tile, q/esc=cancel.
void quick_interact(Player* p);

#endif
