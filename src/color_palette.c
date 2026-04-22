#include "color_palette.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render_color.h"

#define COLOR_PALETTE_INDEX_BASE 256
#define COLOR_PALETTE_INDEX_MAX  (COLOR_PALETTE_INDEX_BASE + 255)

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
        if(contains_needle_ignore_case(force_mode, "truecolor") ||
           contains_needle_ignore_case(force_mode, "24bit"))
        {
            return COLOR_PALETTE_MODE_TRUECOLOR;
        }

        if(contains_needle_ignore_case(force_mode, "256"))
            return COLOR_PALETTE_MODE_256;
        if(contains_needle_ignore_case(force_mode, "16"))
            return COLOR_PALETTE_MODE_16;
    }

    if(term && contains_needle_ignore_case(term, "truecolor"))
        return COLOR_PALETTE_MODE_TRUECOLOR;

    if(term && contains_needle_ignore_case(term, "256color"))
        return COLOR_PALETTE_MODE_256;

    if(colorterm)
    {
        if(contains_needle_ignore_case(colorterm, "truecolor") ||
           contains_needle_ignore_case(colorterm, "24bit"))
        {
            return COLOR_PALETTE_MODE_TRUECOLOR;
        }

        if(contains_needle_ignore_case(colorterm, "256"))
            return COLOR_PALETTE_MODE_256;
    }

    return COLOR_PALETTE_MODE_16;
}

static int color_palette_env_supports_truecolor(void)
{
    const char* term = getenv("TERM");
    const char* colorterm = getenv("COLORTERM");
#ifdef _WIN32
    const char* term_program = getenv("TERM_PROGRAM");
    const char* wt_session = getenv("WT_SESSION");
#endif

    if(term && contains_needle_ignore_case(term, "truecolor"))
        return 1;
    if(colorterm && (contains_needle_ignore_case(colorterm, "truecolor") ||
                     contains_needle_ignore_case(colorterm, "24bit")))
        return 1;
#ifdef _WIN32
    if(term_program && contains_needle_ignore_case(term_program, "windows terminal"))
        return 1;
    if(wt_session && wt_session[0] != '\0')
        return 1;
#endif
    return 0;
}

void color_palette_set_mode(ColorPaletteMode mode)
{
    g_palette_mode = mode;
}

ColorPaletteMode color_palette_get_mode(void)
{
    return g_palette_mode;
}

static int starts_with_ignore_case(const char* text, const char* prefix)
{
    if(!text || !prefix)
        return 0;

    while(*prefix)
    {
        if(tolower((unsigned char)*text) != tolower((unsigned char)*prefix))
            return 0;
        text++;
        prefix++;
    }

    return 1;
}

static int equals_ignore_case(const char* left, const char* right)
{
    if(!left || !right)
        return 0;

    while(*left && *right)
    {
        if(tolower((unsigned char)*left) != tolower((unsigned char)*right))
            return 0;
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static int color_palette_parse_integer(const char* value, int* out)
{
    char* end = NULL;
    long parsed;

    if(!value || !out)
        return 0;

    parsed = strtol(value, &end, 10);
    if(end == value)
        return 0;

    while(end && *end && isspace((unsigned char)*end))
        end++;

    if(end && *end != '\0')
        return 0;

    if(parsed < 0 || parsed > 255)
        return 0;

    *out = (int)parsed;
    return 1;
}

static int color_palette_parse_hex_color(const char* hex, int* r, int* g, int* b)
{
    if(!hex || !r || !g || !b)
        return 0;

    if(hex[0] != '#' || strlen(hex) != 7)
        return 0;

    char component[3] = { 0 };
    char* end = NULL;
    unsigned long parsed;

    component[0] = hex[1];
    component[1] = hex[2];
    parsed = strtoul(component, &end, 16);
    if(end != component + 2)
        return 0;
    *r = (int)parsed;

    component[0] = hex[3];
    component[1] = hex[4];
    parsed = strtoul(component, &end, 16);
    if(end != component + 2)
        return 0;
    *g = (int)parsed;

    component[0] = hex[5];
    component[1] = hex[6];
    parsed = strtoul(component, &end, 16);
    if(end != component + 2)
        return 0;
    *b = (int)parsed;

    return 1;
}

static const ColorPaletteEntry g_color_palette_entries[256] = {
    { 0, "#000000", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#000000" },
    { 1, "MAROON", COLOR_PALETTE_GROUP_RED, "#800000" },
    { 2, "OFFICE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#008000" },
    { 3, "YELLOW_003", COLOR_PALETTE_GROUP_GREEN, "#808000" },
    { 4, "BLUE_004", COLOR_PALETTE_GROUP_BLUE, "#000080" },
    { 5, "PATRIARCH", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#800080" },
    { 6, "CYAN_006", COLOR_PALETTE_GROUP_CYAN, "#008080" },
    { 7, "ARGENT", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#c0c0c0" },
    { 8, "GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#808080" },
    { 9, "LIGHT_RED", COLOR_PALETTE_GROUP_RED, "#ff0000" },
    { 10, "LIGHT_GREEN_010", COLOR_PALETTE_GROUP_GREEN, "#00ff00" },
    { 11, "LIGHT_YELLOW_011", COLOR_PALETTE_GROUP_YELLOW, "#ffff00" },
    { 12, "LIGHT_BLUE_012", COLOR_PALETTE_GROUP_BLUE, "#0000ff" },
    { 13, "LIGHT_MAGENTA_013", COLOR_PALETTE_GROUP_PINK, "#ff00ff" },
    { 14, "LIGHT_CYAN_014", COLOR_PALETTE_GROUP_CYAN, "#00ffff" },
    { 15, "LIGHT_WHITE", COLOR_PALETTE_GROUP_WHITE, "#ffffff" },
    { 16, "BLACK_000", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#000000" },
    { 17, "VERY_DARK_BLUE", COLOR_PALETTE_GROUP_BLUE, "#00005f" },
    { 18, "DARK_BLUE", COLOR_PALETTE_GROUP_BLUE, "#000087" },
    { 19, "DUKE_BLUE", COLOR_PALETTE_GROUP_BLUE, "#0000af" },
    { 20, "MEDIUM_BLUE", COLOR_PALETTE_GROUP_BLUE, "#0000d7" },
    { 21, "LIGHT_BLUE_012", COLOR_PALETTE_GROUP_BLUE, "#0000ff" },
    { 22, "VERY_DARK_LIME_GREEN", COLOR_PALETTE_GROUP_GREEN, "#005f00" },
    { 23, "VERY_DARK_CYAN", COLOR_PALETTE_GROUP_GREEN, "#005f5f" },
    { 24, "SEA_BLUE", COLOR_PALETTE_GROUP_BLUE, "#005f87" },
    { 25, "MEDIUM_PERSIAN_BLUE", COLOR_PALETTE_GROUP_BLUE, "#005faf" },
    { 26, "TRUE_BLUE", COLOR_PALETTE_GROUP_BLUE, "#005fd7" },
    { 27, "BLUE_RIBBON", COLOR_PALETTE_GROUP_BLUE, "#005fff" },
    { 28, "AO", COLOR_PALETTE_GROUP_GREEN, "#008700" },
    { 29, "DEEP_SEA", COLOR_PALETTE_GROUP_GREEN, "#00875f" },
    { 30, "TEAL", COLOR_PALETTE_GROUP_CYAN, "#008787" },
    { 31, "DEEP_CERULEAN", COLOR_PALETTE_GROUP_BLUE, "#0087af" },
    { 32, "STRONG_BLUE", COLOR_PALETTE_GROUP_BLUE, "#0087d7" },
    { 33, "AZURE", COLOR_PALETTE_GROUP_BLUE, "#0087ff" },
    { 34, "DARK_LIME_GREEN", COLOR_PALETTE_GROUP_GREEN, "#00af00" },
    { 35, "JADE", COLOR_PALETTE_GROUP_GREEN, "#00af5f" },
    { 36, "DARK_CYAN", COLOR_PALETTE_GROUP_GREEN, "#00af87" },
    { 37, "TIFFANY_BLUE", COLOR_PALETTE_GROUP_CYAN, "#00afaf" },
    { 38, "CERULEAN", COLOR_PALETTE_GROUP_BLUE, "#00afd7" },
    { 39, "DEEP_SKY_BLUE", COLOR_PALETTE_GROUP_BLUE, "#00afff" },
    { 40, "STRONG_LIME_GREEN", COLOR_PALETTE_GROUP_GREEN, "#00d700" },
    { 41, "MALACHITE", COLOR_PALETTE_GROUP_GREEN, "#00d75f" },
    { 42, "CARIBBEAN_GREEN_042", COLOR_PALETTE_GROUP_GREEN, "#00d787" },
    { 43, "STRONG_CYAN", COLOR_PALETTE_GROUP_GREEN, "#00d7af" },
    { 44, "DARK_TURQUOISE", COLOR_PALETTE_GROUP_CYAN, "#00d7d7" },
    { 45, "VIVID_SKY_BLUE", COLOR_PALETTE_GROUP_BLUE, "#00d7ff" },
    { 46, "LIGHT_GREEN_010", COLOR_PALETTE_GROUP_GREEN, "#00ff00" },
    { 47, "SPRING_GREEN_047", COLOR_PALETTE_GROUP_GREEN, "#00ff5f" },
    { 48, "GUPPIE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#00ff87" },
    { 49, "MEDIUM_SPRING_GREEN", COLOR_PALETTE_GROUP_GREEN, "#00ffaf" },
    { 50, "BRIGHT_TURQUOISE", COLOR_PALETTE_GROUP_GREEN, "#00ffd7" },
    { 51, "LIGHT_CYAN_014", COLOR_PALETTE_GROUP_CYAN, "#00ffff" },
    { 52, "BLOOD_RED", COLOR_PALETTE_GROUP_RED, "#5f0000" },
    { 53, "IMPERIAL_PURPLE", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#5f005f" },
    { 54, "METALLIC_VIOLET", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#5f0087" },
    { 55, "DARK_VIOLET", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#5f00af" },
    { 56, "ELECTRIC_VIOLET_056", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#5f00d7" },
    { 57, "ELECTRIC_INDIGO", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#5f00ff" },
    { 58, "BRONZE_YELLOW", COLOR_PALETTE_GROUP_GREEN, "#5f5f00" },
    { 59, "SCORPION", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#5f5f5f" },
    { 60, "COMET", COLOR_PALETTE_GROUP_BLUE, "#5f5f87" },
    { 61, "DARK_MODERATE_BLUE", COLOR_PALETTE_GROUP_BLUE, "#5f5faf" },
    { 62, "INDIGO", COLOR_PALETTE_GROUP_BLUE, "#5f5fd7" },
    { 63, "CORNFLOWER_BLUE", COLOR_PALETTE_GROUP_BLUE, "#5f5fff" },
    { 64, "AVOCADO", COLOR_PALETTE_GROUP_GREEN, "#5f8700" },
    { 65, "GLADE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#5f875f" },
    { 66, "STEEL_BLUE", COLOR_PALETTE_GROUP_CYAN, "#5f8787" },
    { 67, "DENIM_BLUE", COLOR_PALETTE_GROUP_BLUE, "#5f87af" },
    { 68, "MODERATE_BLUE", COLOR_PALETTE_GROUP_BLUE, "#5f87d7" },
    { 69, "BLUEBERRY", COLOR_PALETTE_GROUP_BLUE, "#5f87ff" },
    { 70, "DARK_GREEN", COLOR_PALETTE_GROUP_GREEN, "#5faf00" },
    { 71, "FOREST_GREEN", COLOR_PALETTE_GROUP_GREEN, "#5faf5f" },
    { 72, "SILVER_TREE", COLOR_PALETTE_GROUP_GREEN, "#5faf87" },
    { 73, "CRYSTAL_BLUE", COLOR_PALETTE_GROUP_BLUE, "#5fafaf" },
    { 74, "AQUA_PEARL", COLOR_PALETTE_GROUP_BLUE, "#5fafd7" },
    { 75, "BLUE_JEANS", COLOR_PALETTE_GROUP_BLUE, "#5fafff" },
    { 76, "STRONG_GREEN", COLOR_PALETTE_GROUP_GREEN, "#5fd700" },
    { 77, "MODERATE_LIME_GREEN", COLOR_PALETTE_GROUP_GREEN, "#5fd75f" },
    { 78, "CARIBBEAN_GREEN_PEARL", COLOR_PALETTE_GROUP_GREEN, "#5fd787" },
    { 79, "EUCALYPTUS", COLOR_PALETTE_GROUP_GREEN, "#5fd7af" },
    { 80, "MODERATE_CYAN", COLOR_PALETTE_GROUP_CYAN, "#5fd7d7" },
    { 81, "MAYA_BLUE", COLOR_PALETTE_GROUP_BLUE, "#5fd7ff" },
    { 82, "BRIGHT_GREEN", COLOR_PALETTE_GROUP_GREEN, "#5fff00" },
    { 83, "LIGHT_LIME_GREEN", COLOR_PALETTE_GROUP_GREEN, "#5fff5f" },
    { 84, "VERY_LIGHT_MALACHITE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#5fff87" },
    { 85, "MEDIUM_AQUAMARINE", COLOR_PALETTE_GROUP_GREEN, "#5fffaf" },
    { 86, "AQUAMARINE_086", COLOR_PALETTE_GROUP_CYAN, "#5fffd7" },
    { 87, "AQUAMARINE_087", COLOR_PALETTE_GROUP_CYAN, "#5fffff" },
    { 88, "DEEP_RED", COLOR_PALETTE_GROUP_RED, "#870000" },
    { 89, "FRENCH_PLUM", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#87005f" },
    { 90, "FRESH_EGGPLANT", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#870087" },
    { 91, "PURPLE", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#8700af" },
    { 92, "STRONG_VIOLET", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#8700d7" },
    { 93, "PURE_VIOLET", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#8700ff" },
    { 94, "BROWN", COLOR_PALETTE_GROUP_BROWN, "#875f00" },
    { 95, "COPPER_ROSE", COLOR_PALETTE_GROUP_BROWN, "#875f5f" },
    { 96, "MOSTLY_DESATURATED_DARK_MAGENTA", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#875f87" },
    { 97, "DARK_MODERATE_VIOLET", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#875faf" },
    { 98, "MODERATE_VIOLET", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#875fd7" },
    { 99, "BLUEBERRY_099", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#875fff" },
    { 100, "OLIVE", COLOR_PALETTE_GROUP_GREEN, "#878700" },
    { 101, "CLAY_CREEK", COLOR_PALETTE_GROUP_GREEN, "#87875f" },
    { 102, "TAUPE_GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#878787" },
    { 103, "SHADOW_BLUE", COLOR_PALETTE_GROUP_BLUE, "#8787af" },
    { 104, "CHETWODE_BLUE", COLOR_PALETTE_GROUP_BLUE, "#8787d7" },
    { 105, "VIOLETS_ARE_BLUE", COLOR_PALETTE_GROUP_BLUE, "#8787ff" },
    { 106, "APPLE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#87af00" },
    { 107, "DARK_MODERATE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#87af5f" },
    { 108, "BAY_LEAF", COLOR_PALETTE_GROUP_GREEN, "#87af87" },
    { 109, "DARK_GRAYISH_CYAN", COLOR_PALETTE_GROUP_CYAN, "#87afaf" },
    { 110, "LIGHT_COBALT_BLUE", COLOR_PALETTE_GROUP_BLUE, "#87afd7" },
    { 111, "FRENCH_SKY_BLUE", COLOR_PALETTE_GROUP_BLUE, "#87afff" },
    { 112, "PISTACHIO", COLOR_PALETTE_GROUP_GREEN, "#87d700" },
    { 113, "MANTIS", COLOR_PALETTE_GROUP_GREEN, "#87d75f" },
    { 114, "PASTEL_GREEN", COLOR_PALETTE_GROUP_GREEN, "#87d787" },
    { 115, "VISTA_BLUE", COLOR_PALETTE_GROUP_CYAN, "#87d7af" },
    { 116, "SLIGHTLY_DESATURATED_CYAN", COLOR_PALETTE_GROUP_CYAN, "#87d7d7" },
    { 117, "PALE_CYAN", COLOR_PALETTE_GROUP_CYAN, "#87d7ff" },
    { 118, "PURE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#87ff00" },
    { 119, "SCREAMIN_GREEN", COLOR_PALETTE_GROUP_GREEN, "#87ff5f" },
    { 120, "VERY_LIGHT_LIME_GREEN", COLOR_PALETTE_GROUP_GREEN, "#87ff87" },
    { 121, "MINT_GREEN_121", COLOR_PALETTE_GROUP_GREEN, "#87ffaf" },
    { 122, "AQUAMARINE", COLOR_PALETTE_GROUP_CYAN, "#87ffd7" },
    { 123, "VERY_LIGHT_CYAN", COLOR_PALETTE_GROUP_CYAN, "#87ffff" },
    { 124, "BRIGHT_RED", COLOR_PALETTE_GROUP_RED, "#af0000" },
    { 125, "DARK_PINK", COLOR_PALETTE_GROUP_RED, "#af005f" },
    { 126, "DARK_MAGENTA", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af0087" },
    { 127, "HELIOTROPE_MAGENTA", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af00af" },
    { 128, "VIVID_MULBERRY", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af00d7" },
    { 129, "ELECTRIC_PURPLE", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af00ff" },
    { 130, "DARK_ORANGE_BROWN_TONE", COLOR_PALETTE_GROUP_BROWN, "#af5f00" },
    { 131, "DARK_MODERATE_RED", COLOR_PALETTE_GROUP_BROWN, "#af5f5f" },
    { 132, "DARK_MODERATE_PINK", COLOR_PALETTE_GROUP_RED, "#af5f87" },
    { 133, "DARK_MODERATE_MAGENTA", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af5faf" },
    { 134, "RICH_LILAC", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af5fd7" },
    { 135, "LAVENDER_INDIGO", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af5fff" },
    { 136, "PIRATE_GOLD", COLOR_PALETTE_GROUP_BROWN, "#af8700" },
    { 137, "BRONZE", COLOR_PALETTE_GROUP_BROWN, "#af875f" },
    { 138, "PHARLAP", COLOR_PALETTE_GROUP_BROWN, "#af8787" },
    { 139, "DARK_GRAYISH_MAGENTA", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af87af" },
    { 140, "LAVENDER", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af87d7" },
    { 141, "BRIGHT_LAVENDER", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#af87ff" },
    { 142, "BUDDHA_GOLD", COLOR_PALETTE_GROUP_BROWN, "#afaf00" },
    { 143, "OLIVE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#afaf5f" },
    { 144, "SAGE", COLOR_PALETTE_GROUP_BROWN, "#afaf87" },
    { 145, "SILVER_FOIL", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#afafaf" },
    { 146, "GRAYISH_BLUE", COLOR_PALETTE_GROUP_BLUE, "#afafd7" },
    { 147, "MAXIMUM_BLUE_PURPLE", COLOR_PALETTE_GROUP_BLUE, "#afafff" },
    { 148, "SHEEN_GREEN", COLOR_PALETTE_GROUP_GREEN, "#afd700" },
    { 149, "MODERATE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#afd75f" },
    { 150, "SLIGHTLY_DESATURATED_GREEN", COLOR_PALETTE_GROUP_GREEN, "#afd787" },
    { 151, "LIGHT_MOSS_GREEN", COLOR_PALETTE_GROUP_GREEN, "#afd7af" },
    { 152, "GRAYISH_CYAN", COLOR_PALETTE_GROUP_CYAN, "#afd7d7" },
    { 153, "PALE_BLUE", COLOR_PALETTE_GROUP_BLUE, "#afd7ff" },
    { 154, "LIME", COLOR_PALETTE_GROUP_GREEN, "#afff00" },
    { 155, "INCHWORM", COLOR_PALETTE_GROUP_GREEN, "#afff5f" },
    { 156, "MINT_GREEN", COLOR_PALETTE_GROUP_GREEN, "#afff87" },
    { 157, "PALE_LIME_GREEN", COLOR_PALETTE_GROUP_GREEN, "#afffaf" },
    { 158, "MAGIC_MINT", COLOR_PALETTE_GROUP_CYAN, "#afffd7" },
    { 159, "CELESTE", COLOR_PALETTE_GROUP_CYAN, "#afffff" },
    { 160, "STRONG_RED", COLOR_PALETTE_GROUP_RED, "#d70000" },
    { 161, "RAZZMATAZZ", COLOR_PALETTE_GROUP_RED, "#d7005f" },
    { 162, "STRONG_PINK", COLOR_PALETTE_GROUP_PINK, "#d70087" },
    { 163, "HOLLYWOOD_CERISE_163", COLOR_PALETTE_GROUP_PINK, "#d700af" },
    { 164, "STRONG_MAGENTA", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#d700d7" },
    { 165, "PHLOX", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#d700ff" },
    { 166, "STRONG_ORANGE", COLOR_PALETTE_GROUP_ORANGE, "#d75f00" },
    { 167, "MODERATE_RED", COLOR_PALETTE_GROUP_RED, "#d75f5f" },
    { 168, "MYSTIC_PEARL", COLOR_PALETTE_GROUP_RED, "#d75f87" },
    { 169, "MODERATE_PINK", COLOR_PALETTE_GROUP_PINK, "#d75faf" },
    { 170, "MODERATE_MAGENTA", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#d75fd7" },
    { 171, "LIGHT_MAGENTA", COLOR_PALETTE_GROUP_PINK, "#d75fff" },
    { 172, "MANGO_TANGO", COLOR_PALETTE_GROUP_BROWN, "#d78700" },
    { 173, "COPPERFIELD", COLOR_PALETTE_GROUP_BROWN, "#d7875f" },
    { 174, "SLIGHTLY_DESATURATED_RED", COLOR_PALETTE_GROUP_PINK, "#d78787" },
    { 175, "SLIGHTLY_DESATURATED_PINK", COLOR_PALETTE_GROUP_PINK, "#d787af" },
    { 176, "SLIGHTLY_DESATURATED_MAGENTA", COLOR_PALETTE_GROUP_PINK, "#d787d7" },
    { 177, "VERY_LIGHT_VIOLET", COLOR_PALETTE_GROUP_PINK, "#d787ff" },
    { 178, "MUSTARD_YELLOW", COLOR_PALETTE_GROUP_BROWN, "#d7af00" },
    { 179, "EARTH_YELLOW", COLOR_PALETTE_GROUP_BROWN, "#d7af5f" },
    { 180, "TAN", COLOR_PALETTE_GROUP_BROWN, "#d7af87" },
    { 181, "GRAYISH_RED", COLOR_PALETTE_GROUP_BROWN, "#d7afaf" },
    { 182, "GRAYISH_MAGENTA", COLOR_PALETTE_GROUP_PINK, "#d7afd7" },
    { 183, "PALE_VIOLET", COLOR_PALETTE_GROUP_PURPLE_VIOLET_AND_MAGENTA, "#d7afff" },
    { 184, "STRONG_YELLOW", COLOR_PALETTE_GROUP_YELLOW, "#d7d700" },
    { 185, "MODERATE_YELLOW", COLOR_PALETTE_GROUP_YELLOW, "#d7d75f" },
    { 186, "SLIGHTLY_DESATURATED_YELLOW", COLOR_PALETTE_GROUP_YELLOW, "#d7d787" },
    { 187, "PASTEL_GRAY", COLOR_PALETTE_GROUP_GREEN, "#d7d7af" },
    { 188, "LIGHT_SILVER", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#d7d7d7" },
    { 189, "VERY_PALE_BLUE", COLOR_PALETTE_GROUP_BLUE, "#d7d7ff" },
    { 190, "CHARTREUSE_YELLOW", COLOR_PALETTE_GROUP_YELLOW, "#d7ff00" },
    { 191, "MAXIMUM_GREEN_YELLOW", COLOR_PALETTE_GROUP_YELLOW, "#d7ff5f" },
    { 192, "HONEYSUCKLE", COLOR_PALETTE_GROUP_YELLOW, "#d7ff87" },
    { 193, "PALE_GREEN", COLOR_PALETTE_GROUP_GREEN, "#d7ffaf" },
    { 194, "SNOWY_MINT", COLOR_PALETTE_GROUP_GREEN, "#d7ffd7" },
    { 195, "VERY_PALE_CYAN", COLOR_PALETTE_GROUP_GREEN, "#d7ffff" },
    { 196, "LIGHT_RED", COLOR_PALETTE_GROUP_RED, "#ff0000" },
    { 197, "VIVID_RASPBERRY", COLOR_PALETTE_GROUP_RED, "#ff005f" },
    { 198, "ROSE", COLOR_PALETTE_GROUP_PINK, "#ff0087" },
    { 199, "PURE_PINK", COLOR_PALETTE_GROUP_PINK, "#ff00af" },
    { 200, "PURE_MAGENTA", COLOR_PALETTE_GROUP_PINK, "#ff00d7" },
    { 201, "LIGHT_MAGENTA_013", COLOR_PALETTE_GROUP_PINK, "#ff00ff" },
    { 202, "BLAZE_ORANGE", COLOR_PALETTE_GROUP_ORANGE, "#ff5f00" },
    { 203, "PASTEL_RED", COLOR_PALETTE_GROUP_RED, "#ff5f5f" },
    { 204, "WILD_WATERMELON", COLOR_PALETTE_GROUP_RED, "#ff5f87" },
    { 205, "HOT_PINK", COLOR_PALETTE_GROUP_PINK, "#ff5faf" },
    { 206, "LIGHT_DEEP_PINK", COLOR_PALETTE_GROUP_PINK, "#ff5fd7" },
    { 207, "PINK_FLAMINGO", COLOR_PALETTE_GROUP_PINK, "#ff5fff" },
    { 208, "FLUSH_ORANGE", COLOR_PALETTE_GROUP_ORANGE, "#ff8700" },
    { 209, "SALMON", COLOR_PALETTE_GROUP_ORANGE, "#ff875f" },
    { 210, "TULIP", COLOR_PALETTE_GROUP_RED, "#ff8787" },
    { 211, "TICKLE_ME_PINK", COLOR_PALETTE_GROUP_PINK, "#ff87af" },
    { 212, "PRINCESS_PERFUME", COLOR_PALETTE_GROUP_PINK, "#ff87d7" },
    { 213, "BLUSH_PINK", COLOR_PALETTE_GROUP_PINK, "#ff87ff" },
    { 214, "PURE_ORANGE", COLOR_PALETTE_GROUP_ORANGE, "#ffaf00" },
    { 215, "LIGHT_ORANGE", COLOR_PALETTE_GROUP_ORANGE, "#ffaf5f" },
    { 216, "VERY_LIGHT_ORANGE", COLOR_PALETTE_GROUP_ORANGE, "#ffaf87" },
    { 217, "MELON", COLOR_PALETTE_GROUP_ORANGE, "#ffafaf" },
    { 218, "COTTON_CANDY", COLOR_PALETTE_GROUP_PINK, "#ffafd7" },
    { 219, "PALE_MAGENTA", COLOR_PALETTE_GROUP_PINK, "#ffafff" },
    { 220, "GOLD", COLOR_PALETTE_GROUP_YELLOW, "#ffd700" },
    { 221, "DANDELION", COLOR_PALETTE_GROUP_YELLOW, "#ffd75f" },
    { 222, "KHAKI", COLOR_PALETTE_GROUP_BROWN, "#ffd787" },
    { 223, "MOCCASIN", COLOR_PALETTE_GROUP_BROWN, "#ffd7af" },
    { 224, "MISTY_ROSE", COLOR_PALETTE_GROUP_PINK, "#ffd7d7" },
    { 225, "BUBBLE_GUM", COLOR_PALETTE_GROUP_PINK, "#ffd7ff" },
    { 226, "LIGHT_YELLOW_011", COLOR_PALETTE_GROUP_YELLOW, "#ffff00" },
    { 227, "LASER_LEMON", COLOR_PALETTE_GROUP_YELLOW, "#ffff5f" },
    { 228, "PASTEL_YELLOW", COLOR_PALETTE_GROUP_YELLOW, "#ffff87" },
    { 229, "PALE_YELLOW", COLOR_PALETTE_GROUP_YELLOW, "#ffffaf" },
    { 230, "VERY_PALE_YELLOW", COLOR_PALETTE_GROUP_WHITE, "#ffffd7" },
    { 231, "LIGHT_WHITE", COLOR_PALETTE_GROUP_WHITE, "#ffffff" },
    { 232, "VAMPIRE_BLACK", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#080808" },
    { 233, "COD_GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#121212" },
    { 234, "EERIE_BLACK", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#1c1c1c" },
    { 235, "RAISIN_BLACK", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#262626" },
    { 236, "DARK_CHARCOAL", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#303030" },
    { 237, "MINE_SHAFT", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#3a3a3a" },
    { 238, "OUTER_SPACE", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#444444" },
    { 239, "DARK_LIVER", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#4e4e4e" },
    { 240, "DAVYS_GREY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#585858" },
    { 241, "GRANITE_GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#626262" },
    { 242, "DOVE_GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#6c6c6c" },
    { 243, "SONIC_SILVER", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#767676" },
    { 244, "GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#808080" },
    { 245, "PHILIPPINE_GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#8a8a8a" },
    { 246, "DUSTY_GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#949494" },
    { 247, "SPANISH_GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#9e9e9e" },
    { 248, "QUICK_SILVER", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#a8a8a8" },
    { 249, "SILVER_CHALICE", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#b2b2b2" },
    { 250, "SILVER", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#bcbcbc" },
    { 251, "SILVER_SAND", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#c6c6c6" },
    { 252, "LIGHT_GRAY", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#d0d0d0" },
    { 253, "ALTO", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#dadada" },
    { 254, "PLATINUM", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#e4e4e4" },
    { 255, "#EEEEEE", COLOR_PALETTE_GROUP_GRAY_AND_BLACK, "#eeeeee" },
};

static const struct {
    const char* alias;
    int index;
} g_color_palette_aliases[] = {
    { "RED", 1 },
    { "GREEN", 2 },
    { "BROWN", 3 },
    { "BLUE", 4 },
    { "MAGENTA", 5 },
    { "CYAN", 6 },
    { "LIGHT_GRAY", 7 },
    { "LIGHT_BLACK", 8 },
    { "LIGHT_GREEN", 10 },
    { "YELLOW", 11 },
    { "LIGHT_BLUE", 12 },
    { "LIGHT_MAGENTA", 13 },
    { "LIGHT_CYAN", 14 },
    { "WHITE", 15 },
    { "BLACK_000", 16 },
};

static int color_palette_is_index(int color)
{
    return color >= COLOR_PALETTE_INDEX_BASE && color <= COLOR_PALETTE_INDEX_MAX;
}

static int color_palette_to_index(int color)
{
    if(!color_palette_is_index(color))
        return -1;
    return color - COLOR_PALETTE_INDEX_BASE;
}

const ColorPaletteEntry* color_palette_entry(int color)
{
    int index = color_palette_to_index(color);
    if(index < 0 && color >= 0 && color < 256)
        index = color;
    if(index < 0 || index > 255)
        return NULL;

    return &g_color_palette_entries[index];
}

int color_palette_parse_color(const char* value, int* out)
{
    if(!value || !out)
        return 0;

    while(*value && isspace((unsigned char)*value))
        value++;

    if(starts_with_ignore_case(value, "idx:"))
        value += 4;
    else if(starts_with_ignore_case(value, "256:"))
        value += 4;

    if(color_palette_parse_integer(value, out))
    {
        *out += COLOR_PALETTE_INDEX_BASE;
        return 1;
    }

    if(starts_with_ignore_case(value, "render_color_"))
        value += strlen("render_color_");

    for(int i = 0; i < 256; i++)
    {
        const ColorPaletteEntry* entry = &g_color_palette_entries[i];
        if(entry && equals_ignore_case(value, entry->name))
        {
            *out = entry->index + COLOR_PALETTE_INDEX_BASE;
            return 1;
        }
    }

    for(size_t i = 0; i < sizeof(g_color_palette_aliases) / sizeof(g_color_palette_aliases[0]); i++)
    {
        if(equals_ignore_case(value, g_color_palette_aliases[i].alias))
        {
            *out = g_color_palette_aliases[i].index + COLOR_PALETTE_INDEX_BASE;
            return 1;
        }
    }

    if(equals_ignore_case(value, "DEFAULT"))
    {
        *out = RENDER_COLOR_DEFAULT;
        return 1;
    }

    return 0;
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
            if((g >= 4 && b <= 2) || (g > 0 && g <= 2 && b <= 1 && r > g))
                return RENDER_COLOR_BROWN;
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

    if(color_palette_is_index(color) || (color >= 0 && color < 256))
    {
        int palette_index = color_palette_is_index(color)
            ? color_palette_to_index(color)
            : color;
        const ColorPaletteEntry* entry = color_palette_entry(palette_index);
        int r, g, b;

        if((color_palette_get_mode() == COLOR_PALETTE_MODE_TRUECOLOR ||
            (color_palette_get_mode() == COLOR_PALETTE_MODE_256 && color_palette_env_supports_truecolor())) &&
            entry && color_palette_parse_hex_color(entry->hex, &r, &g, &b))
        {
            n = snprintf(buffer, buffer_size, "\x1b[38;2;%d;%d;%dm", r, g, b);
            return n > 0 && (size_t)n < buffer_size;
        }

        if(color_palette_get_mode() == COLOR_PALETTE_MODE_256)
            n = snprintf(buffer, buffer_size, "\x1b[38;5;%dm", palette_index);
        else
            n = snprintf(buffer, buffer_size, "\x1b[%dm", fallback_256_to_ansi16(palette_index));

        return n > 0 && (size_t)n < buffer_size;
    }

    n = snprintf(buffer, buffer_size, "\x1b[39m");
    return n > 0 && (size_t)n < buffer_size;
}