#include "furniture.h"
#include "world_items.h"
#include "atlas.h"
#include "log.h"

void furniture_init(Furniture* f, FurnitureType type, int x, int y)
{
    if(!f)
        return;

    object_init(&f->base);
    f->base.base.x = x;
    f->base.base.y = y;
    f->base.base.z = AREA_GROUND_Z;
    f->base.base.hide_below = 1;
    f->type = type;
    f->is_open = 0;
    f->world_container_index = -1;

    switch(type)
    {
        case FURNITURE_CHEST:
            f->base.base.symbol = 'C';
            f->base.base.color = RENDER_COLOR_LIGHT_YELLOW;
            f->interactable = 1;
            f->blocks_movement = 1;
            f->blocks_sight = 0;
            f->blocks_projectile = 0;
            break;
        case FURNITURE_BARREL:
            f->base.base.symbol = 'b';
            f->base.base.color = RENDER_COLOR_BROWN;
            f->interactable = 1;
            f->blocks_movement = 1;
            f->blocks_sight = 0;
            f->blocks_projectile = 0;
            break;
        case FURNITURE_CHAIR:
            f->base.base.symbol = 'k';
            f->base.base.color = RENDER_COLOR_BROWN;
            f->interactable = 0;
            f->blocks_movement = 0;
            f->blocks_sight = 0;
            f->blocks_projectile = 0;
            break;
        case FURNITURE_TABLE:
            f->base.base.symbol = 't';
            f->base.base.color = RENDER_COLOR_BROWN;
            f->interactable = 0;
            f->blocks_movement = 0;
            f->blocks_sight = 0;
            f->blocks_projectile = 0;
            break;
        case FURNITURE_DOOR:
            f->base.base.symbol = '+';
            f->base.base.color = RENDER_COLOR_BROWN;
            f->interactable = 1;
            f->blocks_movement = 1;
            f->blocks_sight = 1;
            f->blocks_projectile = 1;
            break;
        default:
            f->base.base.symbol = '?';
            f->base.base.color = RENDER_COLOR_DEFAULT;
            f->interactable = 0;
            f->blocks_movement = 0;
            f->blocks_sight = 0;
            f->blocks_projectile = 0;
            break;
        case FURNITURE_SIGNPOST:
            f->base.base.symbol = 'S';
            f->base.base.color = RENDER_COLOR_LIGHT_YELLOW;
            f->interactable = 1;
            f->blocks_movement = 0;
            f->blocks_sight = 0;
            f->blocks_projectile = 0;
            break;
        case FURNITURE_BED:
            f->base.base.symbol = 'B';
            f->base.base.color = RENDER_COLOR_LIGHT_BLUE;
            f->interactable = 1;
            f->blocks_movement = 0;
            f->blocks_sight = 0;
            f->blocks_projectile = 0;
            break;

    }

    f->base.base.blocks = f->blocks_movement;
}

Furniture* furniture_at(Area* area, int x, int y)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;

    for(int i = 0; i < area->furniture_count; ++i)
    {
        Furniture* f = &area->furniture[i];
        if(f->type != FURNITURE_NONE && f->base.base.x == x && f->base.base.y == y)
            return f;
    }

    return NULL;
}

int furniture_spawn(Area* area, FurnitureType type, int x, int y)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return -1;

    if(area->furniture_count >= MAX_AREA_FURNITURE)
        return -1;

    if(furniture_at(area, x, y))
        return -1;
    // if furniture array is declared `const Furniture furniture[...]`,
    // change declaration to non-const or use explicit cast (not preferred).
    Furniture* f = &area->furniture[area->furniture_count];
    furniture_init(f, type, x, y);

    if(type == FURNITURE_CHEST)
    {
        int idx = world_container_spawn(area->name, x, y, "Chest");
        if(idx >= 0)
            f->world_container_index = idx;
    }

    area->furniture_count++;
    return area->furniture_count - 1;
}

void furniture_clear(Area* area)
{
    if(!area)
        return;

    area->furniture_count = 0;
}

int furniture_toggle_door(Area* area, int x, int y)
{
    if(!area)
        return 0;

    Furniture* f = furniture_at(area, x, y);
    if(!f || f->type != FURNITURE_DOOR)
        return 0;

    f->is_open = !f->is_open;
    if(f->is_open)
    {
        f->base.base.symbol = '/';
        f->blocks_movement = 0;
        f->blocks_sight = 0;
        f->blocks_projectile = 0;
    }
    else
    {
        f->base.base.symbol = '+';
        f->blocks_movement = 1;
        f->blocks_sight = 1;
        f->blocks_projectile = 1;
    }

    f->base.base.blocks = f->blocks_movement;
    return 1;
}