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

// Draw a single content line in an existing frame.
void ui_frame_draw_line(const UiFrame* frame, int content_line, const char* text);

#endif