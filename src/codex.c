#include "codex.h"
#include "atlas_overlay.h"
#include "bestiary.h"
#include "crafting_compendium.h"
#include "overlay_nav.h"
#include "player.h"
#include "ui_overlay.h"
#include "input.h"
#include "keybind_helpers.h"

#include <stdio.h>

void codex_show_overlay(Player* player)
{
    if(!player)
        return;

    char status[128] = "1 atlas | 2 bestiary | 3 crafting | o close | i/c/j switch overlays";

    while(1)
    {
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;

        ui_overlay_draw_frame("Codex");
        ui_overlay_draw_line(0, "Codex: 1 Atlas | 2 Bestiary | 3 Crafting | o close");
        ui_overlay_draw_line(1, "");
        ui_overlay_draw_line(2, "Use keys 1-3 to open a Codex section.");
        ui_overlay_draw_line(3, "");
        ui_overlay_draw_line(4, "Atlas contains discovered locations.");
        ui_overlay_draw_line(5, "Bestiary contains races and creatures.");
        ui_overlay_draw_line(6, "Crafting Compendium contains known recipes.");

        for(int i = 7; i < status_line; i++)
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

        if(key == '1')
        {
            atlas_show_overlay(player);
        }
        else if(key == '2')
        {
            bestiary_show_overlay(player);
        }
        else if(key == '3')
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
