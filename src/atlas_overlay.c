#include "atlas_overlay.h"
#include "atlas.h"
#include "overlay_nav.h"
#include "input.h"
#include "ui_overlay.h"
#include "map.h"

#include <stdio.h>

/**
 * @file atlas_overlay.c
 * @brief Implementation of the world atlas overlay (discovered locations map).
 *
 * Renders the atlas screen showing discovered zones/areas, allowing navigation
 * between discovered locations and integration with other overlay menus.
 */

void atlas_show_overlay(Player* player)
{
    (void)player;

    int overlay_lines = ui_overlay_content_lines();
    int status_line = (overlay_lines > 1) ? (overlay_lines - 2) : 0;
    int line_i;

    while(1)
    {
        ui_overlay_draw_frame("Atlas - Discovered Locations");

        ui_overlay_draw_line(0, "o/O close | i inventory | c character | m log | j journal");
        ui_overlay_draw_line(1, "");

        line_i = 2;

        for(int i = 0; i < MAX_AREAS && line_i < status_line; i++)
        {
            char text[128];
            if(atlas_is_discovered(i))
            {
                snprintf(text, sizeof(text), "%d) %s", i + 1, atlas[i].name);
            }
            else
            {
                snprintf(text, sizeof(text), "%d) ???", i + 1);
            }
            ui_overlay_draw_line(line_i++, text);
        }

        for(; line_i < status_line; line_i++)
            ui_overlay_draw_line(line_i, "");

        ui_overlay_draw_line(status_line, "Use o/O to return, i/c/m/j to switch overlays.");
        ui_overlay_draw_global_hotkeys();

        int key = read_input_key();

        if(key == 'o' || key == 'O' || key == 'q' || key == 'Q' || key == 27)
            break;

        {
            OverlayType next;
            if(overlay_type_from_key(key, &next) && next != OVERLAY_TYPE_ATLAS)
            {
                overlay_request(next);
                break;
            }
        }
    }
}
