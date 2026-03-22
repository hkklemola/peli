#ifndef UI_OVERLAY_H
#define UI_OVERLAY_H

#include "map.h"

/*
 * Purpose:
 *   Declares overlay rendering helpers and layout-derived overlay metrics.
 *
 * Functions:
 *   - ui_overlay_draw_frame/line: draw overlay frame and content rows.
 *   - ui_overlay_* accessors: query overlay geometry used by UI modules.
 */

// Overlay region covers both HUD and message log zones.
// Legacy compile-time constants are kept for compatibility, but new code
// should prefer ui_overlay_* accessors for layout-derived values.
#define UI_OVERLAY_START_ROW (VIEW_HEIGHT + 4)
#define UI_OVERLAY_HEIGHT 25
#define UI_OVERLAY_CONTENT_START_ROW (UI_OVERLAY_START_ROW + 3)
#define UI_OVERLAY_CONTENT_LINES (UI_OVERLAY_HEIGHT - 4)
#define UI_BOX_DASH_WIDTH VIEW_WIDTH
#define UI_BOX_TEXT_WIDTH (VIEW_WIDTH - 2)
#define UI_BOX_TOTAL_WIDTH (VIEW_WIDTH + 2)
#define UI_OVERLAY_TEXT_WIDTH UI_BOX_TEXT_WIDTH

// Draw overlay frame with title.
void ui_overlay_draw_frame(const char* title);

// Draw one content line in the overlay.
void ui_overlay_draw_line(int content_line, const char* text);

// Draw persistent global overlay hotkeys in a compact boxed row at overlay bottom.
void ui_overlay_draw_global_hotkeys(void);

// Draw a small centered prompt overlay and wait for one keypress.
void ui_overlay_show_mini_prompt(const char* title, const char* line1, const char* line2);

// Return overlay top row index.
int ui_overlay_start_row(void);

// Return overlay total height.
int ui_overlay_height(void);

// Return overlay dash width for horizontal borders.
int ui_overlay_dash_width(void);

// Return overlay printable text width.
int ui_overlay_text_width(void);

// Return overlay width including borders.
int ui_overlay_total_width(void);

// Return overlay content row count.
int ui_overlay_content_lines(void);

#endif