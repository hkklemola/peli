#include "d:/projekti/peli/include/entity.h"

void move_entity(Entity* e, int dx, int dy)
{
    e->x += dx;
    e->y += dy;
}