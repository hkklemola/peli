#ifndef RENDER_COLOR_H
#define RENDER_COLOR_H

/*
 * Purpose:
 *   Defines shared ANSI-compatible render colors for tiles and entities.
 */

/** @enum RenderColor
 *  @brief ANSI color codes for terminal/console rendering.
 *  Values correspond to standard ANSI escape codes (30-39, 90-97).
 */
typedef enum RenderColor {
    /** Black (ANSI 30). */
    RENDER_COLOR_BLACK = 30,
    /** Red (ANSI 31). */
    RENDER_COLOR_RED = 31,
    /** Green (ANSI 32). */
    RENDER_COLOR_GREEN = 32,
    /** Brown/Yellow (ANSI 33). */
    RENDER_COLOR_BROWN = 33,
    /** Blue (ANSI 34). */
    RENDER_COLOR_BLUE = 34,
    /** Magenta (ANSI 35). */
    RENDER_COLOR_MAGENTA = 35,
    /** Cyan (ANSI 36). */
    RENDER_COLOR_CYAN = 36,
    /** Light gray (ANSI 37). */
    RENDER_COLOR_LIGHT_GRAY = 37,
    /** Default terminal color (ANSI 39). */
    RENDER_COLOR_DEFAULT = 39,
    /** Dark gray (ANSI 90). */
    RENDER_COLOR_DARK_GRAY = 90,
    /** Light/bright red (ANSI 91). */
    RENDER_COLOR_LIGHT_RED = 91,
    /** Light/bright green (ANSI 92). */
    RENDER_COLOR_LIGHT_GREEN = 92,
    /** Light/bright yellow (ANSI 93). */
    RENDER_COLOR_LIGHT_YELLOW = 93,
    /** Light/bright blue (ANSI 94). */
    RENDER_COLOR_LIGHT_BLUE = 94,
    /** Light/bright magenta (ANSI 95). */
    RENDER_COLOR_LIGHT_MAGENTA = 95,
    /** Light/bright cyan (ANSI 96). */
    RENDER_COLOR_LIGHT_CYAN = 96,
    /** White (ANSI 97). */
    RENDER_COLOR_WHITE = 97
} RenderColor;

#endif