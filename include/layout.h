#ifndef LAYOUT_H
#define LAYOUT_H

#include "map.h"

/*
 * Purpose:
 *   Declares layout geometry used by viewport, HUD, log, overlays, and startup UI.
 *
 * Functions:
 *   - layout_get_default_config/layout_set_config/layout_get_config: runtime panel sizing.
 *   - layout_clamp_*: clamp user-provided viewport/panel values to safe bounds.
 *   - layout_get_default: builds the default layout state.
 *   - layout_box_* helpers: compute derived box dimensions and row positions.
 */

#define LAYOUT_VIEWPORT_WIDTH_MIN 40
#define LAYOUT_VIEWPORT_WIDTH_MAX MAP_WIDTH
#define LAYOUT_VIEWPORT_WIDTH_DEFAULT VIEW_WIDTH

#define LAYOUT_VIEWPORT_HEIGHT_MIN 8
#define LAYOUT_VIEWPORT_HEIGHT_MAX MAP_HEIGHT
#define LAYOUT_VIEWPORT_HEIGHT_DEFAULT VIEW_HEIGHT

#define LAYOUT_HUD_HEIGHT_MIN 8
#define LAYOUT_HUD_HEIGHT_MAX 24
#define LAYOUT_HUD_HEIGHT_DEFAULT 12

#define LAYOUT_LOG_HEIGHT_MIN 6
#define LAYOUT_LOG_HEIGHT_MAX 48
#define LAYOUT_LOG_HEIGHT_DEFAULT 12

typedef struct LayoutBox
{
    int row;
    int col;
    int inner_width;
    int height;
} LayoutBox;

typedef struct LayoutState
{
    LayoutBox location;
    LayoutBox viewport;
    LayoutBox coords_hint;
    LayoutBox hud;
    LayoutBox log;
    LayoutBox bottom_hotkeys;
    LayoutBox overlay;
    LayoutBox startup;
    int min_console_cols;
    int min_console_rows;
} LayoutState;

typedef struct LayoutConfig
{
    int viewport_width;
    int viewport_height;
    int hud_height;
    int log_height;
} LayoutConfig;

// Fill config with built-in default viewport and panel dimensions.
void layout_get_default_config(LayoutConfig* out);

// Clamp one viewport width value to supported bounds.
int layout_clamp_viewport_width(int viewport_width);

// Clamp one viewport height value to supported bounds.
int layout_clamp_viewport_height(int viewport_height);

// Clamp one HUD panel height value to supported bounds.
int layout_clamp_hud_height(int hud_height);

// Clamp one log panel height value to supported bounds.
int layout_clamp_log_height(int log_height);

// Set active runtime layout config (NULL resets to defaults).
void layout_set_config(const LayoutConfig* config);

// Read active runtime layout config.
void layout_get_config(LayoutConfig* out);

// Build the default layout state for current map/view constants.
void layout_get_default(LayoutState* out);

// Return box width including vertical borders.
int layout_box_total_width(const LayoutBox* box);

// Return printable text width inside borders.
int layout_box_text_width(const LayoutBox* box);

// Return first content row index inside frame borders/title row.
int layout_box_content_start_row(const LayoutBox* box);

// Return number of content rows available inside frame borders.
int layout_box_content_lines(const LayoutBox* box);

// Return bottom row occupied by this box.
int layout_box_bottom_row(const LayoutBox* box);

#endif