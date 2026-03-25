#ifndef INTERACT_H
#define INTERACT_H

#include "player.h"

#include "world_items.h"

// --- MISSING FUNCTION PROTOTYPES ---
int interact_open_container(Player* p, WorldContainer* container);
int interact_pick_up_world_item(Player* p, WorldItem* world_item);
int interact_transfer_world_container_to_equipped(Character* c, WorldContainer* world_container, int equipped_ci);
int interact_equip_container_from_ground(Player* p, WorldItem* world_item, WorldContainer* world_container);

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
