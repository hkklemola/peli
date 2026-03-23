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
void draw_ensure_console_dimensions(void);

// Render world viewport, HUD, and message log for one frame.
void draw_world(Player* p);

// Render overland world map with separate camera center, player marker, and optional target cursor.
void draw_world_map_viewport(int camera_x,
							 int camera_y,
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

#endif

