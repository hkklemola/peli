#include "tile.h"

/*
 * Purpose:
 *   Implements constructors for canonical runtime tile instances.
 *
 * Functions:
 *   - tile_stone_floor: returns stone-floor defaults.
 *   - tile_dirt_floor: returns dirt-floor defaults.
 *   - tile_grass: returns grass defaults.
 *   - tile_tree: returns tree defaults.
 *   - tile_out_of_bounds: returns out-of-bounds defaults.
 *   - tile_wall: returns wall defaults.
 *   - tile_door: returns closed-door defaults.
 */

// Create a default stone-floor tile instance.
Tile tile_stone_floor()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Stone Floor");
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default dirt-floor tile instance.
Tile tile_dirt_floor()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_BROWN;
    snprintf(t.name, sizeof(t.name), "Dirt Floor");
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default grass tile instance.
Tile tile_grass()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_GREEN;
    snprintf(t.name, sizeof(t.name), "Grass");
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default tree tile instance.
Tile tile_tree()
{
    Tile t;
    t.symbol = 'T';
    t.color = RENDER_COLOR_GREEN;
    snprintf(t.name, sizeof(t.name), "Tree");
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

// Create a default out-of-bounds tile instance.
Tile tile_out_of_bounds()
{
    Tile t;
    t.symbol = '~';
    t.color = RENDER_COLOR_LIGHT_BLUE;
    snprintf(t.name, sizeof(t.name), "Out of Bounds");
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

// Create a default wall tile instance.
Tile tile_wall()
{
    Tile t;
    t.symbol = '#';
    t.color = RENDER_COLOR_LIGHT_GRAY;
    snprintf(t.name, sizeof(t.name), "Wall");
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

// Create a default closed-door tile instance.
Tile tile_door()
{
    Tile t;
    t.symbol = '+';
    t.color = RENDER_COLOR_BROWN;
    snprintf(t.name, sizeof(t.name), "Door");
    t.interactable = 1;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    
    return t;
}

