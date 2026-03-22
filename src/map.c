#include "map.h"
#include "atlas.h"
#include "tileset.h"
#include "bestiary.h"
#include "world_items.h"
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

    for(int y = 0; y < area->height; y++)
        for(int x = 0; x < area->width; x++)
            area->map[y][x][layer] = tile;
}

static Tile map_tile_for_glyph_ground(char glyph)
{
    switch(glyph)
    {
        case ';': return TILE_GRASS;
        case '~': return TILE_OUT_OF_BOUNDS;
        default: return TILE_DIRT;
    }
}

static Tile map_tile_for_glyph_floor(char glyph)
{
    switch(glyph)
    {
        case '.': return TILE_STONE_FLOOR;
        case ',': return TILE_DIRT;
        default: return TILE_EMPTY;
    }
}

static Tile map_tile_for_glyph_structure(char glyph)
{
    switch(glyph)
    {
        case '#': return TILE_STONE_BRICK_WALL;
        case 'T': return TILE_TREE;
        case '+': return tile_door();
        case '<': return TILE_STAIRS_UP;
        default: return TILE_EMPTY;
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
            Tile ground = map_tile_for_glyph_ground(glyph);
            Tile floor = map_tile_for_glyph_floor(glyph);
            Tile structure = map_tile_for_glyph_structure(glyph);

            if(glyph == ' ')
                continue;

            area->map[y][x][TILE_LAYER_GROUND] = ground;
            area->map[y][x][TILE_LAYER_FLOOR] = floor;
            area->map[y][x][TILE_LAYER_STRUCTURE] = structure;
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
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;
    if(layer < 0 || layer >= TILE_LAYER_COUNT)
        return NULL;

    return &area->map[y][x][layer];
}

const Tile* map_top_visible_tile(const Area* area, int x, int y, TileLayer* out_layer)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;

    for(int layer = TILE_LAYER_EFFECT; layer >= TILE_LAYER_GROUND; layer--)
    {
        const Tile* tile = &area->map[y][x][layer];
        if(tile_is_empty(tile))
            continue;

        if(out_layer)
            *out_layer = (TileLayer)layer;
        return tile;
    }

    return NULL;
}

int map_cell_blocks_movement(const Area* area, int x, int y)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 1;

    for(int layer = TILE_LAYER_EFFECT; layer >= TILE_LAYER_GROUND; layer--)
    {
        const Tile* tile = &area->map[y][x][layer];
        if(!tile_is_empty(tile) && tile->blocks_movement)
            return 1;
    }

    return 0;
}

int map_cell_blocks_sight(const Area* area, int x, int y)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 1;

    for(int layer = TILE_LAYER_EFFECT; layer >= TILE_LAYER_GROUND; layer--)
    {
        const Tile* tile = &area->map[y][x][layer];
        if(!tile_is_empty(tile) && tile->blocks_sight)
            return 1;
    }

    return 0;
}

int map_collect_visible_static_layers(const Area* area, int x, int y, const Tile** out_tiles, TileLayer* out_layers, int max_count)
{
    int count = 0;

    if(!area || !out_tiles || max_count <= 0)
        return 0;
    if(x < 0 || y < 0 || x >= area->width || y >= area->height)
        return 0;

    for(int layer = TILE_LAYER_EFFECT; layer >= TILE_LAYER_GROUND; layer--)
    {
        const Tile* tile = &area->map[y][x][layer];
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

// Generate the fixed open-air starter glade inside the larger map bounds.
static void generate_starter_glade(Area* area) {
    if(!area)
        return;

    const int center_x = area->width / 2;
    const int center_y = area->height / 2;

    fill_layer_with_tile(area, TILE_LAYER_GROUND, TILE_GRASS);

    paint_rect_layer(area, TILE_LAYER_FLOOR, 0, center_y - 1, area->width, 3, TILE_DIRT);
    paint_rect_layer(area, TILE_LAYER_FLOOR, center_x - 1, 0, 3, area->height, TILE_DIRT);

    paint_rect_layer(area, TILE_LAYER_FLOOR, center_x - 4, center_y - 4, 9, 9, TILE_STONE_FLOOR);

    paint_rect_layer(area, TILE_LAYER_FLOOR, center_x - 18, center_y - 2, 8, 5, TILE_DIRT);
    paint_rect_layer(area, TILE_LAYER_FLOOR, center_x + 11, center_y - 2, 8, 5, TILE_DIRT);
    paint_rect_layer(area, TILE_LAYER_FLOOR, center_x - 2, center_y - 18, 5, 8, TILE_DIRT);
    paint_rect_layer(area, TILE_LAYER_FLOOR, center_x - 2, center_y + 11, 5, 8, TILE_DIRT);

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

    map_spawn_dev_hut(area, center_x + DEV_HUT_OFFSET_X, center_y + DEV_HUT_OFFSET_Y);
}
// Generate dungeon rooms and connecting corridors.
static void generate_dungeon(Area* area) {
    if(!area) return;

    fill_layer_with_tile(area, TILE_LAYER_GROUND, TILE_DIRT);
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

// Generate map data for one area according to its type.
void map_generate_area(Area* area) {
    if(!area) return;
    srand((unsigned int)time(NULL));

    if(area->generation_mode == LOCATION_GENERATION_PREDEFINED)
    {
        if(map_load_predefined_file(area))
        {
            sync_tile_blocking_flags(area);
            return;
        }
    }

    clear_area_layers(area);

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
}

// Convenience wrapper that regenerates current active area.
void generate_map() {
    if(!current_area) return;
    map_generate_area(current_area);
}

