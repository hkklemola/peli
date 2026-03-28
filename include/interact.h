#ifndef INTERACT_H
#define INTERACT_H

#include "player.h"

#include "world_items.h"


// --- INTERACTION SYSTEM API ---

// Open a world container and allow item pickup via overlay.
int interact_open_container(Player* p, WorldContainer* container);

// Pick up a world item and add to inventory (legacy, use slot-based actions for new system).
int interact_pick_up_world_item(Player* p, WorldItem* world_item);

// Transfer a world container's contents to an equipped slot (legacy, use slot-based actions for new system).
int interact_transfer_world_container_to_equipped(Character* c, WorldContainer* world_container, int equipped_ci);

// Equip a container from the ground (legacy, use slot-based actions for new system).
int interact_equip_container_from_ground(Player* p, WorldItem* world_item, WorldContainer* world_container);

// Default inspect interaction range in tiles (Chebyshev distance).
#define INTERACT_RANGE_DEFAULT 1

// Query inspect text at cursor coordinates.
// Returns 1 when description generated, otherwise 0.
int inspect_query_at(Player* p, int tx, int ty, char* out, size_t out_size);

// Attempt one context interaction at cursor coordinates.
// Returns 1 when interaction succeeded and consumed a turn, otherwise 0.
// Handles all slot-based item/equipment/container/tile actions.
int interact_at(Player* p, int tx, int ty);

// Backwards compatibility: old name mapped to new function.
static inline int inspect_interact_at(Player* p, int tx, int ty)
{
    return interact_at(p, tx, ty);
}

// Prompt player for direction and attempt quick interaction.
// Accepts: w/up=up, s/down=down, a/left=left, d/right=right, space/enter=current tile, q/esc=cancel.
// Handles all slot-based item/equipment/container/tile actions.
void quick_interact(Player* p);

#endif
