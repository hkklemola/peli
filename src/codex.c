#include "codex.h"
#include "atlas.h"
#include "atlas_overlay.h"
#include "bestiary.h"
#include "crafting_compendium.h"
#include "draw.h"
#include "journal.h"
#include "log.h"
#include "overlay_nav.h"
#include "player.h"
#include "ui_overlay.h"
#include "input.h"
#include "keybind_helpers.h"
#include "world_map_overlay.h"

#include <stdio.h>

static void codex_show_world_map(Player* player)
{
    ViewportTab previous_tab;

    if(!player)
        return;

    if(current_area && (current_area->type == LOCATION_CRYPT || current_area->type == LOCATION_CAVERN || current_area->type == LOCATION_DUNGEON))
    {
        log_add("Overworld map unavailable in underground areas.");
        ui_overlay_show_mini_prompt("Travel Unavailable",
                                    "You are underground.",
                                    "Reach the surface to use the overland map or atlas fast travel.");
        return;
    }

    previous_tab = draw_get_viewport_tab();
    draw_set_viewport_tab(VIEWPORT_TAB_WORLD);
    (void)world_map_show_overlay(player);
    draw_set_viewport_tab(previous_tab);
}

void codex_show_overlay(Player* player)
{
    if(!player)
        return;

    char status[160] = "1 map | 2 atlas | 3 journal | 4 log | 5 bestiary | 6 crafting | o close";

    while(1)
    {
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;

        ui_overlay_draw_frame("Codex");
        ui_overlay_draw_line(0, "Codex: 1 World Map | 2 Atlas | 3 Journal");
        ui_overlay_draw_line(1, "       4 Log | 5 Bestiary | 6 Crafting | o close");
        ui_overlay_draw_line(2, "");
        ui_overlay_draw_line(3, "Use 1-6 or m/a/j/l/b/k to open a section.");
        ui_overlay_draw_line(4, "");
        ui_overlay_draw_line(5, "World Map passively scouts nearby overland terrain.");
        ui_overlay_draw_line(6, "Atlas tracks discovered locations and travel.");
        ui_overlay_draw_line(7, "Journal and Log archive notes and messages.");
        ui_overlay_draw_line(8, "Bestiary and Crafting hold reference knowledge.");

        for(int i = 9; i < status_line; i++)
            ui_overlay_draw_line(i, "");

        ui_overlay_draw_line(status_line, status);
        ui_overlay_draw_global_hotkeys();

        int key = read_input_key();
        OverlayType requested = OVERLAY_TYPE_NONE;

        if(KEYBIND_OVERLAY_EXIT(key))
            break;

        if(overlay_type_from_key(key, &requested) && requested != OVERLAY_TYPE_CODEX)
        {
            overlay_request(requested);
            break;
        }

        if(key == '1' || key == 'm' || key == 'M')
        {
            codex_show_world_map(player);
        }
        else if(key == '2' || key == 'a' || key == 'A')
        {
            atlas_show_overlay(player);
        }
        else if(key == '3')
        {
            journal_show_overlay(player);
        }
        else if(key == '4')
        {
            log_show_overlay();
        }
        else if(key == '5' || key == 'b' || key == 'B')
        {
            bestiary_show_overlay(player);
        }
        else if(key == '6' || key == 'k' || key == 'K')
        {
            crafting_compendium_show_overlay(player);
        }

        requested = overlay_take_request();
        if(requested != OVERLAY_TYPE_NONE)
        {
            overlay_request(requested);
            break;
        }
    }
}
