#include "tile.h"
#include "map.h"
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

int tile_is_wall_tile(const Tile* tile)
{
    if(!tile || tile_is_empty(tile))
        return 0;

    return tile->layer == TILE_LAYER_WALL && strstr(tile->name, "Wall") != NULL;
}

int tile_is_fence_tile(const Tile* tile)
{
    if(!tile)
        return 0;

    return tile_is_wall_tile(tile) &&
           strcmp(tile->name, "Plank Wall") == 0;
}

int tile_is_double_line_wall(const Tile* tile)
{
    if(!tile)
        return 0;

    return tile_is_wall_tile(tile) && !tile_is_fence_tile(tile);
}

int tile_is_staircase(const Tile* tile)
{
    if(!tile)
        return 0;

    if(strcmp(tile->name, "Staircase") == 0 ||
       strcmp(tile->name, "Stairs Up") == 0 ||
       strcmp(tile->name, "Stairs Down") == 0)
        return 1;

    return tile->symbol == '<' || tile->symbol == '>';
}

int tile_stair_is_horizontal_at(const Area* area, int x, int y, int z)
{
    const int dirs[4][2] = {
        {-1, 0},
        {1, 0},
        {0, -1},
        {0, 1}
    };

    if(!area)
        return 1;

    for(int i = 0; i < 4; ++i)
    {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        const Tile* t = map_tile_at_layer_z((Area*)area, nx, ny, z, TILE_LAYER_WALL);
        if(tile_is_staircase(t))
            return dirs[i][0] != 0;
    }

    for(int dz = -1; dz <= 1; dz += 2)
    {
        int nz = z + dz;
        if(nz < AREA_GROUND_Z || nz > map_max_view_floor(area))
            continue;

        for(int i = 0; i < 4; ++i)
        {
            int nx = x + dirs[i][0];
            int ny = y + dirs[i][1];
            const Tile* t = map_tile_at_layer_z((Area*)area, nx, ny, nz, TILE_LAYER_WALL);
            if(tile_is_staircase(t))
                return dirs[i][0] != 0;
        }
    }

    return 1;
}

int tile_stair_connected_step(const Area* area, int x, int y, int z, int dz)
{
    int next_z;

    if(!area || dz == 0)
        return 0;

    next_z = z + ((dz > 0) ? 1 : -1);
    if(next_z < AREA_GROUND_Z || next_z > map_max_view_floor(area))
        return 0;

    for(int radius = 0; radius <= 2; ++radius)
    {
        for(int dy = -radius; dy <= radius; ++dy)
        {
            for(int dx = -radius; dx <= radius; ++dx)
            {
                int nx = x + dx;
                int ny = y + dy;
                const Tile* t;

                t = map_tile_at_layer_z((Area*)area, nx, ny, next_z, TILE_LAYER_WALL);
                if(tile_is_staircase(t))
                    return 1;
            }
        }
    }

    return 0;
}

int tile_stair_entry_delta_z(const Area* area,
                             int from_x,
                             int from_y,
                             int z,
                             int stair_x,
                             int stair_y)
{
    const int z_offsets[3] = {0, 1, -1};
    int dx;
    int dy;
    int horiz;
    int up_connected;
    int down_connected;
    const Tile* stair;
    int ahead_has_stair = 0;
    int behind_has_stair = 0;

    if(!area)
        return 0;

    dx = stair_x - from_x;
    dy = stair_y - from_y;
    if((dx == 0 && dy == 0) || (dx != 0 && dy != 0) || dx < -1 || dx > 1 || dy < -1 || dy > 1)
        return 0;

    stair = map_tile_at_layer_z((Area*)area, stair_x, stair_y, z, TILE_LAYER_WALL);
    if(!tile_is_staircase(stair))
        return 0;

    horiz = tile_stair_is_horizontal_at(area, stair_x, stair_y, z);
    if(horiz && dy != 0)
        return 0;
    if(!horiz && dx != 0)
        return 0;

    for(int i = 0; i < 3; ++i)
    {
        int nz = z + z_offsets[i];
        const Tile* ahead;
        const Tile* behind;

        if(nz < AREA_GROUND_Z || nz > map_max_view_floor(area))
            continue;

        ahead = map_tile_at_layer_z((Area*)area, stair_x + dx, stair_y + dy, nz, TILE_LAYER_WALL);
        behind = map_tile_at_layer_z((Area*)area, stair_x - dx, stair_y - dy, nz, TILE_LAYER_WALL);
        if(tile_is_staircase(ahead))
            ahead_has_stair = 1;
        if(tile_is_staircase(behind))
            behind_has_stair = 1;
    }

    /* Entering is allowed only from the external side and only if the run continues forward. */
    if(!ahead_has_stair || behind_has_stair)
        return 0;

    up_connected = tile_stair_connected_step(area, stair_x, stair_y, z, 1);
    down_connected = tile_stair_connected_step(area, stair_x, stair_y, z, -1);

    if(up_connected == down_connected)
        return 0;

    return up_connected ? 1 : -1;
}

// Create a default stone-floor tile instance.
Tile tile_empty()
{
    Tile t = {0};
    t.symbol = '\0';
    t.color = RENDER_COLOR_DEFAULT;
    snprintf(t.name, sizeof(t.name), "");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default stone-floor tile instance.
Tile tile_stone_floor()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Stone Floor");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default dirt tile instance (ground layer).
Tile tile_dirt()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_YELLOW;
    snprintf(t.name, sizeof(t.name), "Dirt");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default sand tile instance (ground layer).
Tile tile_sand()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_YELLOW;
    snprintf(t.name, sizeof(t.name), "Sand");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default mud tile instance (ground layer).
Tile tile_mud()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Mud");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default shallow-water tile instance (ground layer).
Tile tile_shallow_water()
{
    Tile t = {0};
    t.symbol = '~';
    t.color = RENDER_COLOR_LIGHT_CYAN;
    snprintf(t.name, sizeof(t.name), "Shallow Water");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 1;
    return t;
}

// Create a default gravel tile instance (ground layer).
Tile tile_gravel()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_GRAY;
    snprintf(t.name, sizeof(t.name), "Gravel");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default rock tile instance (ground layer).
Tile tile_rock()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Rock");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default wood plank tile instance (floor layer).
Tile tile_wood_plank()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_BROWN;
    snprintf(t.name, sizeof(t.name), "Wood Plank");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default clay brick tile instance (floor layer).
Tile tile_clay_brick()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_RED;
    snprintf(t.name, sizeof(t.name), "Clay Brick");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default stone tile instance (floor layer).
Tile tile_stone_tile()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Stone Tile");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default marble tile instance (floor layer).
Tile tile_marble_tile()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_WHITE;
    snprintf(t.name, sizeof(t.name), "Marble Tile");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default straw tile instance (floor layer).
Tile tile_straw()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_LIGHT_YELLOW;
    snprintf(t.name, sizeof(t.name), "Straw");
    t.layer = TILE_LAYER_FLOOR;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default grass tile instance.
Tile tile_grass()
{
    Tile t = {0};
    t.symbol = '.';
    t.color = RENDER_COLOR_GREEN;
    snprintf(t.name, sizeof(t.name), "Grass");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
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
    Tile t = {0};
    t.symbol = info->tree_symbol;
    t.color = info->tree_color;
    snprintf(t.name, sizeof(t.name), "%s", info->tree_name);
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    t.fishable = 0;
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
    Tile t = {0};
    t.symbol = info->stump_symbol;
    t.color = info->stump_color;
    snprintf(t.name, sizeof(t.name), "%s", info->stump_name);
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 0;
    t.blocks_sight = 0;
    t.blocks_projectile = 0;
    t.fishable = 0;
    return t;
}

// Create a default out-of-bounds tile instance.
Tile tile_out_of_bounds()
{
    Tile t = {0};
    t.symbol = '~';
    t.color = RENDER_COLOR_LIGHT_BLUE;
    snprintf(t.name, sizeof(t.name), "Out of Bounds");
    t.layer = TILE_LAYER_GROUND;
    t.hide_below = 1;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    t.fishable = 0;
    return t;
}

// Create a default stone brick wall tile instance (structure layer).
Tile tile_stone_brick_wall()
{
    Tile t = {0};
    t.symbol = '#';
    t.color = RENDER_COLOR_LIGHT_GRAY;
    snprintf(t.name, sizeof(t.name), "Stone Brick Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    t.fishable = 0;
    return t;
}

// Create a default log wall tile instance (structure layer).
Tile tile_log_wall()
{
    Tile t = {0};
    t.symbol = '#';
    t.color = RENDER_COLOR_BROWN;
    snprintf(t.name, sizeof(t.name), "Log Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    t.fishable = 0;
    return t;
}

// Create a default clay brick wall tile instance (structure layer).
Tile tile_clay_brick_wall()
{
    Tile t = {0};
    t.symbol = '#';
    t.color = RENDER_COLOR_LIGHT_RED;
    snprintf(t.name, sizeof(t.name), "Clay Brick Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    t.fishable = 0;
    return t;
}

// Create a default cave wall tile instance (structure layer).
Tile tile_cave_wall()
{
    Tile t = {0};
    t.symbol = '#';
    t.color = RENDER_COLOR_DARK_GRAY;
    snprintf(t.name, sizeof(t.name), "Cave Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    t.fishable = 0;
    return t;
}

// Create a default plank wall tile instance (structure layer).
Tile tile_plank_wall()
{
    Tile t = {0};
    t.symbol = '#';
    t.color = RENDER_COLOR_BROWN;
    snprintf(t.name, sizeof(t.name), "Plank Wall");
    t.layer = TILE_LAYER_WALL;
    t.hide_below = 0;
    t.interactable = 0;
    t.blocks_movement = 1;
    t.blocks_sight = 1;
    t.blocks_projectile = 1;
    t.fishable = 0;
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

int tile_is_fishable(const Tile* tile)
{
    if(!tile)
        return 0;
    return tile->fishable;
}

int tile_is_harvestable(const Tile* tile)
{
    if(!tile)
        return 0;
    return tile->harvestable;
}

