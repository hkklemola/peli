#include "tile.h"
#include "map.h"
#include <stdlib.h>
#include <string.h>

static const TreeSpeciesInfo TREE_SPECIES_DEFAULTS[] = {
    { TREE_SPECIES_OAK,    "Oak Tree",    "Oak Stump",    "Oak Log",    "Oak Tree Trunk",    'T', 't', RENDER_COLOR_GREEN,       RENDER_COLOR_BROWN, 3, 12, 50 },
    { TREE_SPECIES_SPRUCE, "Spruce Tree", "Spruce Stump", "Spruce Log", "Spruce Tree Trunk", 'T', 't', RENDER_COLOR_LIGHT_GREEN, RENDER_COLOR_BROWN, 2, 10, 50 },
    { TREE_SPECIES_PINE,   "Pine Tree",   "Pine Stump",   "Pine Log",   "Pine Tree Trunk",   'T', 't', RENDER_COLOR_LIGHT_GREEN, RENDER_COLOR_BROWN, 1, 8, 50 },
    { TREE_SPECIES_BIRCH,  "Birch Tree",  "Birch Stump",  "Birch Log",  "Birch Tree Trunk",  'T', 't', RENDER_COLOR_WHITE,       RENDER_COLOR_BROWN, 1, 9, 50 },
    { TREE_SPECIES_YEW,    "Yew Tree",    "Yew Stump",    "Yew Log",    "Yew Tree Trunk",    'T', 't', RENDER_COLOR_GREEN,       RENDER_COLOR_BROWN, 4, 14, 50 },
    { TREE_SPECIES_MAPLE,  "Maple Tree",  "Maple Stump",  "Maple Log",  "Maple Tree Trunk",  'T', 't', RENDER_COLOR_LIGHT_RED,   RENDER_COLOR_BROWN, 3, 11, 50 },
};

static TreeSpeciesInfo g_tree_species_info[TREE_SPECIES_COUNT - 1];
static char g_tree_species_tree_name[TREE_SPECIES_COUNT - 1][64];
static char g_tree_species_stump_name[TREE_SPECIES_COUNT - 1][64];
static char g_tree_species_log_name[TREE_SPECIES_COUNT - 1][64];
static char g_tree_species_trunk_name[TREE_SPECIES_COUNT - 1][64];
static int g_tree_species_initialized = 0;

static void copy_text(char* out, size_t out_size, const char* text)
{
    if(!out || out_size == 0)
        return;
    snprintf(out, out_size, "%s", text ? text : "");
}

static void trim_in_place(char* text)
{
    if(!text)
        return;

    char* end = text + strlen(text);
    while(end > text && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *end = '\0';

    char* start = text;
    while(*start == ' ' || *start == '\t')
        start++;
    if(start != text)
        memmove(text, start, strlen(start) + 1);
}

static int equals_ignore_case(const char* left, const char* right)
{
    if(!left || !right)
        return 0;

    while(*left && *right)
    {
        char a = *left++;
        char b = *right++;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if(a != b)
            return 0;
    }

    return *left == *right;
}

static int starts_with_ignore_case(const char* text, const char* prefix)
{
    if(!text || !prefix)
        return 0;

    while(*prefix)
    {
        char a = *text++;
        char b = *prefix++;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if(a != b)
            return 0;
    }
    return 1;
}

static int parse_render_color(const char* value, int* out)
{
    static const struct {
        const char* name;
        int color;
    } mappings[] = {
        { "BLACK", RENDER_COLOR_BLACK },
        { "RED", RENDER_COLOR_RED },
        { "GREEN", RENDER_COLOR_GREEN },
        { "BROWN", RENDER_COLOR_BROWN },
        { "BLUE", RENDER_COLOR_BLUE },
        { "MAGENTA", RENDER_COLOR_MAGENTA },
        { "CYAN", RENDER_COLOR_CYAN },
        { "LIGHT_GRAY", RENDER_COLOR_LIGHT_GRAY },
        { "DEFAULT", RENDER_COLOR_DEFAULT },
        { "DARK_GRAY", RENDER_COLOR_DARK_GRAY },
        { "LIGHT_RED", RENDER_COLOR_LIGHT_RED },
        { "LIGHT_GREEN", RENDER_COLOR_LIGHT_GREEN },
        { "LIGHT_YELLOW", RENDER_COLOR_LIGHT_YELLOW },
        { "LIGHT_BLUE", RENDER_COLOR_LIGHT_BLUE },
        { "LIGHT_MAGENTA", RENDER_COLOR_LIGHT_MAGENTA },
        { "LIGHT_CYAN", RENDER_COLOR_LIGHT_CYAN },
        { "WHITE", RENDER_COLOR_WHITE }
    };

    if(!value || !out)
        return 0;

    const char* normalized = value;
    if(starts_with_ignore_case(normalized, "RENDER_COLOR_"))
        normalized += strlen("RENDER_COLOR_");

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(normalized, mappings[i].name))
        {
            *out = mappings[i].color;
            return 1;
        }
    }

    char* endptr = NULL;
    long numeric = strtol(value, &endptr, 10);
    if(endptr && *endptr == '\0')
    {
        *out = (int)numeric;
        return 1;
    }

    return 0;
}

static int parse_tree_species_value(const char* value, TreeSpecies* out)
{
    if(!value || !out)
        return 0;

    if(equals_ignore_case(value, "oak") || equals_ignore_case(value, "oak tree"))
    {
        *out = TREE_SPECIES_OAK;
        return 1;
    }
    if(equals_ignore_case(value, "spruce") || equals_ignore_case(value, "spruce tree"))
    {
        *out = TREE_SPECIES_SPRUCE;
        return 1;
    }
    if(equals_ignore_case(value, "pine") || equals_ignore_case(value, "pine tree"))
    {
        *out = TREE_SPECIES_PINE;
        return 1;
    }
    if(equals_ignore_case(value, "birch") || equals_ignore_case(value, "birch tree"))
    {
        *out = TREE_SPECIES_BIRCH;
        return 1;
    }
    if(equals_ignore_case(value, "yew") || equals_ignore_case(value, "yew tree"))
    {
        *out = TREE_SPECIES_YEW;
        return 1;
    }
    if(equals_ignore_case(value, "maple") || equals_ignore_case(value, "maple tree"))
    {
        *out = TREE_SPECIES_MAPLE;
        return 1;
    }

    return 0;
}

static const TreeSpeciesInfo* default_tree_species_info(TreeSpecies species)
{
    if(species <= TREE_SPECIES_NONE || species >= TREE_SPECIES_COUNT)
        return NULL;

    for(int i = 0; i < (int)(sizeof(TREE_SPECIES_DEFAULTS) / sizeof(TREE_SPECIES_DEFAULTS[0])); ++i)
    {
        if(TREE_SPECIES_DEFAULTS[i].species == species)
            return &TREE_SPECIES_DEFAULTS[i];
    }
    return NULL;
}

void clear_tree_species_templates(void)
{
    int count = (int)(sizeof(TREE_SPECIES_DEFAULTS) / sizeof(TREE_SPECIES_DEFAULTS[0]));
    for(int i = 0; i < count; ++i)
    {
        const TreeSpeciesInfo* src = &TREE_SPECIES_DEFAULTS[i];
        g_tree_species_info[i] = *src;
        copy_text(g_tree_species_tree_name[i], sizeof(g_tree_species_tree_name[i]), src->tree_name);
        copy_text(g_tree_species_stump_name[i], sizeof(g_tree_species_stump_name[i]), src->stump_name);
        copy_text(g_tree_species_log_name[i], sizeof(g_tree_species_log_name[i]), src->log_name);
        copy_text(g_tree_species_trunk_name[i], sizeof(g_tree_species_trunk_name[i]), src->trunk_name);
        g_tree_species_info[i].tree_name = g_tree_species_tree_name[i];
        g_tree_species_info[i].stump_name = g_tree_species_stump_name[i];
        g_tree_species_info[i].log_name = g_tree_species_log_name[i];
        g_tree_species_info[i].trunk_name = g_tree_species_trunk_name[i];
    }
    g_tree_species_initialized = 1;
}

int tree_species_templates_load(const char* path)
{
    if(!path || path[0] == '\0')
        return 0;

    FILE* file = fopen(path, "r");
    if(!file)
        return 0;

    clear_tree_species_templates();

    char line[256];
    int line_number = 0;
    TreeSpecies current_species = TREE_SPECIES_NONE;
    TreeSpeciesInfo current = {0};
    char current_tree_name[64] = "";
    char current_stump_name[64] = "";
    char current_log_name[64] = "";
    char current_trunk_name[64] = "";
    int current_tree_color = -1;
    int current_stump_color = -1;
    int in_section = 0;

    while(fgets(line, sizeof(line), file))
    {
        char* equals;
        char* key;
        char* value;

        line_number++;
        trim_in_place(line);

        if(line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        if(line[0] == '[')
        {
            if(in_section)
            {
                if(current_species == TREE_SPECIES_NONE)
                {
                    fclose(file);
                    return 0;
                }

                const TreeSpeciesInfo* defaults = default_tree_species_info(current_species);
                if(!defaults)
                {
                    fclose(file);
                    return 0;
                }

                TreeSpeciesInfo loaded = *defaults;
                loaded.tree_symbol = current.tree_symbol ? current.tree_symbol : defaults->tree_symbol;
                loaded.stump_symbol = current.stump_symbol ? current.stump_symbol : defaults->stump_symbol;
                loaded.tree_color = current_tree_color >= 0 ? current_tree_color : defaults->tree_color;
                loaded.stump_color = current_stump_color >= 0 ? current_stump_color : defaults->stump_color;
                loaded.hardness = current.hardness ? current.hardness : defaults->hardness;
                loaded.max_structure_points = current.max_structure_points ? current.max_structure_points : defaults->max_structure_points;
                loaded.height = current.height ? current.height : defaults->height;
                copy_text(g_tree_species_tree_name[(int)current_species - 1], sizeof(g_tree_species_tree_name[0]), current_tree_name[0] ? current_tree_name : defaults->tree_name);
                copy_text(g_tree_species_stump_name[(int)current_species - 1], sizeof(g_tree_species_stump_name[0]), current_stump_name[0] ? current_stump_name : defaults->stump_name);
                copy_text(g_tree_species_log_name[(int)current_species - 1], sizeof(g_tree_species_log_name[0]), current_log_name[0] ? current_log_name : defaults->log_name);
                copy_text(g_tree_species_trunk_name[(int)current_species - 1], sizeof(g_tree_species_trunk_name[0]), current_trunk_name[0] ? current_trunk_name : defaults->trunk_name);
                loaded.tree_name = g_tree_species_tree_name[(int)current_species - 1];
                loaded.stump_name = g_tree_species_stump_name[(int)current_species - 1];
                loaded.log_name = g_tree_species_log_name[(int)current_species - 1];
                loaded.trunk_name = g_tree_species_trunk_name[(int)current_species - 1];
                g_tree_species_info[(int)current_species - 1] = loaded;
            }

            if(equals_ignore_case(line, "[tree_species]"))
            {
                current_species = TREE_SPECIES_NONE;
                current = (TreeSpeciesInfo){0};
                current_tree_name[0] = '\0';
                current_stump_name[0] = '\0';
                current_log_name[0] = '\0';
                current_trunk_name[0] = '\0';
                current_tree_color = -1;
                current_stump_color = -1;
                in_section = 1;
            }
            else
            {
                in_section = 0;
            }
            continue;
        }

        if(!in_section)
            continue;

        equals = strchr(line, '=');
        if(!equals)
        {
            fclose(file);
            return 0;
        }

        *equals = '\0';
        key = line;
        value = equals + 1;
        trim_in_place(key);
        trim_in_place(value);

        if(equals_ignore_case(key, "species") || equals_ignore_case(key, "id"))
        {
            if(!parse_tree_species_value(value, &current_species))
            {
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "tree_name"))
        {
            copy_text(current_tree_name, sizeof(current_tree_name), value);
        }
        else if(equals_ignore_case(key, "stump_name"))
        {
            copy_text(current_stump_name, sizeof(current_stump_name), value);
        }
        else if(equals_ignore_case(key, "log_name"))
        {
            copy_text(current_log_name, sizeof(current_log_name), value);
        }
        else if(equals_ignore_case(key, "trunk_name"))
        {
            copy_text(current_trunk_name, sizeof(current_trunk_name), value);
        }
        else if(equals_ignore_case(key, "symbol") || equals_ignore_case(key, "tree_symbol"))
        {
            if(value[0] == '\0')
            {
                fclose(file);
                return 0;
            }
            current.tree_symbol = value[0];
        }
        else if(equals_ignore_case(key, "stump_symbol"))
        {
            if(value[0] == '\0')
            {
                fclose(file);
                return 0;
            }
            current.stump_symbol = value[0];
        }
        else if(equals_ignore_case(key, "tree_color"))
        {
            if(!parse_render_color(value, &current_tree_color))
            {
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "stump_color"))
        {
            if(!parse_render_color(value, &current_stump_color))
            {
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "hardness"))
        {
            current.hardness = atoi(value);
        }
        else if(equals_ignore_case(key, "max_structure_points"))
        {
            current.max_structure_points = atoi(value);
        }
        else if(equals_ignore_case(key, "height"))
        {
            current.height = atoi(value);
        }
    }

    if(in_section)
    {
        if(current_species == TREE_SPECIES_NONE)
        {
            fclose(file);
            return 0;
        }

        const TreeSpeciesInfo* defaults = default_tree_species_info(current_species);
        if(!defaults)
        {
            fclose(file);
            return 0;
        }

        TreeSpeciesInfo loaded = *defaults;
        loaded.tree_symbol = current.tree_symbol ? current.tree_symbol : defaults->tree_symbol;
        loaded.stump_symbol = current.stump_symbol ? current.stump_symbol : defaults->stump_symbol;
        loaded.tree_color = current_tree_color >= 0 ? current_tree_color : defaults->tree_color;
        loaded.stump_color = current_stump_color >= 0 ? current_stump_color : defaults->stump_color;
        loaded.hardness = current.hardness ? current.hardness : defaults->hardness;
        loaded.max_structure_points = current.max_structure_points ? current.max_structure_points : defaults->max_structure_points;
        loaded.height = current.height ? current.height : defaults->height;
        copy_text(g_tree_species_tree_name[(int)current_species - 1], sizeof(g_tree_species_tree_name[0]), current_tree_name[0] ? current_tree_name : defaults->tree_name);
        copy_text(g_tree_species_stump_name[(int)current_species - 1], sizeof(g_tree_species_stump_name[0]), current_stump_name[0] ? current_stump_name : defaults->stump_name);
        copy_text(g_tree_species_log_name[(int)current_species - 1], sizeof(g_tree_species_log_name[0]), current_log_name[0] ? current_log_name : defaults->log_name);
        copy_text(g_tree_species_trunk_name[(int)current_species - 1], sizeof(g_tree_species_trunk_name[0]), current_trunk_name[0] ? current_trunk_name : defaults->trunk_name);
        loaded.tree_name = g_tree_species_tree_name[(int)current_species - 1];
        loaded.stump_name = g_tree_species_stump_name[(int)current_species - 1];
        loaded.log_name = g_tree_species_log_name[(int)current_species - 1];
        loaded.trunk_name = g_tree_species_trunk_name[(int)current_species - 1];
        g_tree_species_info[(int)current_species - 1] = loaded;
    }

    fclose(file);
    return 1;
}

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
    if(!g_tree_species_initialized)
        clear_tree_species_templates();

    int count = (int)(sizeof(g_tree_species_info) / sizeof(g_tree_species_info[0]));
    for(int i = 0; i < count; ++i)
    {
        if(g_tree_species_info[i].species == species)
            return &g_tree_species_info[i];
    }

    return &g_tree_species_info[0];
}

int tile_is_tree(const Tile* tile)
{
    if(!tile)
        return 0;
    if(strcmp(tile->name, "Tree") == 0)
        return 1;

    int count = (int)(sizeof(g_tree_species_info) / sizeof(g_tree_species_info[0]));
    for(int i = 0; i < count; ++i)
    {
        if(strcmp(tile->name, g_tree_species_info[i].tree_name) == 0)
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

    int count = (int)(sizeof(g_tree_species_info) / sizeof(g_tree_species_info[0]));
    for(int i = 0; i < count; ++i)
    {
        if(strcmp(tile->name, g_tree_species_info[i].stump_name) == 0)
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

    int count = (int)(sizeof(g_tree_species_info) / sizeof(g_tree_species_info[0]));
    for(int i = 0; i < count; ++i)
    {
        if(strcmp(tile->name, g_tree_species_info[i].tree_name) == 0 ||
           strcmp(tile->name, g_tree_species_info[i].stump_name) == 0)
            return g_tree_species_info[i].species;
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

