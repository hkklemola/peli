#include "world_map_overlay.h"

#include <stdio.h>

#include "atlas.h"
#include "draw.h"
#include "input.h"
#include "keybind_helpers.h"
#include "overlay_nav.h"
#include "ui_overlay.h"
#include "world_map.h"

static int pending_area_index = -1;

static int world_map_clamp_coordinate(int value, int max_value)
{
    if(value < 0)
        return 0;
    if(value >= max_value)
        return max_value - 1;
    return value;
}

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
    int base_cost;
    int exhaustion_cost;
    int total_cost;
    int pushed = 0;
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

    base_cost = world_map_step_stamina_cost(nx, ny);
    exhaustion_cost = player_exhaustion_surcharge(player);
    total_cost = base_cost + exhaustion_cost;

    if(player->character.actor.stamina < base_cost)
    {
        snprintf(status, 192, "You need at least %d stamina for that step.", base_cost);
        return 0;
    }

    if(player->character.actor.stamina < total_cost)
    {
        if(exhaustion_cost > 0 && player_try_push_through_exhaustion(player))
        {
            pushed = 1;
            total_cost = base_cost;
        }
        else
        {
            snprintf(status,
                     192,
                     "Need %d stamina (%d base + %d exhaustion). Spend willpower to push through.",
                     total_cost,
                     base_cost,
                     exhaustion_cost);
            return 0;
        }
    }

    player->character.actor.stamina -= total_cost;
    *x = nx;
    *y = ny;
    player_add_exhaustion(player, 1);

    world_map_mark_discovered(nx, ny);
    world_map_mark_visited(nx, ny);

    tile = world_map_get_tile(nx, ny);
    if(tile && tile->zone_index >= 0)
    {
        atlas_upgrade_knowledge(tile->zone_index, LOCATION_KNOWLEDGE_SCOUTED);
        atlas_add_location_hint(tile->zone_index, "Reached by overworld slow travel.");
        snprintf(status,
                 192,
                 "You reached %s (cost %d + exh %d, now %d). Press Enter to enter the location.",
                 atlas[tile->zone_index].name,
                 base_cost,
                 pushed ? 0 : exhaustion_cost,
                 player->exhaustion);
        return 1;
    }

    if(pushed)
        snprintf(status,
                 192,
                 "You push through exhaustion (spent 1 willpower). Cost %d, stamina %d, exhaustion %d.",
                 total_cost,
                 player->character.actor.stamina,
                 player->exhaustion);
    else
        snprintf(status,
                 192,
                 "Slow travel step complete (base %d + exhaustion %d). Stamina %d, exhaustion %d.",
                 base_cost,
                 exhaustion_cost,
                 player->character.actor.stamina,
                 player->exhaustion);
    return 1;
}

static int world_map_show_overlay_internal(Player* player,
                                         int has_focus_override,
                                         int focus_x,
                                         int focus_y,
                                         const char* focus_label)
{
    int cursor_x;
    int cursor_y;
    int camera_x;
    int camera_y;
    int scout_x;
    int scout_y;
    int scout_mode;
    int vision_range;
    int focus_active = 0;
    char status[192];

    if(!player)
        return 0;

    snprintf(status,
             sizeof(status),
             "World map view: T scout nearby tiles | O/Q close. Travel between zones happens by walking to a zone edge.");

    pending_area_index = -1;
    world_map_start_position(&cursor_x, &cursor_y);
    camera_x = cursor_x;
    camera_y = cursor_y;

    if(has_focus_override)
    {
        camera_x = world_map_clamp_coordinate(focus_x, WORLD_MAP_WIDTH);
        camera_y = world_map_clamp_coordinate(focus_y, WORLD_MAP_HEIGHT);
        focus_active = 1;

        if(focus_label && focus_label[0])
            snprintf(status,
                     sizeof(status),
                     "Centered on %s at (%d,%d). T scouts from your current position | O/Q close.",
                     focus_label,
                     camera_x,
                     camera_y);
        else
            snprintf(status,
                     sizeof(status),
                     "Centered on (%d,%d). T scouts from your current position | O/Q close.",
                     camera_x,
                     camera_y);
    }

    world_map_set_overworld_position(cursor_x, cursor_y);
    world_map_mark_discovered(cursor_x, cursor_y);
    world_map_mark_visited(cursor_x, cursor_y);
    scout_mode = 0;
    scout_x = cursor_x;
    scout_y = cursor_y;

    while(1)
    {
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        int line_i = 0;
        int known_count = 0;
        int display_x;
        int display_y;

        vision_range = actor_overworld_vision_range(&player->character.actor);
        display_x = scout_mode ? scout_x : camera_x;
        display_y = scout_mode ? scout_y : camera_y;

        if(scout_mode)
            draw_world_map_viewport(scout_x, scout_y, player, cursor_x, cursor_y, vision_range, 1, scout_x, scout_y);
        else
            draw_world_map_viewport(camera_x, camera_y, player, cursor_x, cursor_y, vision_range, 0, 0, 0);

        if(scout_mode)
            ui_overlay_draw_frame("World Map - Scout Mode");
        else
            ui_overlay_draw_frame("World Map");

        if(scout_mode)
            ui_overlay_draw_line(line_i++, "Scout mode: move target WASD/Arrows | Enter scout | q/Esc exit scout | o close");
        else if(focus_active)
            ui_overlay_draw_line(line_i++, "Focused view from atlas | T scout nearby tiles | O/Q close");
        else
            ui_overlay_draw_line(line_i++, "View only: T scout | Enter/WASD remind travel rule | O/Q close");
        ui_overlay_draw_line(line_i++, "");

        {
            WorldMapTile* here = world_map_get_tile(display_x, display_y);
            char row[192];
            int move_cost = world_map_step_stamina_cost(display_x, display_y);
            int exhaustion_cost = player_exhaustion_surcharge(player);

            if(scout_mode)
                snprintf(row, sizeof(row), "Scout target: (%d,%d)  Travel pos: (%d,%d)  Vision: %d", scout_x, scout_y, cursor_x, cursor_y, vision_range);
            else if(focus_active)
                snprintf(row,
                         sizeof(row),
                         "Viewing: (%d,%d)  Travel pos: (%d,%d)  Vision: %d",
                         camera_x,
                         camera_y,
                         cursor_x,
                         cursor_y,
                         vision_range);
            else
                snprintf(row,
                         sizeof(row),
                         "Position: (%d,%d)  Vision: %d  Stamina: %d/%d  Willpower: %d/%d",
                         cursor_x,
                         cursor_y,
                         vision_range,
                         player->character.actor.stamina,
                         player->character.actor.max_stamina,
                         player->character.actor.willpower,
                         player->character.actor.max_willpower);
            ui_overlay_draw_line(line_i++, row);

            if(here && here->zone_index >= 0)
            {
                const char* zone_state = "unknown";
                if(atlas_is_visited(here->zone_index))
                    zone_state = "visited";
                else if(atlas_is_scouted(here->zone_index))
                    zone_state = "scouted";
                else if(atlas_is_located(here->zone_index))
                    zone_state = "located";
                else if(atlas_is_known(here->zone_index))
                    zone_state = "aware";

                snprintf(row, sizeof(row), "Tile: Zone %s  [%s]", atlas[here->zone_index].name, zone_state);
            }
            else
                snprintf(row, sizeof(row), "Tile: Wilderness (%s)", world_map_biome_name(here ? here->biome : BIOME_NONE));
            ui_overlay_draw_line(line_i++, row);

            snprintf(row,
                     sizeof(row),
                     "Road tier: %d  Move cost: %d + exhaustion %d = %d  Exhaustion: %d",
                     here ? here->road_tier : WORLD_MAP_ROAD_TIER_NONE,
                     move_cost,
                     exhaustion_cost,
                     move_cost + exhaustion_cost,
                     player->exhaustion);
            ui_overlay_draw_line(line_i++, row);
        }

        ui_overlay_draw_line(line_i++, "");
        ui_overlay_draw_line(line_i++, "Known visited zones:");

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
                snprintf(row, sizeof(row), "%d) %.31s at (%d,%d)", known_count, atlas[i].name, zx, zy);
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

            if(KEYBIND_OVERLAY_CLOSE(key))
                break;

            if(scout_mode)
            {
                if(KEYBIND_CANCEL(key))
                {
                    scout_mode = 0;
                    snprintf(status, sizeof(status), "Exited scout mode.");
                    continue;
                }

                if(KEYBIND_UP(key))
                {
                    int ny = scout_y - 1;
                    if(ny >= 0 && ny < WORLD_MAP_HEIGHT &&
                       draw_world_map_tile_in_vision(scout_x, ny, cursor_x, cursor_y, vision_range))
                        scout_y = ny;
                    else
                        snprintf(status, sizeof(status), "Target is outside scout range.");
                }
                else if(KEYBIND_DOWN(key))
                {
                    int ny = scout_y + 1;
                    if(ny >= 0 && ny < WORLD_MAP_HEIGHT &&
                       draw_world_map_tile_in_vision(scout_x, ny, cursor_x, cursor_y, vision_range))
                        scout_y = ny;
                    else
                        snprintf(status, sizeof(status), "Target is outside scout range.");
                }
                else if(KEYBIND_LEFT(key))
                {
                    int nx = scout_x - 1;
                    if(nx >= 0 && nx < WORLD_MAP_WIDTH &&
                       draw_world_map_tile_in_vision(nx, scout_y, cursor_x, cursor_y, vision_range))
                        scout_x = nx;
                    else
                        snprintf(status, sizeof(status), "Target is outside scout range.");
                }
                else if(KEYBIND_RIGHT(key))
                {
                    int nx = scout_x + 1;
                    if(nx >= 0 && nx < WORLD_MAP_WIDTH &&
                       draw_world_map_tile_in_vision(nx, scout_y, cursor_x, cursor_y, vision_range))
                        scout_x = nx;
                    else
                        snprintf(status, sizeof(status), "Target is outside scout range.");
                }
                else if(KEYBIND_CONFIRM(key))
                {
                    WorldMapTile* tile = world_map_get_tile(scout_x, scout_y);
                    if(!tile)
                    {
                        snprintf(status, sizeof(status), "No tile here to scout.");
                    }
                    else if(!draw_world_map_tile_in_vision(scout_x, scout_y, cursor_x, cursor_y, vision_range))
                    {
                        snprintf(status, sizeof(status), "Target is out of scouting range.");
                    }
                    else
                    {
                        world_map_mark_scouted(scout_x, scout_y);
                        if(tile->zone_index >= 0)
                        {
                            atlas_upgrade_knowledge(tile->zone_index, LOCATION_KNOWLEDGE_SCOUTED);
                            atlas_add_location_hint(tile->zone_index, "Scouted from overland.");
                            snprintf(status, sizeof(status), "Scouted location %s.", atlas[tile->zone_index].name);
                        }
                        else
                        {
                            snprintf(status, sizeof(status), "Scouted wilderness at (%d,%d).", scout_x, scout_y);
                        }
                    }
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
            }
            else
            {
                if(KEYBIND_CANCEL(key))
                    break;

                if(KEYBIND_MATCH_ALPHA(key, 't', 'T'))
                {
                    scout_mode = 1;
                    scout_x = cursor_x;
                    scout_y = cursor_y;
                    snprintf(status, sizeof(status), "Entered scout mode. Move target and press Enter to scout.");
                }
                else if(KEYBIND_UP(key) || KEYBIND_DOWN(key) || KEYBIND_LEFT(key) || KEYBIND_RIGHT(key) || KEYBIND_CONFIRM(key))
                {
                    snprintf(status,
                             sizeof(status),
                             "Travel occurs in-world by walking to a zone edge. Use T to scout from your current position.");
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
            }

            if(moved)
            {
                world_map_set_overworld_position(cursor_x, cursor_y);
                continue;
            }
        }
    }

    draw_invalidate_viewport_contents();
    ui_overlay_invalidate_cache();
    return 0;
}

int world_map_show_overlay(Player* player)
{
    return world_map_show_overlay_internal(player, 0, 0, 0, NULL);
}

int world_map_show_overlay_centered(Player* player, int focus_x, int focus_y, const char* focus_label)
{
    return world_map_show_overlay_internal(player, 1, focus_x, focus_y, focus_label);
}

int world_map_overlay_take_selected_area(void)
{
    int selected = pending_area_index;
    pending_area_index = -1;
    return selected;
}
