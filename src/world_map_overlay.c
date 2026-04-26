#include "world_map_overlay.h"

#include <stdio.h>

#include "atlas.h"
#include "draw.h"
#include "input.h"
#include "keybind_helpers.h"
#include "log.h"
#include "overlay_nav.h"
#include "ui_overlay.h"
#include "world_map.h"

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

static void world_map_format_inspect_result(Player* player,
                                            int origin_x,
                                            int origin_y,
                                            int tx,
                                            int ty,
                                            char* out,
                                            size_t out_size)
{
    if(!player || !out || out_size == 0)
        return;

    WorldMapTile* tile = world_map_get_tile(tx, ty);
    if(!tile)
    {
        snprintf(out, out_size, "Invalid world map coordinates %d,%d.", tx, ty);
        return;
    }

    int vision_range = actor_overworld_vision_range(&player->character.actor);
    int visible = draw_world_map_tile_in_vision(tx, ty, origin_x, origin_y, vision_range);
    if(!tile->discovered && !visible)
    {
        snprintf(out,
                 out_size,
                 "The terrain at (%d,%d) is unknown from your current position.",
                 tx,
                 ty);
        return;
    }

    char feature_desc[128] = "";
    int feature_pos = 0;
    if(tile->road_tier > WORLD_MAP_ROAD_TIER_NONE)
        feature_pos += snprintf(feature_desc + feature_pos,
                                sizeof(feature_desc) - feature_pos,
                                "%sroad tier %d",
                                feature_pos ? ", " : "",
                                tile->road_tier);
    if(tile->river_tier > WORLD_MAP_RIVER_NONE)
        feature_pos += snprintf(feature_desc + feature_pos,
                                sizeof(feature_desc) - feature_pos,
                                "%sriver tier %d",
                                feature_pos ? ", " : "",
                                tile->river_tier);
    if(tile->lake_tier > WORLD_MAP_LAKE_NONE)
        feature_pos += snprintf(feature_desc + feature_pos,
                                sizeof(feature_desc) - feature_pos,
                                "%slake tier %d",
                                feature_pos ? ", " : "",
                                tile->lake_tier);
    if(tile->farmland)
        feature_pos += snprintf(feature_desc + feature_pos,
                                sizeof(feature_desc) - feature_pos,
                                "%sfarmland",
                                feature_pos ? ", " : "");
    if(feature_pos == 0)
        snprintf(feature_desc, sizeof(feature_desc), "no major features");

    int move_cost = world_map_step_stamina_cost(tx, ty);
    int exhaustion_cost = player_exhaustion_surcharge(player);
    int total_cost = move_cost + exhaustion_cost;

    if(tile->zone_index >= 0 && tile->zone_index < MAX_AREAS)
    {
        const char* zone_state = "unknown";
        if(atlas_is_visited(tile->zone_index))
            zone_state = "visited";
        else if(atlas_is_scouted(tile->zone_index))
            zone_state = "scouted";
        else if(atlas_is_located(tile->zone_index))
            zone_state = "located";
        else if(atlas_is_known(tile->zone_index))
            zone_state = "aware";

        snprintf(out,
                 out_size,
                 "You inspect (%d,%d): Zone %s [%s], %s. Move cost %d + exhaustion %d = %d.",
                 tx,
                 ty,
                 atlas[tile->zone_index].name,
                 zone_state,
                 feature_desc,
                 move_cost,
                 exhaustion_cost,
                 total_cost);
    }
    else
    {
        snprintf(out,
                 out_size,
                 "You inspect (%d,%d): Wilderness (%s), %s. Move cost %d + exhaustion %d = %d.",
                 tx,
                 ty,
                 world_map_biome_name(tile->biome),
                 tile->discovered ? "discovered" : "visible",
                 move_cost,
                 exhaustion_cost,
                 total_cost);
    }
}

static void world_map_auto_scout_visible_tiles(int origin_x,
                                               int origin_y,
                                               int vision_range,
                                               int* out_new_tiles,
                                               int* out_new_zones)
{
    int min_x;
    int max_x;
    int min_y;
    int max_y;

    if(out_new_tiles)
        *out_new_tiles = 0;
    if(out_new_zones)
        *out_new_zones = 0;
    if(vision_range < 0)
        return;

    min_x = world_map_clamp_coordinate(origin_x - vision_range, WORLD_MAP_WIDTH);
    max_x = world_map_clamp_coordinate(origin_x + vision_range, WORLD_MAP_WIDTH);
    min_y = world_map_clamp_coordinate(origin_y - vision_range, WORLD_MAP_HEIGHT);
    max_y = world_map_clamp_coordinate(origin_y + vision_range, WORLD_MAP_HEIGHT);

    for(int y = min_y; y <= max_y; y++)
    {
        for(int x = min_x; x <= max_x; x++)
        {
            int was_discovered;
            int zone_was_scouted;
            WorldMapTile* tile;

            if(!draw_world_map_tile_in_vision(x, y, origin_x, origin_y, vision_range))
                continue;

            tile = world_map_get_tile(x, y);
            if(!tile)
                continue;

            was_discovered = tile->discovered;
            zone_was_scouted = (tile->zone_index >= 0 && tile->zone_index < MAX_AREAS)
                                   ? atlas_is_scouted(tile->zone_index)
                                   : 0;

            world_map_mark_scouted(x, y);

            if(!was_discovered && out_new_tiles)
                (*out_new_tiles)++;

            if(tile->zone_index >= 0 && tile->zone_index < MAX_AREAS)
            {
                atlas_upgrade_knowledge(tile->zone_index, LOCATION_KNOWLEDGE_SCOUTED);
                if(!zone_was_scouted)
                {
                    atlas_add_location_hint(tile->zone_index, "Scouted from overland.");
                    if(out_new_zones)
                        (*out_new_zones)++;
                }
            }
        }
    }
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
    int inspect_x;
    int inspect_y;
    int inspect_active = 0;
    int vision_range;
    int newly_scouted_tiles = 0;
    int newly_scouted_zones = 0;
    int focus_active = 0;
    char status[192];

    if(!player)
        return 0;

    snprintf(status,
             sizeof(status),
             "World map view: nearby tiles are surveyed automatically | O/Q close.");
    world_map_start_position(&cursor_x, &cursor_y);
    camera_x = cursor_x;
    camera_y = cursor_y;
    inspect_x = cursor_x;
    inspect_y = cursor_y;

    if(has_focus_override)
    {
        camera_x = world_map_clamp_coordinate(focus_x, WORLD_MAP_WIDTH);
        camera_y = world_map_clamp_coordinate(focus_y, WORLD_MAP_HEIGHT);
        focus_active = 1;

        if(focus_label && focus_label[0])
            snprintf(status,
                     sizeof(status),
                     "Centered on %s at (%d,%d). Nearby tiles are surveyed from your current position | O/Q close.",
                     focus_label,
                     camera_x,
                     camera_y);
        else
            snprintf(status,
                     sizeof(status),
                     "Centered on (%d,%d). Nearby tiles are surveyed from your current position | O/Q close.",
                     camera_x,
                     camera_y);
    }

    world_map_set_overworld_position(cursor_x, cursor_y);
    world_map_mark_discovered(cursor_x, cursor_y);
    world_map_mark_visited(cursor_x, cursor_y);
    vision_range = actor_overworld_vision_range(&player->character.actor);
    world_map_auto_scout_visible_tiles(cursor_x,
                                       cursor_y,
                                       vision_range,
                                       &newly_scouted_tiles,
                                       &newly_scouted_zones);

    if(focus_active)
    {
        if(focus_label && focus_label[0])
            snprintf(status,
                     sizeof(status),
                     "Centered on %s. Surveyed %d nearby tile%s and %d zone%s from your position.",
                     focus_label,
                     newly_scouted_tiles,
                     newly_scouted_tiles == 1 ? "" : "s",
                     newly_scouted_zones,
                     newly_scouted_zones == 1 ? "" : "s");
        else
            snprintf(status,
                     sizeof(status),
                     "Centered view. Surveyed %d nearby tile%s and %d zone%s from your position.",
                     newly_scouted_tiles,
                     newly_scouted_tiles == 1 ? "" : "s",
                     newly_scouted_zones,
                     newly_scouted_zones == 1 ? "" : "s");
    }
    else
    {
        snprintf(status,
                 sizeof(status),
                 "Surveyed %d nearby tile%s and %d zone%s from your position. Travel still happens via zone edges or atlas fast travel.",
                 newly_scouted_tiles,
                 newly_scouted_tiles == 1 ? "" : "s",
                 newly_scouted_zones,
                 newly_scouted_zones == 1 ? "" : "s");
    }

    while(1)
    {
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        int line_i = 0;
        int known_count = 0;
        int display_x = camera_x;
        int display_y = camera_y;

        vision_range = actor_overworld_vision_range(&player->character.actor);
        draw_world_map_viewport(camera_x,
                                camera_y,
                                player,
                                cursor_x,
                                cursor_y,
                                inspect_active ? inspect_x : -1,
                                inspect_active ? inspect_y : -1,
                                vision_range);

        if(inspect_active)
        {
            display_x = inspect_x;
            display_y = inspect_y;
        }

        ui_overlay_draw_frame("World Map");

        if(focus_active)
            ui_overlay_draw_line(line_i++, "Focused view from atlas | Informational only | O/Q close");
        else
            ui_overlay_draw_line(line_i++, "View only: nearby tiles auto-survey on open | O/Q close");
        if(inspect_active)
            ui_overlay_draw_line(line_i++, "Inspect mode: arrows move cursor | Enter inspect | T toggle | Q cancel");
        else
            ui_overlay_draw_line(line_i++, "Press T to inspect tiles, O/Q close");
        ui_overlay_draw_line(line_i++, "");

        {
            WorldMapTile* here = world_map_get_tile(display_x, display_y);
            char row[192];
            int move_cost = world_map_step_stamina_cost(display_x, display_y);
            int exhaustion_cost = player_exhaustion_surcharge(player);

            if(focus_active)
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

            if(KEYBIND_OVERLAY_CLOSE(key))
                break;

            if(KEYBIND_CANCEL(key))
            {
                if(inspect_active)
                {
                    inspect_active = 0;
                    snprintf(status,
                             sizeof(status),
                             "Inspect mode canceled. Nearby tiles remain surveyed automatically.");
                    continue;
                }
                break;
            }
            else if(KEYBIND_MATCH_ALPHA(key, 't', 'T'))
            {
                inspect_active = !inspect_active;
                if(inspect_active)
                {
                    if(inspect_x < 0 || inspect_x >= WORLD_MAP_WIDTH || inspect_y < 0 || inspect_y >= WORLD_MAP_HEIGHT)
                    {
                        inspect_x = cursor_x;
                        inspect_y = cursor_y;
                    }
                    snprintf(status,
                             sizeof(status),
                             "Inspect mode: move cursor with arrows, Enter inspect, T exit.");
                }
                else
                {
                    snprintf(status,
                             sizeof(status),
                             "Inspect mode exited. Nearby tiles are surveyed automatically.");
                }
                continue;
            }
            else if(inspect_active && (KEYBIND_UP_LEFT(key) || KEYBIND_UP_RIGHT(key) || KEYBIND_DOWN_LEFT(key) || KEYBIND_DOWN_RIGHT(key) || KEYBIND_UP(key) || KEYBIND_DOWN(key) || KEYBIND_LEFT(key) || KEYBIND_RIGHT(key)))
            {
                if(KEYBIND_UP_LEFT(key))
                {
                    inspect_x--;
                    inspect_y--;
                }
                else if(KEYBIND_UP_RIGHT(key))
                {
                    inspect_x++;
                    inspect_y--;
                }
                else if(KEYBIND_DOWN_LEFT(key))
                {
                    inspect_x--;
                    inspect_y++;
                }
                else if(KEYBIND_DOWN_RIGHT(key))
                {
                    inspect_x++;
                    inspect_y++;
                }
                else if(KEYBIND_UP(key))
                    inspect_y--;
                else if(KEYBIND_DOWN(key))
                    inspect_y++;
                else if(KEYBIND_LEFT(key))
                    inspect_x--;
                else if(KEYBIND_RIGHT(key))
                    inspect_x++;

                inspect_x = world_map_clamp_coordinate(inspect_x, WORLD_MAP_WIDTH);
                inspect_y = world_map_clamp_coordinate(inspect_y, WORLD_MAP_HEIGHT);
                snprintf(status,
                         sizeof(status),
                         "Inspect cursor moved to (%d,%d). Press Enter to inspect.",
                         inspect_x,
                         inspect_y);
                continue;
            }
            else if(inspect_active && KEYBIND_CONFIRM(key))
            {
                char result_text[192] = "";
                world_map_format_inspect_result(player,
                                               cursor_x,
                                               cursor_y,
                                               inspect_x,
                                               inspect_y,
                                               result_text,
                                               sizeof(result_text));
                log_add("%s", result_text);
                snprintf(status, sizeof(status), "%s", result_text);
                continue;
            }
            else if(KEYBIND_UP(key) || KEYBIND_DOWN(key) || KEYBIND_LEFT(key) || KEYBIND_RIGHT(key) || KEYBIND_CONFIRM(key))
            {
                snprintf(status,
                         sizeof(status),
                         "Travel occurs in-world by walking to a zone edge, or by atlas fast travel to visited locations.");
            }
            else
            {
                OverlayType next;
                if(overlay_type_from_key(key, &next) && next != OVERLAY_TYPE_CODEX)
                {
                    overlay_request(next);
                    break;
                }
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
