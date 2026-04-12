#include "atlas_overlay.h"
#include "atlas.h"
#include "overlay_nav.h"
#include "input.h"
#include "keybind_helpers.h"
#include "ui_overlay.h"
#include "map.h"
#include "bestiary.h"
#include "world_map_overlay.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * @file atlas_overlay.c
 * @brief Implementation of the world atlas overlay (known locations map).
 *
 * Renders the atlas screen showing known zones/areas, allowing navigation
 * between discovered locations and integration with other overlay menus.
 */

static int pending_travel_index = -1;

typedef enum AtlasPageMode {
    ATLAS_PAGE_INDEX = 0,
    ATLAS_PAGE_LOCATION,
} AtlasPageMode;

static const char* atlas_knowledge_label(LocationKnowledge knowledge)
{
    switch(knowledge)
    {
        case LOCATION_KNOWLEDGE_AWARE:
            return "aware";
        case LOCATION_KNOWLEDGE_LOCATED:
            return "located";
        case LOCATION_KNOWLEDGE_SCOUTED:
            return "scouted";
        case LOCATION_KNOWLEDGE_VISITED:
            return "visited";
        case LOCATION_KNOWLEDGE_UNAWARE:
        default:
            return "unaware";
    }
}

static int atlas_has_hostile_within_radius(const Player* player, int radius)
{
    int px;
    int py;
    int radius_sq;

    if(!player || radius < 0)
        return 0;

    px = player->character.actor.entity.x;
    py = player->character.actor.entity.y;
    radius_sq = radius * radius;

    for(int i = 0; i < MAX_CREATURES; i++)
    {
        const Creature* creature = &creatures[i];
        int dx;
        int dy;

        if(!creature->alive || !creature->template || !creature_is_hostile(creature))
            continue;

        dx = creature->actor.entity.x - px;
        dy = creature->actor.entity.y - py;
        if((dx * dx) + (dy * dy) <= radius_sq)
            return 1;
    }

    return 0;
}

static const char* atlas_ts_or_unknown(const char* ts)
{
    return (ts && ts[0]) ? ts : "unknown";
}

int atlas_show_overlay_mode(Player* player, AtlasOverlayMode mode)
{
    AtlasOverlayMode current_mode = mode;
    char status_text[192] = "Use M on a located page to open an informational world map view. o/O returns, i/c/j switch overlays.";
    int fast_travel_only = 0;
    AtlasPageMode page_mode = ATLAS_PAGE_INDEX;
    int location_page_index = -1;

    int overlay_lines = ui_overlay_content_lines();
    int status_line = (overlay_lines > 1) ? (overlay_lines - 2) : 0;
    int line_i;

    pending_travel_index = -1;

    while(1)
    {
        ui_overlay_draw_frame("Atlas - Known Locations");

        if(current_mode == ATLAS_OVERLAY_MODE_TRAVEL_SELECT)
            ui_overlay_draw_line(0, "Travel index: choose location number | o/O cancel | i/c/j switch overlays");
        else if(page_mode == ATLAS_PAGE_INDEX)
            ui_overlay_draw_line(0, "Atlas index: 1-9 open page | t fast travel | o/O close | i/c/j switch overlays");
        else
            ui_overlay_draw_line(0, "Location page: a/d or arrows switch | m view on map | b index | t travel | o/O close");
        ui_overlay_draw_line(1, "");

        line_i = 2;

        if(page_mode == ATLAS_PAGE_INDEX || current_mode == ATLAS_OVERLAY_MODE_TRAVEL_SELECT)
        {
            int display_slot = 0;

            for(int i = 0; i < MAX_AREAS && line_i < status_line; i++)
            {
                char text[128];
                LocationKnowledge knowledge;

                if(!atlas_is_known(i))
                    continue;

                knowledge = atlas_get_knowledge(i);
                snprintf(text,
                         sizeof(text),
                         "%d) %s [%s]",
                         display_slot + 1,
                         atlas[i].name,
                         atlas_knowledge_label(knowledge));
                ui_overlay_draw_line(line_i++, text);
                display_slot++;
            }

            if(display_slot == 0 && line_i < status_line)
                ui_overlay_draw_line(line_i++, "No known locations.");
        }
        else if(location_page_index >= 0 && location_page_index < MAX_AREAS)
        {
            const AtlasLocationInfo* info = atlas_get_location_info(location_page_index);
            LocationKnowledge knowledge = atlas_get_knowledge(location_page_index);
            char text[192];

            snprintf(text,
                     sizeof(text),
                     "Location: %s [%s]",
                     atlas[location_page_index].name,
                     atlas_knowledge_label(atlas_get_knowledge(location_page_index)));
            ui_overlay_draw_line(line_i++, text);
            ui_overlay_draw_line(line_i++, "");

            if(knowledge >= LOCATION_KNOWLEDGE_AWARE)
            {
                snprintf(text, sizeof(text), "First aware:   %s", atlas_ts_or_unknown(info ? info->first_aware_ts : NULL));
                ui_overlay_draw_line(line_i++, text);
            }
            if(knowledge >= LOCATION_KNOWLEDGE_LOCATED)
            {
                snprintf(text, sizeof(text), "First located: %s", atlas_ts_or_unknown(info ? info->first_located_ts : NULL));
                ui_overlay_draw_line(line_i++, text);
                snprintf(text,
                         sizeof(text),
                         "Coordinates:   (%d,%d)  [M view on map]",
                         atlas[location_page_index].world_x,
                         atlas[location_page_index].world_y);
                ui_overlay_draw_line(line_i++, text);
            }
            else
            {
                ui_overlay_draw_line(line_i++, "Coordinates:   unknown");
            }
            if(knowledge >= LOCATION_KNOWLEDGE_SCOUTED)
            {
                snprintf(text, sizeof(text), "First scouted: %s", atlas_ts_or_unknown(info ? info->first_scouted_ts : NULL));
                ui_overlay_draw_line(line_i++, text);
            }
            if(knowledge >= LOCATION_KNOWLEDGE_VISITED)
            {
                snprintf(text, sizeof(text), "First visit:   %s", atlas_ts_or_unknown(info ? info->first_visit_ts : NULL));
                ui_overlay_draw_line(line_i++, text);
                snprintf(text, sizeof(text), "Latest visit:  %s", atlas_ts_or_unknown(info ? info->latest_visit_ts : NULL));
                ui_overlay_draw_line(line_i++, text);
            }

            ui_overlay_draw_line(line_i++, "");
            ui_overlay_draw_line(line_i++, "Hints / Information:");

            if(info && info->hint_count > 0)
            {
                for(int i = 0; i < info->hint_count && line_i < status_line; i++)
                {
                    snprintf(text, sizeof(text), "- %s", info->hints[i]);
                    ui_overlay_draw_line(line_i++, text);
                }
            }
            else if(line_i < status_line)
            {
                ui_overlay_draw_line(line_i++, "- No hints recorded yet.");
            }
        }

        for(; line_i < status_line; line_i++)
            ui_overlay_draw_line(line_i, "");

        ui_overlay_draw_line(status_line, status_text);
        ui_overlay_draw_global_hotkeys();

        {
            int key = read_input_key();
            int known_indices[MAX_AREAS];
            int known_count = 0;

            for(int i = 0; i < MAX_AREAS; i++)
            {
                if(atlas_is_known(i))
                    known_indices[known_count++] = i;
            }

            if(KEYBIND_OVERLAY_EXIT(key))
                break;

            if(current_mode == ATLAS_OVERLAY_MODE_VIEW && KEYBIND_MATCH_ALPHA(key, 't', 'T'))
            {
                if(current_area && (current_area->type == LOCATION_CRYPT || current_area->type == LOCATION_CAVERN || current_area->type == LOCATION_DUNGEON))
                {
                    snprintf(status_text, sizeof(status_text), "Travel is disabled in underground areas.");
                    ui_overlay_show_mini_prompt("Travel Unavailable",
                                                "Quick travel is disabled underground.",
                                                "Reach the surface to use the overland map or atlas fast travel.");
                    continue;
                }

                current_mode = ATLAS_OVERLAY_MODE_TRAVEL_SELECT;
                fast_travel_only = 1;
                snprintf(status_text, sizeof(status_text), "Fast travel: choose a visited destination.");
                continue;
            }

                if(current_mode == ATLAS_OVERLAY_MODE_VIEW && page_mode == ATLAS_PAGE_LOCATION && known_count > 0 &&
                    (KEYBIND_LEFT(key) || KEYBIND_RIGHT(key)))
            {
                int current_known_idx = 0;

                for(int i = 0; i < known_count; i++)
                {
                    if(known_indices[i] == location_page_index)
                    {
                        current_known_idx = i;
                        break;
                    }
                }

                if(KEYBIND_LEFT(key))
                    current_known_idx = (current_known_idx - 1 + known_count) % known_count;
                else
                    current_known_idx = (current_known_idx + 1) % known_count;

                location_page_index = known_indices[current_known_idx];
                snprintf(status_text, sizeof(status_text), "Page opened: %s", atlas[location_page_index].name);
                continue;
            }

            if(current_mode == ATLAS_OVERLAY_MODE_VIEW && page_mode == ATLAS_PAGE_LOCATION && KEYBIND_WORLD_MAP_TOGGLE(key))
            {
                if(location_page_index < 0 || location_page_index >= MAX_AREAS)
                {
                    snprintf(status_text, sizeof(status_text), "That atlas page is not available.");
                    continue;
                }

                if(!atlas_is_located(location_page_index))
                {
                    snprintf(status_text,
                             sizeof(status_text),
                             "%s has not been precisely located yet.",
                             atlas[location_page_index].name);
                    ui_overlay_show_mini_prompt("Coordinates Unknown",
                                                "You know of that place, but not its exact map coordinates yet.",
                                                "Scout it or find a route map first.");
                    continue;
                }

                snprintf(status_text, sizeof(status_text), "Opening a centered world map view for %s.", atlas[location_page_index].name);
                (void)world_map_show_overlay_centered(player,
                                                     atlas[location_page_index].world_x,
                                                     atlas[location_page_index].world_y,
                                                     atlas[location_page_index].name);
                continue;
            }

            if(current_mode == ATLAS_OVERLAY_MODE_VIEW && page_mode == ATLAS_PAGE_LOCATION && (key == 'b' || key == 'B'))
            {
                page_mode = ATLAS_PAGE_INDEX;
                location_page_index = -1;
                snprintf(status_text, sizeof(status_text), "Atlas index.");
                continue;
            }

            if(current_mode == ATLAS_OVERLAY_MODE_TRAVEL_SELECT && key >= '1' && key <= '9')
            {
                int display_index = key - '1';
                int area_index;
                LocationKnowledge knowledge;

                if(display_index < 0 || display_index >= known_count)
                {
                    snprintf(status_text, sizeof(status_text), "That destination is not available.");
                    continue;
                }

                area_index = known_indices[display_index];
                knowledge = atlas_get_knowledge(area_index);

                if(fast_travel_only && !atlas_can_fast_travel(area_index))
                {
                    snprintf(status_text, sizeof(status_text), "Fast travel requires a visited location.");
                    ui_overlay_show_mini_prompt("Travel Unavailable",
                                                "Fast travel requires a visited location.",
                                                "Visit it once before using fast travel.");
                    continue;
                }

                if(!fast_travel_only && !atlas_is_located(area_index))
                {
                    const char* detail = (knowledge == LOCATION_KNOWLEDGE_AWARE)
                                             ? "You know of that place, but not where it is."
                                             : "That destination is not available yet.";
                    snprintf(status_text, sizeof(status_text), "That location is not located yet.");
                    ui_overlay_show_mini_prompt("Travel Unavailable",
                                                detail,
                                                "Find exact directions before traveling.");
                    continue;
                }

                if(current_area == &atlas[area_index])
                {
                    snprintf(status_text, sizeof(status_text), "You are already in %s.", atlas[area_index].name);
                    ui_overlay_show_mini_prompt("Travel Failed",
                                                "You are already at that destination.",
                                                "Choose a different location.");
                    continue;
                }

                if(atlas_has_hostile_within_radius(player, 20))
                {
                    snprintf(status_text,
                             sizeof(status_text),
                             "Travel blocked: hostile enemy within 20 tiles.");
                    ui_overlay_show_mini_prompt("Travel Blocked",
                                                "Hostile enemy within 20 tiles.",
                                                "Create distance before traveling.");
                    continue;
                }

                pending_travel_index = area_index;
                return 1;
            }

            if(current_mode == ATLAS_OVERLAY_MODE_VIEW && page_mode == ATLAS_PAGE_INDEX && key >= '1' && key <= '9')
            {
                int display_index = key - '1';

                if(display_index < 0 || display_index >= known_count)
                {
                    snprintf(status_text, sizeof(status_text), "That location page is not available.");
                    continue;
                }

                location_page_index = known_indices[display_index];
                page_mode = ATLAS_PAGE_LOCATION;
                snprintf(status_text, sizeof(status_text), "Page opened: %s", atlas[location_page_index].name);
                continue;
            }

            {
                OverlayType next;
                if(overlay_type_from_key(key, &next) && next != OVERLAY_TYPE_ATLAS)
                {
                    overlay_request(next);
                    break;
                }
            }

            if(current_mode == ATLAS_OVERLAY_MODE_TRAVEL_SELECT)
            {
                if(fast_travel_only)
                    snprintf(status_text, sizeof(status_text), "Choose a visited destination, or press o/O to cancel.");
                else
                    snprintf(status_text, sizeof(status_text), "Choose a located destination, or press o/O to cancel.");
            }
        }
    }

    return 0;
}

void atlas_show_overlay(Player* player)
{
    (void)atlas_show_overlay_mode(player, ATLAS_OVERLAY_MODE_VIEW);
}

int atlas_overlay_take_selected_travel(void)
{
    int selected = pending_travel_index;
    pending_travel_index = -1;
    return selected;
}
