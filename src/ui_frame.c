#include "ui_frame.h"

#include <stdio.h>
#include <string.h>

#include "color_palette.h"
#include "render_color.h"

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

// Draw one framed text row with a foreground color.
static void draw_row_text_color(const UiFrame* frame, int row, const char* text, int color)
{
    int text_width = ui_frame_text_width(frame);
    const char* safe_text = text ? text : "";
    int visible_length = (int)strlen(safe_text);
    int print_length = visible_length < text_width ? visible_length : text_width;
    char escape[32];
    int has_color = 0;

    move_cursor(row, frame->col);
    putchar('|');
    putchar(' ');

    if(color != RENDER_COLOR_DEFAULT && color_palette_make_fg_escape(color, escape, sizeof(escape)))
    {
        fputs(escape, stdout);
        has_color = 1;
    }

    if(print_length > 0)
        fwrite(safe_text, 1, (size_t)print_length, stdout);

    if(has_color)
        fputs("\x1b[39m", stdout);

    for(int i = 0; i < text_width - print_length; i++)
        putchar(' ');

    putchar(' ');
    putchar('|');
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

// Draw one content line inside an existing frame with a foreground color.
void ui_frame_draw_line_color(const UiFrame* frame, int content_line, const char* text, int color)
{
    int content_lines;

    if(!frame) return;

    content_lines = ui_frame_content_lines(frame);
    if(content_line < 0 || content_line >= content_lines)
        return;

    draw_row_text_color(frame, frame->row + 3 + content_line, text, color);
}

void ui_frame_surface_reset(UiFrameSurfaceCache* cache)
{
    if(!cache)
        return;

    memset(cache, 0, sizeof(*cache));
}

void ui_frame_surface_invalidate(UiFrameSurfaceCache* cache)
{
    if(!cache)
        return;

    for(int i = 0; i < UI_FRAME_SURFACE_MAX_LINES; i++)
        cache->line_valid[i] = 0;
}

void ui_frame_surface_begin(UiFrameSurfaceCache* cache, const UiFrame* frame, const char* title)
{
    const char* safe_title;
    int frame_changed;
    int title_changed;

    if(!frame)
        return;

    if(!cache)
    {
        ui_frame_draw(frame, title);
        return;
    }

    safe_title = title ? title : "";

    if(!cache->initialized)
    {
        ui_frame_surface_reset(cache);
        cache->initialized = 1;
    }

    frame_changed = memcmp(&cache->last_frame, frame, sizeof(UiFrame)) != 0;
    title_changed = strncmp(cache->last_title, safe_title, UI_FRAME_SURFACE_TEXT_WIDTH - 1) != 0;

    if(frame_changed || title_changed)
    {
        ui_frame_draw(frame, title);
        ui_frame_surface_invalidate(cache);
        cache->last_frame = *frame;
        strncpy(cache->last_title, safe_title, UI_FRAME_SURFACE_TEXT_WIDTH - 1);
        cache->last_title[UI_FRAME_SURFACE_TEXT_WIDTH - 1] = '\0';
    }
}

void ui_frame_surface_draw_line(UiFrameSurfaceCache* cache, const UiFrame* frame, int content_line, const char* text)
{
    const char* safe_text;
    int content_lines;

    if(!frame)
        return;

    if(!cache)
    {
        ui_frame_draw_line(frame, content_line, text);
        return;
    }

    if(!cache->initialized)
    {
        ui_frame_surface_reset(cache);
        cache->initialized = 1;
    }

    safe_text = text ? text : "";
    content_lines = ui_frame_content_lines(frame);
    if(content_line < 0 || content_line >= content_lines)
        return;

    if(content_line < UI_FRAME_SURFACE_MAX_LINES)
    {
        if(cache->line_valid[content_line] &&
           strncmp(cache->line_cache[content_line], safe_text, UI_FRAME_SURFACE_TEXT_WIDTH - 1) == 0)
            return;

        ui_frame_draw_line(frame, content_line, text);
        strncpy(cache->line_cache[content_line], safe_text, UI_FRAME_SURFACE_TEXT_WIDTH - 1);
        cache->line_cache[content_line][UI_FRAME_SURFACE_TEXT_WIDTH - 1] = '\0';
        cache->line_valid[content_line] = 1;
        return;
    }

    ui_frame_draw_line(frame, content_line, text);
}