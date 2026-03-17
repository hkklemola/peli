#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/tileset.h"
#include <stdlib.h> // rand, srand
#include <time.h>   // time

// Simple rectangle room
typedef struct {
    int x, y, w, h;
} Room;

// Fill the map with walls
static void fill_walls(Area* area) {
    for(int y = 0; y < MAP_HEIGHT; y++)
        for(int x = 0; x < MAP_WIDTH; x++)
            area->map[y][x] = TILE_WALL;
}

// Create a room: fill area with floor tiles
static void create_room(Area* area, Room r) {
    for(int y = r.y; y < r.y + r.h && y < MAP_HEIGHT; y++)
        for(int x = r.x; x < r.x + r.w && x < MAP_WIDTH; x++)
            area->map[y][x] = TILE_FLOOR;
}

// Connect two rooms with corridors
static void create_h_corridor(Area* area, int x1, int x2, int y) {
    for(int x = x1 < x2 ? x1 : x2; x <= (x1 > x2 ? x1 : x2); x++)
        area->map[y][x] = TILE_FLOOR;
}

static void create_v_corridor(Area* area, int y1, int y2, int x) {
    for(int y = y1 < y2 ? y1 : y2; y <= (y1 > y2 ? y1 : y2); y++)
        area->map[y][x] = TILE_FLOOR;
}

// Generate dungeon layout
void generate_map() {
    Area* area = current_area;
    if(!area) return;

    srand((unsigned int)time(NULL));

    fill_walls(area);

    Room rooms[MAX_ROOMS];
    int room_count = 0;

    for(int i = 0; i < MAX_ROOMS; i++) {
        int w = ROOM_MIN_SIZE + rand() % (ROOM_MAX_SIZE - ROOM_MIN_SIZE + 1);
        int h = ROOM_MIN_SIZE + rand() % (ROOM_MAX_SIZE - ROOM_MIN_SIZE + 1);
        int x = rand() % (MAP_WIDTH - w - 1) + 1;
        int y = rand() % (MAP_HEIGHT - h - 1) + 1;

        Room new_room = { x, y, w, h };
        create_room(area, new_room);

        // Connect to previous room
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