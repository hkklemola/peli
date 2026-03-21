#include "draw.h"
#include "atlas.h"
#include "hud.h"
#include "log.h"
#include "character.h"
#include "bestiary.h"
#include "layout.h"
#include "map.h"
#include "player.h"
#include "render_color.h"
#include "target_lock.h"
#include "tileset.h"
#include "world_items.h"
#include <stdio.h>
#include <stdlib.h> // for system()
#include <string.h>

/*
 * Purpose:
 *   Renders world viewport plus HUD and message-log panels.
 *
 * Functions:
 *   - move_cursor: ANSI cursor positioning helper.
 *   - draw_ensure_console_dimensions: ensures terminal size fits current layout.
 *   - draw_location_zone: renders centered current-location title box.
 *   - draw_viewport: renders map viewport with creature/player overlays.
 *   - draw_hud_zone: prints HUD lines produced by hud_get_lines.
 *   - draw_log_zone: prints latest message-log lines.
 *   - draw_world: top-level frame render call.
 */

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct ViewportCell {
    char symbol;
    unsigned char color;
} ViewportCell;

typedef struct RenderedGlyph {
    char symbol;
    RenderColor color;
} RenderedGlyph;

static ViewportCell* prev_map = NULL;
static int prev_map_width = 0;
static int prev_map_height = 0;
static int viewport_needs_full_redraw = 1;
static int ansi_colors_checked = 0;
static int ansi_colors_enabled = 0;
static int layout_signature_valid = 0;
static int layout_signature[10] = {0};

static int inspect_cursor_active = 0;
static int inspect_cursor_x = 0;
static int inspect_cursor_y = 0;
static int lock_highlight_active = 0;
static int lock_highlight_x = 0;
static int lock_highlight_y = 0;
static TargetLockResolved lock_highlight_target;

static void move_cursor(int row, int col);

/**
 * @brief Force a full viewport redraw on the next render pass.
 * @note Used when game state changes invalidate incremental rendering (e.g., level change).
 */
void draw_force_full_redraw(void)
{
    viewport_needs_full_redraw = 1;
}

/**
 * @brief Invalidate all viewport rendering caches and force full redraw.
 * @note Clears viewport cell cache, layout signature, and lock highlight state.
 *        Called when major structural changes occur (e.g., area transitions, window resize).
 */
void draw_invalidate_viewport_cache(void)
{
    viewport_needs_full_redraw = 1;
    layout_signature_valid = 0;
    lock_highlight_active = 0;

    if(prev_map && prev_map_width > 0 && prev_map_height > 0)
        memset(prev_map, 0, (size_t)prev_map_width * (size_t)prev_map_height * sizeof(*prev_map));
}

/**
 * @brief Update lock highlight state from player's current target lock.
 * @param p The player whose lock state should be highlighted in the viewport.
 * @note Queries the first resolved target lock and caches its position for rendering.
 */
static void draw_update_lock_state(Player* p)
{
    lock_highlight_active = 0;
    if(!p)
        return;

    if(target_lock_resolve_live(p, &lock_highlight_target, 1))
    {
        lock_highlight_active = 1;
        lock_highlight_x = lock_highlight_target.x;
        lock_highlight_y = lock_highlight_target.y;
    }
}

// Render player/inspect/lock coordinates in the spacer row under the viewport.
static void draw_coords_zone(Player* p)
{
    LayoutState layout;
    char line[256];
    int row;
    int width;
    int px;
    int py;
    TargetLockResolved locked;
    int has_lock;

    layout_get_default(&layout);
    row = layout.viewport.row + layout.viewport.height;
    width = layout.viewport.inner_width + 2;
    if(width < 1)
        return;

    px = p->character.actor.entity.x;
    py = p->character.actor.entity.y;
    has_lock = target_lock_resolve(p, &locked, 1);

    if(inspect_cursor_active && has_lock)
    {
        char target_text[96];
        target_lock_describe(&locked, target_text, sizeof(target_text));
        snprintf(line, sizeof(line), "Player (%d,%d)  Inspect (%d,%d)  Target %s",
                 px, py, inspect_cursor_x, inspect_cursor_y, target_text);
    }
    else if(inspect_cursor_active)
    {
        snprintf(line, sizeof(line), "Player (%d,%d)  Inspect (%d,%d)  Target none",
                 px, py, inspect_cursor_x, inspect_cursor_y);
    }
    else if(has_lock)
    {
        char target_text[96];
        target_lock_describe(&locked, target_text, sizeof(target_text));
        snprintf(line, sizeof(line), "Player (%d,%d)  Target %s", px, py, target_text);
    }
    else
    {
        snprintf(line, sizeof(line), "Player (%d,%d)  Target none", px, py);
    }

    move_cursor(row, layout.viewport.col);
    printf("%-*.*s", width, width, line);
}

// Enable ANSI color output when the active console supports it.
static void draw_enable_color_output(void)
{
    if(ansi_colors_checked)
        return;

    ansi_colors_checked = 1;

#ifdef _WIN32
    {
        HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;

        if(stdout_handle == INVALID_HANDLE_VALUE)
            return;
        if(!GetConsoleMode(stdout_handle, &mode))
            return;

        if((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) ||
           SetConsoleMode(stdout_handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            ansi_colors_enabled = 1;
    }
#else
    ansi_colors_enabled = 1;
#endif
}

// Print one glyph with color when supported, otherwise print it plainly.
static void draw_put_glyph(char symbol, RenderColor color)
{
    if(!ansi_colors_enabled || color == RENDER_COLOR_DEFAULT)
    {
        putchar(symbol);
        return;
    }

    printf("\x1b[%dm%c\x1b[39m", (int)color, symbol);
}

// Request full redraw when layout geometry changed since last frame.
static void draw_refresh_layout_signature(void)
{
    LayoutState layout;
    int signature[10];

    layout_get_default(&layout);
    signature[0] = layout.viewport.row;
    signature[1] = layout.viewport.col;
    signature[2] = layout.viewport.inner_width;
    signature[3] = layout.viewport.height;
    signature[4] = layout.hud.row;
    signature[5] = layout.hud.height;
    signature[6] = layout.log.row;
    signature[7] = layout.log.height;
    signature[8] = layout.coords_hint.row;
    signature[9] = layout.bottom_hotkeys.row;

    if(!layout_signature_valid || memcmp(layout_signature, signature, sizeof(signature)) != 0)
    {
        memcpy(layout_signature, signature, sizeof(signature));
        layout_signature_valid = 1;
        draw_force_full_redraw();
    }
}

// Render context-dependent controls hint row directly below coordinates.
static void draw_coords_hint_zone(void)
{
    LayoutState layout;
    char line[256];
    int row;
    int col;
    int width;

    layout_get_default(&layout);
    row = layout.coords_hint.row;
    col = layout.coords_hint.col;
    width = layout_box_total_width(&layout.coords_hint);
    if(width < 1)
        return;

    if(inspect_cursor_active)
    {
        snprintf(line,
                 sizeof(line),
                 "Inspect: arrows/WASD move | Enter inspect | E interact | 1-9 attack mode | L lock | Q cancel");
    }
    else
    {
        snprintf(line,
                 sizeof(line),
                 "Controls: WASD move | T inspect | I inventory | C character | M log | J journal | O atlas");
    }

    move_cursor(row, col);
    printf("%-*.*s", width, width, line);
}

// Resolve the visible symbol and color for one map coordinate.
static RenderedGlyph draw_resolve_glyph(Player* p, int mx, int my)
{
    RenderedGlyph glyph;
    Creature* c;
    WorldItem* world_item;

    if(!current_area || mx < 0 || my < 0 || mx >= current_area->width || my >= current_area->height)
    {
        glyph.symbol = TILE_OUT_OF_BOUNDS.symbol;
        glyph.color = TILE_OUT_OF_BOUNDS.color;
        return glyph;
    }

    glyph.symbol = current_area->map[my][mx].symbol;
    glyph.color = current_area->map[my][mx].color;

    if(inspect_cursor_active && mx == inspect_cursor_x && my == inspect_cursor_y)
    {
        glyph.symbol = 'X';
        glyph.color = RENDER_COLOR_LIGHT_YELLOW;
        return glyph;
    }

    c = bestiary_creature_at(mx, my);
    if(c && c->alive)
    {
        glyph.symbol = c->actor.entity.symbol;
        glyph.color = c->actor.entity.color;
    }
    else
    {
        world_item = world_item_at(mx, my);
        if(world_item)
        {
            glyph.symbol = world_item->item.entity.symbol;
            glyph.color = world_item->item.entity.color;
        }
    }

    if(lock_highlight_active && mx == lock_highlight_x && my == lock_highlight_y)
    {
        glyph.color = RENDER_COLOR_LIGHT_MAGENTA;
    }

    if(p->character.actor.entity.x == mx && p->character.actor.entity.y == my)
    {
        glyph.symbol = p->character.actor.entity.symbol;
        glyph.color = p->character.actor.entity.color;
    }

    return glyph;
}

// Ensure previous-frame viewport cache matches current viewport dimensions.
static int draw_ensure_prev_map_buffer(int width, int height)
{
    size_t size_needed;
    ViewportCell* resized;

    if(width < 1 || height < 1)
        return 0;

    if(prev_map && prev_map_width == width && prev_map_height == height)
        return 1;

    size_needed = (size_t)width * (size_t)height * sizeof(*prev_map);
    resized = (ViewportCell*)realloc(prev_map, size_needed);
    if(!resized)
    {
        free(prev_map);
        prev_map = NULL;
        prev_map_width = 0;
        prev_map_height = 0;
        return 0;
    }

    prev_map = resized;
    prev_map_width = width;
    prev_map_height = height;
    memset(prev_map, 0, size_needed);
    viewport_needs_full_redraw = 1;
    return 1;
}

// Return pointer to cached viewport cell, or NULL when unavailable.
static ViewportCell* prev_map_at(int x, int y)
{
    if(!prev_map || x < 0 || y < 0 || x >= prev_map_width || y >= prev_map_height)
        return NULL;

    return &prev_map[((size_t)y * (size_t)prev_map_width) + (size_t)x];
}

// Move terminal cursor to row/column using ANSI escape sequences.
static void move_cursor(int row, int col)
{
    printf("\x1b[%d;%dH", row, col);
}

// Render a three-row location box centered above viewport.
static void draw_location_zone(void)
{
    LayoutState layout;
    const char* location_name;
    int dash_width;
    int text_width;
    int name_len;
    int left_pad;
    int right_pad;

    layout_get_default(&layout);

    dash_width = layout.location.inner_width;
    if(dash_width < 1)
        return;

    text_width = layout_box_text_width(&layout.location);
    if(text_width < 1)
        text_width = 1;

    location_name = (current_area && current_area->name[0]) ? current_area->name : "Unknown Location";
    name_len = (int)strlen(location_name);
    if(name_len > text_width)
        name_len = text_width;

    left_pad = (text_width - name_len) / 2;
    if(left_pad < 0) left_pad = 0;
    right_pad = text_width - name_len - left_pad;
    if(right_pad < 0) right_pad = 0;

    move_cursor(layout.location.row, layout.location.col);
    putchar('+');
    for(int i = 0; i < dash_width; i++)
        putchar('-');
    putchar('+');

    move_cursor(layout.location.row + 1, layout.location.col);
    putchar('|');
    for(int i = 0; i < left_pad; i++)
        putchar(' ');
    if(name_len > 0)
        fwrite(location_name, (size_t)name_len, 1, stdout);
    for(int i = 0; i < right_pad; i++)
        putchar(' ');
    putchar('|');

    move_cursor(layout.location.row + 2, layout.location.col);
    putchar('+');
    for(int i = 0; i < dash_width; i++)
        putchar('-');
    putchar('+');
}

// Ensure the active console dimensions satisfy the configured UI layout.
void draw_ensure_console_dimensions(void)
{
#ifdef _WIN32
    static int resize_unavailable = 0;
    LayoutState layout;
    HANDLE stdout_handle;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int cols;
    int rows;
    int min_cols;
    int min_rows;

    layout_get_default(&layout);
    min_cols = layout.min_console_cols;
    min_rows = layout.min_console_rows;

    if(resize_unavailable)
        return;

    stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if(stdout_handle == INVALID_HANDLE_VALUE)
        return;
    if(!GetConsoleScreenBufferInfo(stdout_handle, &csbi))
        return;

    cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    if(cols != min_cols || rows != min_rows)
    {
        char cmd[96];
        int target_cols = min_cols;
        int target_rows = min_rows;

        snprintf(cmd, sizeof(cmd), "mode con cols=%d lines=%d >nul", target_cols, target_rows);
        if(system(cmd) != 0)
        {
            resize_unavailable = 1;
            return;
        }
        viewport_needs_full_redraw = 1;
    }
#endif
}

// Render map viewport with incremental redraw when possible.
static void draw_viewport(Player* p)
{
    LayoutState layout;
    int viewport_inner_width;
    int viewport_inner_height;
    int camera_x;
    int camera_y;
    int has_prev_map;

    layout_get_default(&layout);
    draw_enable_color_output();
    viewport_inner_width = layout.viewport.inner_width;
    viewport_inner_height = layout.viewport.height - 2;

    if(viewport_inner_width > current_area->width) viewport_inner_width = current_area->width;
    if(viewport_inner_height > current_area->height) viewport_inner_height = current_area->height;
    if(viewport_inner_width < 1 || viewport_inner_height < 1) return;

    has_prev_map = draw_ensure_prev_map_buffer(viewport_inner_width, viewport_inner_height);
    if(!has_prev_map)
        viewport_needs_full_redraw = 1;

    camera_x = p->character.actor.entity.x - viewport_inner_width / 2;
    camera_y = p->character.actor.entity.y - viewport_inner_height / 2;
    if(camera_x < 0) camera_x = 0;
    if(camera_y < 0) camera_y = 0;
    if(camera_x + viewport_inner_width > current_area->width) camera_x = current_area->width - viewport_inner_width;
    if(camera_y + viewport_inner_height > current_area->height) camera_y = current_area->height - viewport_inner_height;

    if(viewport_needs_full_redraw || !has_prev_map)
    {
        system("cls");
        move_cursor(layout.viewport.row, layout.viewport.col);
        putchar('+'); for(int i=0;i<viewport_inner_width;i++) putchar('-'); putchar('+');
        for(int vy = 0; vy < viewport_inner_height; vy++)
        {
            move_cursor(layout.viewport.row + 1 + vy, layout.viewport.col);
            putchar('|');
            for(int vx = 0; vx < viewport_inner_width; vx++)
            {
                int mx = camera_x + vx;
                int my = camera_y + vy;
                RenderedGlyph glyph = draw_resolve_glyph(p, mx, my);

                draw_put_glyph(glyph.symbol, glyph.color);
                if(has_prev_map)
                {
                    ViewportCell* prev_cell = prev_map_at(vx, vy);
                    if(prev_cell)
                    {
                        prev_cell->symbol = glyph.symbol;
                        prev_cell->color = (unsigned char)glyph.color;
                    }
                }
            }
            putchar('|');
        }
        move_cursor(layout.viewport.row + viewport_inner_height + 1, layout.viewport.col);
        putchar('+'); for(int i=0;i<viewport_inner_width;i++) putchar('-'); putchar('+');
        viewport_needs_full_redraw = 0;
        return;
    }

    for(int vy = 0; vy < viewport_inner_height; vy++)
    {
        for(int vx = 0; vx < viewport_inner_width; vx++)
        {
            int mx = camera_x + vx;
            int my = camera_y + vy;
            RenderedGlyph glyph = draw_resolve_glyph(p, mx, my);

            {
                ViewportCell* prev_cell = prev_map_at(vx, vy);
                if(!prev_cell || prev_cell->symbol != glyph.symbol || prev_cell->color != (unsigned char)glyph.color)
                {
                    move_cursor(layout.viewport.row + 1 + vy, layout.viewport.col + 1 + vx);
                    draw_put_glyph(glyph.symbol, glyph.color);
                    if(prev_cell)
                    {
                        prev_cell->symbol = glyph.symbol;
                        prev_cell->color = (unsigned char)glyph.color;
                    }
                }
            }
        }
    }
}

// Render HUD box content lines.
static void draw_hud_zone(Player* p)
{
    LayoutState layout;
    int text_width;
    int line_count;
    int n;
    char lines[LAYOUT_HUD_HEIGHT_MAX][HUD_LINE_LENGTH];

    layout_get_default(&layout);
    text_width = layout_box_text_width(&layout.hud);
    if(text_width < 1) text_width = 1;
    line_count = layout.hud.height;
    if(line_count < 1) return;
    if(line_count > LAYOUT_HUD_HEIGHT_MAX) line_count = LAYOUT_HUD_HEIGHT_MAX;

    n = hud_get_lines(p, lines, line_count);

    for(int i = 0; i < line_count; i++) {
        move_cursor(layout.hud.row + i, layout.hud.col);
        if(i < n)
            printf("%s", lines[i]);
        else
            printf("| %-*.*s |", text_width, text_width, "");
    }
}

// Render compact message log panel.
static void draw_log_zone(void)
{
    LayoutState layout;
    int line_count;
    int text_width;
    int dash_width;
    int content_lines;
    int n;
    char lines[LAYOUT_LOG_HEIGHT_MAX][LOG_ENTRY_LENGTH];

    layout_get_default(&layout);
    line_count = layout.log.height;
    if(line_count < 4) return;
    if(line_count > LAYOUT_LOG_HEIGHT_MAX) line_count = LAYOUT_LOG_HEIGHT_MAX;

    text_width = layout_box_text_width(&layout.log);
    if(text_width < 1) text_width = 1;
    dash_width = layout.log.inner_width;
    if(dash_width < 1) dash_width = 1;

    content_lines = line_count - 4;
    n = log_get_latest(lines, content_lines);

    for(int i = 0; i < line_count; i++) {
        move_cursor(layout.log.row + i, layout.log.col);
        if(i == 0 || i == 2 || i == line_count - 1) {
            putchar('+');
            for(int d = 0; d < dash_width; d++)
                putchar('-');
            putchar('+');
        }
        else if(i == 1) {
            printf("| %-*.*s |", text_width, text_width, "Message Log (press 'm' for full)");
        }
        else if(i >= 3 && i < 3 + n) {
            printf("| %-*.*s |", text_width, text_width, lines[i - 3]);
        }
        else {
            printf("| %-*.*s |", text_width, text_width, "");
        }
    }
}

// Render world-view overlay tab reminders on their own bottom row.
static void draw_bottom_hotkeys_zone(void)
{
    static const char* hotkeys_text = "Overlay tabs: i inventory | c character | m log | j journal | o atlas";
    LayoutState layout;
    char boxed[256];
    int row;
    int col;
    int total_width;
    int inner_text_width;

    layout_get_default(&layout);
    row = layout.bottom_hotkeys.row;
    col = layout.bottom_hotkeys.col;
    total_width = layout_box_total_width(&layout.bottom_hotkeys);
    if(total_width < 5)
        return;

    inner_text_width = total_width - 4;
    if(inner_text_width < 1)
        inner_text_width = 1;

    snprintf(boxed, sizeof(boxed), "| %-*.*s |", inner_text_width, inner_text_width, hotkeys_text);

    move_cursor(row, col);
    printf("%-*.*s", total_width, total_width, boxed);
}

// Set a temporary cursor position for inspect mode.
void draw_set_inspect_cursor(int x, int y)
{
    inspect_cursor_active = 1;
    inspect_cursor_x = x;
    inspect_cursor_y = y;
}

// Clear inspect cursor overlay.
void draw_clear_inspect_cursor(void)
{
    inspect_cursor_active = 0;
}

// Render the complete frame: viewport, HUD, and log.
void draw_world(Player* p)
{
    if(!current_area) return;
    draw_ensure_console_dimensions();
    draw_refresh_layout_signature();
    draw_update_lock_state(p);
    draw_viewport(p);
    draw_coords_zone(p);
    draw_coords_hint_zone();
    draw_location_zone();
    draw_hud_zone(p);
    draw_log_zone();
    draw_bottom_hotkeys_zone();
}

