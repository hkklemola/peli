#include "d:/projekti/peli/include/tile.h"

// Create a walkable floor tile
Tile tile_floor()
{
    Tile t;
    t.symbol = '.';
    t.walkable = 1;
    t.opaque = 0;
    t.interactable = 0;
    t.blocks = 0;
    return t;
}

// Create a solid wall tile
Tile tile_wall()
{
    Tile t;
    t.symbol = '#';
    t.walkable = 0;
    t.opaque = 1;
    t.interactable = 0;
    t.blocks = 1;
    return t;
}

// Create a door tile (walkable, interactable)
Tile tile_door()
{
    Tile t;
    t.symbol = '+';
    t.walkable = 1;
    t.opaque = 1;      // initially closed
    t.interactable = 1;
    t.blocks = 1;      // blocks movement when closed
    
    return t;
}