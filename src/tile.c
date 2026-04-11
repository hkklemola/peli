#include "tile.h"
#include <string.h>

static const TreeSpeciesInfo TREE_SPECIES_TABLE[] = {
    { TREE_SPECIES_OAK,    "Oak Tree",    "Oak Stump",    "Oak Log",    'T', 't', RENDER_COLOR_GREEN,       RENDER_COLOR_BROWN, 3, 12 },
    { TREE_SPECIES_SPRUCE, "Spruce Tree", "Spruce Stump", "Spruce Log", 'T', 't', RENDER_COLOR_LIGHT_GREEN, RENDER_COLOR_BROWN, 2, 10 },
    { TREE_SPECIES_PINE,   "Pine Tree",   "Pine Stump",   "Pine Log",   'T', 't', RENDER_COLOR_LIGHT_GREEN, RENDER_COLOR_BROWN, 1, 8 },
    { TREE_SPECIES_BIRCH,  "Birch Tree",  "Birch Stump",  "Birch Log",  'T', 't', RENDER_COLOR_WHITE,       RENDER_COLOR_BROWN, 1, 9 },
    { TREE_SPECIES_YEW,    "Yew Tree",    "Yew Stump",    "Yew Log",    'T', 't', RENDER_COLOR_GREEN,       RENDER_COLOR_BROWN, 4, 14 },
    { TREE_SPECIES_MAPLE,  "Maple Tree",  "Maple Stump",  "Maple Log",  'T', 't', RENDER_COLOR_LIGHT_RED,   RENDER_COLOR_BROWN, 3, 11 },
};

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

const TreeSpeciesInfo* tree_species_info(TreeSpecies species)
{
    for(int i = 0; i < (int)(sizeof(TREE_SPECIES_TABLE) / sizeof(TREE_SPECIES_TABLE[0])); ++i)
    {
        if(TREE_SPECIES_TABLE[i].species == species)
            return &TREE_SPECIES_TABLE[i];
    }

    return &TREE_SPECIES_TABLE[0];
}

int tile_is_tree(const Tile* tile)
{
    if(!tile)
        return 0;
    if(strcmp(tile->name, "Tree") == 0)
        return 1;

    for(int i = 0; i < (int)(sizeof(TREE_SPECIES_TABLE) / sizeof(TREE_SPECIES_TABLE[0])); ++i)
    {
        if(strcmp(tile->name, TREE_SPECIES_TABLE[i].tree_name) == 0)
            return 1;
    }

    return 0;
}

int tile_is_tree_stump(const Tile* tile)
{
    if(!tile)
        return 0;
    if(strcmp(tile->name, "Tree Stump") == 0)
        return 1;

    for(int i = 0; i < (int)(sizeof(TREE_SPECIES_TABLE) / sizeof(TREE_SPECIES_TABLE[0])); ++i)
    {
        if(strcmp(tile->name, TREE_SPECIES_TABLE[i].stump_name) == 0)
            return 1;
    }

    return 0;
}

TreeSpecies tile_tree_species(const Tile* tile)
{
    if(!tile)
        return TREE_SPECIES_NONE;
    if(strcmp(tile->name, "Tree") == 0 || strcmp(tile->name, "Tree Stump") == 0)
        return TREE_SPECIES_OAK;

    for(int i = 0; i < (int)(sizeof(TREE_SPECIES_TABLE) / sizeof(TREE_SPECIES_TABLE[0])); ++i)
    {
        if(strcmp(tile->name, TREE_SPECIES_TABLE[i].tree_name) == 0 ||
           strcmp(tile->name, TREE_SPECIES_TABLE[i].stump_name) == 0)
            return TREE_SPECIES_TABLE[i].species;
    }

    return TREE_SPECIES_NONE;
}

// Create a default stone-floor tile instance.
Tile tile_empty()
{
    Tile t;
    t.symbol = '\0';
    t.color = RENDER_COLOR_DEFAULT;
    snprintf(t.name, sizeof(t.name), "");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default stone-floor tile instance.
Tile tile_stone_floor()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Stone Floor");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default dirt tile instance (ground layer).
Tile tile_dirt()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_YELLOW;
    snprintf(t.name, sizeof(t.name), "Dirt");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default sand tile instance (ground layer).
Tile tile_sand()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_YELLOW;
    snprintf(t.name, sizeof(t.name), "Sand");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default mud tile instance (ground layer).
Tile tile_mud()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Mud");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default shallow-water tile instance (ground layer).
Tile tile_shallow_water()
{
    Tile t;
    t.symbol = '~';
    t.color = RENDER_COLOR_LIGHT_CYAN;
    snprintf(t.name, sizeof(t.name), "Shallow Water");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default gravel tile instance (ground layer).
Tile tile_gravel()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_GRAY;
    snprintf(t.name, sizeof(t.name), "Gravel");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default rock tile instance (ground layer).
Tile tile_rock()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Rock");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default wood plank tile instance (floor layer).
Tile tile_wood_plank()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_BROWN;
    snprintf(t.name, sizeof(t.name), "Wood Plank");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default clay brick tile instance (floor layer).
Tile tile_clay_brick()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_RED;
    snprintf(t.name, sizeof(t.name), "Clay Brick");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default stone tile instance (floor layer).
Tile tile_stone_tile()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Stone Tile");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default marble tile instance (floor layer).
Tile tile_marble_tile()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_WHITE;
    snprintf(t.name, sizeof(t.name), "Marble Tile");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default straw tile instance (floor layer).
Tile tile_straw()
{
    Tile t;
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_YELLOW;
    snprintf(t.name, sizeof(t.name), "Straw");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
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
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default tree tile instance.
Tile tile_tree()
{
    return tile_tree_for_species(TREE_SPECIES_OAK);
}

Tile tile_tree_for_species(TreeSpecies species)
{
    const TreeSpeciesInfo* info = tree_species_info(species);
    Tile t;
    t.symbol = info->tree_symbol;
    t.color = info->tree_color;
    snprintf(t.name, sizeof(t.name), "%s", info->tree_name);
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

// Create a default tree stump tile instance.
Tile tile_tree_stump()
{
    return tile_tree_stump_for_species(TREE_SPECIES_OAK);
}

Tile tile_tree_stump_for_species(TreeSpecies species)
{
    const TreeSpeciesInfo* info = tree_species_info(species);
    Tile t;
    t.symbol = info->stump_symbol;
    t.color = info->stump_color;
    snprintf(t.name, sizeof(t.name), "%s", info->stump_name);
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    return t;
}

// Create a default out-of-bounds tile instance.
Tile tile_out_of_bounds()
{
    Tile t;
    t.symbol = '~';
    t.color = RENDER_COLOR_LIGHT_BLUE;
    snprintf(t.name, sizeof(t.name), "Out of Bounds");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

// Create a default stone brick wall tile instance (structure layer).
Tile tile_stone_brick_wall()
{
    Tile t;
    t.symbol = '#';
    t.color = RENDER_COLOR_LIGHT_GRAY;
    snprintf(t.name, sizeof(t.name), "Stone Brick Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

// Create a default log wall tile instance (structure layer).
Tile tile_log_wall()
{
    Tile t;
    t.symbol = '#';
    t.color = RENDER_COLOR_BROWN;
    snprintf(t.name, sizeof(t.name), "Log Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

// Create a default clay brick wall tile instance (structure layer).
Tile tile_clay_brick_wall()
{
    Tile t;
    t.symbol = '#';
    t.color = RENDER_COLOR_LIGHT_RED;
    snprintf(t.name, sizeof(t.name), "Clay Brick Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

// Create a default cave wall tile instance (structure layer).
Tile tile_cave_wall()
{
    Tile t;
    t.symbol = '#';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Cave Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

// Create a default plank wall tile instance (structure layer).
Tile tile_plank_wall()
{
    Tile t;
    t.symbol = '#';
    t.color = RENDER_COLOR_BROWN;
    snprintf(t.name, sizeof(t.name), "Plank Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    return t;
}

int tile_is_empty(const Tile* tile)
{
    if(!tile)
        return 1;

    return tile->symbol == '\0';
}

TileSurfaceKind tile_surface_kind(const Tile* tile)
{
    if(!tile || tile_is_empty(tile))
        return TILE_SURFACE_EMPTY;

    if(tile->symbol == '~')
        return TILE_SURFACE_HAZARD;

    switch(tile->layer)
    {
        case TILE_LAYER_GROUND:
            return TILE_SURFACE_NATURAL;
        case TILE_LAYER_FLOOR:
            return TILE_SURFACE_CONSTRUCTED;
        case TILE_LAYER_WALL:
            return TILE_SURFACE_WALL;
        default:
            break;
    }

    if(strstr(tile->name, "Wall") || strstr(tile->name, "Door") || strstr(tile->name, "Tree"))
        return TILE_SURFACE_WALL;

    return TILE_SURFACE_NATURAL;
}

int tile_layer_accepts_surface(TileLayer layer, TileSurfaceKind kind)
{
    if(kind == TILE_SURFACE_EMPTY)
        return 1;

    switch(layer)
    {
        case TILE_LAYER_GROUND:
            return kind == TILE_SURFACE_NATURAL || kind == TILE_SURFACE_HAZARD;
        case TILE_LAYER_FLOOR:
            return kind == TILE_SURFACE_CONSTRUCTED;
        case TILE_LAYER_WALL:
            return kind == TILE_SURFACE_WALL;
        default:
            return 1;
    }
}

