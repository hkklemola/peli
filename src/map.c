#include "map.h"
#include "atlas.h"
#include "tileset.h"
#include <stdlib.h> // rand, srand
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
    int x, y, w, h;
} Room;

// Fill the entire area with one tile preset.
static void fill_with_tile(Area* area, Tile tile) {
    if(!area)
        return;

    for(int y = 0; y < area->height; y++)
        for(int x = 0; x < area->width; x++)
            area->map[y][x] = tile;
}

// Paint a clamped rectangle using one tile preset.
static void paint_rect(Area* area, int x, int y, int w, int h, Tile tile) {
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
            area->map[py][px] = tile;
}

static void sync_tile_blocking_flags(Area* area) {
    for(int y = 0; y < area->height; y++) {
        for(int x = 0; x < area->width; x++) {
            Tile* tile = &area->map[y][x];

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

// Check line of sight between two points using Bresenham's algorithm.
// The path is blocked if any intermediate tile blocks sight.
int map_has_line_of_sight(int x0, int y0, int x1, int y1)
{
    if(!current_area)
        return 0;

    if(x0 < 0 || y0 < 0 || x0 >= current_area->width || y0 >= current_area->height)
        return 0;
    if(x1 < 0 || y1 < 0 || x1 >= current_area->width || y1 >= current_area->height)
        return 0;

    const int max_dist = 30;
    const int dx_dist = x1 - x0;
    const int dy_dist = y1 - y0;
    if((dx_dist * dx_dist) + (dy_dist * dy_dist) > (max_dist * max_dist))
        return 0;

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

        if(current_area->map[y][x].blocks_sight)
            return 0;
    }
}


// Fill entire map with wall tiles.
static void fill_walls(Area* area) {
    fill_with_tile(area, TILE_WALL);
}

// Fill entire map with floor tiles.
static void fill_floor(Area* area) {
    fill_with_tile(area, TILE_STONE_FLOOR);
}

// Carve one rectangular room as floor.
static void create_room(Area* area, Room r) {
    paint_rect(area, r.x, r.y, r.w, r.h, TILE_STONE_FLOOR);
}

// Carve one horizontal floor corridor.
static void create_h_corridor(Area* area, int x1, int x2, int y) {
    for(int x = x1 < x2 ? x1 : x2; x <= (x1 > x2 ? x1 : x2); x++)
        area->map[y][x] = TILE_STONE_FLOOR;
}

// Carve one vertical floor corridor.
static void create_v_corridor(Area* area, int y1, int y2, int x) {
    for(int y = y1 < y2 ? y1 : y2; y <= (y1 > y2 ? y1 : y2); y++)
        area->map[y][x] = TILE_STONE_FLOOR;
}

// Generate the fixed open-air starter glade inside the larger map bounds.
static void generate_starter_glade(Area* area) {
    if(!area)
        return;

    const int center_x = area->width / 2;
    const int center_y = area->height / 2;

    fill_with_tile(area, TILE_GRASS);

    paint_rect(area, 0, center_y - 1, area->width, 3, TILE_DIRT_FLOOR);
    paint_rect(area, center_x - 1, 0, 3, area->height, TILE_DIRT_FLOOR);

    paint_rect(area, center_x - 4, center_y - 4, 9, 9, TILE_STONE_FLOOR);

    paint_rect(area, center_x - 18, center_y - 2, 8, 5, TILE_DIRT_FLOOR);
    paint_rect(area, center_x + 11, center_y - 2, 8, 5, TILE_DIRT_FLOOR);
    paint_rect(area, center_x - 2, center_y - 18, 5, 8, TILE_DIRT_FLOOR);
    paint_rect(area, center_x - 2, center_y + 11, 5, 8, TILE_DIRT_FLOOR);

    for(int y = 10; y < area->height; y += 20) {
        for(int x = 10; x < area->width; x += 20) {
            if(abs(x - center_x) < 12 && abs(y - center_y) < 12)
                continue;
            paint_rect(area, x - 2, y - 2, 4, 4, TILE_TREE);
        }
    }

    paint_rect(area, 4, 4, 4, 4, TILE_STONE_FLOOR);
    paint_rect(area, area->width - 8, 4, 4, 4, TILE_STONE_FLOOR);
    paint_rect(area, 4, area->height - 8, 4, 4, TILE_STONE_FLOOR);
    paint_rect(area, area->width - 8, area->height - 8, 4, 4, TILE_STONE_FLOOR);
}
// Generate dungeon rooms and connecting corridors.
static void generate_dungeon(Area* area) {
    if(!area) return;

    fill_walls(area);
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
    fill_floor(area);
    // Town shape: border walls
    for(int x = 0; x < area->width; x++) {
        area->map[0][x] = TILE_WALL;
        area->map[area->height - 1][x] = TILE_WALL;
    }
    for(int y = 0; y < area->height; y++) {
        area->map[y][0] = TILE_WALL;
        area->map[y][area->width - 1] = TILE_WALL;
    }
}

// Generate map data for one area according to its type.
void map_generate_area(Area* area) {
    if(!area) return;
    srand((unsigned int)time(NULL));

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

