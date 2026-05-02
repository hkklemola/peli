#include "startup.h"
#include "audio.h"
#include "color_palette.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "draw.h"
#include "input.h"
#include "keybind_helpers.h"
#include "layout.h"
#include "race.h"
#include "savegame.h"
#include "template_content.h"
#include "ui_frame.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/*
 * Purpose:
 *   Implements splash screen, startup menu state machine, and settings persistence.
 *
 * Functions:
 *   - startup_settings_*: default/sanitize/load/save settings helpers.
 *   - startup_sleep_ms: platform sleep helper.
 *   - apply_layout_from_settings: sync settings into runtime layout config.
 *   - startup_frame/startup_content_lines: derive startup panel geometry.
 *   - draw_* helpers: render splash/menu/info pages.
 *   - wait_for_* helpers: input waits for timed splash/back navigation.
 *   - startup_run: runs startup state loop and returns selected action.
 */

#define STARTUP_LINE_LENGTH 256

#define STARTUP_MENU_ITEM_COUNT 5
#define SETTINGS_MENU_ITEM_COUNT 11
#define SPLASH_TIMEOUT_MS 30000
#define DISPLAY_PRESET_COUNT 3

typedef struct DisplayPreset
{
    const char* name;
    const char* target_resolution;
    int viewport_width;
    int viewport_height;
    int hud_height;
    int log_height;
    int verified;
} DisplayPreset;

typedef enum StartupState
{
    STARTUP_STATE_SPLASH = 0,
    STARTUP_STATE_CHARACTER_CREATOR,
    STARTUP_STATE_RACE_SELECT,
    STARTUP_STATE_MENU,
    STARTUP_STATE_CREDITS,
    STARTUP_STATE_CONTINUE_STUB,
    STARTUP_STATE_CONTINUE_SLOT_SELECT,
    STARTUP_STATE_NEW_GAME_OVERWRITE_SLOT,
    STARTUP_STATE_SETTINGS
} StartupState;

typedef enum SettingsMenuItem
{
    SETTINGS_MENU_PRESET = 0,
    SETTINGS_MENU_COLOR_PALETTE,
    SETTINGS_MENU_MUSIC_ENABLED,
    SETTINGS_MENU_MUSIC_VOLUME,
    SETTINGS_MENU_VIEWPORT_WIDTH,
    SETTINGS_MENU_VIEWPORT_HEIGHT,
    SETTINGS_MENU_HUD_HEIGHT,
    SETTINGS_MENU_LOG_HEIGHT,
    SETTINGS_MENU_VERIFIED,
    SETTINGS_MENU_SAVE_AND_BACK,
    SETTINGS_MENU_CANCEL
} SettingsMenuItem;

static const char* startup_menu_items[STARTUP_MENU_ITEM_COUNT] = {
    "Start Game",
    "Continue",
    "Settings",
    "Credits",
    "Quit"
};

static const DisplayPreset display_presets[DISPLAY_PRESET_COUNT] = {
    {"HD Balanced", "1366x768", 80, 14, 12, 12, 0},
    {"FHD Gameplay", "1920x1080", 120, 22, 11, 6, 1},
    {"QHD Expanded", "2560x1440", 256, 40, 14, 10, 1}
};

static const char* color_palette_mode_names[] = {
    "16-color",
    "256-color",
    "Truecolor"
};

static UiFrameSurfaceCache startup_surface_cache;

// Return whether key means "back" in startup sub-pages.
/**
 * @brief Check if an input key represents the "back" action in startup menus.
 * @param key The input key code.
 * @return 1 if key is Enter (13), Escape (27), or 'b'/'B'/'q'/'Q', 0 otherwise.
 */
static int startup_is_back_key(int key)
{
    return key == 13 || key == 27 || key == 'b' || key == 'B' || key == 'q' || key == 'Q';
}

static int startup_is_cancel_key(int key)
{
    return key == 27 || key == 'b' || key == 'B' || key == 'q' || key == 'Q';
}

/**
 * @brief Get the display label for a settings menu field by index.
 * @param field_index The SettingsMenuItem enum value.
 * @return A human-readable field name (e.g., "Viewport width", "HUD height").
 */
static const char* settings_key_label(int field_index)
{
    if(field_index == SETTINGS_MENU_PRESET)
        return "Display preset";
    if(field_index == SETTINGS_MENU_COLOR_PALETTE)
        return "Color palette";
    if(field_index == SETTINGS_MENU_MUSIC_ENABLED)
        return "Music";
    if(field_index == SETTINGS_MENU_MUSIC_VOLUME)
        return "Music volume";
    if(field_index == SETTINGS_MENU_VIEWPORT_WIDTH)
        return "Viewport width";
    if(field_index == SETTINGS_MENU_VIEWPORT_HEIGHT)
        return "Viewport height";
    if(field_index == SETTINGS_MENU_HUD_HEIGHT)
        return "HUD height";
    if(field_index == SETTINGS_MENU_LOG_HEIGHT)
        return "Log height";
    return "Setting";
}

/**
 * @brief Remove leading and trailing whitespace from a string in-place.
 * @param text Pointer to the text string to trim.
 * @return Pointer to the first non-whitespace character in the string, or end-of-string.
 * @note Modifies the input string in-place by advancing the pointer and nulling trailing space.
 */
static char* trim_whitespace(char* text)
{
    char* end;

    if(!text) return text;

    while(*text && isspace((unsigned char)*text))
        text++;

    if(*text == '\0')
        return text;

    end = text + strlen(text) - 1;
    while(end > text && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }

    return text;
}

// Apply startup settings to runtime layout system.
static void apply_layout_from_settings(const StartupSettings* settings)
{
    LayoutConfig layout_config;

    if(!settings)
    {
        layout_set_config(NULL);
        return;
    }

    layout_config.viewport_width = settings->viewport_width;
    layout_config.viewport_height = settings->viewport_height;
    layout_config.hud_height = settings->hud_height;
    layout_config.log_height = settings->log_height;
    layout_set_config(&layout_config);
}

// Return whether menu index is one of the adjustable numeric fields.
static int settings_menu_is_adjustable(int menu_index)
{
    return menu_index == SETTINGS_MENU_MUSIC_VOLUME ||
           menu_index == SETTINGS_MENU_VIEWPORT_WIDTH ||
           menu_index == SETTINGS_MENU_VIEWPORT_HEIGHT ||
           menu_index == SETTINGS_MENU_HUD_HEIGHT ||
           menu_index == SETTINGS_MENU_LOG_HEIGHT;
}

// Debug-only settings rows are shown when dev test mode is enabled.
static int startup_settings_debug_visible(const StartupSettings* settings)
{
    return settings && settings->dev_test_loot;
}

static int settings_menu_item_visible(int menu_index, const StartupSettings* settings)
{
    if(menu_index == SETTINGS_MENU_VERIFIED)
        return startup_settings_debug_visible(settings);
    return 1;
}

static int settings_menu_next_visible_index(int selected_index, int direction, const StartupSettings* settings)
{
    int next = selected_index;

    do
    {
        next += direction;
        if(next < 0)
            next = SETTINGS_MENU_ITEM_COUNT - 1;
        else if(next >= SETTINGS_MENU_ITEM_COUNT)
            next = 0;
    } while(!settings_menu_item_visible(next, settings));

    return next;
}

// Return preset index for exact settings match, or -1 when custom.
static int settings_match_preset_index(const StartupSettings* settings)
{
    int i;

    if(!settings)
        return -1;

    for(i = 0; i < DISPLAY_PRESET_COUNT; i++)
    {
        if(settings->viewport_width == display_presets[i].viewport_width &&
           settings->viewport_height == display_presets[i].viewport_height &&
           settings->hud_height == display_presets[i].hud_height &&
           settings->log_height == display_presets[i].log_height)
            return i;
    }

    return -1;
}

// Apply one preset tuple into the active settings object.
static int settings_apply_preset(StartupSettings* settings, int preset_index)
{
    if(!settings || preset_index < 0 || preset_index >= DISPLAY_PRESET_COUNT)
        return 0;

    settings->viewport_width = display_presets[preset_index].viewport_width;
    settings->viewport_height = display_presets[preset_index].viewport_height;
    settings->hud_height = display_presets[preset_index].hud_height;
    settings->log_height = display_presets[preset_index].log_height;
    startup_settings_sanitize(settings);
    return 1;
}

// Change one settings value by delta and clamp to supported bounds.
static int settings_adjust_value(StartupSettings* settings, int menu_index, int delta)
{
    int before;

    if(!settings || !settings_menu_is_adjustable(menu_index))
        return 0;

    if(menu_index == SETTINGS_MENU_MUSIC_VOLUME)
    {
        before = settings->music_volume;
        settings->music_volume += delta;
        if(settings->music_volume < 0)
            settings->music_volume = 0;
        if(settings->music_volume > 10)
            settings->music_volume = 10;
        return settings->music_volume != before;
    }

    if(menu_index == SETTINGS_MENU_VIEWPORT_WIDTH)
    {
        before = settings->viewport_width;
        settings->viewport_width = layout_clamp_viewport_width(settings->viewport_width + delta);
        return settings->viewport_width != before;
    }

    if(menu_index == SETTINGS_MENU_VIEWPORT_HEIGHT)
    {
        before = settings->viewport_height;
        settings->viewport_height = layout_clamp_viewport_height(settings->viewport_height + delta);
        return settings->viewport_height != before;
    }

    if(menu_index == SETTINGS_MENU_HUD_HEIGHT)
    {
        before = settings->hud_height;
        settings->hud_height = layout_clamp_hud_height(settings->hud_height + delta);
        return settings->hud_height != before;
    }

    before = settings->log_height;
    settings->log_height = layout_clamp_log_height(settings->log_height + delta);
    return settings->log_height != before;
}

// Fill settings with built-in defaults.
void startup_settings_defaults(StartupSettings* out)
{
    LayoutConfig defaults;

    if(!out) return;

    layout_get_default_config(&defaults);
    out->viewport_width = defaults.viewport_width;
    out->viewport_height = defaults.viewport_height;
    out->hud_height = defaults.hud_height;
    out->log_height = defaults.log_height;
    out->color_palette_mode = color_palette_detect_mode();
    out->music_enabled = 1;
    out->music_volume = 10;
    out->dev_test_loot = 0;
    out->selected_save_slot = 1;
    strcpy(out->player_name, "Hero");
    strcpy(out->player_race_id, "human");
    memset(&out->player_starting_attributes, 0, sizeof(out->player_starting_attributes));
    out->has_player_starting_attributes = 0;
    color_palette_set_mode(out->color_palette_mode);
}

// Clamp settings to supported panel ranges.
void startup_settings_sanitize(StartupSettings* settings)
{
    if(!settings) return;

    settings->viewport_width = layout_clamp_viewport_width(settings->viewport_width);
    settings->viewport_height = layout_clamp_viewport_height(settings->viewport_height);
    settings->hud_height = layout_clamp_hud_height(settings->hud_height);
    settings->log_height = layout_clamp_log_height(settings->log_height);
    if(settings->color_palette_mode < COLOR_PALETTE_MODE_16 || settings->color_palette_mode > COLOR_PALETTE_MODE_TRUECOLOR)
        settings->color_palette_mode = COLOR_PALETTE_MODE_16;
    settings->music_enabled = settings->music_enabled ? 1 : 0;
    if(settings->music_volume < 0)
        settings->music_volume = 0;
    if(settings->music_volume > 10)
        settings->music_volume = 10;
    settings->dev_test_loot = settings->dev_test_loot ? 1 : 0;
    if(settings->selected_save_slot < 1)
        settings->selected_save_slot = 1;
    if(settings->selected_save_slot > SAVEGAME_SLOT_COUNT)
        settings->selected_save_slot = SAVEGAME_SLOT_COUNT;
    if(settings->player_race_id[0] == '\0')
        strcpy(settings->player_race_id, "human");
    if(!settings->has_player_starting_attributes)
        memset(&settings->player_starting_attributes, 0, sizeof(settings->player_starting_attributes));
}

// Save current settings to an INI file.
StartupSettingsResult startup_settings_save(const char* path, const StartupSettings* settings)
{
    FILE* file;
    StartupSettings sanitized;

    if(!path || !path[0] || !settings)
        return STARTUP_SETTINGS_RESULT_IO_ERROR;

    sanitized = *settings;
    startup_settings_sanitize(&sanitized);

    file = fopen(path, "w");
    if(!file)
        return STARTUP_SETTINGS_RESULT_IO_ERROR;

    fprintf(file, "# Peli startup settings\n");
    fprintf(file, "[display]\n");
    fprintf(file, "viewport_width=%d\n", sanitized.viewport_width);
    fprintf(file, "viewport_height=%d\n", sanitized.viewport_height);
    fprintf(file, "hud_height=%d\n", sanitized.hud_height);
    fprintf(file, "log_height=%d\n", sanitized.log_height);
    fprintf(file, "color_palette_mode=%d\n", sanitized.color_palette_mode);
    fprintf(file, "music_enabled=%d\n", sanitized.music_enabled);
    fprintf(file, "music_volume=%d\n", sanitized.music_volume);
    fprintf(file, "dev_test_loot=%d\n", sanitized.dev_test_loot);
    fprintf(file, "selected_save_slot=%d\n", sanitized.selected_save_slot);

    if(fclose(file) != 0)
        return STARTUP_SETTINGS_RESULT_IO_ERROR;

    return STARTUP_SETTINGS_RESULT_OK;
}

// Load settings from INI file, falling back to defaults when needed.
StartupSettingsResult startup_settings_load(const char* path, StartupSettings* out)
{
    FILE* file;
    char line[STARTUP_LINE_LENGTH];
    int saw_known_key = 0;
    int saw_invalid_value = 0;

    if(!out)
        return STARTUP_SETTINGS_RESULT_IO_ERROR;

    startup_settings_defaults(out);

    if(!path || !path[0])
        return STARTUP_SETTINGS_RESULT_IO_ERROR;

    file = fopen(path, "r");
    if(!file)
    {
        if(startup_settings_save(path, out) == STARTUP_SETTINGS_RESULT_IO_ERROR)
            return STARTUP_SETTINGS_RESULT_IO_ERROR;
        return STARTUP_SETTINGS_RESULT_MISSING;
    }

    while(fgets(line, sizeof(line), file))
    {
        char* eq;
        char* key;
        char* value;
        long parsed;
        char* end_ptr;

        key = trim_whitespace(line);
        if(key[0] == '\0' || key[0] == '#' || key[0] == ';' || key[0] == '[')
            continue;

        eq = strchr(key, '=');
        if(!eq)
            continue;

        *eq = '\0';
        value = trim_whitespace(eq + 1);
        key = trim_whitespace(key);

          if(strcmp(key, "viewport_width") != 0 &&
              strcmp(key, "viewport_height") != 0 &&
              strcmp(key, "hud_height") != 0 &&
              strcmp(key, "log_height") != 0 &&
              strcmp(key, "color_palette_mode") != 0 &&
              strcmp(key, "music_enabled") != 0 &&
              strcmp(key, "music_volume") != 0 &&
              strcmp(key, "dev_test_loot") != 0 &&
              strcmp(key, "selected_save_slot") != 0)
            continue;

        if(value[0] == '\0')
        {
            saw_invalid_value = 1;
            continue;
        }

        parsed = strtol(value, &end_ptr, 10);
        end_ptr = trim_whitespace(end_ptr);
        if(*end_ptr != '\0' || parsed < INT_MIN || parsed > INT_MAX)
        {
            saw_invalid_value = 1;
            continue;
        }

        if(strcmp(key, "viewport_width") == 0)
            out->viewport_width = (int)parsed;
        else if(strcmp(key, "viewport_height") == 0)
            out->viewport_height = (int)parsed;
        else if(strcmp(key, "hud_height") == 0)
            out->hud_height = (int)parsed;
        else if(strcmp(key, "log_height") == 0)
            out->log_height = (int)parsed;
        else if(strcmp(key, "color_palette_mode") == 0)
            out->color_palette_mode = (int)parsed;
        else if(strcmp(key, "music_enabled") == 0)
            out->music_enabled = ((int)parsed) ? 1 : 0;
        else if(strcmp(key, "music_volume") == 0)
            out->music_volume = (int)parsed;
        else if(strcmp(key, "dev_test_loot") == 0)
            out->dev_test_loot = ((int)parsed) ? 1 : 0;
        else if(strcmp(key, "selected_save_slot") == 0)
            out->selected_save_slot = (int)parsed;

        saw_known_key = 1;
    }

    if(fclose(file) != 0)
    {
        startup_settings_defaults(out);
        return STARTUP_SETTINGS_RESULT_IO_ERROR;
    }

    startup_settings_sanitize(out);
    color_palette_set_mode(out->color_palette_mode);

    if(!saw_known_key || saw_invalid_value)
    {
        if(startup_settings_save(path, out) == STARTUP_SETTINGS_RESULT_IO_ERROR)
            return STARTUP_SETTINGS_RESULT_IO_ERROR;
        return STARTUP_SETTINGS_RESULT_INVALID;
    }

    return STARTUP_SETTINGS_RESULT_OK;
}

// Sleep helper in milliseconds.
static void startup_sleep_ms(int ms)
{
    if(ms <= 0)
        return;

#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req = { ms / 1000, (ms % 1000) * 1000000 };
    (void)nanosleep(&req, NULL);
#endif
}

// Build startup frame geometry from default layout.
static UiFrame startup_frame(void)
{
    LayoutState layout;
    UiFrame frame;

    layout_get_default(&layout);
    frame.row = layout.startup.row;
    frame.col = layout.startup.col;
    frame.inner_width = layout.startup.inner_width;
    frame.height = layout.startup.height;
    return frame;
}

// Return number of writable content lines in startup frame.
static int startup_content_lines(void)
{
    UiFrame frame = startup_frame();
    return ui_frame_content_lines(&frame);
}

static void startup_reset_surface_cache(void)
{
    ui_frame_surface_reset(&startup_surface_cache);
}

// Draw one content line in startup frame.
static void draw_content_line(int content_line, const char* text)
{
    UiFrame frame = startup_frame();
    ui_frame_surface_draw_line(&startup_surface_cache, &frame, content_line, text);
}

// Draw one colored content line in startup frame.
static void draw_content_line_color(int content_line, const char* text, int color)
{
    UiFrame frame = startup_frame();
    ui_frame_draw_line_color(&frame, content_line, text, color);
}

// Clear and draw startup frame with a title.
static void startup_begin_screen(const char* title)
{
    UiFrame frame = startup_frame();

    if(draw_ensure_console_dimensions())
        startup_reset_surface_cache();
    ui_frame_surface_begin(&startup_surface_cache, &frame, title);
}

// Render splash screen content.
static void draw_splash_screen(void)
{
    startup_begin_screen("PLACEHOLDER_NAME");

    draw_content_line(1, "");
    draw_content_line(2, "   ╔═══════════════════════════════════════════════════════════╗");
    draw_content_line(3, "   ║                                                           ║");
    draw_content_line(4, "   ║         P L A C E H O L D E R _ N A M E                  ║");
    draw_content_line(5, "   ║                                                           ║");
    draw_content_line(6, "   ║           A Terminal Roguelike Adventure                  ║");
    draw_content_line(7, "   ║                                                           ║");
    draw_content_line(8, "   ╚═══════════════════════════════════════════════════════════╝");
    draw_content_line(9, "");
    draw_content_line(10, "                    Explore. Fight. Loot. Survive.");
    draw_content_line(11, "");
    draw_content_line(12, "");
    draw_content_line(13, "                      You can now pet the dog!");
    draw_content_line(14, "");
    draw_content_line(15, "");
    draw_content_line(16, "                   Press any key to continue");
    draw_content_line(17, "                      or wait 30 seconds...");

    fflush(stdout);
}

// Wait for key press or splash timeout.
static void wait_for_splash_advance(void)
{
    clock_t start_time = clock();

    while(1)
    {
        double elapsed_ms;
        if(read_input_key_nonblocking() != -1)
            break;

        elapsed_ms = ((double)(clock() - start_time) * 1000.0) / CLOCKS_PER_SEC;
        if(elapsed_ms >= (double)SPLASH_TIMEOUT_MS)
            break;

        startup_sleep_ms(20);
    }
}

// Render character creator screen for entering player name.
static void draw_character_creator(const char* name_draft, const char* status)
{
    char line[STARTUP_LINE_LENGTH];
    int bottom_line = startup_content_lines() - 1;
    if(bottom_line < 0) bottom_line = 0;

    startup_begin_screen("Create Character");
    draw_content_line(0, "Enter your character name: ");
    draw_content_line(1, "");
    
    snprintf(line, sizeof(line), "> %s", name_draft);
    draw_content_line(3, line);
    
    draw_content_line(5, "(Enter to confirm, Backspace to delete, Esc to cancel)");
    draw_content_line(bottom_line, status && status[0] ? status : "Ready.");
    fflush(stdout);
}

static int startup_wrap_text_lines(const char* text,
                                   char out_lines[][STARTUP_LINE_LENGTH],
                                   int max_lines,
                                   int max_width)
{
    const char* cursor;
    int line_count = 0;

    if(!text || !out_lines || max_lines <= 0)
        return 0;

    if(max_width < 8)
        max_width = 8;
    if(max_width >= STARTUP_LINE_LENGTH)
        max_width = STARTUP_LINE_LENGTH - 1;

    for(int i = 0; i < max_lines; i++)
        out_lines[i][0] = '\0';

    cursor = text;
    while(*cursor && line_count < max_lines)
    {
        char line[STARTUP_LINE_LENGTH];
        int length = 0;
        int last_space = -1;

        while(*cursor && isspace((unsigned char)*cursor))
        {
            if(*cursor == '\n')
            {
                cursor++;
                if(line_count < max_lines)
                    out_lines[line_count++][0] = '\0';
                goto next_line;
            }
            cursor++;
        }

        while(*cursor && *cursor != '\n' && length < max_width)
        {
            line[length] = *cursor;
            if(isspace((unsigned char)line[length]))
                last_space = length;
            length++;
            cursor++;
        }

        if(length == max_width && *cursor && *cursor != '\n' && !isspace((unsigned char)*cursor) && last_space > 0)
        {
            int rewind = length - (last_space + 1);
            cursor -= rewind;
            length = last_space;
        }

        while(length > 0 && isspace((unsigned char)line[length - 1]))
            length--;

        line[length] = '\0';
        snprintf(out_lines[line_count], STARTUP_LINE_LENGTH, "%s", line);
        line_count++;

        if(*cursor == '\n')
            cursor++;

next_line:
        ;
    }

    return line_count;
}

static int startup_race_selector_list_rows(void)
{
    int bottom_line = startup_content_lines() - 1;
    int list_top = 4;
    int preview_rows = 7;
    int list_bottom;

    if(bottom_line < 0)
        bottom_line = 0;

    if((bottom_line - list_top) < 7)
        preview_rows = 5;
    if((bottom_line - list_top) < 5)
        preview_rows = 4;
    if((bottom_line - list_top) < 4)
        preview_rows = 3;
    if((bottom_line - list_top) < 3)
        preview_rows = 1;

    list_bottom = bottom_line - preview_rows - 1;
    if(list_bottom < list_top)
        list_bottom = list_top;

    return (list_bottom - list_top) + 1;
}

static void startup_format_race_roll_stat_line(const Actor* actor, char* out, size_t out_size)
{
    if(!out || out_size == 0)
        return;

    if(!actor)
    {
        snprintf(out, out_size, "STR -- CON -- END -- AGI -- DEX -- SPD --");
        return;
    }

    snprintf(out,
             out_size,
             "STR %2d CON %2d END %2d AGI %2d DEX %2d SPD %2d",
             actor->strength,
             actor->constitution,
             actor->endurance,
             actor->agility,
             actor->dexterity,
             actor->speed);
}

static void startup_format_race_roll_stat_line_2(const Actor* actor, char* out, size_t out_size)
{
    if(!out || out_size == 0)
        return;

    if(!actor)
    {
        snprintf(out, out_size, "INT -- WIS -- RES -- COM -- CHA -- BEA --");
        return;
    }

    snprintf(out,
             out_size,
             "INT %2d WIS %2d RES %2d COM %2d CHA %2d BEA %2d",
             actor->intellect,
             actor->wisdom,
             actor->resolve,
             actor->composure,
             actor->charisma,
             actor->beauty);
}

static void startup_format_race_roll_stat_line_3(const Actor* actor, char* out, size_t out_size)
{
    if(!out || out_size == 0)
        return;

    if(!actor)
    {
        snprintf(out, out_size, "PER -- WIT --");
        return;
    }

    snprintf(out,
             out_size,
             "PER %2d WIT %2d",
             actor->perception,
             actor->wits);
}

static void draw_race_selector(int selected_index,
                               int scroll_offset,
                               const char* player_name,
                               const Actor* rolled_actor,
                               const char* status)
{
    char line[STARTUP_LINE_LENGTH];
    char wrapped_desc[16][STARTUP_LINE_LENGTH];
    int total = race_templates_count();
    int bottom_line = startup_content_lines() - 1;
    int list_top = 4;
    int list_bottom;
    int preview_rows = 7;
    int preview_top;
    int preview_text_start;
    int preview_text_rows;
    int preview_width;
    int wrapped_capacity;
    int list_rows;
    int list_end;
    const RaceTemplate* selected = race_template_at(selected_index);

    if(bottom_line < 0)
        bottom_line = 0;

    if((bottom_line - list_top) < 7)
        preview_rows = 5;
    if((bottom_line - list_top) < 5)
        preview_rows = 4;
    if((bottom_line - list_top) < 4)
        preview_rows = 3;
    if((bottom_line - list_top) < 3)
        preview_rows = 1;

    list_bottom = bottom_line - preview_rows - 1;
    if(list_bottom < list_top)
        list_bottom = list_top;

    list_rows = (list_bottom - list_top) + 1;
    preview_top = list_bottom + 1;
    preview_text_start = preview_top + 4;
    preview_text_rows = (bottom_line - 1) - preview_text_start + 1;
    if(preview_text_rows < 0)
        preview_text_rows = 0;

    preview_width = startup_frame().inner_width;
    if(preview_width < 8)
        preview_width = 8;
    if(preview_width >= STARTUP_LINE_LENGTH)
        preview_width = STARTUP_LINE_LENGTH - 1;

    wrapped_capacity = (int)(sizeof(wrapped_desc) / sizeof(wrapped_desc[0]));
    if(preview_text_rows < wrapped_capacity)
        wrapped_capacity = preview_text_rows;

    list_end = scroll_offset + list_rows;
    if(list_end > total)
        list_end = total;

    startup_begin_screen("Create Character: Race");
    snprintf(line, sizeof(line), "Character: %s", (player_name && player_name[0]) ? player_name : "Hero");
    draw_content_line(0, line);
    draw_content_line(1, "Select race: W/X move | S/Enter select | R reroll stats | Esc cancel");
    draw_content_line(2, "");

    for(int row = list_top; row < bottom_line; row++)
        draw_content_line(row, "");

    for(int i = scroll_offset; i < list_end; i++)
    {
        const RaceTemplate* race = race_template_at(i);
        int row = list_top + (i - scroll_offset);
        if(!race)
            continue;
        snprintf(line, sizeof(line), "%c %s", (i == selected_index) ? '>' : ' ', race->name);
        draw_content_line_color(row, line, race->glyph_color);
    }

    if(preview_top <= bottom_line - 1)
    {
        if(selected)
            snprintf(line, sizeof(line), "Preview: %s (%s)", selected->name, selected->id);
        else
            snprintf(line, sizeof(line), "Preview: unavailable");
        draw_content_line(preview_top, line);
    }

    if(preview_top + 1 <= bottom_line - 1)
    {
        startup_format_race_roll_stat_line(rolled_actor, line, sizeof(line));
        draw_content_line(preview_top + 1, line);
    }

    if(preview_top + 2 <= bottom_line - 1)
    {
        startup_format_race_roll_stat_line_2(rolled_actor, line, sizeof(line));
        draw_content_line(preview_top + 2, line);
    }

    if(preview_top + 3 <= bottom_line - 1)
    {
        startup_format_race_roll_stat_line_3(rolled_actor, line, sizeof(line));
        draw_content_line(preview_top + 3, line);
    }

    if(preview_text_rows > 0)
    {
        int wrapped_count = 0;
        const char* desc = NULL;

        if(selected)
            desc = selected->description;

        if(desc && desc[0] != '\0')
        {
            wrapped_count = startup_wrap_text_lines(desc,
                                                    wrapped_desc,
                                                    wrapped_capacity,
                                                    preview_width);
        }

        if(wrapped_count <= 0)
        {
            wrapped_count = startup_wrap_text_lines("No lore description yet.",
                                                    wrapped_desc,
                                                    wrapped_capacity,
                                                    preview_width);
        }

        for(int i = 0; i < preview_text_rows; i++)
        {
            int row = preview_text_start + i;
            if(i < wrapped_count)
                draw_content_line(row, wrapped_desc[i]);
            else
                draw_content_line(row, "");
        }
    }

    draw_content_line(bottom_line, status && status[0] ? status : "Choose a race.");
    fflush(stdout);
}

// Render startup main menu with current selection and status.
static void draw_main_menu(int selected_index, const char* status)
{
    char line[STARTUP_LINE_LENGTH];
    int bottom_line = startup_content_lines() - 1;
    if(bottom_line < 0) bottom_line = 0;

    startup_begin_screen("Main Menu");
    draw_content_line(0, "Use W/X or Up/Down to move, S/Enter to select, Q to quit.");
    draw_content_line(1, "");

    for(int i = 0; i < STARTUP_MENU_ITEM_COUNT; i++)
    {
        snprintf(line, sizeof(line), "%c %s", (i == selected_index) ? '>' : ' ', startup_menu_items[i]);
        draw_content_line(3 + i, line);
    }

    draw_content_line(bottom_line, status && status[0] ? status : "Ready.");
    fflush(stdout);
}

static void startup_format_playtime(unsigned long long seconds, char* out, size_t out_size)
{
    unsigned long long hours;
    unsigned long long minutes;

    if(!out || out_size == 0)
        return;

    hours = seconds / 3600ULL;
    minutes = (seconds % 3600ULL) / 60ULL;
    snprintf(out, out_size, "%lluh %llum", hours, minutes);
}

static const char* startup_ts_or_unknown(const char* ts)
{
    if(ts && ts[0])
        return ts;
    return "unknown";
}

static int startup_count_occupied_slots(const SavegameSlotInfo* slots, int slot_count)
{
    int occupied = 0;

    if(!slots || slot_count <= 0)
        return 0;

    for(int i = 0; i < slot_count; i++)
    {
        if(slots[i].occupied)
            occupied++;
    }

    return occupied;
}

static int startup_first_occupied_slot(const SavegameSlotInfo* slots, int slot_count)
{
    if(!slots || slot_count <= 0)
        return -1;

    for(int i = 0; i < slot_count; i++)
    {
        if(slots[i].occupied)
            return i;
    }

    return -1;
}

static int startup_first_empty_slot(const SavegameSlotInfo* slots, int slot_count)
{
    if(!slots || slot_count <= 0)
        return -1;

    for(int i = 0; i < slot_count; i++)
    {
        if(!slots[i].occupied)
            return i;
    }

    return -1;
}

static void draw_save_slot_menu(const char* title,
                                const SavegameSlotInfo* slots,
                                int slot_count,
                                int selected_index,
                                const char* status,
                                const char* footer,
                                int delete_mode)
{
    char line[STARTUP_LINE_LENGTH];
    int bottom_line = startup_content_lines() - 1;
    int row = 3;

    if(bottom_line < 0)
        bottom_line = 0;

    startup_begin_screen(title);
    if(delete_mode)
        draw_content_line(0, "DELETE MODE: S/Enter deletes slot | DEL toggles off | Esc/B cancel");
    else
        draw_content_line(0, "W/X move | S/Enter select | DEL delete mode | Esc/B back");
    draw_content_line(1, "");

    for(int i = 0; i < slot_count && row < bottom_line; i++)
    {
        const SavegameSlotInfo* info = &slots[i];
        char playtime[32];

        if(!info->occupied)
        {
            snprintf(line, sizeof(line), "%c Slot %d: [Empty]", (i == selected_index) ? (delete_mode ? '!' : '>') : ' ', i + 1);
            draw_content_line(row++, line);
            continue;
        }

        startup_format_playtime(info->playtime_seconds, playtime, sizeof(playtime));
        snprintf(line,
                 sizeof(line),
                 "%c Slot %d: %s | Lv %d | %s | %s | C %s | S %s",
                 (i == selected_index) ? (delete_mode ? '!' : '>') : ' ',
                 i + 1,
                 info->player_name,
                 info->level,
                 info->area_name,
                 playtime,
                 startup_ts_or_unknown(info->created_timestamp),
                 startup_ts_or_unknown(info->last_saved_timestamp));
        draw_content_line(row++, line);
    }

    if(footer && footer[0] && bottom_line > 0)
        draw_content_line(bottom_line - 1, footer);

    draw_content_line(bottom_line, status && status[0] ? status : "Ready.");
    fflush(stdout);
}

// Render startup settings screen and current values.
static void draw_settings_menu(const StartupSettings* settings, int selected_index, const char* status)
{
    char line[STARTUP_LINE_LENGTH];
    int bottom_line = startup_content_lines() - 1;
    int row = 2;
    int action_row;
    int preset_index = settings_match_preset_index(settings);
    const char* preset_name = (preset_index >= 0) ? display_presets[preset_index].name : "Custom";
    const char* preset_target = (preset_index >= 0) ? display_presets[preset_index].target_resolution : "manual";
    int verified = (preset_index >= 0) ? display_presets[preset_index].verified : 0;

    if(bottom_line < 0) bottom_line = 0;

    startup_begin_screen("Settings");
    draw_content_line(0, "W/X select | A/D or Left/Right change | S/Enter confirm | Esc/B cancel");
    draw_content_line(1, "");

    snprintf(line, sizeof(line), "%c Display Preset: %s (target %s)",
        (selected_index == SETTINGS_MENU_PRESET) ? '>' : ' ',
        preset_name,
        preset_target);
    draw_content_line(row++, line);

    if(settings)
    {
        int palette_mode = settings->color_palette_mode;
        if(palette_mode < COLOR_PALETTE_MODE_16 || palette_mode > COLOR_PALETTE_MODE_TRUECOLOR)
            palette_mode = COLOR_PALETTE_MODE_16;

        snprintf(line, sizeof(line), "%c Color Palette: %s",
            (selected_index == SETTINGS_MENU_COLOR_PALETTE) ? '>' : ' ',
            color_palette_mode_names[palette_mode]);
    }
    else
    {
        snprintf(line, sizeof(line), "%c Color Palette: %s",
            (selected_index == SETTINGS_MENU_COLOR_PALETTE) ? '>' : ' ',
            color_palette_mode_names[COLOR_PALETTE_MODE_16]);
    }
    draw_content_line(row++, line);

    snprintf(line, sizeof(line), "%c Music: %s",
        (selected_index == SETTINGS_MENU_MUSIC_ENABLED) ? '>' : ' ',
        settings && settings->music_enabled ? "On" : "Off");
    draw_content_line(row++, line);

    snprintf(line, sizeof(line), "%c Music Volume: %d/10",
        (selected_index == SETTINGS_MENU_MUSIC_VOLUME) ? '>' : ' ',
        settings ? settings->music_volume : 10);
    draw_content_line(row++, line);

    snprintf(line, sizeof(line), "%c Viewport Width: %d (range %d-%d)",
        (selected_index == SETTINGS_MENU_VIEWPORT_WIDTH) ? '>' : ' ',
        settings ? settings->viewport_width : LAYOUT_VIEWPORT_WIDTH_DEFAULT,
        LAYOUT_VIEWPORT_WIDTH_MIN,
        LAYOUT_VIEWPORT_WIDTH_MAX);
    draw_content_line(row++, line);

    snprintf(line, sizeof(line), "%c Viewport Height: %d (range %d-%d)",
        (selected_index == SETTINGS_MENU_VIEWPORT_HEIGHT) ? '>' : ' ',
        settings ? settings->viewport_height : LAYOUT_VIEWPORT_HEIGHT_DEFAULT,
        LAYOUT_VIEWPORT_HEIGHT_MIN,
        LAYOUT_VIEWPORT_HEIGHT_MAX);
    draw_content_line(row++, line);

    snprintf(line, sizeof(line), "%c HUD Height: %d (range %d-%d)",
        (selected_index == SETTINGS_MENU_HUD_HEIGHT) ? '>' : ' ',
        settings ? settings->hud_height : LAYOUT_HUD_HEIGHT_DEFAULT,
        LAYOUT_HUD_HEIGHT_MIN,
        LAYOUT_HUD_HEIGHT_MAX);
    draw_content_line(row++, line);

    snprintf(line, sizeof(line), "%c Log Height: %d (range %d-%d)",
        (selected_index == SETTINGS_MENU_LOG_HEIGHT) ? '>' : ' ',
        settings ? settings->log_height : LAYOUT_LOG_HEIGHT_DEFAULT,
        LAYOUT_LOG_HEIGHT_MIN,
        LAYOUT_LOG_HEIGHT_MAX);
    draw_content_line(row++, line);

    if(settings_menu_item_visible(SETTINGS_MENU_VERIFIED, settings))
    {
        snprintf(line, sizeof(line), "%c Verified: %d (debug only)",
            (selected_index == SETTINGS_MENU_VERIFIED) ? '>' : ' ',
            verified);
        draw_content_line(row++, line);
    }

    action_row = row;
    if(action_row > bottom_line - 2)
        action_row = bottom_line - 2;
    if(action_row < 0)
        action_row = 0;

    for(int clear_row = row; clear_row < action_row; clear_row++)
        draw_content_line(clear_row, "");

    snprintf(line, sizeof(line), "%c Save and Back", (selected_index == SETTINGS_MENU_SAVE_AND_BACK) ? '>' : ' ');
    draw_content_line(action_row, line);

    snprintf(line, sizeof(line), "%c Cancel", (selected_index == SETTINGS_MENU_CANCEL) ? '>' : ' ');
    draw_content_line(action_row + 1, line);

    for(int clear_row = action_row + 2; clear_row < bottom_line; clear_row++)
        draw_content_line(clear_row, "");

    draw_content_line(bottom_line, status && status[0] ? status : "Choose a preset or tune values, then Save and Back.");
    fflush(stdout);
}

// Run settings interaction loop; returns 1 when saved, 0 when canceled.
static int startup_run_settings_menu_loop(StartupSettings* settings, char* out_status, size_t out_status_size)
{
    StartupSettings committed_settings;
    StartupSettings working_settings;
    int settings_selected_index = 0;
    char settings_status[STARTUP_LINE_LENGTH] = "";

    if(!settings)
        return 0;

    committed_settings = *settings;
    working_settings = *settings;

    while(1)
    {
        int key;

        apply_layout_from_settings(&working_settings);
        draw_settings_menu(&working_settings, settings_selected_index, settings_status);
        key = read_input_key();

        if(key == 27 || key == 'b' || key == 'B' || key == 'q' || key == 'Q')
        {
            *settings = committed_settings;
            apply_layout_from_settings(settings);
            if(out_status && out_status_size > 0)
                snprintf(out_status, out_status_size, "Settings unchanged.");
            return 0;
        }

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            settings_selected_index = settings_menu_next_visible_index(settings_selected_index, -1, &working_settings);
            settings_status[0] = '\0';
            continue;
        }

        if(KEYBIND_DOWN(key))
        {
            settings_selected_index = settings_menu_next_visible_index(settings_selected_index, 1, &working_settings);
            settings_status[0] = '\0';
            continue;
        }

        if((key == 'a' || key == 'A' || key == INPUT_KEY_LEFT) && settings_menu_is_adjustable(settings_selected_index))
        {
            int changed = settings_adjust_value(&working_settings, settings_selected_index, -1);
            apply_layout_from_settings(&working_settings);
            if(settings_selected_index == SETTINGS_MENU_MUSIC_VOLUME && changed)
                audio_set_volume(working_settings.music_volume);
            snprintf(settings_status, sizeof(settings_status), "%s %s.", settings_key_label(settings_selected_index), changed ? "updated" : "already at minimum");
            continue;
        }

        if((key == 'a' || key == 'A' || key == INPUT_KEY_LEFT) && settings_selected_index == SETTINGS_MENU_COLOR_PALETTE)
        {
            int palette_mode = working_settings.color_palette_mode;

            if(palette_mode <= COLOR_PALETTE_MODE_16)
                palette_mode = COLOR_PALETTE_MODE_TRUECOLOR;
            else
                palette_mode = (ColorPaletteMode)(palette_mode - 1);

            working_settings.color_palette_mode = palette_mode;
            color_palette_set_mode(palette_mode);
            snprintf(settings_status, sizeof(settings_status), "Color palette: %s.", color_palette_mode_names[palette_mode]);
            continue;
        }

        if((key == 'a' || key == 'A' || key == INPUT_KEY_LEFT) && settings_selected_index == SETTINGS_MENU_MUSIC_ENABLED)
        {
            working_settings.music_enabled = !working_settings.music_enabled;
            if(working_settings.music_enabled)
            {
                audio_set_volume(working_settings.music_volume);
                audio_play_music("data/audio/Under.mid", 1);
            }
            else
            {
                audio_stop_music();
            }
            snprintf(settings_status, sizeof(settings_status), "Music %s.", working_settings.music_enabled ? "enabled" : "disabled");
            continue;
        }

        if((key == 'a' || key == 'A' || key == INPUT_KEY_LEFT) && settings_selected_index == SETTINGS_MENU_PRESET)
        {
            int current_preset = settings_match_preset_index(&working_settings);
            int next_preset;

            if(current_preset < 0)
                next_preset = DISPLAY_PRESET_COUNT - 1;
            else
                next_preset = (current_preset + DISPLAY_PRESET_COUNT - 1) % DISPLAY_PRESET_COUNT;

            settings_apply_preset(&working_settings, next_preset);
            apply_layout_from_settings(&working_settings);
            snprintf(settings_status, sizeof(settings_status), "Preset applied: %s (%s).",
                display_presets[next_preset].name,
                display_presets[next_preset].target_resolution);
            continue;
        }

        if((key == 'd' || key == 'D' || key == INPUT_KEY_RIGHT) && settings_selected_index == SETTINGS_MENU_COLOR_PALETTE)
        {
            int palette_mode = working_settings.color_palette_mode;

            if(palette_mode >= COLOR_PALETTE_MODE_TRUECOLOR)
                palette_mode = COLOR_PALETTE_MODE_16;
            else
                palette_mode = (ColorPaletteMode)(palette_mode + 1);

            working_settings.color_palette_mode = palette_mode;
            color_palette_set_mode(palette_mode);
            snprintf(settings_status, sizeof(settings_status), "Color palette: %s.", color_palette_mode_names[palette_mode]);
            continue;
        }

        if((key == 'd' || key == 'D' || key == INPUT_KEY_RIGHT) && settings_selected_index == SETTINGS_MENU_MUSIC_ENABLED)
        {
            working_settings.music_enabled = !working_settings.music_enabled;
            if(working_settings.music_enabled)
            {
                audio_set_volume(working_settings.music_volume);
                audio_play_music("data/audio/Under.mid", 1);
            }
            else
            {
                audio_stop_music();
            }
            snprintf(settings_status, sizeof(settings_status), "Music %s.", working_settings.music_enabled ? "enabled" : "disabled");
            continue;
        }

        if((key == 'd' || key == 'D' || key == INPUT_KEY_RIGHT) && settings_menu_is_adjustable(settings_selected_index))
        {
            int changed = settings_adjust_value(&working_settings, settings_selected_index, 1);
            apply_layout_from_settings(&working_settings);
            if(settings_selected_index == SETTINGS_MENU_MUSIC_VOLUME && changed)
                audio_set_volume(working_settings.music_volume);
            snprintf(settings_status, sizeof(settings_status), "%s %s.", settings_key_label(settings_selected_index), changed ? "updated" : "already at maximum");
            continue;
        }

        if((key == 'd' || key == 'D' || key == INPUT_KEY_RIGHT) && settings_selected_index == SETTINGS_MENU_PRESET)
        {
            int current_preset = settings_match_preset_index(&working_settings);
            int next_preset;

            if(current_preset < 0)
                next_preset = 0;
            else
                next_preset = (current_preset + 1) % DISPLAY_PRESET_COUNT;

            settings_apply_preset(&working_settings, next_preset);
            apply_layout_from_settings(&working_settings);
            snprintf(settings_status, sizeof(settings_status), "Preset applied: %s (%s).",
                display_presets[next_preset].name,
                display_presets[next_preset].target_resolution);
            continue;
        }

        if(key != 13)
            continue;

        if(settings_selected_index == SETTINGS_MENU_MUSIC_ENABLED)
        {
            working_settings.music_enabled = !working_settings.music_enabled;
            if(working_settings.music_enabled)
            {
                audio_set_volume(working_settings.music_volume);
                audio_play_music("data/audio/Under.mid", 1);
            }
            else
            {
                audio_stop_music();
            }
            snprintf(settings_status, sizeof(settings_status), "Music %s.", working_settings.music_enabled ? "enabled" : "disabled");
            continue;
        }

        if(settings_selected_index == SETTINGS_MENU_SAVE_AND_BACK)
        {
            StartupSettingsResult save_result;

            *settings = working_settings;
            startup_settings_sanitize(settings);
            apply_layout_from_settings(settings);

            save_result = startup_settings_save(STARTUP_SETTINGS_FILE, settings);
            if(out_status && out_status_size > 0)
            {
                if(save_result == STARTUP_SETTINGS_RESULT_IO_ERROR)
                    snprintf(out_status, out_status_size, "Settings applied, but save failed.");
                else
                    snprintf(out_status, out_status_size, "Settings saved.");
            }
            return 1;
        }

        if(settings_selected_index == SETTINGS_MENU_CANCEL)
        {
            *settings = committed_settings;
            apply_layout_from_settings(settings);
            if(out_status && out_status_size > 0)
                snprintf(out_status, out_status_size, "Settings unchanged.");
            return 0;
        }
    }
}

// Render generic two-line info page.
static void draw_info_page(const char* title, const char* l1, const char* l2)
{
    int bottom_line = startup_content_lines() - 1;
    if(bottom_line < 0) bottom_line = 0;

    startup_begin_screen(title);
    draw_content_line(2, l1);
    draw_content_line(4, l2);
    draw_content_line(bottom_line, "Press Enter, Esc, B, or Q to go back.");
    fflush(stdout);
}

// Render credits page.
static void draw_credits_page(void)
{
    int bottom_line = startup_content_lines() - 1;
    if(bottom_line < 0) bottom_line = 0;

    startup_begin_screen("Credits");
    draw_content_line(1, "Peli project");
    draw_content_line(3, "Built as a C terminal roguelike prototype.");
    draw_content_line(5, "Gameplay systems: map generation, combat, inventory, and overlays.");
    draw_content_line(7, "Current batch: startup splash/menu + deferred world initialization.");
    draw_content_line(bottom_line, "Press Enter, Esc, B, or Q to go back.");
    fflush(stdout);
}

// Wait for one of the accepted "back" keys.
static void wait_for_back_key(void)
{
    while(1)
    {
        int key = read_input_key();
        if(startup_is_back_key(key))
            return;
    }
}

// Run startup flow until user starts game or quits.
StartupAction startup_run(StartupSettings* settings)
{
    StartupSettings local_settings;
    StartupState state = STARTUP_STATE_SPLASH;
    int selected_index = 0;
    int slot_selected_index = 0;
    int delete_mode = 0;
    int menu_music_started = 0;
    SavegameSlotInfo slot_infos[SAVEGAME_SLOT_COUNT];
    int slot_count = SAVEGAME_SLOT_COUNT;
    char status[STARTUP_LINE_LENGTH] = "";

    if(settings)
    {
        startup_settings_sanitize(settings);
    }
    else
    {
        startup_settings_defaults(&local_settings);
        settings = &local_settings;
    }

    startup_reset_surface_cache();
    apply_layout_from_settings(settings);

    while(1)
    {
        int key;

        if(state == STARTUP_STATE_SPLASH)
        {
            draw_splash_screen();
            wait_for_splash_advance();
            state = STARTUP_STATE_MENU;
            status[0] = '\0';
            continue;
        }

        if(state == STARTUP_STATE_CHARACTER_CREATOR)
        {
            char name_draft[64] = "";
            char creator_status[STARTUP_LINE_LENGTH] = "Enter your character name.";
            int name_len = 0;

            if(settings->player_name[0] != '\0')
            {
                strcpy(name_draft, settings->player_name);
                name_len = strlen(name_draft);
            }

            while(1)
            {
                draw_character_creator(name_draft, creator_status);
                key = read_input_key();

                if(key == 27)  // Escape - go back to menu
                {
                    state = STARTUP_STATE_MENU;
                    settings->has_player_starting_attributes = 0;
                    status[0] = '\0';
                    break;
                }

                if(key == 13)  // Enter - confirm
                {
                    if(name_len > 0)
                    {
                        strcpy(settings->player_name, name_draft);
                        settings->has_player_starting_attributes = 0;
                        state = STARTUP_STATE_RACE_SELECT;
                        status[0] = '\0';
                        break;
                    }
                    else
                    {
                        snprintf(creator_status, sizeof(creator_status), "Name cannot be empty.");
                    }
                    continue;
                }

                if((key == 8 || key == 127) && name_len > 0)  // Backspace/Delete
                {
                    name_len--;
                    name_draft[name_len] = '\0';
                    snprintf(creator_status, sizeof(creator_status), "");
                    continue;
                }

                if(key >= 32 && key <= 126 && name_len < 63)  // Printable characters
                {
                    name_draft[name_len++] = (char)key;
                    name_draft[name_len] = '\0';
                    snprintf(creator_status, sizeof(creator_status), "");
                    continue;
                }
            }
            continue;
        }

        if(state == STARTUP_STATE_RACE_SELECT)
        {
            int selected_race = 0;
            int scroll_offset = 0;
            int race_count;
            char race_status[STARTUP_LINE_LENGTH] = "Select your race.";
            Actor rolled_actor;
            int has_roll = 0;

            if(!template_content_load_all())
            {
                state = STARTUP_STATE_MENU;
                snprintf(status, sizeof(status), "Could not load races: %s", template_content_last_error());
                continue;
            }

            race_count = race_templates_count();
            if(race_count <= 0)
            {
                state = STARTUP_STATE_MENU;
                snprintf(status, sizeof(status), "No race templates were loaded.");
                continue;
            }

            if(settings->player_race_id[0] != '\0')
            {
                for(int i = 0; i < race_count; i++)
                {
                    const RaceTemplate* race = race_template_at(i);
                    if(race && strcmp(race->id, settings->player_race_id) == 0)
                    {
                        selected_race = i;
                        break;
                    }
                }
            }

            while(1)
            {
                int key;
                int list_rows;

                list_rows = startup_race_selector_list_rows();

                if(selected_race < scroll_offset)
                    scroll_offset = selected_race;
                if(selected_race >= scroll_offset + list_rows)
                    scroll_offset = selected_race - list_rows + 1;

                {
                    const RaceTemplate* selected_template = race_template_at(selected_race);
                    if(selected_template && (!has_roll || strcmp(rolled_actor.race_id, selected_template->id) != 0))
                    {
                        race_roll_average_attributes(&rolled_actor, selected_template);
                        has_roll = 1;
                    }
                }

                draw_race_selector(selected_race,
                                   scroll_offset,
                                   settings->player_name,
                                   has_roll ? &rolled_actor : NULL,
                                   race_status);
                key = read_input_key();

                if(key == 27)
                {
                    state = STARTUP_STATE_MENU;
                    snprintf(status, sizeof(status), "Start game canceled.");
                    break;
                }

                if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
                {
                    if(selected_race > 0)
                        selected_race--;
                    continue;
                }

                if(KEYBIND_DOWN(key))
                {
                    if(selected_race < race_count - 1)
                        selected_race++;
                    continue;
                }

                if(key == 'r' || key == 'R')
                {
                    const RaceTemplate* selected_template = race_template_at(selected_race);
                    if(selected_template)
                    {
                        race_roll_average_attributes(&rolled_actor, selected_template);
                        has_roll = 1;
                        snprintf(race_status, sizeof(race_status), "Stats rerolled.");
                    }
                    continue;
                }

                if(!KEYBIND_SELECT(key))
                    continue;

                {
                    const RaceTemplate* chosen = race_template_at(selected_race);
                    if(chosen)
                    {
                        snprintf(settings->player_race_id, sizeof(settings->player_race_id), "%s", chosen->id);
                        if(has_roll)
                        {
                            settings->player_starting_attributes = rolled_actor;
                            settings->has_player_starting_attributes = 1;
                        }
                        else
                        {
                            memset(&settings->player_starting_attributes, 0, sizeof(settings->player_starting_attributes));
                            settings->has_player_starting_attributes = 0;
                        }
                        return STARTUP_ACTION_START_GAME;
                    }
                }

                snprintf(race_status, sizeof(race_status), "Unable to select race. Try again.");
            }

            continue;
        }

        if(state == STARTUP_STATE_MENU)
        {
            if(settings->music_enabled)
            {
                audio_set_volume(settings->music_volume);
                if(!audio_is_playing())
                {
                    if(!audio_play_music("data/audio/Under.mid", 1))
                        fprintf(stderr, "Failed to start menu music.\n");
                    menu_music_started = 1;
                }
            }
            else if(audio_is_playing())
            {
                audio_stop_music();
                menu_music_started = 0;
            }
            audio_tick();
            draw_main_menu(selected_index, status);
            key = read_input_key();

            if(key == 'q' || key == 'Q')
                return STARTUP_ACTION_QUIT;

            if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
            {
                selected_index--;
                if(selected_index < 0)
                    selected_index = STARTUP_MENU_ITEM_COUNT - 1;
                status[0] = '\0';
                continue;
            }

            if(KEYBIND_DOWN(key))
            {
                selected_index++;
                if(selected_index >= STARTUP_MENU_ITEM_COUNT)
                    selected_index = 0;
                status[0] = '\0';
                continue;
            }

            if(!KEYBIND_SELECT(key))
                continue;

            if(selected_index == 0)
            {
                int occupied_count;
                int first_empty;

                (void)savegame_list_slots(slot_infos, slot_count);
                occupied_count = startup_count_occupied_slots(slot_infos, slot_count);
                first_empty = startup_first_empty_slot(slot_infos, slot_count);

                if(first_empty >= 0)
                {
                    settings->selected_save_slot = first_empty + 1;
                    state = STARTUP_STATE_CHARACTER_CREATOR;
                    snprintf(status, sizeof(status), "New game will use Slot %d.", settings->selected_save_slot);
                    continue;
                }

                if(occupied_count >= SAVEGAME_SLOT_COUNT)
                {
                    slot_selected_index = (settings->selected_save_slot - 1);
                    if(slot_selected_index < 0 || slot_selected_index >= slot_count)
                        slot_selected_index = 0;
                    state = STARTUP_STATE_NEW_GAME_OVERWRITE_SLOT;
                    snprintf(status, sizeof(status), "All slots are full. Pick a slot to overwrite.");
                    continue;
                }

                state = STARTUP_STATE_CHARACTER_CREATOR;
                status[0] = '\0';
                continue;
            }
            if(selected_index == 1)
            {
                int occupied_count;
                int first_occupied;

                (void)savegame_list_slots(slot_infos, slot_count);
                occupied_count = startup_count_occupied_slots(slot_infos, slot_count);
                first_occupied = startup_first_occupied_slot(slot_infos, slot_count);

                if(occupied_count == 0)
                {
                    state = STARTUP_STATE_CONTINUE_STUB;
                    continue;
                }

                if(occupied_count == 1 && first_occupied >= 0)
                {
                    settings->selected_save_slot = first_occupied + 1;
                    return STARTUP_ACTION_CONTINUE_GAME;
                }

                slot_selected_index = (settings->selected_save_slot - 1);
                if(slot_selected_index < 0 || slot_selected_index >= slot_count)
                    slot_selected_index = 0;
                delete_mode = 0;
                state = STARTUP_STATE_CONTINUE_SLOT_SELECT;
            }
            else if(selected_index == 2)
            {
                state = STARTUP_STATE_SETTINGS;
            }
            else if(selected_index == 3)
                state = STARTUP_STATE_CREDITS;
            else
                return STARTUP_ACTION_QUIT;

            continue;
        }

        if(state == STARTUP_STATE_CREDITS)
        {
            draw_credits_page();
            wait_for_back_key();
            state = STARTUP_STATE_MENU;
            status[0] = '\0';
            continue;
        }

        if(state == STARTUP_STATE_CONTINUE_STUB)
        {
            draw_info_page("Continue", "No saved game found.", "Start a new game first to create a save.");
            wait_for_back_key();
            state = STARTUP_STATE_MENU;
            snprintf(status, sizeof(status), "Continue is unavailable without a save.");
            continue;
        }

        if(state == STARTUP_STATE_CONTINUE_SLOT_SELECT)
        {
            draw_save_slot_menu("Continue: Select Slot",
                                slot_infos,
                                slot_count,
                                slot_selected_index,
                                status,
                                "Only occupied slots can be loaded.",
                                delete_mode);
            key = read_input_key();

            if(startup_is_cancel_key(key))
            {
                if(delete_mode)
                {
                    delete_mode = 0;
                    startup_reset_surface_cache();
                    status[0] = '\0';
                    continue;
                }
                state = STARTUP_STATE_MENU;
                snprintf(status, sizeof(status), "Continue canceled.");
                continue;
            }

            if(key == INPUT_KEY_DEL)
            {
                delete_mode = !delete_mode;
                startup_reset_surface_cache();
                status[0] = '\0';
                continue;
            }

            if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
            {
                slot_selected_index--;
                if(slot_selected_index < 0)
                    slot_selected_index = slot_count - 1;
                status[0] = '\0';
                continue;
            }

            if(KEYBIND_DOWN(key))
            {
                slot_selected_index++;
                if(slot_selected_index >= slot_count)
                    slot_selected_index = 0;
                status[0] = '\0';
                continue;
            }

            if(!KEYBIND_SELECT(key))
                continue;

            if(delete_mode)
            {
                if(!slot_infos[slot_selected_index].occupied)
                {
                    snprintf(status, sizeof(status), "Slot %d is already empty.", slot_selected_index + 1);
                    continue;
                }
                startup_begin_screen("Delete Save");
                draw_content_line(0, "Are you sure you want to delete this save? This cannot be undone.");
                draw_content_line(1, "");
                {
                    char confirm_line[STARTUP_LINE_LENGTH];
                    snprintf(confirm_line, sizeof(confirm_line),
                             "Slot %d: %s | Lv %d | %s",
                             slot_selected_index + 1,
                             slot_infos[slot_selected_index].player_name,
                             slot_infos[slot_selected_index].level,
                             slot_infos[slot_selected_index].area_name);
                    draw_content_line(2, confirm_line);
                }
                draw_content_line(3, "");
                draw_content_line(4, "Press Y or Enter to confirm. Any other key cancels.");
                fflush(stdout);
                key = read_input_key();
                if(key == 'y' || key == 'Y' || key == 13)
                {
                    if(savegame_delete_slot(slot_selected_index + 1))
                    {
                        (void)savegame_list_slots(slot_infos, slot_count);
                        snprintf(status, sizeof(status), "Slot %d deleted.", slot_selected_index + 1);
                    }
                    else
                    {
                        snprintf(status, sizeof(status), "Failed to delete Slot %d.", slot_selected_index + 1);
                    }
                    delete_mode = 0;
                }
                else
                {
                    snprintf(status, sizeof(status), "Delete canceled.");
                }
                startup_reset_surface_cache();
                continue;
            }

            if(slot_infos[slot_selected_index].occupied)
            {
                settings->selected_save_slot = slot_selected_index + 1;
                return STARTUP_ACTION_CONTINUE_GAME;
            }

            snprintf(status, sizeof(status), "Slot %d is empty.", slot_selected_index + 1);
            continue;
        }

        if(state == STARTUP_STATE_NEW_GAME_OVERWRITE_SLOT)
        {
            draw_save_slot_menu("New Game: Overwrite Slot",
                                slot_infos,
                                slot_count,
                                slot_selected_index,
                                status,
                                "All slots are full. Select one slot to overwrite.",
                                0);
            key = read_input_key();

            if(startup_is_cancel_key(key))
            {
                state = STARTUP_STATE_MENU;
                snprintf(status, sizeof(status), "Start game canceled.");
                continue;
            }

            if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
            {
                slot_selected_index--;
                if(slot_selected_index < 0)
                    slot_selected_index = slot_count - 1;
                status[0] = '\0';
                continue;
            }

            if(KEYBIND_DOWN(key))
            {
                slot_selected_index++;
                if(slot_selected_index >= slot_count)
                    slot_selected_index = 0;
                status[0] = '\0';
                continue;
            }

            if(!KEYBIND_SELECT(key))
                continue;

            settings->selected_save_slot = slot_selected_index + 1;
            state = STARTUP_STATE_CHARACTER_CREATOR;
            snprintf(status, sizeof(status), "New game will overwrite Slot %d.", settings->selected_save_slot);
            continue;
        }

        if(state == STARTUP_STATE_SETTINGS)
        {
            (void)startup_run_settings_menu_loop(settings, status, sizeof(status));
            state = STARTUP_STATE_MENU;
            continue;
        }
    }
}

int startup_open_settings_menu(StartupSettings* settings)
{
    char status[STARTUP_LINE_LENGTH];

    if(!settings)
        return 0;

    startup_settings_sanitize(settings);
    apply_layout_from_settings(settings);
    startup_reset_surface_cache();
    status[0] = '\0';
    return startup_run_settings_menu_loop(settings, status, sizeof(status));
}