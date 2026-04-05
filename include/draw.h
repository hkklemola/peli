#ifndef DRAW_H
#define DRAW_H

#include "player.h"

/*
 * Purpose:
 *   Declares top-level rendering entry points for world and UI.
 *
 * Functions:
 *   - draw_ensure_console_dimensions: resizes console when required.
 *   - draw_world: renders the viewport and UI panels for current frame.
 */

// Ensure the console window is large enough for the active layout.
// Returns 1 if the console was resized, 0 otherwise.
int draw_ensure_console_dimensions(void);

// Render world viewport, HUD, and message log for one frame.
void draw_world(Player* p);

// Render only the viewport layer using incremental cache updates.
// Intended for transient visual effects/animations that should not redraw HUD or log panels.
void draw_world_viewport_only(Player* p);

// Mark a world-space rectangle as dirty so the next incremental viewport draw can limit work to it.
void draw_mark_world_rect_dirty(int x0, int y0, int x1, int y1);

// Render overland world map with separate camera center, player marker, and optional target cursor.
void draw_world_map_viewport(int camera_x,
							 int camera_y,
							 Player* p,
							 int player_x,
							 int player_y,
							 int vision_range,
							 int target_active,
							 int target_x,
							 int target_y);

// Check overland tile is within vision range from cursor.
int draw_world_map_tile_in_vision(int wx, int wy, int cursor_x, int cursor_y, int vision_range);

// Set a temporary cursor position for inspect mode.
void draw_set_inspect_cursor(int x, int y);

// Clear inspect cursor overlay.
void draw_clear_inspect_cursor(void);

// Force a full world redraw on next draw_world call.
void draw_force_full_redraw(void);

// Invalidate viewport cache and force full redraw.
void draw_invalidate_viewport_cache(void);

// Return currently active viewed floor (or player floor when following).
int draw_get_view_layer(const Player* p);

// Set explicit viewed floor for zone rendering.
void draw_set_view_layer(int layer);

// Nudge viewed floor by delta (+1/-1).
void draw_nudge_view_layer(int delta, const Player* p);

// Return rendering view to player's current floor.
void draw_reset_view_layer_to_player(void);

#endif

