#ifndef ENTITY_H
#define ENTITY_H

#include "tile.h"
#include "render_color.h"

/*
 * Purpose:
 *   Defines the minimal map-space representation shared by actors and items.
 */

typedef struct Entity {
    int x, y;           // position on horizontal plane
    int z;              // vertical level/depth in zone maps
    char symbol;        // map representation
    int color;          // glyph color (legacy ANSI code or 0-255 palette index)
    int blocks;         // does it block movement?
    TileLayer layer;    // logical render/inspection layer for dynamic entities
    int hide_below;     // 1 hides lower layers during inspection traversal
} Entity;

#endif
