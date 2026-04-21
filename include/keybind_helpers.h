#ifndef KEYBIND_HELPERS_H
#define KEYBIND_HELPERS_H

#include "input.h"

// Common overlay tab hint strings used in different UI contexts.
#define HOTKEYS_OVERLAY_TABS_TEXT "Tabs: i inventory | c character | l log | j journal | o codex | q close"
#define HOTKEYS_BOTTOM_OVERLAY_TEXT "Overlay tabs: i inventory | c character | l log | j journal | o atlas | Esc menu"
#define HOTKEYS_WORLD_ACTIONS_TEXT "Controls: WASD/arrows move | Space pass | M zone/world | E interact | T inspect | F attack"
#define HOTKEYS_INSPECT_ACTIONS_TEXT "Inspect: WASD/arrows move | Enter inspect | E interact | L lock | Q cancel"

// Basic case-insensitive alpha matcher for fixed hotkeys.
#define KEYBIND_MATCH_ALPHA(key, lower, upper) ((key) == (lower) || (key) == (upper))
#define KEYBIND_LOG_OVERLAY(key) KEYBIND_MATCH_ALPHA((key), 'l', 'L')
#define KEYBIND_WORLD_MAP_TOGGLE(key) KEYBIND_MATCH_ALPHA((key), 'm', 'M')

// Direction helpers (WASD and arrow keys).
#define KEYBIND_UP(key) (KEYBIND_MATCH_ALPHA((key), 'w', 'W') || (key) == INPUT_KEY_UP)
#define KEYBIND_DOWN(key) (KEYBIND_MATCH_ALPHA((key), 's', 'S') || (key) == INPUT_KEY_DOWN)
#define KEYBIND_LEFT(key) (KEYBIND_MATCH_ALPHA((key), 'a', 'A') || (key) == INPUT_KEY_LEFT)
#define KEYBIND_RIGHT(key) (KEYBIND_MATCH_ALPHA((key), 'd', 'D') || (key) == INPUT_KEY_RIGHT)

// Common action helpers.
#define KEYBIND_CONFIRM(key) ((key) == 13)
#define KEYBIND_CANCEL(key) (KEYBIND_MATCH_ALPHA((key), 'q', 'Q') || (key) == 27)
#define KEYBIND_OVERLAY_CLOSE(key) KEYBIND_MATCH_ALPHA((key), 'o', 'O')
#define KEYBIND_OVERLAY_EXIT(key) (KEYBIND_OVERLAY_CLOSE((key)) || KEYBIND_CANCEL((key)))

#endif