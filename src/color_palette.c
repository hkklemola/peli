#include "color_palette.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render_color.h"

static ColorPaletteMode g_palette_mode = COLOR_PALETTE_MODE_16;

static int contains_needle_ignore_case(const char* haystack, const char* needle)
{
    size_t needle_len;

    if(!haystack || !needle)
        return 0;

    needle_len = strlen(needle);
    if(needle_len == 0)
        return 1;

    for(const char* h = haystack; *h; h++)
    {
        size_t i = 0;
        while(i < needle_len && h[i] &&
              tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i]))
        {
            i++;
        }

        if(i == needle_len)
            return 1;
    }

    return 0;
}

ColorPaletteMode color_palette_detect_mode(void)
{
    const char* force_mode = getenv("PELI_COLOR_MODE");
    const char* term = getenv("TERM");
    const char* colorterm = getenv("COLORTERM");

    if(force_mode)
    {
        if(contains_needle_ignore_case(force_mode, "256"))
            return COLOR_PALETTE_MODE_256;
        if(contains_needle_ignore_case(force_mode, "16"))
            return COLOR_PALETTE_MODE_16;
    }

    if(term && contains_needle_ignore_case(term, "256color"))
        return COLOR_PALETTE_MODE_256;

    if(colorterm &&
       (contains_needle_ignore_case(colorterm, "truecolor") ||
        contains_needle_ignore_case(colorterm, "24bit") ||
        contains_needle_ignore_case(colorterm, "256")))
    {
        return COLOR_PALETTE_MODE_256;
    }

    return COLOR_PALETTE_MODE_16;
}

void color_palette_set_mode(ColorPaletteMode mode)
{
    g_palette_mode = mode;
}

ColorPaletteMode color_palette_get_mode(void)
{
    return g_palette_mode;
}

int color_palette_is_ansi_code(int color)
{
    return ((color >= 30 && color <= 37) ||
            color == 39 ||
            (color >= 90 && color <= 97));
}

static int fallback_256_to_ansi16(int color_index)
{
    static const int low_ansi[16] = {
        RENDER_COLOR_BLACK,         // 0
        RENDER_COLOR_RED,           // 1
        RENDER_COLOR_GREEN,         // 2
        RENDER_COLOR_BROWN,         // 3
        RENDER_COLOR_BLUE,          // 4
        RENDER_COLOR_MAGENTA,       // 5
        RENDER_COLOR_CYAN,          // 6
        RENDER_COLOR_LIGHT_GRAY,    // 7
        RENDER_COLOR_DARK_GRAY,     // 8
        RENDER_COLOR_LIGHT_RED,     // 9
        RENDER_COLOR_LIGHT_GREEN,   // 10
        RENDER_COLOR_LIGHT_YELLOW,  // 11
        RENDER_COLOR_LIGHT_BLUE,    // 12
        RENDER_COLOR_LIGHT_MAGENTA, // 13
        RENDER_COLOR_LIGHT_CYAN,    // 14
        RENDER_COLOR_WHITE          // 15
    };

    if(color_index < 0 || color_index > 255)
        return RENDER_COLOR_DEFAULT;

    if(color_index < 16)
        return low_ansi[color_index];

    if(color_index >= 232)
    {
        if(color_index < 238) return RENDER_COLOR_BLACK;
        if(color_index < 244) return RENDER_COLOR_DARK_GRAY;
        if(color_index < 250) return RENDER_COLOR_LIGHT_GRAY;
        return RENDER_COLOR_WHITE;
    }

    {
        int cube = color_index - 16;
        int r = cube / 36;
        int g = (cube % 36) / 6;
        int b = cube % 6;

        if(r >= g && r >= b)
        {
            if(g >= 4 && b <= 2) return RENDER_COLOR_BROWN;
            return (r >= 4) ? RENDER_COLOR_LIGHT_RED : RENDER_COLOR_RED;
        }

        if(g >= r && g >= b)
        {
            if(r >= 4) return RENDER_COLOR_LIGHT_YELLOW;
            return (g >= 4) ? RENDER_COLOR_LIGHT_GREEN : RENDER_COLOR_GREEN;
        }

        return (b >= 4) ? RENDER_COLOR_LIGHT_BLUE : RENDER_COLOR_BLUE;
    }
}

int color_palette_make_fg_escape(int color, char* buffer, size_t buffer_size)
{
    int n;

    if(!buffer || buffer_size == 0)
        return 0;

    if(color == RENDER_COLOR_DEFAULT)
    {
        n = snprintf(buffer, buffer_size, "\x1b[39m");
        return n > 0 && (size_t)n < buffer_size;
    }

    if(color_palette_is_ansi_code(color))
    {
        n = snprintf(buffer, buffer_size, "\x1b[%dm", color);
        return n > 0 && (size_t)n < buffer_size;
    }

    if(color >= 0 && color <= 255)
    {
        if(color_palette_get_mode() == COLOR_PALETTE_MODE_256)
            n = snprintf(buffer, buffer_size, "\x1b[38;5;%dm", color);
        else
            n = snprintf(buffer, buffer_size, "\x1b[%dm", fallback_256_to_ansi16(color));

        return n > 0 && (size_t)n < buffer_size;
    }

    n = snprintf(buffer, buffer_size, "\x1b[39m");
    return n > 0 && (size_t)n < buffer_size;
}