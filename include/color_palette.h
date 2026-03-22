#ifndef COLOR_PALETTE_H
#define COLOR_PALETTE_H

#include <stddef.h>

typedef enum ColorPaletteMode {
    COLOR_PALETTE_MODE_16 = 0,
    COLOR_PALETTE_MODE_256 = 1
} ColorPaletteMode;

// Detect the preferred palette mode from terminal environment variables.
ColorPaletteMode color_palette_detect_mode(void);

// Override or set the active palette mode used by ANSI formatting.
void color_palette_set_mode(ColorPaletteMode mode);

// Read back the active palette mode.
ColorPaletteMode color_palette_get_mode(void);

// Return 1 when color is a legacy ANSI code (30-37, 39, 90-97), 0 otherwise.
int color_palette_is_ansi_code(int color);

// Build a foreground ANSI escape sequence into buffer. Returns 1 on success.
int color_palette_make_fg_escape(int color, char* buffer, size_t buffer_size);

#endif