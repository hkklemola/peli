#include "draw.h"
#include "actor.h"
#include "atlas.h"
#include "furniture.h"
#include "hud.h"
#include "keybind_helpers.h"
#include "log.h"
#include "character.h"
#include "bestiary.h"
#include "npc.h"
#include "color_palette.h"
#include "layout.h"
#include "map.h"
#include "player.h"
#include "render_color.h"
#include "target_lock.h"
#include "tileset.h"
#include "world_map.h"
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

#include "cp437.h"

typedef struct ViewportCell {
    unsigned char symbol;
    int color;
} ViewportCell;

typedef struct RenderedGlyph {
    unsigned char symbol;
    int color;
} RenderedGlyph;

static ViewportCell* prev_map = NULL;
static int prev_map_width = 0;
static int prev_map_height = 0;
static int viewport_needs_full_redraw = 1;
static int viewport_dirty_active = 0;
static int viewport_dirty_min_x = 0;
static int viewport_dirty_min_y = 0;
static int viewport_dirty_max_x = 0;
static int viewport_dirty_max_y = 0;
static int ansi_colors_checked = 0;
static int ansi_colors_enabled = 0;
static int palette_mode_checked = 0;
static int layout_signature_valid = 0;
static int layout_signature[10] = {0};
static int glyph_color_active = 0;
static int glyph_last_color = RENDER_COLOR_DEFAULT;

static int inspect_cursor_active = 0;
static int inspect_cursor_x = 0;
static int inspect_cursor_y = 0;
static int viewed_layer_override = -1;
static int lock_highlight_active = 0;
static int lock_highlight_x = 0;
static int lock_highlight_y = 0;
static TargetLockResolved lock_highlight_target;
static ViewportTab active_viewport_tab = VIEWPORT_TAB_ZONE;

static void move_cursor(int row, int col);
static void draw_world_map_focus_position(int* out_x, int* out_y);
static int draw_fog_dim_color(int color);

static void draw_glyph_set_ascii(RenderedGlyph* glyph, unsigned char symbol, int color)
{
    if(!glyph)
        return;

    glyph->symbol = symbol;
    glyph->color = color;
}

static void draw_reset_viewport_dirty_region(void)
{
    viewport_dirty_active = 0;
    viewport_dirty_min_x = 0;
    viewport_dirty_min_y = 0;
    viewport_dirty_max_x = 0;
    viewport_dirty_max_y = 0;
}

#ifdef _WIN32
static void draw_enable_windows_utf8(void)
{
    HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if(out_handle == INVALID_HANDLE_VALUE || out_handle == NULL)
        return;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    DWORD mode;
    if(GetConsoleMode(out_handle, &mode))
    {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(out_handle, mode);
    }
}
#endif

static void draw_clear_screen(void)
{
#ifdef _WIN32
    draw_enable_windows_utf8();
    if(ansi_colors_enabled)
    {
        printf("\x1b[2J\x1b[H");
        fflush(stdout);
    }
    else
    {
        system("cls");
    }
#else
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
#endif
}

/**
 * @brief Force a full viewport redraw on the next render pass.
 * @note Used when game state changes invalidate incremental rendering (e.g., level change).
 */
void draw_force_full_redraw(void)
{
    viewport_needs_full_redraw = 1;
    draw_reset_viewport_dirty_region();
}

void draw_invalidate_viewport_contents(void)
{
    viewport_needs_full_redraw = 0;
    draw_reset_viewport_dirty_region();

    if(prev_map && prev_map_width > 0 && prev_map_height > 0)
        memset(prev_map, 0, (size_t)prev_map_width * (size_t)prev_map_height * sizeof(*prev_map));
}

void draw_mark_world_rect_dirty(int x0, int y0, int x1, int y1)
{
    int swap;

    if(x0 > x1)
    {
        swap = x0;
        x0 = x1;
        x1 = swap;
    }
    if(y0 > y1)
    {
        swap = y0;
        y0 = y1;
        y1 = swap;
    }

    if(!viewport_dirty_active)
    {
        viewport_dirty_active = 1;
        viewport_dirty_min_x = x0;
        viewport_dirty_min_y = y0;
        viewport_dirty_max_x = x1;
        viewport_dirty_max_y = y1;
        return;
    }

    if(x0 < viewport_dirty_min_x) viewport_dirty_min_x = x0;
    if(y0 < viewport_dirty_min_y) viewport_dirty_min_y = y0;
    if(x1 > viewport_dirty_max_x) viewport_dirty_max_x = x1;
    if(y1 > viewport_dirty_max_y) viewport_dirty_max_y = y1;
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
    draw_reset_viewport_dirty_region();

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

static int draw_effective_view_layer(const Player* p)
{
    int player_layer = 0;

    if(p)
        player_layer = map_clamp_view_floor(current_area, p->character.actor.entity.z);

    if(viewed_layer_override < 0)
        return player_layer;

    return map_clamp_view_floor(current_area, viewed_layer_override);
}

int draw_get_view_layer(const Player* p)
{
    return draw_effective_view_layer(p);
}

void draw_set_view_layer(int layer)
{
    viewed_layer_override = map_clamp_view_floor(current_area, layer);
    draw_force_full_redraw();
}

void draw_nudge_view_layer(int delta, const Player* p)
{
    int current_layer = draw_effective_view_layer(p);
    draw_set_view_layer(current_layer + delta);
}

void draw_reset_view_layer_to_player(void)
{
    viewed_layer_override = -1;
    draw_force_full_redraw();
}

ViewportTab draw_get_viewport_tab(void)
{
    return active_viewport_tab;
}

void draw_set_viewport_tab(ViewportTab tab)
{
    if(tab != VIEWPORT_TAB_ZONE && tab != VIEWPORT_TAB_WORLD)
        tab = VIEWPORT_TAB_ZONE;

    if(active_viewport_tab == tab)
        return;

    active_viewport_tab = tab;
    draw_invalidate_viewport_contents();
}

void draw_toggle_viewport_tab(void)
{
    draw_set_viewport_tab(active_viewport_tab == VIEWPORT_TAB_ZONE ? VIEWPORT_TAB_WORLD : VIEWPORT_TAB_ZONE);
}

static void draw_world_map_focus_position(int* out_x, int* out_y)
{
    int current_index = -1;

    if(!out_x || !out_y)
        return;

    *out_x = WORLD_MAP_WIDTH / 2;
    *out_y = WORLD_MAP_HEIGHT / 2;

    if(world_map_get_overworld_position(out_x, out_y))
        return;

    if(current_area)
        current_index = atlas_find_location(current_area->name);

    if(current_index >= 0)
        (void)world_map_find_zone(current_index, out_x, out_y);
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
    int pz;
    int vz;
    TargetLockResolved locked;
    int has_lock;

    layout_get_default(&layout);
    row = layout.viewport.row + layout.viewport.height;
    width = layout.viewport.inner_width + 2;
    if(width < 1)
        return;

    px = p->character.actor.entity.x;
    py = p->character.actor.entity.y;
    pz = p->character.actor.entity.z;
    vz = draw_effective_view_layer(p);
    has_lock = target_lock_resolve(p, &locked, 1);

    if(active_viewport_tab == VIEWPORT_TAB_WORLD)
    {
        int wx;
        int wy;

        draw_world_map_focus_position(&wx, &wy);
        snprintf(line,
                 sizeof(line),
                 "Overworld (%d,%d)  Local (%d,%d,%d)  View: World",
                 wx,
                 wy,
                 px,
                 py,
                 pz);
    }
    else if(inspect_cursor_active && has_lock)
    {
        char target_text[96];
        target_lock_describe(&locked, target_text, sizeof(target_text));
        snprintf(line, sizeof(line), "Player (%d,%d,%d)  View Z:%d  Inspect (%d,%d)  Target %s",
                 px, py, pz, vz, inspect_cursor_x, inspect_cursor_y, target_text);
    }
    else if(inspect_cursor_active)
    {
        snprintf(line, sizeof(line), "Player (%d,%d,%d)  View Z:%d  Inspect (%d,%d)  Target none",
                 px, py, pz, vz, inspect_cursor_x, inspect_cursor_y);
    }
    else if(has_lock)
    {
        char target_text[96];
        target_lock_describe(&locked, target_text, sizeof(target_text));
        snprintf(line, sizeof(line), "Player (%d,%d,%d)  View Z:%d  Target %s", px, py, pz, vz, target_text);
    }
    else
    {
        snprintf(line, sizeof(line), "Player (%d,%d,%d)  View Z:%d  Target none", px, py, pz, vz);
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

    if(!palette_mode_checked)
    {
        if(color_palette_get_mode() == COLOR_PALETTE_MODE_16)
            color_palette_set_mode(color_palette_detect_mode());
        palette_mode_checked = 1;
    }
}

// Print one glyph with color when supported, otherwise print it plainly.
static void draw_put_glyph(unsigned char symbol, int color)
{
    char escape[32];
    char output[5];

    if(!ansi_colors_enabled || color == RENDER_COLOR_DEFAULT)
    {
        if(glyph_color_active)
        {
            fputs("\x1b[39m", stdout);
            glyph_color_active = 0;
            glyph_last_color = RENDER_COLOR_DEFAULT;
        }
    }
    else if(!glyph_color_active || glyph_last_color != color)
    {
        if(!color_palette_make_fg_escape(color, escape, sizeof(escape)))
        {
            if(glyph_color_active)
            {
                fputs("\x1b[39m", stdout);
                glyph_color_active = 0;
                glyph_last_color = RENDER_COLOR_DEFAULT;
            }
            cp437_to_utf8(symbol, output, sizeof(output));
            fputs(output, stdout);
            return;
        }

        fputs(escape, stdout);
        glyph_color_active = 1;
        glyph_last_color = color;
    }

    if(symbol >= 0x20 && symbol <= 0x7E)
    {
        putchar((char)symbol);
        return;
    }

    cp437_to_utf8(symbol, output, sizeof(output));
    fputs(output, stdout);
}

static void draw_put_glyph_flush_color(void)
{
    if(glyph_color_active)
    {
        fputs("\x1b[39m", stdout);
        glyph_color_active = 0;
        glyph_last_color = RENDER_COLOR_DEFAULT;
    }
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
    const char* line;
    int row;
    int col;
    int width;

    layout_get_default(&layout);

    row = layout.coords_hint.row;
    col = layout.coords_hint.col;
    width = layout_box_total_width(&layout.coords_hint);
    if(width < 1)
        return;

    if(active_viewport_tab == VIEWPORT_TAB_WORLD)
        line = "World View: T detailed map | I inventory | O codex";
    else
        line = inspect_cursor_active ? HOTKEYS_INSPECT_ACTIONS_TEXT : HOTKEYS_WORLD_ACTIONS_TEXT;

    move_cursor(row, col);
    printf("%-*.*s", width, width, line);
}

static int draw_sign(int v)
{
    if(v < 0) return -1;
    if(v > 0) return 1;
    return 0;
}

static char draw_trail_symbol_from_step(int step_x, int step_y, int hit_frame)
{
    if(hit_frame)
        return '*';

    if(step_x == 0)
        return '|';
    if(step_y == 0)
        return '-';
    return (step_x == step_y) ? '\\' : '/';
}

static int draw_attack_animation_marker(const Player* p, int mx, int my, int pz, char* out_symbol, int* out_color)
{
    const AttackAnimationState* anim;
    Creature* endpoint_creature;
    int dx;
    int dy;
    int step_x;
    int step_y;
    int distance;
    int visible_steps;
    int marker_x;
    int marker_y;
    int hit_frame = 0;

    if(!p || !out_symbol || !out_color)
        return 0;

    anim = &p->attack_animation;
    if(!anim->active || anim->frame_max <= 0)
        return 0;
    if(anim->origin_z != pz || anim->target_z != pz)
        return 0;

    dx = anim->target_x - anim->origin_x;
    dy = anim->target_y - anim->origin_y;
    step_x = draw_sign(dx);
    step_y = draw_sign(dy);
    distance = abs(dx);
    if(abs(dy) > distance)
        distance = abs(dy);

    if(distance <= 0)
        return 0;

    marker_x = anim->target_x;
    marker_y = anim->target_y;

    if(anim->type == ATTACK_ANIM_RANGED)
    {
        int travel_steps = anim->frame + 1;

        if(travel_steps > distance)
            travel_steps = distance;
        if(travel_steps < 1)
            travel_steps = 1;

        marker_x = anim->origin_x + (step_x * travel_steps);
        marker_y = anim->origin_y + (step_y * travel_steps);
        hit_frame = (travel_steps >= distance && anim->frame >= (anim->frame_max - 1)) ? 1 : 0;
    }
    else
    {
        endpoint_creature = bestiary_creature_at_3d(anim->target_x, anim->target_y, pz);
        visible_steps = anim->frame + 1;
        if(visible_steps > distance)
            visible_steps = distance;

        if(visible_steps < 1)
            return 0;

        for(int step = 1; step <= visible_steps; step++)
        {
            marker_x = anim->origin_x + (step_x * step);
            marker_y = anim->origin_y + (step_y * step);

            if(marker_x != mx || marker_y != my)
                continue;

            if(step == distance && endpoint_creature && endpoint_creature->alive && distance > 1)
                return 0;

            hit_frame = (step == distance && anim->frame >= (anim->frame_max - 1)) ? 1 : 0;
            *out_symbol = draw_trail_symbol_from_step(step_x, step_y, hit_frame);
            *out_color = RENDER_COLOR_LIGHT_YELLOW;
            return 1;
        }

        return 0;
    }

    if(marker_x != mx || marker_y != my)
        return 0;

    *out_symbol = draw_trail_symbol_from_step(step_x, step_y, hit_frame);
    *out_color = (anim->type == ATTACK_ANIM_RANGED) ? RENDER_COLOR_LIGHT_CYAN : RENDER_COLOR_LIGHT_YELLOW;
    return 1;
}

// Resolve the visible symbol and color for one map coordinate.
static RenderedGlyph draw_resolve_glyph(Player* p, int mx, int my)
{
    RenderedGlyph glyph;
    Creature* c;
    NPC* npc;
    WorldItem* world_item;
    WorldContainer* world_container;
    const Tile* base_tile;
    unsigned char marker_symbol;
    int marker_color;
    int px;
    int py;
    int pz;
    int vision_range;
    unsigned char attack_symbol;
    int attack_color;
    int dx;
    int dy;
    int tile_currently_visible;
    int view_layer;
    int player_here;

    if(!current_area || mx < 0 || my < 0 || mx >= current_area->width || my >= current_area->height)
    {
        draw_glyph_set_ascii(&glyph, TILE_OUT_OF_BOUNDS.symbol, TILE_OUT_OF_BOUNDS.color);
        return glyph;
    }

    px = p->character.actor.entity.x;
    py = p->character.actor.entity.y;
    pz = p->character.actor.entity.z;
    vision_range = actor_area_vision_range(&p->character.actor);

    player_here = (px == mx && py == my);
    if(!map_is_tile_discovered(current_area, mx, my) && !player_here)
    {
        draw_glyph_set_ascii(&glyph, ' ', RENDER_COLOR_DEFAULT);
        return glyph;
    }

    view_layer = draw_effective_view_layer(p);
    base_tile = map_top_visible_tile_at_view(current_area, mx, my, view_layer, NULL);
    if(base_tile)
    {
        draw_glyph_set_ascii(&glyph, base_tile->symbol, base_tile->color);
    }
    else
    {
        draw_glyph_set_ascii(&glyph, ' ', RENDER_COLOR_DEFAULT);
    }

    if(inspect_cursor_active && mx == inspect_cursor_x && my == inspect_cursor_y)
    {
        draw_glyph_set_ascii(&glyph, 'X', RENDER_COLOR_LIGHT_YELLOW);
        return glyph;
    }

    dx = mx - px;
    dy = my - py;
    tile_currently_visible = player_here;
    if(!tile_currently_visible)
    {
        int in_range = ((dx * dx) + (dy * dy)) <= (vision_range * vision_range);
        if(in_range && map_has_line_of_sight(px, py, mx, my))
            tile_currently_visible = 1;
    }

    if(!tile_currently_visible)
        glyph.color = draw_fog_dim_color(glyph.color);

    c = bestiary_creature_at_3d(mx, my, pz);
    npc = npc_at_3d(mx, my, pz);
    world_item = world_item_at_3d(mx, my, pz);
    world_container = world_container_at_3d(mx, my, pz);
    {
        Furniture* furn = furniture_at(current_area, mx, my);

        if(tile_currently_visible)
        {
            if(c && c->alive)
            {
                draw_glyph_set_ascii(&glyph, c->actor.entity.symbol, c->actor.entity.color);
                map_set_entity_marker(current_area, mx, my, pz, glyph.symbol, glyph.color);
            }
            else if(npc && npc->active)
            {
                draw_glyph_set_ascii(&glyph,
                                    npc->character.actor.entity.symbol,
                                    npc->character.actor.entity.color);
                map_set_entity_marker(current_area, mx, my, pz, glyph.symbol, glyph.color);
            }
            else if(furn && furn->type != FURNITURE_NONE)
            {
                draw_glyph_set_ascii(&glyph, furn->base.base.symbol, furn->base.base.color);
                map_set_entity_marker(current_area, mx, my, pz, glyph.symbol, glyph.color);
            }
            else if(world_item)
            {
                draw_glyph_set_ascii(&glyph,
                                    world_item->item.object.base.symbol,
                                    world_item->item.object.base.color);
                map_set_entity_marker(current_area, mx, my, pz, glyph.symbol, glyph.color);
            }
            else if(world_container && world_container->active)
            {
                draw_glyph_set_ascii(&glyph, '#', RENDER_COLOR_LIGHT_YELLOW);
                map_set_entity_marker(current_area, mx, my, pz, glyph.symbol, glyph.color);
            }
            else
            {
                map_clear_entity_marker(current_area, mx, my, pz);
            }
        }
        else if(map_get_entity_marker(current_area, mx, my, pz, &marker_symbol, &marker_color))
        {
            glyph.symbol = marker_symbol;
            glyph.color = draw_fog_dim_color(marker_color);
        }
    }

    if(draw_attack_animation_marker(p, mx, my, pz, &attack_symbol, &attack_color))
    {
        draw_glyph_set_ascii(&glyph, attack_symbol, attack_color);
    }

    if(lock_highlight_active && mx == lock_highlight_x && my == lock_highlight_y)
    {
        glyph.color = RENDER_COLOR_LIGHT_MAGENTA;
    }

    if(player_here)
    {
        draw_glyph_set_ascii(&glyph,
                            p->character.actor.entity.symbol,
                            p->character.actor.entity.color);
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

// Render location title box; viewport top border acts as the shared bottom seam.
static void draw_location_zone(void)
{
    LayoutState layout;
    const char* location_name;
    char title[256];
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
    snprintf(title,
             sizeof(title),
             active_viewport_tab == VIEWPORT_TAB_WORLD ? "Zone [World]  Overland Map" : "[Zone] World  %s",
             location_name);

    name_len = (int)strlen(title);
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
        fwrite(title, (size_t)name_len, 1, stdout);
    for(int i = 0; i < right_pad; i++)
        putchar(' ');
    putchar('|');

}

// Ensure the active console dimensions satisfy the configured UI layout.
// Returns 1 if the console was resized, 0 otherwise.
int draw_ensure_console_dimensions(void)
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
        return 0;

    stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if(stdout_handle == INVALID_HANDLE_VALUE)
        return 0;
    if(!GetConsoleScreenBufferInfo(stdout_handle, &csbi))
        return 0;

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
            return 0;
        }
        viewport_needs_full_redraw = 1;
        return 1;
    }
    return 0;
#else
    return 0;
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
    int start_vx = 0;
    int start_vy = 0;
    int end_vx;
    int end_vy;

    layout_get_default(&layout);
    draw_enable_color_output();
    viewport_inner_width = layout.viewport.inner_width;
    viewport_inner_height = layout.viewport.height - 2;

    if(viewport_inner_width > current_area->width) viewport_inner_width = current_area->width;
    if(viewport_inner_height > current_area->height) viewport_inner_height = current_area->height;
    if(viewport_inner_width < 1 || viewport_inner_height < 1) return;

    end_vx = viewport_inner_width - 1;
    end_vy = viewport_inner_height - 1;

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
        draw_clear_screen();
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
                        prev_cell->color = glyph.color;
                    }
                }
            }
            draw_put_glyph_flush_color();
            putchar('|');
        }
        move_cursor(layout.viewport.row + viewport_inner_height + 1, layout.viewport.col);
        putchar('+'); for(int i=0;i<viewport_inner_width;i++) putchar('-'); putchar('+');
        viewport_needs_full_redraw = 0;
        draw_reset_viewport_dirty_region();
        return;
    }

    if(viewport_dirty_active)
    {
        start_vx = viewport_dirty_min_x - camera_x;
        start_vy = viewport_dirty_min_y - camera_y;
        end_vx = viewport_dirty_max_x - camera_x;
        end_vy = viewport_dirty_max_y - camera_y;

        if(start_vx < 0) start_vx = 0;
        if(start_vy < 0) start_vy = 0;
        if(end_vx >= viewport_inner_width) end_vx = viewport_inner_width - 1;
        if(end_vy >= viewport_inner_height) end_vy = viewport_inner_height - 1;

        if(start_vx > end_vx || start_vy > end_vy)
        {
            draw_reset_viewport_dirty_region();
            return;
        }
    }

    for(int vy = start_vy; vy <= end_vy; vy++)
    {
        for(int vx = start_vx; vx <= end_vx; vx++)
        {
            int mx = camera_x + vx;
            int my = camera_y + vy;
            RenderedGlyph glyph = draw_resolve_glyph(p, mx, my);

            {
                ViewportCell* prev_cell = prev_map_at(vx, vy);
                if(!prev_cell ||
                   prev_cell->symbol != glyph.symbol ||
                   prev_cell->color != glyph.color)
                {
                    move_cursor(layout.viewport.row + 1 + vy, layout.viewport.col + 1 + vx);
                    draw_put_glyph(glyph.symbol, glyph.color);
                    draw_put_glyph_flush_color();
                    if(prev_cell)
                    {
                        prev_cell->symbol = glyph.symbol;
                        prev_cell->color = glyph.color;
                    }
                }
            }
        }
    }

    draw_reset_viewport_dirty_region();
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
    if(line_count < 3) return;
    if(line_count > LAYOUT_LOG_HEIGHT_MAX) line_count = LAYOUT_LOG_HEIGHT_MAX;

    text_width = layout_box_text_width(&layout.log);
    if(text_width < 1) text_width = 1;
    dash_width = layout.log.inner_width;
    if(dash_width < 1) dash_width = 1;

    content_lines = line_count - 2;
    n = log_get_latest(lines, content_lines);

    for(int i = 0; i < line_count; i++) {
        move_cursor(layout.log.row + i, layout.log.col);
        if(i == line_count - 1) {
            putchar('+');
            for(int d = 0; d < dash_width; d++)
                putchar('-');
            putchar('+');
        }
        else if(i == 0) {
            printf("| %-*.*s |", text_width, text_width, "Message Log (press 'l' for full)");
        }
        else if(i >= 1 && i < 1 + n) {
            printf("| %-*.*s |", text_width, text_width, lines[i - 1]);
        }
        else {
            printf("| %-*.*s |", text_width, text_width, "");
        }
    }
}

// Render world-view overlay tab reminders on their own bottom row.
static void draw_bottom_hotkeys_zone(void)
{
    static const char* hotkeys_text = HOTKEYS_BOTTOM_OVERLAY_TEXT;
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

static int draw_fog_dim_color(int color)
{
    switch(color)
    {
        case RENDER_COLOR_BLACK:
        case RENDER_COLOR_DARK_GRAY:
            return RENDER_COLOR_DARK_GRAY;
        case RENDER_COLOR_DEFAULT:
        case RENDER_COLOR_LIGHT_GRAY:
        case RENDER_COLOR_WHITE:
        case RENDER_COLOR_RED:
        case RENDER_COLOR_GREEN:
        case RENDER_COLOR_BROWN:
        case RENDER_COLOR_BLUE:
        case RENDER_COLOR_MAGENTA:
        case RENDER_COLOR_CYAN:
        case RENDER_COLOR_LIGHT_RED:
        case RENDER_COLOR_LIGHT_GREEN:
        case RENDER_COLOR_LIGHT_YELLOW:
        case RENDER_COLOR_LIGHT_BLUE:
        case RENDER_COLOR_LIGHT_MAGENTA:
        case RENDER_COLOR_LIGHT_CYAN:
        default:
            return RENDER_COLOR_LIGHT_GRAY;
    }
}


// Biome-to-palette mapping (indices chosen to match suggested hex colors as closely as possible)
static const int biome_palette_index[BIOME_COUNT] = {
    [BIOME_NONE]       = 244, // GRAY
    [BIOME_GRASSLANDS] = 112, // PISTACHIO (#87d700 ~ #7CB342)
    [BIOME_FOREST]     = 71,  // FOREST_GREEN (#5faf5f ~ #2E7D32)
    [BIOME_DESERT]     = 222, // KHAKI (#ffd787 ~ #E6C97A)
    [BIOME_TUNDRA]     = 188, // LIGHT_SILVER (#d7d7d7 ~ #CFD8DC)
    [BIOME_SEA]        = 24,  // SEA_BLUE (#005f87 ~ #1565C0)
    [BIOME_SAVANNAH]   = 143, // OLIVE_GREEN (#afaf5f ~ #C0CA33)
    [BIOME_MOUNTAINS]  = 8,   // GRAY (#808080 ~ #8D8D8D)
    [BIOME_FOOTHILLS]  = 137, // BRONZE (#af875f ~ #A1887F)
    [BIOME_SWAMP]      = 65,  // GLADE_GREEN (#5f875f ~ #6D8B74)
    [BIOME_JUNGLE]     = 28,  // AO (#008700 ~ #1B5E20)
    [BIOME_TAIGA]      = 108, // BAY_LEAF (#87af87)
    [BIOME_SHRUBLAND]  = 144, // SAGE (#afaf87)
    [BIOME_STEPPE]     = 180, // TAN (#d7af87)
    [BIOME_GLACIER]    = 189, // VERY_PALE_BLUE (#d7d7ff)
};

static RenderedGlyph draw_biome_glyph(WorldMapBiome biome, int discovered)
{
    RenderedGlyph g;
    draw_glyph_set_ascii(&g, '.', biome_palette_index[BIOME_NONE]);

    if (biome >= 0 && biome < BIOME_COUNT) {
        g.symbol = discovered ? '.' : ',';
        g.color = biome_palette_index[biome];
    }
    return g;
}


static RenderedGlyph draw_road_glyph(int road_tier, int discovered)
{
    RenderedGlyph g;
    draw_glyph_set_ascii(&g, ':', discovered ? RENDER_COLOR_BROWN : RENDER_COLOR_DARK_GRAY);

    switch(road_tier)
    {
        case WORLD_MAP_ROAD_TIER_TRAIL:
            g.symbol = ':';
            g.color = discovered ? RENDER_COLOR_BROWN : RENDER_COLOR_DARK_GRAY;
            break;
        case WORLD_MAP_ROAD_TIER_PAVED:
            g.symbol = '#';
            g.color = discovered ? RENDER_COLOR_LIGHT_GRAY : RENDER_COLOR_DARK_GRAY;
            break;
        case WORLD_MAP_ROAD_TIER_HIGHWAY:
            g.symbol = '=';
            g.color = discovered ? RENDER_COLOR_LIGHT_CYAN : RENDER_COLOR_CYAN;
            break;
        case WORLD_MAP_ROAD_TIER_NONE:
        default:
            break;
    }

    return g;
}

static RenderedGlyph draw_river_glyph(int river_tier, int discovered)
{
    RenderedGlyph g;
    draw_glyph_set_ascii(&g, '~', discovered ? RENDER_COLOR_CYAN : RENDER_COLOR_DARK_GRAY);

    switch(river_tier)
    {
        case WORLD_MAP_RIVER_MAJOR:
        case WORLD_MAP_RIVER_MEDIUM:
        case WORLD_MAP_RIVER_LARGE:
        case WORLD_MAP_RIVER_MASSIVE:
        case WORLD_MAP_RIVER_GIGANTIC:
            g.symbol = '=';
            g.color = discovered ? RENDER_COLOR_LIGHT_CYAN : RENDER_COLOR_CYAN;
            break;
        case WORLD_MAP_RIVER_MINOR:
        case WORLD_MAP_RIVER_TINY:
        case WORLD_MAP_RIVER_SMALL:
            g.symbol = '~';
            g.color = discovered ? RENDER_COLOR_CYAN : RENDER_COLOR_DARK_GRAY;
            break;
        case WORLD_MAP_RIVER_NONE:
        default:
            break;
    }

    return g;
}

static RenderedGlyph draw_lake_glyph(int lake_tier, int discovered)
{
    RenderedGlyph g;
    draw_glyph_set_ascii(&g, 'o', discovered ? RENDER_COLOR_CYAN : RENDER_COLOR_DARK_GRAY);

    switch(lake_tier)
    {
        case WORLD_MAP_LAKE_LARGE:
            g.symbol = 'O';
            g.color = discovered ? RENDER_COLOR_LIGHT_CYAN : RENDER_COLOR_CYAN;
            break;
        case WORLD_MAP_LAKE_SMALL:
            g.symbol = 'o';
            g.color = discovered ? RENDER_COLOR_CYAN : RENDER_COLOR_DARK_GRAY;
            break;
        case WORLD_MAP_LAKE_NONE:
        default:
            break;
    }

    return g;
}

static int draw_tile_has_water_feature(const WorldMapTile* tile)
{
    return tile && (tile->river_tier > WORLD_MAP_RIVER_NONE || tile->lake_tier > WORLD_MAP_LAKE_NONE);
}

static int draw_tile_has_major_water(const WorldMapTile* tile)
{
    return tile && (tile->river_tier >= WORLD_MAP_RIVER_MAJOR || tile->lake_tier >= WORLD_MAP_LAKE_LARGE);
}

static RenderedGlyph draw_tile_water_glyph(const WorldMapTile* tile, int discovered)
{
    if(tile->lake_tier > WORLD_MAP_LAKE_NONE)
        return draw_lake_glyph(tile->lake_tier, discovered);
    return draw_river_glyph(tile->river_tier, discovered);
}

int draw_world_map_tile_in_vision(int wx, int wy, int cursor_x, int cursor_y, int vision_range)
{
    int dx;
    int dy;

    if(vision_range < 0)
        return 0;

    dx = wx - cursor_x;
    dy = wy - cursor_y;
    return (dx * dx) + (dy * dy) <= (vision_range * vision_range);
}

static RenderedGlyph draw_resolve_world_map_glyph(int wx,
                                                  int wy,
                                                  int player_x,
                                                  int player_y,
                                                  int inspect_x,
                                                  int inspect_y,
                                                  int vision_range)
{
    RenderedGlyph glyph;
    WorldMapTile* tile;
    int in_vision;

    draw_glyph_set_ascii(&glyph, ' ', RENDER_COLOR_DEFAULT);

    if(wx < 0 || wx >= WORLD_MAP_WIDTH || wy < 0 || wy >= WORLD_MAP_HEIGHT)
    {
        draw_glyph_set_ascii(&glyph, '~', RENDER_COLOR_DARK_GRAY);
        return glyph;
    }

    if(wx == player_x && wy == player_y)
    {
        draw_glyph_set_ascii(&glyph, '@', RENDER_COLOR_LIGHT_YELLOW);
        return glyph;
    }

    if(inspect_x >= 0 && inspect_y >= 0 && wx == inspect_x && wy == inspect_y)
    {
        draw_glyph_set_ascii(&glyph, 'X', RENDER_COLOR_LIGHT_MAGENTA);
        return glyph;
    }

    tile = world_map_get_tile(wx, wy);
    in_vision = draw_world_map_tile_in_vision(wx, wy, player_x, player_y, vision_range);

    if(!tile || (!tile->discovered && !in_vision))
        return glyph;

    if(tile->zone_index >= 0 && tile->zone_index < MAX_AREAS)
    {
        LocationKnowledge knowledge = atlas_get_knowledge(tile->zone_index);
        char marker = atlas[tile->zone_index].name[0];
        if(marker == '\0')
            marker = 'O';

        if(knowledge <= LOCATION_KNOWLEDGE_UNAWARE)
        {
            if(tile->discovered)
            {
                return draw_biome_glyph(tile->biome, 1);
            }
            return draw_biome_glyph(tile->biome, 0);
        }

        if(!atlas_is_scouted(tile->zone_index) && !atlas_is_visited(tile->zone_index))
            marker = '?';

        glyph.symbol = marker;
        if(tile->discovered)
            glyph.color = tile->visited ? RENDER_COLOR_LIGHT_CYAN : (marker == '?' ? RENDER_COLOR_LIGHT_GRAY : RENDER_COLOR_LIGHT_GREEN);
        else
            glyph.color = RENDER_COLOR_LIGHT_GRAY;
        return glyph;
    }

    if(tile->discovered)
    {
        if(tile->road_tier > WORLD_MAP_ROAD_TIER_NONE)
        {
            if(draw_tile_has_water_feature(tile))
            {
                if(tile->road_tier >= WORLD_MAP_ROAD_TIER_PAVED)
                    return draw_road_glyph(tile->road_tier, 1);
                if(tile->road_tier == WORLD_MAP_ROAD_TIER_TRAIL && draw_tile_has_major_water(tile))
                    return draw_tile_water_glyph(tile, 1);
                return draw_road_glyph(tile->road_tier, 1);
            }
            return draw_road_glyph(tile->road_tier, 1);
        }

        if(draw_tile_has_water_feature(tile))
            return draw_tile_water_glyph(tile, 1);
        return draw_biome_glyph(tile->biome, 1);
    }
    else
    {
        if(tile->road_tier > WORLD_MAP_ROAD_TIER_NONE && in_vision)
        {
            if(draw_tile_has_water_feature(tile))
            {
                if(tile->road_tier >= WORLD_MAP_ROAD_TIER_PAVED)
                    return draw_road_glyph(tile->road_tier, 0);
                if(tile->road_tier == WORLD_MAP_ROAD_TIER_TRAIL && draw_tile_has_major_water(tile))
                    return draw_tile_water_glyph(tile, 0);
                return draw_road_glyph(tile->road_tier, 0);
            }
            return draw_road_glyph(tile->road_tier, 0);
        }

        if(draw_tile_has_water_feature(tile) && in_vision)
            return draw_tile_water_glyph(tile, 0);
        return draw_biome_glyph(tile->biome, 0);
    }
    return glyph;
}

void draw_world_map_viewport(int camera_center_x,
                             int camera_center_y,
                             Player* p,
                             int player_x,
                             int player_y,
                             int inspect_x,
                             int inspect_y,
                             int vision_range)
{
    LayoutState layout;
    LayoutConfig config;
    int viewport_inner_width;
    int viewport_inner_height;
    int camera_x;
    int camera_y;
    int text_width;
    char location_text[128];

    draw_ensure_console_dimensions();
    draw_enable_color_output();
    draw_refresh_layout_signature();

    layout_get_default(&layout);
    layout_get_config(&config);
    
    viewport_inner_width = config.viewport_width;
    viewport_inner_height = config.viewport_height;

    if(viewport_inner_width > WORLD_MAP_WIDTH) viewport_inner_width = WORLD_MAP_WIDTH;
    if(viewport_inner_height > WORLD_MAP_HEIGHT) viewport_inner_height = WORLD_MAP_HEIGHT;
    if(viewport_inner_width < 1 || viewport_inner_height < 1) return;

    camera_x = camera_center_x - viewport_inner_width / 2;
    camera_y = camera_center_y - viewport_inner_height / 2;
    if(camera_x < 0) camera_x = 0;
    if(camera_y < 0) camera_y = 0;
    if(camera_x + viewport_inner_width > WORLD_MAP_WIDTH) camera_x = WORLD_MAP_WIDTH - viewport_inner_width;
    if(camera_y + viewport_inner_height > WORLD_MAP_HEIGHT) camera_y = WORLD_MAP_HEIGHT - viewport_inner_height;

    move_cursor(layout.viewport.row, layout.viewport.col);
    putchar('+');
    for(int i = 0; i < viewport_inner_width; i++) putchar('-');
    putchar('+');

    for(int vy = 0; vy < viewport_inner_height; vy++)
    {
        move_cursor(layout.viewport.row + 1 + vy, layout.viewport.col);
        putchar('|');
        for(int vx = 0; vx < viewport_inner_width; vx++)
        {
            RenderedGlyph glyph = draw_resolve_world_map_glyph(camera_x + vx,
                                                                camera_y + vy,
                                                                player_x,
                                                                player_y,
                                                                inspect_x,
                                                                inspect_y,
                                                                vision_range);
            draw_put_glyph(glyph.symbol, glyph.color);
        }
        draw_put_glyph_flush_color();
        putchar('|');
    }

    move_cursor(layout.viewport.row + viewport_inner_height + 1, layout.viewport.col);
    putchar('+');
    for(int i = 0; i < viewport_inner_width; i++) putchar('-');
    putchar('+');

    text_width = layout_box_text_width(&layout.location);
    if(text_width < 1)
        text_width = 1;

    snprintf(location_text, sizeof(location_text), "Zone [World]  Overland Map (%d,%d)", camera_center_x, camera_center_y);

    move_cursor(layout.location.row, layout.location.col);
    putchar('+');
    for(int i = 0; i < layout.location.inner_width; i++) putchar('-');
    putchar('+');

    move_cursor(layout.location.row + 1, layout.location.col);
    printf("| %-*.*s |", text_width, text_width, location_text);

    move_cursor(layout.location.row + 2, layout.location.col);
    putchar('+');
    for(int i = 0; i < layout.location.inner_width; i++) putchar('-');
    putchar('+');

    (void)p;

    draw_invalidate_viewport_contents();
}

static void draw_active_viewport(Player* p)
{
    if(!p)
        return;

    if(active_viewport_tab == VIEWPORT_TAB_WORLD)
    {
        int world_x;
        int world_y;
        int vision_range = actor_overworld_vision_range(&p->character.actor);

        draw_world_map_focus_position(&world_x, &world_y);
        draw_world_map_viewport(world_x, world_y, p, world_x, world_y, -1, -1, vision_range);
        return;
    }

    draw_update_lock_state(p);
    draw_viewport(p);
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
    draw_active_viewport(p);
    draw_coords_zone(p);
    draw_coords_hint_zone();
    if(active_viewport_tab == VIEWPORT_TAB_ZONE)
        draw_location_zone();
    draw_hud_zone(p);
    draw_log_zone();
    draw_bottom_hotkeys_zone();
    fflush(stdout);
}

// Render just the viewport using incremental cache updates for transient effects.
void draw_world_viewport_only(Player* p)
{
    if(!current_area || !p)
        return;

    draw_active_viewport(p);
    fflush(stdout);
}

