#include "ui_frame.h"

#include <stdio.h>

/*
 * Purpose:
 *   Implements generic framed-box drawing helpers for text overlays.
 *
 * Functions:
 *   - move_cursor: ANSI cursor helper.
 *   - draw_border/draw_row_text: internal frame row primitives.
 *   - ui_frame_text_width/content_lines: geometry helpers.
 *   - ui_frame_draw: draws full frame shell.
 *   - ui_frame_draw_line: writes a single frame content row.
 */

// Move terminal cursor to a row/column coordinate.
static void move_cursor(int row, int col)
{
    printf("\x1b[%d;%dH", row, col);
}

// Draw one border row for a frame.
static void draw_border(const UiFrame* frame, int row)
{
    move_cursor(row, frame->col);
    putchar('+');
    for(int i = 0; i < frame->inner_width; i++)
        putchar('-');
    putchar('+');
}

// Draw one framed text row at a specific screen row.
static void draw_row_text(const UiFrame* frame, int row, const char* text)
{
    int text_width = ui_frame_text_width(frame);

    move_cursor(row, frame->col);
    printf("| %-*.*s |", text_width, text_width, text ? text : "");
}

// Return printable text width inside frame borders.
int ui_frame_text_width(const UiFrame* frame)
{
    if(!frame) return 0;
    if(frame->inner_width <= 2) return 1;
    return frame->inner_width - 2;
}

// Return number of content rows available in frame body.
int ui_frame_content_lines(const UiFrame* frame)
{
    if(!frame) return 0;
    if(frame->height <= 4) return 0;
    return frame->height - 4;
}

// Draw full frame including title row and empty body rows.
void ui_frame_draw(const UiFrame* frame, const char* title)
{
    int content_lines;

    if(!frame) return;

    draw_border(frame, frame->row);
    draw_row_text(frame, frame->row + 1, title ? title : "");
    draw_border(frame, frame->row + 2);

    content_lines = ui_frame_content_lines(frame);
    for(int i = 0; i < content_lines; i++)
        draw_row_text(frame, frame->row + 3 + i, "");

    draw_border(frame, frame->row + frame->height - 1);
}

// Draw one content line inside an existing frame.
void ui_frame_draw_line(const UiFrame* frame, int content_line, const char* text)
{
    int content_lines;

    if(!frame) return;

    content_lines = ui_frame_content_lines(frame);
    if(content_line < 0 || content_line >= content_lines)
        return;

    draw_row_text(frame, frame->row + 3 + content_line, text);
}