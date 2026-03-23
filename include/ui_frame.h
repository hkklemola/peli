#ifndef UI_FRAME_H
#define UI_FRAME_H

/*
 * Purpose:
 *   Declares generic framed-box rendering helpers for text UI panels.
 *
 * Functions:
 *   - ui_frame_text_width/content_lines: geometry helpers.
 *   - ui_frame_draw: draw full frame with title and empty content rows.
 *   - ui_frame_draw_line: draw one content row inside a frame.
 */

typedef struct UiFrame
{
    int row;
    int col;
    int inner_width;
    int height;
} UiFrame;

// Return printable text width inside frame borders.
int ui_frame_text_width(const UiFrame* frame);

// Return number of content rows available in frame body.
int ui_frame_content_lines(const UiFrame* frame);

// Draw a full framed box and title.
void ui_frame_draw(const UiFrame* frame, const char* title);

#define UI_FRAME_SURFACE_MAX_LINES 128
#define UI_FRAME_SURFACE_TEXT_WIDTH 256

typedef struct UiFrameSurfaceCache
{
    int initialized;
    UiFrame last_frame;
    char last_title[UI_FRAME_SURFACE_TEXT_WIDTH];
    char line_cache[UI_FRAME_SURFACE_MAX_LINES][UI_FRAME_SURFACE_TEXT_WIDTH];
    int line_valid[UI_FRAME_SURFACE_MAX_LINES];
} UiFrameSurfaceCache;

// Draw a single content line in an existing frame.
void ui_frame_draw_line(const UiFrame* frame, int content_line, const char* text);

// Reset all cached frame and row state.
void ui_frame_surface_reset(UiFrameSurfaceCache* cache);

// Ensure frame shell is drawn when frame geometry/title changed.
void ui_frame_surface_begin(UiFrameSurfaceCache* cache, const UiFrame* frame, const char* title);

// Invalidate cached content rows so subsequent row draws repaint.
void ui_frame_surface_invalidate(UiFrameSurfaceCache* cache);

// Draw one content row only when row text differs from cache.
void ui_frame_surface_draw_line(UiFrameSurfaceCache* cache, const UiFrame* frame, int content_line, const char* text);

#endif