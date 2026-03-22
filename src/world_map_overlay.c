#include "world_map_overlay.h"

#include <stdio.h>

#include "atlas.h"
#include "draw.h"
#include "input.h"
#include "overlay_nav.h"
#include "ui_overlay.h"
#include "world_map.h"

static int pending_area_index = -1;

static void world_map_start_position(int* out_x, int* out_y)
{
    int current_index = -1;

    if(!out_x || !out_y)
        return;

    *out_x = WORLD_MAP_WIDTH / 2;
    *out_y = WORLD_MAP_HEIGHT / 2;

    if(world_map_get_overworld_position(out_x, out_y))
        return;

    if(current_area)
        current_index = atlas_find_location(current_area->name);

    if(current_index >= 0)
        (void)world_map_find_zone(current_index, out_x, out_y);
}

static int world_map_try_step(Player* player, int* x, int* y, int dx, int dy, char status[192])
{
    int nx;
    int ny;
    WorldMapTile* tile;

    if(!player || !x || !y || !status)
        return 0;

    nx = *x + dx;
    ny = *y + dy;

    if(nx < 0 || nx >= WORLD_MAP_WIDTH || ny < 0 || ny >= WORLD_MAP_HEIGHT)
    {
        snprintf(status, 192, "You cannot travel beyond the map boundary.");
        return 0;
    }

    if(player->character.actor.stamina <= 0)
    {
        snprintf(status, 192, "You are too exhausted to continue slow travel.");
        return 0;
    }

    player->character.actor.stamina--;
    *x = nx;
    *y = ny;

    world_map_mark_discovered(nx, ny);
    world_map_mark_visited(nx, ny);

    tile = world_map_get_tile(nx, ny);
    if(tile && tile->zone_index >= 0)
    {
        atlas_upgrade_knowledge(tile->zone_index, LOCATION_KNOWLEDGE_LOCATED);
        atlas_add_location_hint(tile->zone_index, "Reached by overworld slow travel.");
        snprintf(status, 192, "You reached %s. Press Enter to enter the location.", atlas[tile->zone_index].name);
        return 1;
    }

    snprintf(status, 192, "Slow travel step complete. Stamina now %d.", player->character.actor.stamina);
    return 1;
}

int world_map_show_overlay(Player* player)
{
    int cursor_x;
    int cursor_y;
    char status[192] = "Slow travel mode: move with WASD/Arrows, Enter to enter location, o/q to close.";

    if(!player)
        return 0;

    pending_area_index = -1;
    world_map_start_position(&cursor_x, &cursor_y);
    world_map_set_overworld_position(cursor_x, cursor_y);
    world_map_mark_discovered(cursor_x, cursor_y);
    world_map_mark_visited(cursor_x, cursor_y);

    while(1)
    {
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        int line_i = 0;
        int known_count = 0;

        draw_world_map_viewport(cursor_x, cursor_y);

        ui_overlay_draw_frame("World Map - Slow Travel Exploration");
        ui_overlay_draw_line(line_i++, "Move: WASD/Arrows | Enter: enter zone | 1-9: quick center on visited zones | o/q: close");
        ui_overlay_draw_line(line_i++, "");

        {
            WorldMapTile* here = world_map_get_tile(cursor_x, cursor_y);
            char row[192];
            snprintf(row, sizeof(row), "Position: (%d,%d)  Stamina: %d/%d", cursor_x, cursor_y, player->character.actor.stamina, player->character.actor.max_stamina);
            ui_overlay_draw_line(line_i++, row);

            if(here && here->zone_index >= 0)
                snprintf(row, sizeof(row), "Tile: Zone %s  [%s]", atlas[here->zone_index].name, atlas_is_visited(here->zone_index) ? "visited" : "unvisited");
            else
                snprintf(row, sizeof(row), "Tile: Wilderness");
            ui_overlay_draw_line(line_i++, row);
        }

        ui_overlay_draw_line(line_i++, "");
        ui_overlay_draw_line(line_i++, "Visited zone shortcuts:");

        for(int i = 0; i < MAX_AREAS && line_i < status_line; i++)
        {
            int zx;
            int zy;
            char row[192];

            if(!atlas_is_visited(i))
                continue;
            if(!world_map_find_zone(i, &zx, &zy))
                continue;

            known_count++;
            if(known_count <= 9)
            {
                snprintf(row, sizeof(row), "%d) %s at (%d,%d)", known_count, atlas[i].name, zx, zy);
                ui_overlay_draw_line(line_i++, row);
            }
        }

        if(known_count == 0 && line_i < status_line)
            ui_overlay_draw_line(line_i++, "No visited zones recorded.");

        for(; line_i < status_line; line_i++)
            ui_overlay_draw_line(line_i, "");

        ui_overlay_draw_line(status_line, status);
        ui_overlay_draw_global_hotkeys();

        {
            int key = read_input_key();
            int moved = 0;

            if(key == 'o' || key == 'O' || key == 'q' || key == 'Q' || key == 27)
                break;

            if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
                moved = world_map_try_step(player, &cursor_x, &cursor_y, 0, -1, status);
            else if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
                moved = world_map_try_step(player, &cursor_x, &cursor_y, 0, 1, status);
            else if(key == 'a' || key == 'A' || key == INPUT_KEY_LEFT)
                moved = world_map_try_step(player, &cursor_x, &cursor_y, -1, 0, status);
            else if(key == 'd' || key == 'D' || key == INPUT_KEY_RIGHT)
                moved = world_map_try_step(player, &cursor_x, &cursor_y, 1, 0, status);
            else if(key >= '1' && key <= '9')
            {
                int choice = key - '0';
                int slot = 0;
                int found = 0;

                for(int i = 0; i < MAX_AREAS; i++)
                {
                    int zx;
                    int zy;

                    if(!atlas_is_visited(i))
                        continue;
                    if(!world_map_find_zone(i, &zx, &zy))
                        continue;

                    slot++;
                    if(slot == choice)
                    {
                        cursor_x = zx;
                        cursor_y = zy;
                        world_map_set_overworld_position(cursor_x, cursor_y);
                        snprintf(status, sizeof(status), "Centered on %s.", atlas[i].name);
                        found = 1;
                        break;
                    }
                }

                if(!found)
                    snprintf(status, sizeof(status), "Shortcut %d is not available.", choice);
            }
            else if(key == 13)
            {
                WorldMapTile* tile = world_map_get_tile(cursor_x, cursor_y);
                if(tile && tile->zone_index >= 0)
                {
                    pending_area_index = tile->zone_index;
                    draw_force_full_redraw();
                    return 1;
                }
                snprintf(status, sizeof(status), "There is no location entrance on this tile.");
            }
            else
            {
                OverlayType next;
                if(overlay_type_from_key(key, &next) && next != OVERLAY_TYPE_ATLAS)
                {
                    overlay_request(next);
                    break;
                }
            }

            if(moved)
            {
                world_map_set_overworld_position(cursor_x, cursor_y);
                continue;
            }
        }
    }

    draw_force_full_redraw();
    return 0;
}

int world_map_overlay_take_selected_area(void)
{
    int selected = pending_area_index;
    pending_area_index = -1;
    return selected;
}
