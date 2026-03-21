#ifndef ENTITY_H
#define ENTITY_H

#include "render_color.h"

/*
 * Purpose:
 *   Defines the minimal map-space representation shared by actors and items.
 */

typedef struct Entity {
    int x, y;           // position
    char symbol;        // map representation
    RenderColor color;  // glyph color
    int blocks;         // does it block movement?
} Entity;

#endif
