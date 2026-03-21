#include "entity.h"

/*
 * Purpose:
 *   Implements basic coordinate mutation helpers for entities.
 *
 * Functions:
 *   - move_entity: applies a delta to entity coordinates.
 */

// Apply movement delta to an entity's map coordinates.
void move_entity(Entity* e, int dx, int dy)
{
    e->x += dx;
    e->y += dy;
}

