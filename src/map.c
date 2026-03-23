#include "map.h"
#include "atlas.h"
#include "tileset.h"
#include "bestiary.h"
#include "world_items.h"
#include "character.h"
#include "player.h"
#include <stdio.h>
#include <stdlib.h> // rand, srand
#include <string.h>
#include <time.h>   // time

/*
 * Purpose:
 *   Implements procedural map generation for dungeon and town area types.
 *
 * Functions:
 *   - sync_tile_blocking_flags: normalizes tile blocking fields after generation.
 *   - fill_walls/fill_floor/create_room/corridors: low-level carving helpers.
 *   - generate_starter_glade/generate_dungeon/generate_town: area-type specific generation pipelines.
 *   - map_generate_area: public generation entry point for a target area.
 *   - generate_map: convenience generator for current_area.
 */

// Simple rectangle room
typedef struct {
    int x, y, w, h;  /**< top-left position and dimensions */
} Room;

static Area* g_hermit_tower_area = NULL;
static int g_hermit_tower_origin_x = 0;
static int g_hermit_tower_origin_y = 0;
static Tile g_hermit_tower_floor_tiles[HERMIT_TOWER_MAX_FLOORS][HERMIT_TOWER_HEIGHT][HERMIT_TOWER_WIDTH];
static Tile g_hermit_tower_structure_tiles[HERMIT_TOWER_MAX_FLOORS][HERMIT_TOWER_HEIGHT][HERMIT_TOWER_WIDTH];

static int map_active_floor_index(const Area* area);
static const Tile* map_tile_at_layer_for_floor(const Area* area, int x, int y, TileLayer layer, int floor);

static int map_area_index(const Area* area)
{
    if(!area)
        return -1;

    for(int i = 0; i < MAX_AREAS; i++)
    {
        if(&atlas[i] == area)
            return i;
    }

    return -1;
}

static int map_tower_local_coords(const Area* area, int x, int y, int* out_local_x, int* out_local_y)
{
    int local_x;
    int local_y;

    if(!area || area != g_hermit_tower_area)
        return 0;

    local_x = x - g_hermit_tower_origin_x;
    local_y = y - g_hermit_tower_origin_y;

    if(local_x < 0 || local_x >= HERMIT_TOWER_WIDTH || local_y < 0 || local_y >= HERMIT_TOWER_HEIGHT)
        return 0;

    if(out_local_x)
        *out_local_x = local_x;
    if(out_local_y)
        *out_local_y = local_y;
    return 1;
}

static int map_cell_blocks_projectile(const Area* area, int x, int y)
{
    int floor;

    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 1;

    floor = map_active_floor_index(area);

    for(int layer = TILE_LAYER_EFFECT; layer >= TILE_LAYER_GROUND; layer--)
    {
        const Tile* tile = map_tile_at_layer_for_floor(area, x, y, (TileLayer)layer, floor);
        if(!tile_is_empty(tile) && tile->blocks_projectile)
            return 1;
    }

    return 0;
}

int map_has_projectile_path(int x0, int y0, int x1, int y1)
{
    int dx;
    int sx;
    int dy;
    int sy;
    int err;

    if(!current_area)
        return 0;

    if(x0 < 0 || y0 < 0 || x0 >= current_area->width || y0 >= current_area->height)
        return 0;
    if(x1 < 0 || y1 < 0 || x1 >= current_area->width || y1 >= current_area->height)
        return 0;

    dx = abs(x1 - x0);
    sx = (x0 < x1) ? 1 : -1;
    dy = -abs(y1 - y0);
    sy = (y0 < y1) ? 1 : -1;
    err = dx + dy;

    while(1)
    {
        if(x0 == x1 && y0 == y1)
            return 1;

        {
            int e2 = 2 * err;
            if(e2 >= dy)
            {
                err += dy;
                x0 += sx;
            }
            if(e2 <= dx)
            {
                err += dx;
                y0 += sy;
            }
        }

        if(x0 == x1 && y0 == y1)
            return 1;

        if(map_cell_blocks_projectile(current_area, x0, y0))
            return 0;
    }
}

static int map_min_view_floor(const Area* area)
{
    if(area && area == g_hermit_tower_area)
        return HERMIT_TOWER_BASE_Z;
    return AREA_MIN_Z;
}

int map_max_view_floor(const Area* area)
{
    if(area && area == g_hermit_tower_area)
        return HERMIT_TOWER_TOP_Z;
    return AREA_MAX_Z;
}

int map_clamp_view_floor(const Area* area, int floor)
{
    int min_floor = map_min_view_floor(area);
    int max_floor = map_max_view_floor(area);

    if(floor < min_floor)
        return min_floor;
    if(floor > max_floor)
        return max_floor;
    return floor;
}

static int map_active_floor_index(const Area* area)
{
    int z = map_clamp_view_floor(area, character_z());

    if(area && area == g_hermit_tower_area)
    {
        int floor = z - HERMIT_TOWER_BASE_Z;

        if(floor < 0)
            floor = 0;
        if(floor > HERMIT_TOWER_MAX_FLOORS - 1)
            floor = HERMIT_TOWER_MAX_FLOORS - 1;
        return floor;
    }

    return 0;
}

int map_is_tile_discovered(const Area* area, int x, int y)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 0;
    return area->discovered[y][x] ? 1 : 0;
}

void map_mark_tile_discovered(Area* area, int x, int y)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return;
    area->discovered[y][x] = 1;
}

void map_clear_discovery(Area* area)
{
    if(!area)
        return;

    for(int y = 0; y < area->height; y++)
        for(int x = 0; x < area->width; x++)
            area->discovered[y][x] = 0;
}

void map_clear_entity_markers(Area* area)
{
    if(!area)
        return;

    for(int y = 0; y < area->height; y++)
    {
        for(int x = 0; x < area->width; x++)
        {
            area->entity_marker_active[y][x] = 0;
            area->entity_marker_symbol[y][x] = ' ';
            area->entity_marker_color[y][x] = RENDER_COLOR_DEFAULT;
            area->entity_marker_z[y][x] = AREA_GROUND_Z;
        }
    }
}

void map_set_entity_marker(Area* area, int x, int y, int z, char symbol, int color)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return;

    area->entity_marker_active[y][x] = 1;
    area->entity_marker_symbol[y][x] = symbol;
    area->entity_marker_color[y][x] = color;
    area->entity_marker_z[y][x] = z;
}

void map_clear_entity_marker(Area* area, int x, int y, int z)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return;

    if(!area->entity_marker_active[y][x])
        return;

    if(area->entity_marker_z[y][x] != z)
        return;

    area->entity_marker_active[y][x] = 0;
    area->entity_marker_symbol[y][x] = ' ';
    area->entity_marker_color[y][x] = RENDER_COLOR_DEFAULT;
}

int map_get_entity_marker(const Area* area, int x, int y, int z, char* out_symbol, int* out_color)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 0;

    if(!area->entity_marker_active[y][x])
        return 0;

    if(area->entity_marker_z[y][x] != z)
        return 0;

    if(out_symbol)
        *out_symbol = area->entity_marker_symbol[y][x];
    if(out_color)
        *out_color = area->entity_marker_color[y][x];
    return 1;
}

void map_reveal_from_point(Area* area, int origin_x, int origin_y, int vision_range)
{
    if(!area || area != current_area)
        return;

    if(origin_x < 0 || origin_y < 0 || origin_x >= area->width || origin_y >= area->height)
        return;

    if(vision_range < 0)
        vision_range = 0;

    map_mark_tile_discovered(area, origin_x, origin_y);

    for(int y = origin_y - vision_range; y <= origin_y + vision_range; y++)
    {
        for(int x = origin_x - vision_range; x <= origin_x + vision_range; x++)
        {
            int dx;
            int dy;
            if(x < 0 || y < 0 || x >= area->width || y >= area->height)
                continue;

            dx = x - origin_x;
            dy = y - origin_y;
            if((dx * dx) + (dy * dy) > (vision_range * vision_range))
                continue;

            if(map_has_line_of_sight(origin_x, origin_y, x, y))
                map_mark_tile_discovered(area, x, y);
        }
    }
}

static const Tile* map_tile_at_layer_for_floor(const Area* area, int x, int y, TileLayer layer, int floor)
{
    int local_x;
    int local_y;

    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;
    if(layer < 0 || layer >= TILE_LAYER_COUNT)
        return NULL;

    floor = map_clamp_view_floor(area, floor);

    if(map_tower_local_coords(area, x, y, &local_x, &local_y))
    {
        int tower_floor = floor - HERMIT_TOWER_BASE_Z;

        if(tower_floor < 0)
            tower_floor = 0;
        if(tower_floor > HERMIT_TOWER_MAX_FLOORS - 1)
            tower_floor = HERMIT_TOWER_MAX_FLOORS - 1;

        if(layer == TILE_LAYER_FLOOR)
            return &g_hermit_tower_floor_tiles[tower_floor][local_y][local_x];
        if(layer == TILE_LAYER_STRUCTURE)
            return &g_hermit_tower_structure_tiles[tower_floor][local_y][local_x];

        if(tower_floor > 0)
            return &TILE_EMPTY;
    }

    return &area->map[y][x][layer];
}

static void clear_area_layers(Area* area)
{
    if(!area)
        return;

    for(int y = 0; y < area->height; y++)
    {
        for(int x = 0; x < area->width; x++)
        {
            for(int layer = 0; layer < TILE_LAYER_COUNT; layer++)
                area->map[y][x][layer] = TILE_EMPTY;
        }
    }
}

static void fill_layer_with_tile(Area* area, TileLayer layer, Tile tile)
{
    if(!area || layer < 0 || layer >= TILE_LAYER_COUNT)
        return;

    if(!tile_layer_accepts_surface(layer, tile_surface_kind(&tile)))
        return;

    for(int y = 0; y < area->height; y++)
        for(int x = 0; x < area->width; x++)
            area->map[y][x][layer] = tile;
}

static int map_parse_predefined_glyph(char glyph, TileLayer* out_layer, Tile* out_tile)
{
    if(!out_layer || !out_tile)
        return -1;

    switch(glyph)
    {
        case ' ':
            return 0;
        case ';':
            *out_layer = TILE_LAYER_GROUND;
            *out_tile = TILE_GRASS;
            return 1;
        case ',':
            *out_layer = TILE_LAYER_GROUND;
            *out_tile = TILE_DIRT;
            return 1;
        case ':':
            *out_layer = TILE_LAYER_GROUND;
            *out_tile = TILE_MUD;
            return 1;
        case '^':
            *out_layer = TILE_LAYER_GROUND;
            *out_tile = TILE_ROCK;
            return 1;
        case '~':
            *out_layer = TILE_LAYER_GROUND;
            *out_tile = TILE_OUT_OF_BOUNDS;
            return 1;
        case '.':
            *out_layer = TILE_LAYER_FLOOR;
            *out_tile = TILE_STONE_FLOOR;
            return 1;
        case '=':
            *out_layer = TILE_LAYER_FLOOR;
            *out_tile = TILE_WOOD_PLANK;
            return 1;
        case '#':
            *out_layer = TILE_LAYER_STRUCTURE;
            *out_tile = TILE_STONE_BRICK_WALL;
            return 1;
        case 'T':
            *out_layer = TILE_LAYER_STRUCTURE;
            *out_tile = TILE_TREE;
            return 1;
        case '+':
            *out_layer = TILE_LAYER_STRUCTURE;
            *out_tile = tile_door();
            return 1;
        case '<':
            *out_layer = TILE_LAYER_STRUCTURE;
            *out_tile = TILE_STAIRS_UP;
            return 1;
        case '>':
            *out_layer = TILE_LAYER_STRUCTURE;
            *out_tile = TILE_STAIRS_DOWN;
            return 1;
        default:
            return -1;
    }
}

static int map_load_predefined_file(Area* area)
{
    FILE* file;
    char line[MAP_WIDTH + 8];
    int y = 0;

    if(!area || area->predefined_map_path[0] == '\0')
        return 0;

    file = fopen(area->predefined_map_path, "r");
    if(!file)
        return 0;

    clear_area_layers(area);
    fill_layer_with_tile(area, TILE_LAYER_GROUND, TILE_DIRT);

    while(y < area->height && fgets(line, sizeof(line), file))
    {
        size_t len = strlen(line);
        while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        for(int x = 0; x < area->width; x++)
        {
            char glyph = (x < (int)len) ? line[x] : ' ';
            TileLayer layer;
            Tile tile;
            int parse_result;

            parse_result = map_parse_predefined_glyph(glyph, &layer, &tile);
            if(parse_result == 0)
                continue;
            if(parse_result < 0)
            {
                fclose(file);
                return 0;
            }

            if(!tile_layer_accepts_surface(layer, tile_surface_kind(&tile)))
            {
                fclose(file);
                return 0;
            }

            area->map[y][x][layer] = tile;
        }

        y++;
    }

    fclose(file);
    return 1;
}

/**
 * @brief Fill the entire area with a single tile preset.
 * @param area The area to fill (all cells overwritten).
 * @param tile The tile pattern to fill with.
 */
static void fill_with_tile(Area* area, Tile tile) {
    fill_layer_with_tile(area, tile.layer, tile);
}

/**
 * @brief Paint a rectangle region with a tile preset, with bounds clamping.
 * @param area The area to paint into.
 * @param x Top-left x coordinate.
 * @param y Top-left y coordinate.
 * @param w Width in tiles.
 * @param h Height in tiles.
 * @param tile The tile pattern to paint.
 * @note Coordinates are clamped to area bounds; negative coords are adjusted to 0.
 */
static void paint_rect_layer(Area* area, TileLayer layer, int x, int y, int w, int h, Tile tile) {
    int start_x;
    int start_y;
    int end_x;
    int end_y;

    if(!area || w <= 0 || h <= 0)
        return;

    if(!tile_layer_accepts_surface(layer, tile_surface_kind(&tile)))
        return;

    start_x = (x < 0) ? 0 : x;
    start_y = (y < 0) ? 0 : y;
    end_x = x + w;
    end_y = y + h;

    if(end_x > area->width)
        end_x = area->width;
    if(end_y > area->height)
        end_y = area->height;

    for(int py = start_y; py < end_y; py++)
        for(int px = start_x; px < end_x; px++)
            area->map[py][px][layer] = tile;
}

static void paint_rect(Area* area, int x, int y, int w, int h, Tile tile) {
    paint_rect_layer(area, tile.layer, x, y, w, h, tile);
}

/**
 * @brief Synchronize tile blocking flags based on symbol types.
 *        Ensures that walls (#, ~, +), floors (.), and other tiles have correct
 *        blocking flags for movement, line-of-sight, and projectiles.
 * @param area The area to synchronize.
 * @note Called after procedural generation to ensure physics consistency.
 */
static void sync_tile_blocking_flags(Area* area) {
    for(int y = 0; y < area->height; y++) {
        for(int x = 0; x < area->width; x++) {
            for(int layer = 0; layer < TILE_LAYER_COUNT; layer++) {
                Tile* tile = &area->map[y][x][layer];

                if(tile_is_empty(tile))
                    continue;

                if(tile->symbol == '#') {
                    tile->blocks_movement = 1;
                    tile->blocks_sight = 1;
                    tile->blocks_projectile = 1;
                } else if(tile->symbol == '~') {
                    tile->blocks_movement = 1;
                    tile->blocks_sight = 1;
                    tile->blocks_projectile = 1;
                } else if(tile->symbol == '.') {
                    tile->blocks_movement = 0;
                    tile->blocks_sight = 0;
                    tile->blocks_projectile = 0;
                } else if(tile->symbol == '+') {
                    tile->blocks_movement = 1;
                    tile->blocks_sight = 1;
                    tile->blocks_projectile = 1;
                } else {
                    tile->blocks_movement = tile->blocks_movement ? 1 : 0;
                    tile->blocks_sight = tile->blocks_sight ? 1 : 0;
                    tile->blocks_projectile = tile->blocks_projectile ? 1 : 0;
                }
            }

        }
    }
}

static int map_validate_surface_layers(const Area* area)
{
    int violations = 0;

    if(!area)
        return 0;

    for(int y = 0; y < area->height; y++)
    {
        for(int x = 0; x < area->width; x++)
        {
            const Tile* ground = &area->map[y][x][TILE_LAYER_GROUND];
            const Tile* floor = &area->map[y][x][TILE_LAYER_FLOOR];
            const Tile* structure = &area->map[y][x][TILE_LAYER_STRUCTURE];

            if(!tile_layer_accepts_surface(TILE_LAYER_GROUND, tile_surface_kind(ground)))
                violations++;
            if(!tile_layer_accepts_surface(TILE_LAYER_FLOOR, tile_surface_kind(floor)))
                violations++;
            if(!tile_layer_accepts_surface(TILE_LAYER_STRUCTURE, tile_surface_kind(structure)))
                violations++;
        }
    }

    return violations;
}

const Tile* map_top_visible_tile(const Area* area, int x, int y, TileLayer* out_layer);

int find_floor_tile_for_stairs(const Area* area, int* out_x, int* out_y)
{
    if(!area || !out_x || !out_y)
        return 0;

    for(int y = 1; y < area->height - 1; y++)
    {
        for(int x = 1; x < area->width - 1; x++)
        {
            const Tile* top = map_top_visible_tile(area, x, y, NULL);
            if(!top)
                continue;

            if(top->blocks_movement || top->blocks_sight)
                continue;

            if(!bestiary_creature_at(x, y) && !world_item_at(x, y))
            {
                *out_x = x;
                *out_y = y;
                return 1;
            }
        }
    }

    return 0;
}

void place_stairs_tile(Area* area, int x, int y)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return;

    Tile* tile = map_tile_at_layer(area, x, y, TILE_LAYER_STRUCTURE);
    if(!tile)
        return;

    *tile = TILE_STAIRS_UP;
}

Tile* map_tile_at_layer(Area* area, int x, int y, TileLayer layer)
{
    int local_x;
    int local_y;
    int floor;

    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;
    if(layer < 0 || layer >= TILE_LAYER_COUNT)
        return NULL;

    floor = map_active_floor_index(area);
    if(map_tower_local_coords(area, x, y, &local_x, &local_y))
    {
        if(layer == TILE_LAYER_FLOOR)
            return &g_hermit_tower_floor_tiles[floor][local_y][local_x];
        if(layer == TILE_LAYER_STRUCTURE)
            return &g_hermit_tower_structure_tiles[floor][local_y][local_x];
    }

    return &area->map[y][x][layer];
}

const Tile* map_top_visible_tile(const Area* area, int x, int y, TileLayer* out_layer)
{
    return map_top_visible_tile_at_view(area, x, y, map_active_floor_index(area), out_layer);
}

const Tile* map_top_visible_tile_at_view(const Area* area, int x, int y, int view_floor, TileLayer* out_layer)
{
    int has_tower_cell = map_tower_local_coords(area, x, y, NULL, NULL);

    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;

    view_floor = map_clamp_view_floor(area, view_floor);

    for(int floor = view_floor; floor >= 0; floor--)
    {
        if(!has_tower_cell && floor > 0)
            continue;

        for(int layer = TILE_LAYER_EFFECT; layer >= TILE_LAYER_GROUND; layer--)
        {
            const Tile* tile = map_tile_at_layer_for_floor(area, x, y, (TileLayer)layer, floor);
            if(!tile || tile_is_empty(tile))
                continue;

            if(out_layer)
                *out_layer = (TileLayer)layer;
            return tile;
        }
    }

    return NULL;
}

int map_cell_blocks_movement(const Area* area, int x, int y)
{
    int floor;

    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 1;

    floor = map_active_floor_index(area);

    for(int layer = TILE_LAYER_EFFECT; layer >= TILE_LAYER_GROUND; layer--)
    {
        const Tile* tile = map_tile_at_layer_for_floor(area, x, y, (TileLayer)layer, floor);
        if(!tile_is_empty(tile) && tile->blocks_movement)
            return 1;
    }

    return 0;
}

int map_cell_blocks_sight(const Area* area, int x, int y)
{
    int floor;

    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 1;

    floor = map_active_floor_index(area);

    for(int layer = TILE_LAYER_EFFECT; layer >= TILE_LAYER_GROUND; layer--)
    {
        const Tile* tile = map_tile_at_layer_for_floor(area, x, y, (TileLayer)layer, floor);
        if(!tile_is_empty(tile) && tile->blocks_sight)
            return 1;
    }

    return 0;
}

int map_collect_visible_static_layers(const Area* area, int x, int y, const Tile** out_tiles, TileLayer* out_layers, int max_count)
{
    int count = 0;
    int floor;

    if(!area || !out_tiles || max_count <= 0)
        return 0;
    if(x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 0;

    floor = map_active_floor_index(area);

    for(int layer = TILE_LAYER_EFFECT; layer >= TILE_LAYER_GROUND; layer--)
    {
        const Tile* tile = map_tile_at_layer_for_floor(area, x, y, (TileLayer)layer, floor);
        if(tile_is_empty(tile))
            continue;

        out_tiles[count] = tile;
        if(out_layers)
            out_layers[count] = (TileLayer)layer;
        count++;

        if(count >= max_count)
            break;

        if(tile->hide_below)
            break;
    }

    return count;
}

/**
 * @brief Check line-of-sight between two points using Bresenham's line algorithm.
 *        The path is blocked if any intermediate tile has the blocks_sight flag.
 *        Maximum range is 30 tiles (Euclidean distance).
 * @param x0 Starting x coordinate.
 * @param y0 Starting y coordinate.
 * @param x1 Ending x coordinate.
 * @param y1 Ending y coordinate.
 * @return 1 if there is unobstructed line of sight, 0 otherwise.
 * @note This is a critical algorithm for vision cones, creature awareness, and spell targeting.
 *       Uses Bresenham's algorithm (https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm)
 *       to efficiently trace the line from (x0,y0) to (x1,y1).
 */
int map_has_line_of_sight(int x0, int y0, int x1, int y1)
{
    if(!current_area)
        return 0;

    if(x0 < 0 || y0 < 0 || x0 >= current_area->width || y0 >= current_area->height)
        return 0;
    if(x1 < 0 || y1 < 0 || x1 >= current_area->width || y1 >= current_area->height)
        return 0;

    /* Reject targets beyond maximum sight range. */
    const int max_dist = 30;
    const int dx_dist = x1 - x0;
    const int dy_dist = y1 - y0;
    if((dx_dist * dx_dist) + (dy_dist * dy_dist) > (max_dist * max_dist))
        return 0;

    /* Bresenham's algorithm: iterate along line checking for blocking tiles. */
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    int x = x0;
    int y = y0;

    while(1)
    {
        if(x == x1 && y == y1)
            return 1;

        int e2 = 2 * err;
        if(e2 >= dy)
        {
            err += dy;
            x += sx;
        }
        if(e2 <= dx)
        {
            err += dx;
            y += sy;
        }

        if(x == x1 && y == y1)
            return 1;

        if(map_cell_blocks_sight(current_area, x, y))
            return 0;
    }
}


// Fill entire map with wall tiles.
static void fill_walls(Area* area) {
    fill_with_tile(area, TILE_STONE_BRICK_WALL);
}

// Fill entire map with floor tiles.
static void fill_floor(Area* area) {
    fill_with_tile(area, TILE_STONE_FLOOR);
}

// Carve one rectangular room as floor.
static void create_room(Area* area, Room r) {
    paint_rect_layer(area, TILE_LAYER_FLOOR, r.x, r.y, r.w, r.h, TILE_STONE_FLOOR);
    paint_rect_layer(area, TILE_LAYER_STRUCTURE, r.x, r.y, r.w, r.h, TILE_EMPTY);
}

// Carve one horizontal floor corridor.
static void create_h_corridor(Area* area, int x1, int x2, int y) {
    for(int x = x1 < x2 ? x1 : x2; x <= (x1 > x2 ? x1 : x2); x++)
    {
        area->map[y][x][TILE_LAYER_FLOOR] = TILE_STONE_FLOOR;
        area->map[y][x][TILE_LAYER_STRUCTURE] = TILE_EMPTY;
    }
}

// Carve one vertical floor corridor.
static void create_v_corridor(Area* area, int y1, int y2, int x) {
    for(int y = y1 < y2 ? y1 : y2; y <= (y1 > y2 ? y1 : y2); y++)
    {
        area->map[y][x][TILE_LAYER_FLOOR] = TILE_STONE_FLOOR;
        area->map[y][x][TILE_LAYER_STRUCTURE] = TILE_EMPTY;
    }
}

void map_spawn_dev_hut(Area* area, int origin_x, int origin_y)
{
    static const int chest_offsets[6][2] = {
        { 2, 2 }, { 5, 2 }, { 8, 2 },
        { 2, 5 }, { 5, 5 }, { 8, 5 }
    };
    int x;
    int y;

    if(!area)
        return;

    x = origin_x;
    y = origin_y;

    if(x < 1) x = 1;
    if(y < 1) y = 1;
    if(x + DEV_HUT_WIDTH >= area->width - 1)
        x = area->width - DEV_HUT_WIDTH - 2;
    if(y + DEV_HUT_HEIGHT >= area->height - 1)
        y = area->height - DEV_HUT_HEIGHT - 2;

    paint_rect_layer(area, TILE_LAYER_STRUCTURE, x, y, DEV_HUT_WIDTH, DEV_HUT_HEIGHT, TILE_LOG_WALL);
    paint_rect_layer(area, TILE_LAYER_FLOOR, x + 1, y + 1, DEV_HUT_WIDTH - 2, DEV_HUT_HEIGHT - 2, TILE_WOOD_PLANK);
    paint_rect_layer(area, TILE_LAYER_STRUCTURE, x + 1, y + 1, DEV_HUT_WIDTH - 2, DEV_HUT_HEIGHT - 2, TILE_EMPTY);

    area->map[y + DEV_HUT_HEIGHT - 1][x + (DEV_HUT_WIDTH / 2)][TILE_LAYER_STRUCTURE] = tile_door();

    for(int i = 0; i < 6; i++)
    {
        int cx = x + chest_offsets[i][0];
        int cy = y + chest_offsets[i][1];
        area->map[cy][cx][TILE_LAYER_STRUCTURE] = TILE_CHEST;
    }
}

void map_spawn_hermit_tower(Area* area, int origin_x, int origin_y)
{
    int x;
    int y;
    int stair_x;
    int stair_y;
    int area_index;

    if(!area)
        return;

    x = origin_x;
    y = origin_y;

    if(x < 2) x = 2;
    if(y < 2) y = 2;
    if(x + HERMIT_TOWER_WIDTH >= area->width - 2)
        x = area->width - HERMIT_TOWER_WIDTH - 3;
    if(y + HERMIT_TOWER_HEIGHT >= area->height - 2)
        y = area->height - HERMIT_TOWER_HEIGHT - 3;

    g_hermit_tower_area = area;
    g_hermit_tower_origin_x = x;
    g_hermit_tower_origin_y = y;

    for(int floor = 0; floor < HERMIT_TOWER_MAX_FLOORS; floor++)
    {
        for(int ty = 0; ty < HERMIT_TOWER_HEIGHT; ty++)
        {
            for(int tx = 0; tx < HERMIT_TOWER_WIDTH; tx++)
            {
                g_hermit_tower_floor_tiles[floor][ty][tx] = TILE_WOOD_PLANK;
                g_hermit_tower_structure_tiles[floor][ty][tx] = TILE_EMPTY;

                if(tx == 0 || ty == 0 || tx == HERMIT_TOWER_WIDTH - 1 || ty == HERMIT_TOWER_HEIGHT - 1)
                    g_hermit_tower_structure_tiles[floor][ty][tx] = TILE_STONE_BRICK_WALL;
            }
        }
    }

    g_hermit_tower_structure_tiles[0][HERMIT_TOWER_HEIGHT - 1][HERMIT_TOWER_WIDTH / 2] = tile_door();

    stair_x = HERMIT_TOWER_WIDTH / 2;
    stair_y = HERMIT_TOWER_HEIGHT / 2;

    for(int floor = 0; floor < HERMIT_TOWER_MAX_FLOORS; floor++)
    {
        if(floor < HERMIT_TOWER_MAX_FLOORS - 1)
            g_hermit_tower_structure_tiles[floor][stair_y][stair_x] = TILE_STAIRS_UP;
        if(floor > 0)
            g_hermit_tower_structure_tiles[floor][stair_y][stair_x - 1] = TILE_STAIRS_DOWN;
    }

    // Per-floor landmarks for visual distinction.
    g_hermit_tower_structure_tiles[0][2][2] = TILE_TABLE;
    g_hermit_tower_structure_tiles[0][2][3] = TILE_CHAIR;

    g_hermit_tower_structure_tiles[1][2][2] = TILE_CHEST;
    g_hermit_tower_structure_tiles[1][2][HERMIT_TOWER_WIDTH - 3] = TILE_TABLE;

    g_hermit_tower_structure_tiles[2][2][2] = TILE_BARREL;
    g_hermit_tower_structure_tiles[2][2][HERMIT_TOWER_WIDTH - 3] = TILE_CHEST;

    g_hermit_tower_structure_tiles[3][2][2] = TILE_SIGNPOST;
    g_hermit_tower_structure_tiles[3][HERMIT_TOWER_HEIGHT - 3][HERMIT_TOWER_WIDTH - 3] = TILE_CHAIR;

    g_hermit_tower_structure_tiles[4][2][HERMIT_TOWER_WIDTH / 2] = TILE_TABLE;
    g_hermit_tower_structure_tiles[4][3][HERMIT_TOWER_WIDTH / 2] = TILE_SIGNPOST;

    // Keep base map footprint passable and visually coherent when fallback reaches floor 0.
    paint_rect_layer(area, TILE_LAYER_FLOOR, x + 1, y + 1, HERMIT_TOWER_WIDTH - 2, HERMIT_TOWER_HEIGHT - 2, TILE_WOOD_PLANK);
    paint_rect_layer(area, TILE_LAYER_STRUCTURE, x, y, HERMIT_TOWER_WIDTH, HERMIT_TOWER_HEIGHT, TILE_STONE_BRICK_WALL);
    paint_rect_layer(area, TILE_LAYER_STRUCTURE, x + 1, y + 1, HERMIT_TOWER_WIDTH - 2, HERMIT_TOWER_HEIGHT - 2, TILE_EMPTY);
    area->map[y + HERMIT_TOWER_HEIGHT - 1][x + (HERMIT_TOWER_WIDTH / 2)][TILE_LAYER_STRUCTURE] = tile_door();

    area_index = map_area_index(area);
    if(area_index >= 0)
    {
        const int sage_x = x + 2;
        const int sage_y = y + 2;
        const int observatory_x = x + (HERMIT_TOWER_WIDTH / 2);
        const int observatory_y = y + 3;

        world_map_signpost_add_sign(area_index,
                                    sage_x,
                                    sage_y,
                                    HERMIT_TOWER_BASE_Z + 3,
                                    1,
                                    "North",
                                    "Signpost: North route points to Goblin Warrens.");
        world_map_signpost_add_sign(area_index,
                                    sage_x,
                                    sage_y,
                                    HERMIT_TOWER_BASE_Z + 3,
                                    2,
                                    "East",
                                    "Signpost: East route points to Ancient Crypt.");
        world_map_signpost_add_sign(area_index,
                                    sage_x,
                                    sage_y,
                                    HERMIT_TOWER_BASE_Z + 3,
                                    5,
                                    "West",
                                    "Signpost: West route points to Castle Ruins.");

        world_map_signpost_add_sign(area_index,
                                    observatory_x,
                                    observatory_y,
                                    HERMIT_TOWER_BASE_Z + 4,
                                    7,
                                    "South",
                                    "Signpost: South route points to Forest Lake.");
    }
}

// Generate the fixed open-air starter glade inside the larger map bounds.
static void generate_starter_glade(Area* area) {
    if(!area)
        return;

    const int center_x = area->width / 2;
    const int center_y = area->height / 2;

    fill_layer_with_tile(area, TILE_LAYER_GROUND, TILE_GRASS);

    paint_rect_layer(area, TILE_LAYER_GROUND, 0, center_y - 1, area->width, 3, TILE_DIRT);
    paint_rect_layer(area, TILE_LAYER_GROUND, center_x - 1, 0, 3, area->height, TILE_DIRT);

    paint_rect_layer(area, TILE_LAYER_FLOOR, center_x - 4, center_y - 4, 9, 9, TILE_STONE_FLOOR);

    paint_rect_layer(area, TILE_LAYER_GROUND, center_x - 18, center_y - 2, 8, 5, TILE_DIRT);
    paint_rect_layer(area, TILE_LAYER_GROUND, center_x + 11, center_y - 2, 8, 5, TILE_DIRT);
    paint_rect_layer(area, TILE_LAYER_GROUND, center_x - 2, center_y - 18, 5, 8, TILE_DIRT);
    paint_rect_layer(area, TILE_LAYER_GROUND, center_x - 2, center_y + 11, 5, 8, TILE_DIRT);

    for(int y = 10; y < area->height; y += 20) {
        for(int x = 10; x < area->width; x += 20) {
            if(abs(x - center_x) < 12 && abs(y - center_y) < 12)
                continue;
            paint_rect_layer(area, TILE_LAYER_STRUCTURE, x - 2, y - 2, 4, 4, TILE_TREE);
        }
    }

    paint_rect_layer(area, TILE_LAYER_FLOOR, 4, 4, 4, 4, TILE_STONE_FLOOR);
    paint_rect_layer(area, TILE_LAYER_FLOOR, area->width - 8, 4, 4, 4, TILE_STONE_FLOOR);
    paint_rect_layer(area, TILE_LAYER_FLOOR, 4, area->height - 8, 4, 4, TILE_STONE_FLOOR);
    paint_rect_layer(area, TILE_LAYER_FLOOR, area->width - 8, area->height - 8, 4, 4, TILE_STONE_FLOOR);

    area->map[center_y - 5][center_x][TILE_LAYER_STRUCTURE] = TILE_SIGNPOST;

    {
        int area_index = map_area_index(area);
        if(area_index >= 0)
        {
            world_map_signpost_add_sign(area_index,
                                        center_x,
                                        center_y - 5,
                                        AREA_GROUND_Z,
                                        4,
                                        "North",
                                        "Signpost: North route points to Old Mine.");
            world_map_signpost_add_sign(area_index,
                                        center_x,
                                        center_y - 5,
                                        AREA_GROUND_Z,
                                        5,
                                        "East",
                                        "Signpost: East route points to Castle Ruins.");
            world_map_signpost_add_sign(area_index,
                                        center_x,
                                        center_y - 5,
                                        AREA_GROUND_Z,
                                        6,
                                        "South",
                                        "Signpost: South route points to Village.");
            world_map_signpost_add_sign(area_index,
                                        center_x,
                                        center_y - 5,
                                        AREA_GROUND_Z,
                                        7,
                                        "West",
                                        "Signpost: West route points to Forest Lake.");
        }
    }

    map_spawn_dev_hut(area, center_x + DEV_HUT_OFFSET_X, center_y + DEV_HUT_OFFSET_Y);
    map_spawn_hermit_tower(area, center_x + HERMIT_TOWER_OFFSET_X, center_y + HERMIT_TOWER_OFFSET_Y);
}
// Generate dungeon rooms and connecting corridors.
static void generate_dungeon(Area* area) {
    if(!area) return;

    fill_layer_with_tile(area, TILE_LAYER_GROUND, TILE_DIRT);
    fill_layer_with_tile(area, TILE_LAYER_FLOOR, TILE_EMPTY);
    fill_layer_with_tile(area, TILE_LAYER_STRUCTURE, TILE_STONE_BRICK_WALL);
    Room rooms[MAX_ROOMS];
    int room_count = 0;

    for(int i = 0; i < MAX_ROOMS; i++) {
        int w = ROOM_MIN_SIZE + rand() % (ROOM_MAX_SIZE - ROOM_MIN_SIZE + 1);
        int h = ROOM_MIN_SIZE + rand() % (ROOM_MAX_SIZE - ROOM_MIN_SIZE + 1);
        int x = rand() % (area->width - w - 1) + 1;
        int y = rand() % (area->height - h - 1) + 1;

        Room new_room = { x, y, w, h };
        create_room(area, new_room);

        if(room_count > 0) {
            int prev_x = rooms[room_count - 1].x + rooms[room_count - 1].w / 2;
            int prev_y = rooms[room_count - 1].y + rooms[room_count - 1].h / 2;
            int new_x = new_room.x + new_room.w / 2;
            int new_y = new_room.y + new_room.h / 2;

            if(rand() % 2) {
                create_h_corridor(area, prev_x, new_x, prev_y);
                create_v_corridor(area, prev_y, new_y, new_x);
            } else {
                create_v_corridor(area, prev_y, new_y, prev_x);
                create_h_corridor(area, prev_x, new_x, new_y);
            }
        }

        rooms[room_count++] = new_room;
    }
}

// Generate a town map with floor interior and wall borders.
static void generate_town(Area* area) {
    if(!area) return;
    fill_layer_with_tile(area, TILE_LAYER_GROUND, TILE_DIRT);
    fill_layer_with_tile(area, TILE_LAYER_FLOOR, TILE_STONE_FLOOR);
    // Town shape: border walls
    for(int x = 0; x < area->width; x++) {
        area->map[0][x][TILE_LAYER_STRUCTURE] = TILE_STONE_BRICK_WALL;
        area->map[area->height - 1][x][TILE_LAYER_STRUCTURE] = TILE_STONE_BRICK_WALL;
    }
    for(int y = 0; y < area->height; y++) {
        area->map[y][0][TILE_LAYER_STRUCTURE] = TILE_STONE_BRICK_WALL;
        area->map[y][area->width - 1][TILE_LAYER_STRUCTURE] = TILE_STONE_BRICK_WALL;
    }
}

static int map_roll_percent(int chance)
{
    if(chance <= 0)
        return 0;
    if(chance >= 100)
        return 1;
    return (rand() % 100) < chance;
}

static void generate_biome_wilderness(Area* area)
{
    Tile base_ground = TILE_GRASS;
    Tile blocker_tile = TILE_EMPTY;
    int blocker_percent = 0;
    int center_x;
    int center_y;

    if(!area)
        return;

    switch(area->biome)
    {
        case BIOME_DESERT:
            base_ground = TILE_SAND;
            blocker_tile = TILE_ROCK;
            blocker_percent = 6;
            break;
        case BIOME_TUNDRA:
            base_ground = TILE_GRAVEL;
            blocker_tile = TILE_ROCK;
            blocker_percent = 5;
            break;
        case BIOME_MOUNTAINS:
            base_ground = TILE_ROCK;
            blocker_tile = TILE_CAVE_WALL;
            blocker_percent = 22;
            break;
        case BIOME_FOOTHILLS:
            base_ground = TILE_GRAVEL;
            blocker_tile = TILE_ROCK;
            blocker_percent = 12;
            break;
        case BIOME_SWAMP:
            base_ground = TILE_MUD;
            blocker_tile = TILE_TREE;
            blocker_percent = 14;
            break;
        case BIOME_JUNGLE:
            base_ground = TILE_GRASS;
            blocker_tile = TILE_TREE;
            blocker_percent = 24;
            break;
        case BIOME_FOREST:
            base_ground = TILE_GRASS;
            blocker_tile = TILE_TREE;
            blocker_percent = 18;
            break;
        case BIOME_FARMLANDS:
            base_ground = TILE_DIRT;
            blocker_tile = TILE_LOG_WALL;
            blocker_percent = 4;
            break;
        case BIOME_SAVANNAH:
            base_ground = TILE_SAND;
            blocker_tile = TILE_TREE;
            blocker_percent = 4;
            break;
        case BIOME_RIVER:
            base_ground = TILE_MUD;
            blocker_tile = TILE_OUT_OF_BOUNDS;
            blocker_percent = 28;
            break;
        case BIOME_LAKE:
        case BIOME_SEA:
            base_ground = TILE_SAND;
            blocker_tile = TILE_OUT_OF_BOUNDS;
            blocker_percent = 55;
            break;
        case BIOME_GRASSLANDS:
        case BIOME_NONE:
        default:
            base_ground = TILE_GRASS;
            blocker_tile = TILE_TREE;
            blocker_percent = 7;
            break;
    }

    fill_layer_with_tile(area, TILE_LAYER_GROUND, base_ground);
    fill_layer_with_tile(area, TILE_LAYER_FLOOR, TILE_EMPTY);
    fill_layer_with_tile(area, TILE_LAYER_STRUCTURE, TILE_EMPTY);

    for(int y = 1; y < area->height - 1; y++)
    {
        for(int x = 1; x < area->width - 1; x++)
        {
            if(map_roll_percent(blocker_percent))
                area->map[y][x][TILE_LAYER_STRUCTURE] = blocker_tile;
            else if(map_roll_percent(20))
                area->map[y][x][TILE_LAYER_GROUND] = TILE_DIRT;
        }
    }

    center_x = area->width / 2;
    center_y = area->height / 2;

    paint_rect_layer(area, TILE_LAYER_GROUND, center_x - 7, center_y - 7, 15, 15, base_ground);
    paint_rect_layer(area, TILE_LAYER_FLOOR, center_x - 7, center_y - 7, 15, 15, TILE_STONE_FLOOR);
    paint_rect_layer(area, TILE_LAYER_STRUCTURE, center_x - 7, center_y - 7, 15, 15, TILE_EMPTY);
}

// Generate map data for one area according to its type.
void map_generate_area(Area* area) {
    int area_index;

    if(!area) return;

    map_clear_entity_markers(area);

    area_index = map_area_index(area);
    if(area_index >= 0)
        world_map_signposts_clear_area(area_index);

    if(area->is_generated && area->generation_seed != 0)
        srand(area->generation_seed);
    else
        srand((unsigned int)time(NULL));

    if(area->generation_mode == LOCATION_GENERATION_PREDEFINED)
    {
        if(!map_load_predefined_file(area))
        {
            clear_area_layers(area);
            fill_layer_with_tile(area, TILE_LAYER_GROUND, TILE_DIRT);
        }

        sync_tile_blocking_flags(area);
        if(map_validate_surface_layers(area) > 0)
            fprintf(stderr, "[map] Layer validation failed for predefined area '%s'.\n", area->name);
        return;
    }

    clear_area_layers(area);
    map_clear_discovery(area);

    if(area->is_generated)
    {
        generate_biome_wilderness(area);
        sync_tile_blocking_flags(area);
        if(map_validate_surface_layers(area) > 0)
            fprintf(stderr, "[map] Layer validation failed for generated area '%s'.\n", area->name);
        return;
    }

    switch(area->type) {
        case LOCATION_STARTER:
            generate_starter_glade(area);
            break;
        case LOCATION_TOWN:
            generate_town(area);
            break;
        case LOCATION_CRYPT:
        case LOCATION_CAVERN:
        case LOCATION_DUNGEON:
        default:
            generate_dungeon(area);
            break;
    }

    sync_tile_blocking_flags(area);
    if(map_validate_surface_layers(area) > 0)
        fprintf(stderr, "[map] Layer validation failed for area '%s'.\n", area->name);
}

// Convenience wrapper that regenerates current active area.
void generate_map() {
    if(!current_area) return;
    map_generate_area(current_area);
}

