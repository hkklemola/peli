#include "atlas.h"
#include "furniture.h"
#include "map.h"
#include "tileset.h"
#include "bestiary.h"
#include "world_items.h"
#include "character.h"
#include "player.h"
#include "item_data.h"
#include "tile.h"
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



static int map_active_floor_index(const Area* area);
static int map_upper_floor_slot_from_z(const Area* area, int z);
static int map_upper_floor_contains(const Area* area, int x, int y, int z);
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

static int map_ensure_area_storage(Area* area)
{
    if(!area)
        return 0;

    if(!area->map)
        area->map = calloc((size_t)MAP_HEIGHT, sizeof(*area->map));
    if(!area->discovered)
        area->discovered = calloc((size_t)MAP_HEIGHT, sizeof(*area->discovered));
    if(!area->entity_marker_active)
        area->entity_marker_active = calloc((size_t)MAP_HEIGHT, sizeof(*area->entity_marker_active));
    if(!area->entity_marker_symbol)
        area->entity_marker_symbol = calloc((size_t)MAP_HEIGHT, sizeof(*area->entity_marker_symbol));
    if(!area->entity_marker_color)
        area->entity_marker_color = calloc((size_t)MAP_HEIGHT, sizeof(*area->entity_marker_color));
    if(!area->entity_marker_z)
        area->entity_marker_z = calloc((size_t)MAP_HEIGHT, sizeof(*area->entity_marker_z));

    if(!area->map || !area->discovered || !area->entity_marker_active ||
       !area->entity_marker_symbol || !area->entity_marker_color || !area->entity_marker_z)
    {
        free(area->map);
        free(area->discovered);
        free(area->entity_marker_active);
        free(area->entity_marker_symbol);
        free(area->entity_marker_color);
        free(area->entity_marker_z);
        area->map = NULL;
        area->discovered = NULL;
        area->entity_marker_active = NULL;
        area->entity_marker_symbol = NULL;
        area->entity_marker_color = NULL;
        area->entity_marker_z = NULL;
        return 0;
    }

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

static int map_upper_floor_slot_from_z(const Area* area, int z)
{
    if(!area || area->upper_floor_count <= 0)
        return -1;
    if(z <= AREA_GROUND_Z)
        return -1;
    if(z > AREA_GROUND_Z + area->upper_floor_count)
        return -1;
    return z - AREA_GROUND_Z - 1;
}

static int map_upper_floor_contains(const Area* area, int x, int y, int z)
{
    int slot = map_upper_floor_slot_from_z(area, z);

    if(slot < 0)
        return 0;

    return x >= area->upper_floor_origin_x &&
           y >= area->upper_floor_origin_y &&
           x < area->upper_floor_origin_x + area->upper_floor_width &&
           y < area->upper_floor_origin_y + area->upper_floor_height;
}

static int map_min_view_floor(const Area* area)
{
    (void)area;
    return AREA_GROUND_Z;
}

int map_max_view_floor(const Area* area)
{
    if(!area || area->upper_floor_count <= 0)
        return AREA_GROUND_Z;

    return AREA_GROUND_Z + area->upper_floor_count;
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
    int z = character_z();

    if(z < AREA_GROUND_Z)
        z = AREA_GROUND_Z;

    return map_clamp_view_floor(area, z);
}

int map_is_tile_discovered(const Area* area, int x, int y)
{
    if(!area || !area->discovered || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 0;
    return area->discovered[y][x] ? 1 : 0;
}

void map_mark_tile_discovered(Area* area, int x, int y)
{
    if(!area || !area->discovered || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return;
    area->discovered[y][x] = 1;
}

void map_clear_discovery(Area* area)
{
    if(!area || !area->discovered)
        return;

    for(int y = 0; y < area->height; y++)
        for(int x = 0; x < area->width; x++)
            area->discovered[y][x] = 0;
}

void map_clear_entity_markers(Area* area)
{
    if(!area || !area->entity_marker_active || !area->entity_marker_symbol ||
       !area->entity_marker_color || !area->entity_marker_z)
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
    if(!area || !area->entity_marker_active || !area->entity_marker_symbol ||
       !area->entity_marker_color || !area->entity_marker_z ||
       x < 0 || y < 0 || x >= area->width || y >= area->height)
        return;

    area->entity_marker_active[y][x] = 1;
    area->entity_marker_symbol[y][x] = symbol;
    area->entity_marker_color[y][x] = color;
    area->entity_marker_z[y][x] = z;
}

void map_clear_entity_marker(Area* area, int x, int y, int z)
{
    if(!area || !area->entity_marker_active || !area->entity_marker_symbol ||
       !area->entity_marker_color || !area->entity_marker_z ||
       x < 0 || y < 0 || x >= area->width || y >= area->height)
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
    if(!area || !area->entity_marker_active || !area->entity_marker_symbol ||
       !area->entity_marker_color || !area->entity_marker_z ||
       x < 0 || y < 0 || x >= area->width || y >= area->height)
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
    int slot;

    if(!area || !area->map || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;
    if(layer < 0 || layer >= TILE_LAYER_COUNT)
        return NULL;

    if(map_upper_floor_contains(area, x, y, floor))
    {
        slot = map_upper_floor_slot_from_z(area, floor);
        local_x = x - area->upper_floor_origin_x;
        local_y = y - area->upper_floor_origin_y;
        return &area->upper_floor_maps[slot][local_y][local_x][layer];
    }

    if(floor > AREA_GROUND_Z)
        return &TILE_EMPTY;

    return &area->map[y][x][layer];
}

Tile* map_tile_at_layer_z(Area* area, int x, int y, int z, TileLayer layer)
{
    int local_x;
    int local_y;
    int slot;

    if(!area || !area->map || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;
    if(layer < 0 || layer >= TILE_LAYER_COUNT)
        return NULL;

    if(map_upper_floor_contains(area, x, y, z))
    {
        slot = map_upper_floor_slot_from_z(area, z);
        local_x = x - area->upper_floor_origin_x;
        local_y = y - area->upper_floor_origin_y;
        return &area->upper_floor_maps[slot][local_y][local_x][layer];
    }

    if(z > AREA_GROUND_Z)
        return NULL;

    return &area->map[y][x][layer];
}

static void clear_area_layers(Area* area)
{
    if(!area)
        return;

    area->upper_floor_origin_x = 0;
    area->upper_floor_origin_y = 0;
    area->upper_floor_width = 0;
    area->upper_floor_height = 0;
    area->upper_floor_count = 0;

    for(int y = 0; y < area->height; y++)
    {
        for(int x = 0; x < area->width; x++)
        {
            for(int layer = 0; layer < TILE_LAYER_COUNT; layer++)
                area->map[y][x][layer] = TILE_EMPTY;
        }
    }

    for(int floor = 0; floor < MAX_AREA_FLOORS - 1; floor++)
    {
        for(int y = 0; y < AREA_UPPER_FLOOR_MAX_HEIGHT; y++)
        {
            for(int x = 0; x < AREA_UPPER_FLOOR_MAX_WIDTH; x++)
            {
                for(int layer = 0; layer < TILE_LAYER_COUNT; layer++)
                    area->upper_floor_maps[floor][y][x][layer] = TILE_EMPTY;
            }
        }
    }
}

static void fill_layer_with_tile(Area* area, TileLayer layer, Tile tile)
{
    if(!area || layer < 0 || layer >= TILE_LAYER_COUNT)
        return;

    if(!tile_layer_accepts_surface(layer == TILE_LAYER_WALL ? TILE_LAYER_WALL : layer, tile_surface_kind(&tile)))
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
            *out_layer = TILE_LAYER_WALL;
            *out_tile = TILE_STONE_BRICK_WALL;
            return 1;
        case 'T':
            *out_layer = TILE_LAYER_WALL;
            *out_tile = TILE_TREE;
            return 1;
        case '+':
            // Door is now entity-based; do not place as tile
            return 0;
        case '<':
            *out_layer = TILE_LAYER_WALL;
            *out_tile = TILE_STAIRS_UP;
            return 1;
        case '>':
            *out_layer = TILE_LAYER_WALL;
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

static void paint_rect_layer_z(Area* area, int z, TileLayer layer, int x, int y, int w, int h, Tile tile)
{
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
    {
        for(int px = start_x; px < end_x; px++)
        {
            Tile* dest = map_tile_at_layer_z(area, px, py, z, layer);
            if(dest)
                *dest = tile;
        }
    }
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
            const Tile* structure = &area->map[y][x][TILE_LAYER_WALL];

            if(!tile_layer_accepts_surface(TILE_LAYER_GROUND, tile_surface_kind(ground)))
                violations++;
            if(!tile_layer_accepts_surface(TILE_LAYER_FLOOR, tile_surface_kind(floor)))
                violations++;
            if(!tile_layer_accepts_surface(TILE_LAYER_WALL, tile_surface_kind(structure)))
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

    Tile* tile = map_tile_at_layer(area, x, y, TILE_LAYER_WALL);
    if(!tile)
        return;

    *tile = TILE_STAIRS_UP;
}

Tile* map_tile_at_layer(Area* area, int x, int y, TileLayer layer)
{
    int floor;

    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;
    if(layer < 0 || layer >= TILE_LAYER_COUNT)
        return NULL;

    floor = map_active_floor_index(area);
    return map_tile_at_layer_z(area, x, y, floor, layer);
}

const Tile* map_top_visible_tile(const Area* area, int x, int y, TileLayer* out_layer)
{
    return map_top_visible_tile_at_view(area, x, y, map_active_floor_index(area), out_layer);
}

const Tile* map_top_visible_tile_at_view(const Area* area, int x, int y, int view_floor, TileLayer* out_layer)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;

    view_floor = map_clamp_view_floor(area, view_floor);

    for(int floor = view_floor; floor >= 0; floor--)
    {

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
    Furniture* furn;

    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 1;

    furn = furniture_at(area, x, y);
    if(furn && furn->blocks_movement)
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
    Furniture* furn;

    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 1;

    furn = furniture_at(area, x, y);
    if(furn && furn->blocks_sight)
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
    paint_rect_layer(area, TILE_LAYER_WALL, r.x, r.y, r.w, r.h, TILE_EMPTY);
}

// Carve one horizontal floor corridor.
static void create_h_corridor(Area* area, int x1, int x2, int y) {
    for(int x = x1 < x2 ? x1 : x2; x <= (x1 > x2 ? x1 : x2); x++)
    {
        area->map[y][x][TILE_LAYER_FLOOR] = TILE_STONE_FLOOR;
        area->map[y][x][TILE_LAYER_WALL] = TILE_EMPTY;
    }
}

// Carve one vertical floor corridor.
static void create_v_corridor(Area* area, int y1, int y2, int x) {
    for(int y = y1 < y2 ? y1 : y2; y <= (y1 > y2 ? y1 : y2); y++)
    {
        area->map[y][x][TILE_LAYER_FLOOR] = TILE_STONE_FLOOR;
        area->map[y][x][TILE_LAYER_WALL] = TILE_EMPTY;
    }
}

static void map_container_add_template_item(int container_index, const char* template_name, int quantity_override)
{
    const ItemTemplate* tmpl;
    Item item;

    if(container_index < 0 || container_index >= MAX_WORLD_CONTAINERS || !template_name || !template_name[0])
        return;

    tmpl = item_template_by_name(template_name);
    if(!tmpl)
        return;

    item_init_from_template(&item, tmpl, -1, -1);
    if(quantity_override > 0)
        item.quantity = quantity_override;
    if(item.quantity < 1)
        item.quantity = 1;

    while(1)
    {
        Item stack = item;
        if(stack.stackable && stack.stack_max > 0 && stack.quantity > stack.stack_max)
            stack.quantity = stack.stack_max;
        if(!world_container_add_item(container_index, &stack))
            break;
        if(!item.stackable || quantity_override <= 0 || item.quantity <= stack.quantity)
            break;
        item.quantity -= stack.quantity;
    }
}

static void map_container_add_item_set(int container_index, const char* const* item_names, int item_count)
{
    if(container_index < 0 || container_index >= MAX_WORLD_CONTAINERS || !item_names || item_count <= 0)
        return;

    for(int i = 0; i < item_count; ++i)
    {
        if(item_names[i] && item_names[i][0] != '\0')
            map_container_add_template_item(container_index, item_names[i], 1);
    }
}

static void map_container_add_gold(int container_index, int amount)
{
    Item gold;

    if(container_index < 0 || container_index >= MAX_WORLD_CONTAINERS || amount <= 0)
        return;

    item_init(&gold, "Gold Coins", '$', -1, -1, ITEM_TYPE_KEY, 1, amount);
    gold.stack_max = 999;
    gold.object.base.color = RENDER_COLOR_LIGHT_YELLOW;
    (void)world_container_add_item(container_index, &gold);
}

static void map_container_add_ammo_stack(int container_index, const char* ammo_name, int amount)
{
    Item ammo;

    if(container_index < 0 || container_index >= MAX_WORLD_CONTAINERS || !ammo_name || !ammo_name[0] || amount <= 0)
        return;

    item_init(&ammo, ammo_name, ',', -1, -1, ITEM_TYPE_CONSUMABLE, 1, amount);
    ammo.stack_max = 99;
    ammo.is_ammo = 1;
    ammo.object.base.color = RENDER_COLOR_LIGHT_YELLOW;
    (void)world_container_add_item(container_index, &ammo);
}

void map_spawn_starter_hut(Area* area, int origin_x, int origin_y)
{
    static const char* const arming_set[] = {
        "Arming Cap",
        "Quilted Collar",
        "Padded Mantle",
        "Gambeson",
        "Padded Sleeves",
        "Arming Gloves",
        "Arming Belt",
        "Padded Hose",
        "Padded Footwraps"
    };
    static const char* const mail_set[] = {
        "Mail Coif",
        "Mail Standard",
        "Mail Mantle",
        "Mail Hauberk",
        "Mail Skirt",
        "Mail Sleeves",
        "Mail Mittens",
        "Mail Chausses",
        "Mail Boots"
    };
    static const char* const plate_set[] = {
        "Plate Helm",
        "Steel Gorget",
        "Plate Pauldrons",
        "Breastplate",
        "Plate Fauld",
        "Plate Vambraces",
        "Steel Gauntlets",
        "Plate Cuisses",
        "Sabatons"
    };
    int x;
    int y;
    int wardrobe_index;
    int chest_index;
    int weapon_rack_index;
    int armor_rack_1_index;
    int armor_rack_2_index;
    int armor_rack_3_index;

    if(!area)
        return;

    x = origin_x;
    y = origin_y;

    if(x < 1) x = 1;
    if(y < 1) y = 1;
    if(x + STARTER_HUT_WIDTH >= area->width - 1)
        x = area->width - STARTER_HUT_WIDTH - 2;
    if(y + STARTER_HUT_HEIGHT >= area->height - 1)
        y = area->height - STARTER_HUT_HEIGHT - 2;

    paint_rect_layer(area, TILE_LAYER_WALL, x, y, STARTER_HUT_WIDTH, STARTER_HUT_HEIGHT, TILE_LOG_WALL);
    paint_rect_layer(area, TILE_LAYER_FLOOR, x + 1, y + 1, STARTER_HUT_WIDTH - 2, STARTER_HUT_HEIGHT - 2, TILE_WOOD_PLANK);
    paint_rect_layer(area, TILE_LAYER_WALL, x + 1, y + 1, STARTER_HUT_WIDTH - 2, STARTER_HUT_HEIGHT - 2, tile_empty());

    (void)furniture_spawn(area, FURNITURE_DOOR, x + (STARTER_HUT_WIDTH / 2), y + STARTER_HUT_HEIGHT - 1);
    area->map[y + STARTER_HUT_HEIGHT - 1][x + (STARTER_HUT_WIDTH / 2)][TILE_LAYER_WALL] = tile_empty();

    (void)furniture_spawn(area, FURNITURE_BED, x + 2, y + 3);
    (void)furniture_spawn(area, FURNITURE_TABLE, x + 5, y + 4);
    (void)furniture_spawn(area, FURNITURE_CHAIR, x + 4, y + 5);
    wardrobe_index = furniture_spawn(area, FURNITURE_WARDROBE, x + 1, y + 2);
    armor_rack_1_index = furniture_spawn(area, FURNITURE_ARMOR_RACK, x + 3, y + 1);
    armor_rack_2_index = furniture_spawn(area, FURNITURE_ARMOR_RACK, x + 5, y + 1);
    armor_rack_3_index = furniture_spawn(area, FURNITURE_ARMOR_RACK, x + 7, y + 1);
    chest_index = furniture_spawn(area, FURNITURE_CHEST, x + 9, y + 2);
    weapon_rack_index = furniture_spawn(area, FURNITURE_WEAPON_RACK, x + 9, y + 4);

    if(wardrobe_index >= 0)
    {
        int container_index = area->furniture[wardrobe_index].world_container_index;
        map_container_add_template_item(container_index, "Linen Footwraps", 1);
        map_container_add_template_item(container_index, "Linen Trousers", 1);
        map_container_add_template_item(container_index, "Linen Shirt", 1);
        map_container_add_template_item(container_index, "Felt Hat", 1);
        map_container_add_template_item(container_index, "Arming Cap", 1);
        map_container_add_template_item(container_index, "Gambeson", 1);
        map_container_add_template_item(container_index, "Small Linen Pouch", 1);
    }

    if(armor_rack_1_index >= 0)
    {
        int container_index = area->furniture[armor_rack_1_index].world_container_index;
        map_container_add_item_set(container_index, arming_set, (int)(sizeof(arming_set) / sizeof(arming_set[0])));
    }

    if(armor_rack_2_index >= 0)
    {
        int container_index = area->furniture[armor_rack_2_index].world_container_index;
        map_container_add_item_set(container_index, mail_set, (int)(sizeof(mail_set) / sizeof(mail_set[0])));
    }

    if(armor_rack_3_index >= 0)
    {
        int container_index = area->furniture[armor_rack_3_index].world_container_index;
        map_container_add_item_set(container_index, plate_set, (int)(sizeof(plate_set) / sizeof(plate_set[0])));
    }

    if(chest_index >= 0)
    {
        int container_index = area->furniture[chest_index].world_container_index;
        map_container_add_gold(container_index, 50);
        map_container_add_template_item(container_index, "Healing Potion", 5);
        map_container_add_template_item(container_index, "Iron Ore", 3);
        map_container_add_template_item(container_index, "Wood Log", 2);
        map_container_add_template_item(container_index, "Cloth Bolt", 1);
    }

    (void)furniture_spawn(area, FURNITURE_ANVIL, x + 6, y + 5);
    (void)furniture_spawn(area, FURNITURE_FORGE, x + 7, y + 5);

    if(weapon_rack_index >= 0)
    {
        int container_index = area->furniture[weapon_rack_index].world_container_index;
        map_container_add_template_item(container_index, "Quarterstaff", 1);
        map_container_add_template_item(container_index, "Hatchet", 1);
        map_container_add_template_item(container_index, "Short Bow", 1);
        map_container_add_ammo_stack(container_index, "Arrow", 20);
        map_container_add_template_item(container_index, "Leather Quiver", 1);
    }
}

static void map_spawn_hermit_tower_floor(Area* area, int x, int y, int z, int floor_index)
{
    int chest_index = -1;
    int wardrobe_index = -1;
    int rack_index = -1;
    int stair_x;
    int stair_a_y;
    int stair_b_y;
    int stairs_down_y;
    int stairs_up_y;

    if(!area)
        return;

    stair_x = x + (HERMIT_TOWER_WIDTH / 2);
    stair_a_y = y + (HERMIT_TOWER_HEIGHT / 2) - 1;
    stair_b_y = stair_a_y + 1;
    stairs_down_y = (floor_index % 2 == 1) ? stair_a_y : stair_b_y;
    stairs_up_y = (floor_index % 2 == 0) ? stair_a_y : stair_b_y;

    paint_rect_layer_z(area, z, TILE_LAYER_WALL, x, y, HERMIT_TOWER_WIDTH, HERMIT_TOWER_HEIGHT, TILE_STONE_BRICK_WALL);
    paint_rect_layer_z(area, z, TILE_LAYER_FLOOR, x + 1, y + 1, HERMIT_TOWER_WIDTH - 2, HERMIT_TOWER_HEIGHT - 2, TILE_WOOD_PLANK);
    paint_rect_layer_z(area, z, TILE_LAYER_WALL, x + 1, y + 1, HERMIT_TOWER_WIDTH - 2, HERMIT_TOWER_HEIGHT - 2, tile_empty());

    if(z == HERMIT_TOWER_BASE_Z)
    {
        Tile* entry = map_tile_at_layer_z(area, x + (HERMIT_TOWER_WIDTH / 2), y + HERMIT_TOWER_HEIGHT - 1, z, TILE_LAYER_WALL);
        if(entry)
            *entry = tile_empty();
        (void)furniture_spawn_at_z(area, FURNITURE_DOOR, x + (HERMIT_TOWER_WIDTH / 2), y + HERMIT_TOWER_HEIGHT - 1, z);
    }

    if(floor_index > 0)
    {
        Tile* stairs_down = map_tile_at_layer_z(area, stair_x, stairs_down_y, z, TILE_LAYER_WALL);
        if(stairs_down)
            *stairs_down = TILE_STAIRS_DOWN;
    }

    if(floor_index < HERMIT_TOWER_MAX_FLOORS - 1)
    {
        Tile* stairs_up = map_tile_at_layer_z(area, stair_x, stairs_up_y, z, TILE_LAYER_WALL);
        if(stairs_up)
            *stairs_up = TILE_STAIRS_UP;
    }

    switch(floor_index)
    {
        case 0:
            (void)furniture_spawn_at_z(area, FURNITURE_TABLE, x + 2, y + 2, z);
            (void)furniture_spawn_at_z(area, FURNITURE_CHAIR, x + 3, y + 2, z);
            (void)furniture_spawn_at_z(area, FURNITURE_BED, x + HERMIT_TOWER_WIDTH - 3, y + 2, z);
            wardrobe_index = furniture_spawn_at_z(area, FURNITURE_WARDROBE, x + 2, y + HERMIT_TOWER_HEIGHT - 3, z);
            chest_index = furniture_spawn_at_z(area, FURNITURE_CHEST, x + HERMIT_TOWER_WIDTH - 3, y + HERMIT_TOWER_HEIGHT - 3, z);
            (void)furniture_spawn_at_z(area, FURNITURE_BARREL, x + HERMIT_TOWER_WIDTH - 3, y + (HERMIT_TOWER_HEIGHT / 2), z);
            break;
        case 1:
            (void)furniture_spawn_at_z(area, FURNITURE_TABLE, x + 2, y + 2, z);
            (void)furniture_spawn_at_z(area, FURNITURE_TABLE, x + HERMIT_TOWER_WIDTH - 3, y + 2, z);
            (void)furniture_spawn_at_z(area, FURNITURE_CHAIR, x + 2, y + 3, z);
            (void)furniture_spawn_at_z(area, FURNITURE_CHAIR, x + HERMIT_TOWER_WIDTH - 3, y + 3, z);
            chest_index = furniture_spawn_at_z(area, FURNITURE_CHEST, x + 2, y + HERMIT_TOWER_HEIGHT - 3, z);
            (void)furniture_spawn_at_z(area, FURNITURE_BARREL, x + HERMIT_TOWER_WIDTH - 3, y + HERMIT_TOWER_HEIGHT - 3, z);
            break;
        case 2:
            rack_index = furniture_spawn_at_z(area, FURNITURE_WEAPON_RACK, x + 2, y + 2, z);
            chest_index = furniture_spawn_at_z(area, FURNITURE_CHEST, x + HERMIT_TOWER_WIDTH - 3, y + 2, z);
            (void)furniture_spawn_at_z(area, FURNITURE_TABLE, x + 2, y + HERMIT_TOWER_HEIGHT - 3, z);
            (void)furniture_spawn_at_z(area, FURNITURE_CHAIR, x + 3, y + HERMIT_TOWER_HEIGHT - 3, z);
            (void)furniture_spawn_at_z(area, FURNITURE_BARREL, x + HERMIT_TOWER_WIDTH - 3, y + HERMIT_TOWER_HEIGHT - 3, z);
            break;
        case 3:
            (void)furniture_spawn_at_z(area, FURNITURE_BED, x + 2, y + 2, z);
            (void)furniture_spawn_at_z(area, FURNITURE_BED, x + HERMIT_TOWER_WIDTH - 3, y + 2, z);
            wardrobe_index = furniture_spawn_at_z(area, FURNITURE_WARDROBE, x + 2, y + HERMIT_TOWER_HEIGHT - 3, z);
            chest_index = furniture_spawn_at_z(area, FURNITURE_CHEST, x + HERMIT_TOWER_WIDTH - 3, y + HERMIT_TOWER_HEIGHT - 3, z);
            break;
        case 4:
        default:
            (void)furniture_spawn_at_z(area, FURNITURE_TABLE, x + (HERMIT_TOWER_WIDTH / 2) - 1, y + 2, z);
            (void)furniture_spawn_at_z(area, FURNITURE_CHAIR, x + (HERMIT_TOWER_WIDTH / 2), y + 2, z);
            chest_index = furniture_spawn_at_z(area, FURNITURE_CHEST, x + 2, y + HERMIT_TOWER_HEIGHT - 3, z);
            (void)furniture_spawn_at_z(area, FURNITURE_BARREL, x + HERMIT_TOWER_WIDTH - 3, y + HERMIT_TOWER_HEIGHT - 3, z);
            break;
    }

    if(wardrobe_index >= 0)
    {
        int container_index = area->furniture[wardrobe_index].world_container_index;
        map_container_add_template_item(container_index, "Linen Footwraps", 1);
        map_container_add_template_item(container_index, "Linen Trousers", 1);
        map_container_add_template_item(container_index, "Linen Shirt", 1);
    }

    if(chest_index >= 0)
    {
        int container_index = area->furniture[chest_index].world_container_index;

        switch(floor_index)
        {
            case 0:
                map_container_add_template_item(container_index, "Mana Potion", 1);
                map_container_add_template_item(container_index, "Bedroll", 1);
                break;
            case 1:
                map_container_add_gold(container_index, 20);
                map_container_add_template_item(container_index, "Healing Potion", 2);
                break;
            case 2:
                map_container_add_template_item(container_index, "Quarterstaff", 1);
                map_container_add_template_item(container_index, "Hatchet", 1);
                break;
            case 3:
                map_container_add_template_item(container_index, "Bedroll", 1);
                map_container_add_template_item(container_index, "Healing Potion", 1);
                map_container_add_template_item(container_index, "Mail Coif", 1);
                map_container_add_template_item(container_index, "Mail Hauberk", 1);
                break;
            case 4:
            default:
                map_container_add_gold(container_index, 35);
                map_container_add_template_item(container_index, "Mana Potion", 2);
                map_container_add_template_item(container_index, "Plate Helm", 1);
                map_container_add_template_item(container_index, "Breastplate", 1);
                break;
        }
    }

    if(rack_index >= 0)
    {
        int container_index = area->furniture[rack_index].world_container_index;
        map_container_add_template_item(container_index, "Short Bow", 1);
        map_container_add_ammo_stack(container_index, "Arrow", 20);
    }
}

void map_spawn_hermit_tower(Area* area, int origin_x, int origin_y)
{
    int x;
    int y;

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

    area->upper_floor_origin_x = x;
    area->upper_floor_origin_y = y;
    area->upper_floor_width = HERMIT_TOWER_WIDTH;
    area->upper_floor_height = HERMIT_TOWER_HEIGHT;
    area->upper_floor_count = HERMIT_TOWER_MAX_FLOORS - 1;

    for(int floor_index = 0; floor_index < HERMIT_TOWER_MAX_FLOORS; floor_index++)
    {
        int z = HERMIT_TOWER_BASE_Z + floor_index;
        map_spawn_hermit_tower_floor(area, x, y, z, floor_index);
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
            paint_rect_layer(area, TILE_LAYER_WALL, x - 2, y - 2, 4, 4, TILE_TREE);
        }
    }

    paint_rect_layer(area, TILE_LAYER_FLOOR, 4, 4, 4, 4, TILE_STONE_FLOOR);
    paint_rect_layer(area, TILE_LAYER_FLOOR, area->width - 8, 4, 4, 4, TILE_STONE_FLOOR);
    paint_rect_layer(area, TILE_LAYER_FLOOR, 4, area->height - 8, 4, 4, TILE_STONE_FLOOR);
    paint_rect_layer(area, TILE_LAYER_FLOOR, area->width - 8, area->height - 8, 4, 4, TILE_STONE_FLOOR);

    (void) furniture_spawn(area, FURNITURE_SIGNPOST, center_x, center_y - 5);
    (void) furniture_spawn(area, FURNITURE_TARGET_DUMMY, center_x + 2, center_y + 2);
    area->map[center_y - 5][center_x][TILE_LAYER_WALL] = tile_empty();

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

    map_spawn_starter_hut(area, center_x + STARTER_HUT_OFFSET_X, center_y + STARTER_HUT_OFFSET_Y);
    map_spawn_hermit_tower(area, center_x + HERMIT_TOWER_OFFSET_X, center_y + HERMIT_TOWER_OFFSET_Y);
}
// Generate dungeon rooms and connecting corridors.
static void generate_dungeon(Area* area) {
    if(!area) return;

    fill_layer_with_tile(area, TILE_LAYER_GROUND, TILE_DIRT);
    fill_layer_with_tile(area, TILE_LAYER_FLOOR, TILE_EMPTY);
    fill_layer_with_tile(area, TILE_LAYER_WALL, TILE_STONE_BRICK_WALL);
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
        area->map[0][x][TILE_LAYER_WALL] = TILE_STONE_BRICK_WALL;
        area->map[area->height - 1][x][TILE_LAYER_WALL] = TILE_STONE_BRICK_WALL;
    }
    for(int y = 0; y < area->height; y++) {
        area->map[y][0][TILE_LAYER_WALL] = TILE_STONE_BRICK_WALL;
        area->map[y][area->width - 1][TILE_LAYER_WALL] = TILE_STONE_BRICK_WALL;
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
    fill_layer_with_tile(area, TILE_LAYER_WALL, TILE_EMPTY);

    for(int y = 1; y < area->height - 1; y++)
    {
        for(int x = 1; x < area->width - 1; x++)
        {
            if(map_roll_percent(blocker_percent))
                area->map[y][x][TILE_LAYER_WALL] = blocker_tile;
            else if(map_roll_percent(20))
                area->map[y][x][TILE_LAYER_GROUND] = TILE_DIRT;
        }
    }

    center_x = area->width / 2;
    center_y = area->height / 2;

    paint_rect_layer(area, TILE_LAYER_GROUND, center_x - 7, center_y - 7, 15, 15, base_ground);
    paint_rect_layer(area, TILE_LAYER_FLOOR, center_x - 7, center_y - 7, 15, 15, TILE_STONE_FLOOR);
    paint_rect_layer(area, TILE_LAYER_WALL, center_x - 7, center_y - 7, 15, 15, TILE_EMPTY);
}

// Generate map data for one area according to its type.
void map_generate_area(Area* area) {
    int area_index;

    if(!area) return;
    if(!map_ensure_area_storage(area))
    {
        fprintf(stderr, "[map] Failed to allocate area storage for '%s' (%dx%d).\n",
                area->name,
                area->width,
                area->height);
        return;
    }

    map_clear_entity_markers(area);
    furniture_clear(area);

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
        area->map_generated = 1;
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
        area->map_generated = 1;
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
    area->map_generated = 1;
}

// Convenience wrapper that regenerates current active area.
void generate_map() {
    if(!current_area) return;
    map_generate_area(current_area);
}

