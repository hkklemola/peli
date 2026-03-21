#include "ui_overlay.h"
#include "layout.h"
#include "ui_frame.h"

#include <stdio.h>

/*
 * Purpose:
 *   Implements overlay drawing APIs backed by layout-derived frame geometry.
 *
 * Functions:
 *   - overlay_frame: builds current overlay frame geometry.
 *   - ui_overlay_draw_frame/line: drawing APIs for overlay consumers.
 *   - ui_overlay_* accessors: expose overlay dimensions and bounds.
 */

// Build overlay frame geometry from current default layout.
static UiFrame overlay_frame(void)
{
    LayoutState layout;
    UiFrame frame;

    layout_get_default(&layout);
    frame.row = layout.overlay.row;
    frame.col = layout.overlay.col;
    frame.inner_width = layout.overlay.inner_width;
    frame.height = layout.overlay.height;
    return frame;
}

// Draw overlay frame with title.
void ui_overlay_draw_frame(const char* title)
{
    UiFrame frame = overlay_frame();
    ui_frame_draw(&frame, title);
}

// Draw one content line in overlay body.
void ui_overlay_draw_line(int content_line, const char* text)
{
    UiFrame frame = overlay_frame();
    ui_frame_draw_line(&frame, content_line, text);
}

// Draw shared overlay-switch reminders on the bottom content row.
void ui_overlay_draw_global_hotkeys(void)
{
    char boxed[256];
    int overlay_text_width;
    int inner_text_width;
    static const char* hotkeys_text = "Tabs: i inventory | c character | m log | j journal | o atlas | q close";
    int lines = ui_overlay_content_lines();
    if(lines <= 0)
        return;

    overlay_text_width = ui_overlay_text_width();
    if(overlay_text_width < 5)
    {
        ui_overlay_draw_line(lines - 1, hotkeys_text);
        return;
    }

    inner_text_width = overlay_text_width - 4;
    if(inner_text_width < 1)
        inner_text_width = 1;

    snprintf(boxed, sizeof(boxed), "| %-*.*s |", inner_text_width, inner_text_width, hotkeys_text);
    ui_overlay_draw_line(lines - 1, boxed);
}

// Return overlay top row.
int ui_overlay_start_row(void)
{
    LayoutState layout;
    layout_get_default(&layout);
    return layout.overlay.row;
}

// Return overlay total height.
int ui_overlay_height(void)
{
    LayoutState layout;
    layout_get_default(&layout);
    return layout.overlay.height;
}

// Return overlay inner width used by border dashes.
int ui_overlay_dash_width(void)
{
    UiFrame frame = overlay_frame();
    return frame.inner_width;
}

// Return overlay printable text width.
int ui_overlay_text_width(void)
{
    UiFrame frame = overlay_frame();
    return ui_frame_text_width(&frame);
}

// Return overlay width including borders.
int ui_overlay_total_width(void)
{
    UiFrame frame = overlay_frame();
    return frame.inner_width + 2;
}

// Return number of overlay content rows.
int ui_overlay_content_lines(void)
{
    UiFrame frame = overlay_frame();
    return ui_frame_content_lines(&frame);
}