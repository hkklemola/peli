#ifndef INTERACT_H
#define INTERACT_H

#include "player.h"

// Default inspect interaction range in tiles (Chebyshev distance).
// Keep as a named constant so range can be made dynamic later.
#define INTERACT_RANGE_DEFAULT 1

// Attempt one context interaction at inspect cursor coordinates.
// Returns 1 when interaction succeeded and consumed a turn, otherwise 0.
int inspect_interact_at(Player* p, int tx, int ty);

#endif
