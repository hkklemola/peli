#include "layout.h"

#include "atlas.h"
#include "hud.h"

/*
 * Purpose:
 *   Implements default UI geometry and layout-derived dimension helpers.
 *
 * Functions:
 *   - int_max: local helper for integer max calculations.
 *   - layout_*config helpers: runtime config defaults, clamping, and access.
 *   - layout_box_*: derive dimensions from LayoutBox values.
 *   - layout_get_default: builds canonical viewport/HUD/log/overlay layout.
 */

#define STARTUP_BOX_ROW 2
#define STARTUP_BOX_HEIGHT 22
#define LOCATION_BOX_HEIGHT 3

static LayoutConfig active_layout_config = {
    LAYOUT_VIEWPORT_WIDTH_DEFAULT,
    LAYOUT_VIEWPORT_HEIGHT_DEFAULT,
    LAYOUT_HUD_HEIGHT_DEFAULT,
    LAYOUT_LOG_HEIGHT_DEFAULT
};
static int layout_config_initialized = 0;

// Return greater of two integers.
static int int_max(int a, int b)
{
    return (a > b) ? a : b;
}

// Ensure runtime layout config has a valid default value.
static void layout_ensure_config_initialized(void)
{
    if(layout_config_initialized)
        return;

    active_layout_config.viewport_width = LAYOUT_VIEWPORT_WIDTH_DEFAULT;
    active_layout_config.viewport_height = LAYOUT_VIEWPORT_HEIGHT_DEFAULT;
    active_layout_config.hud_height = LAYOUT_HUD_HEIGHT_DEFAULT;
    active_layout_config.log_height = LAYOUT_LOG_HEIGHT_DEFAULT;
    layout_config_initialized = 1;
}

// Fill config with built-in default viewport and panel sizes.
void layout_get_default_config(LayoutConfig* out)
{
    if(!out) return;
    out->viewport_width = LAYOUT_VIEWPORT_WIDTH_DEFAULT;
    out->viewport_height = LAYOUT_VIEWPORT_HEIGHT_DEFAULT;
    out->hud_height = LAYOUT_HUD_HEIGHT_DEFAULT;
    out->log_height = LAYOUT_LOG_HEIGHT_DEFAULT;
}

// Clamp one viewport width to safe bounds.
int layout_clamp_viewport_width(int viewport_width)
{
    if(viewport_width < LAYOUT_VIEWPORT_WIDTH_MIN) return LAYOUT_VIEWPORT_WIDTH_MIN;
    if(viewport_width > LAYOUT_VIEWPORT_WIDTH_MAX) return LAYOUT_VIEWPORT_WIDTH_MAX;
    return viewport_width;
}

// Clamp one viewport height to safe bounds.
int layout_clamp_viewport_height(int viewport_height)
{
    if(viewport_height < LAYOUT_VIEWPORT_HEIGHT_MIN) return LAYOUT_VIEWPORT_HEIGHT_MIN;
    if(viewport_height > LAYOUT_VIEWPORT_HEIGHT_MAX) return LAYOUT_VIEWPORT_HEIGHT_MAX;
    return viewport_height;
}

// Clamp one HUD panel height to safe bounds.
int layout_clamp_hud_height(int hud_height)
{
    if(hud_height < LAYOUT_HUD_HEIGHT_MIN) return LAYOUT_HUD_HEIGHT_MIN;
    if(hud_height > LAYOUT_HUD_HEIGHT_MAX) return LAYOUT_HUD_HEIGHT_MAX;
    return hud_height;
}

// Clamp one log panel height to safe bounds.
int layout_clamp_log_height(int log_height)
{
    if(log_height < LAYOUT_LOG_HEIGHT_MIN) return LAYOUT_LOG_HEIGHT_MIN;
    if(log_height > LAYOUT_LOG_HEIGHT_MAX) return LAYOUT_LOG_HEIGHT_MAX;
    return log_height;
}

// Set active runtime config, or reset to defaults when NULL.
void layout_set_config(const LayoutConfig* config)
{
    layout_ensure_config_initialized();

    if(!config)
    {
        layout_get_default_config(&active_layout_config);
        return;
    }

    active_layout_config.viewport_width = layout_clamp_viewport_width(config->viewport_width);
    active_layout_config.viewport_height = layout_clamp_viewport_height(config->viewport_height);
    active_layout_config.hud_height = layout_clamp_hud_height(config->hud_height);
    active_layout_config.log_height = layout_clamp_log_height(config->log_height);
}

// Return current active runtime config.
void layout_get_config(LayoutConfig* out)
{
    if(!out) return;
    layout_ensure_config_initialized();
    *out = active_layout_config;
}

// Return box width including side borders.
int layout_box_total_width(const LayoutBox* box)
{
    if(!box) return 0;
    return box->inner_width + 2;
}

// Return printable text width inside side borders.
int layout_box_text_width(const LayoutBox* box)
{
    if(!box) return 0;
    return box->inner_width - 2;
}

// Return first content row in framed box.
int layout_box_content_start_row(const LayoutBox* box)
{
    if(!box) return 0;
    return box->row + 3;
}

// Return number of content rows available in framed box.
int layout_box_content_lines(const LayoutBox* box)
{
    if(!box) return 0;
    return box->height - 4;
}

// Return final occupied row of framed box.
int layout_box_bottom_row(const LayoutBox* box)
{
    if(!box) return 0;
    return box->row + box->height - 1;
}

// Build default layout geometry for all screen regions.
void layout_get_default(LayoutState* out)
{
    LayoutConfig config;
    int viewport_width;
    int viewport_height;
    int configured_hud_height;
    int configured_log_height;
    int effective_hud_height;
    int effective_log_height;
    int preferred_hud_height;
    int reclaimed_rows;
    int overflow_rows;
    int coords_row;
    int coords_hint_row;

    if(!out) return;
    layout_get_config(&config);

    viewport_width = config.viewport_width;
    viewport_height = config.viewport_height;
    configured_hud_height = config.hud_height;
    configured_log_height = config.log_height;

    preferred_hud_height = hud_preferred_height();
    preferred_hud_height = layout_clamp_hud_height(preferred_hud_height);

    effective_hud_height = configured_hud_height;
    if(preferred_hud_height < effective_hud_height)
        effective_hud_height = preferred_hud_height;

    reclaimed_rows = configured_hud_height - effective_hud_height;
    effective_log_height = configured_log_height + reclaimed_rows;
    if(effective_log_height > LAYOUT_LOG_HEIGHT_MAX)
    {
        overflow_rows = effective_log_height - LAYOUT_LOG_HEIGHT_MAX;
        effective_log_height = LAYOUT_LOG_HEIGHT_MAX;
        effective_hud_height += overflow_rows;
    }

    effective_hud_height = layout_clamp_hud_height(effective_hud_height);
    effective_log_height = layout_clamp_log_height(effective_log_height);

    if(current_area)
    {
        if(viewport_width > current_area->width)
            viewport_width = current_area->width;
        if(viewport_height > current_area->height)
            viewport_height = current_area->height;
    }

    out->location.row = 1;
    out->location.col = 1;
    out->location.inner_width = viewport_width;
    out->location.height = LOCATION_BOX_HEIGHT;

    out->viewport.row = out->location.row + out->location.height + 1;
    out->viewport.col = out->location.col;
    out->viewport.inner_width = viewport_width;
    out->viewport.height = viewport_height + 2;

    coords_row = out->viewport.row + out->viewport.height;
    coords_hint_row = coords_row + 1;

    out->coords_hint.row = coords_hint_row;
    out->coords_hint.col = out->viewport.col;
    out->coords_hint.inner_width = viewport_width;
    out->coords_hint.height = 1;

    out->hud.row = coords_hint_row + 1;
    out->hud.col = out->viewport.col;
    out->hud.inner_width = viewport_width;
    out->hud.height = effective_hud_height;

    out->log.row = out->hud.row + out->hud.height + 1;
    out->log.col = out->viewport.col;
    out->log.inner_width = viewport_width;
    out->log.height = effective_log_height;

    out->bottom_hotkeys.row = out->log.row + out->log.height + 1;
    out->bottom_hotkeys.col = out->log.col;
    out->bottom_hotkeys.inner_width = out->log.inner_width;
    out->bottom_hotkeys.height = 1;

    out->overlay.row = out->hud.row;
    out->overlay.col = out->hud.col;
    out->overlay.inner_width = out->hud.inner_width;
    out->overlay.height = out->hud.height + 1 + out->log.height;

    out->startup.row = STARTUP_BOX_ROW;
    out->startup.col = out->viewport.col;
    out->startup.inner_width = config.viewport_width;
    out->startup.height = STARTUP_BOX_HEIGHT;

    out->min_console_cols = int_max(layout_box_total_width(&out->location), layout_box_total_width(&out->viewport));
    out->min_console_cols = int_max(out->min_console_cols, layout_box_total_width(&out->overlay));
    out->min_console_cols = int_max(out->min_console_cols, layout_box_total_width(&out->startup));

    out->min_console_rows = int_max(layout_box_bottom_row(&out->location), layout_box_bottom_row(&out->overlay));
    out->min_console_rows = int_max(out->min_console_rows, layout_box_bottom_row(&out->startup));
    out->min_console_rows = int_max(out->min_console_rows, coords_row);
    out->min_console_rows = int_max(out->min_console_rows, layout_box_bottom_row(&out->coords_hint));
    out->min_console_rows = int_max(out->min_console_rows, layout_box_bottom_row(&out->bottom_hotkeys));
}