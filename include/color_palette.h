#ifndef COLOR_PALETTE_H
#define COLOR_PALETTE_H

#include <stddef.h>

typedef enum ColorPaletteMode {
    COLOR_PALETTE_MODE_16 = 0,
    COLOR_PALETTE_MODE_256 = 1
} ColorPaletteMode;

typedef enum ColorPaletteGroup {
    COLOR_PALETTE_GROUP_GRAY_AND_BLACK = 0,
    COLOR_PALETTE_GROUP_RED,
    COLOR_PALETTE_GROUP_GREEN,
    COLOR_PALETTE_GROUP_YELLOW,
    COLOR_PALETTE_GROUP_BLUE,
    COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA,
    COLOR_PALETTE_GROUP_CYAN,
    COLOR_PALETTE_GROUP_PINK,
    COLOR_PALETTE_GROUP_WHITE,
    COLOR_PALETTE_GROUP_BROWN,
    COLOR_PALETTE_GROUP_ORANGE,
    COLOR_PALETTE_GROUP_COUNT
} ColorPaletteGroup;

typedef struct ColorPaletteEntry {
    int index;
    const char* name;
    ColorPaletteGroup group;
    const char* hex;
} ColorPaletteEntry;

// Detect the preferred palette mode from terminal environment variables.
ColorPaletteMode color_palette_detect_mode(void);

// Lookup color metadata for a 0-255 palette index.
const ColorPaletteEntry* color_palette_entry(int color);

// Parse a color value from a string like "RED", "LIGHT_BLUE", "256:160", or "123".
int color_palette_parse_color(const char* value, int* out);

// Override or set the active palette mode used by ANSI formatting.
void color_palette_set_mode(ColorPaletteMode mode);

// Read back the active palette mode.
ColorPaletteMode color_palette_get_mode(void);

// Return 1 when color is a legacy ANSI code (30-37, 39, 90-97), 0 otherwise.
int color_palette_is_ansi_code(int color);

// Build a foreground ANSI escape sequence into buffer. Returns 1 on success.
int color_palette_make_fg_escape(int color, char* buffer, size_t buffer_size);

#endif