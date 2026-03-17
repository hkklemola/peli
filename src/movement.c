#include "d:/projekti/peli/include/movement.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"

// Handles movement, collisions, and interactions
void try_move(Creature* c, int dx, int dy) {
    int nx = c->entity.x + dx;
    int ny = c->entity.y + dy;

    // bounds check
    if(nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT)
        return;

    Tile* t = &current_area->map[ny][nx];   // ✅ now this works

    if(!t->walkable) {
        log_add("You bump into %s.", t->name);
        return;
    }

    Creature* blocker = bestiary_creature_at(nx, ny);
    if(blocker)
        return;

    c->entity.x = nx;
    c->entity.y = ny;
}