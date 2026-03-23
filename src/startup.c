#include "startup.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "draw.h"
#include "input.h"
#include "layout.h"
#include "savegame.h"
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
#define SETTINGS_MENU_ITEM_COUNT 6
#define SPLASH_TIMEOUT_MS 30000

typedef enum StartupState
{
    STARTUP_STATE_SPLASH = 0,
    STARTUP_STATE_CHARACTER_CREATOR,
    STARTUP_STATE_MENU,
    STARTUP_STATE_CREDITS,
    STARTUP_STATE_CONTINUE_STUB,
    STARTUP_STATE_SETTINGS
} StartupState;

typedef enum SettingsMenuItem
{
    SETTINGS_MENU_VIEWPORT_WIDTH = 0,
    SETTINGS_MENU_VIEWPORT_HEIGHT,
    SETTINGS_MENU_HUD_HEIGHT,
    SETTINGS_MENU_LOG_HEIGHT,
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

/**
 * @brief Get the display label for a settings menu field by index.
 * @param field_index The SettingsMenuItem enum value.
 * @return A human-readable field name (e.g., "Viewport width", "HUD height").
 */
static const char* settings_key_label(int field_index)
{
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
    return menu_index == SETTINGS_MENU_VIEWPORT_WIDTH ||
           menu_index == SETTINGS_MENU_VIEWPORT_HEIGHT ||
           menu_index == SETTINGS_MENU_HUD_HEIGHT ||
           menu_index == SETTINGS_MENU_LOG_HEIGHT;
}

// Change one settings value by delta and clamp to supported bounds.
static int settings_adjust_value(StartupSettings* settings, int menu_index, int delta)
{
    int before;

    if(!settings || !settings_menu_is_adjustable(menu_index))
        return 0;

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
    out->dev_test_loot = 0;
    strcpy(out->player_name, "Hero");
}

// Clamp settings to supported panel ranges.
void startup_settings_sanitize(StartupSettings* settings)
{
    if(!settings) return;

    settings->viewport_width = layout_clamp_viewport_width(settings->viewport_width);
    settings->viewport_height = layout_clamp_viewport_height(settings->viewport_height);
    settings->hud_height = layout_clamp_hud_height(settings->hud_height);
    settings->log_height = layout_clamp_log_height(settings->log_height);
    settings->dev_test_loot = settings->dev_test_loot ? 1 : 0;
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
    fprintf(file, "dev_test_loot=%d\n", sanitized.dev_test_loot);

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
                            strcmp(key, "dev_test_loot") != 0)
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
        else if(strcmp(key, "dev_test_loot") == 0)
            out->dev_test_loot = ((int)parsed) ? 1 : 0;
        else
            out->log_height = (int)parsed;

        saw_known_key = 1;
    }

    if(fclose(file) != 0)
    {
        startup_settings_defaults(out);
        return STARTUP_SETTINGS_RESULT_IO_ERROR;
    }

    startup_settings_sanitize(out);

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
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((unsigned int)(ms * 1000));
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

// Draw one content line in startup frame.
static void draw_content_line(int content_line, const char* text)
{
    UiFrame frame = startup_frame();
    ui_frame_draw_line(&frame, content_line, text);
}

// Clear and draw startup frame with a title.
static void startup_begin_screen(const char* title)
{
    UiFrame frame = startup_frame();

    draw_ensure_console_dimensions();
    system("cls");
    ui_frame_draw(&frame, title);
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

// Render startup main menu with current selection and status.
static void draw_main_menu(int selected_index, const char* status)
{
    char line[STARTUP_LINE_LENGTH];
    int bottom_line = startup_content_lines() - 1;
    if(bottom_line < 0) bottom_line = 0;

    startup_begin_screen("Main Menu");
    draw_content_line(0, "Use W/S or Up/Down to move, Enter to select, Q to quit.");
    draw_content_line(1, "");

    for(int i = 0; i < STARTUP_MENU_ITEM_COUNT; i++)
    {
        snprintf(line, sizeof(line), "%c %s", (i == selected_index) ? '>' : ' ', startup_menu_items[i]);
        draw_content_line(3 + i, line);
    }

    draw_content_line(bottom_line, status && status[0] ? status : "Ready.");
    fflush(stdout);
}

// Render startup settings screen and current values.
static void draw_settings_menu(const StartupSettings* settings, int selected_index, const char* status)
{
    char line[STARTUP_LINE_LENGTH];
    int bottom_line = startup_content_lines() - 1;

    if(bottom_line < 0) bottom_line = 0;

    startup_begin_screen("Settings");
    draw_content_line(0, "W/S select | A/D or Left/Right change | Enter confirm | Esc/B cancel");
    draw_content_line(1, "");

    snprintf(line, sizeof(line), "%c Viewport Width: %d (range %d-%d)",
        (selected_index == SETTINGS_MENU_VIEWPORT_WIDTH) ? '>' : ' ',
        settings ? settings->viewport_width : LAYOUT_VIEWPORT_WIDTH_DEFAULT,
        LAYOUT_VIEWPORT_WIDTH_MIN,
        LAYOUT_VIEWPORT_WIDTH_MAX);
    draw_content_line(3, line);

    snprintf(line, sizeof(line), "%c Viewport Height: %d (range %d-%d)",
        (selected_index == SETTINGS_MENU_VIEWPORT_HEIGHT) ? '>' : ' ',
        settings ? settings->viewport_height : LAYOUT_VIEWPORT_HEIGHT_DEFAULT,
        LAYOUT_VIEWPORT_HEIGHT_MIN,
        LAYOUT_VIEWPORT_HEIGHT_MAX);
    draw_content_line(4, line);

    snprintf(line, sizeof(line), "%c HUD Height: %d (range %d-%d)",
        (selected_index == SETTINGS_MENU_HUD_HEIGHT) ? '>' : ' ',
        settings ? settings->hud_height : LAYOUT_HUD_HEIGHT_DEFAULT,
        LAYOUT_HUD_HEIGHT_MIN,
        LAYOUT_HUD_HEIGHT_MAX);
    draw_content_line(5, line);

    snprintf(line, sizeof(line), "%c Log Height: %d (range %d-%d)",
        (selected_index == SETTINGS_MENU_LOG_HEIGHT) ? '>' : ' ',
        settings ? settings->log_height : LAYOUT_LOG_HEIGHT_DEFAULT,
        LAYOUT_LOG_HEIGHT_MIN,
        LAYOUT_LOG_HEIGHT_MAX);
    draw_content_line(6, line);

    snprintf(line, sizeof(line), "%c Save and Back", (selected_index == SETTINGS_MENU_SAVE_AND_BACK) ? '>' : ' ');
    draw_content_line(8, line);

    snprintf(line, sizeof(line), "%c Cancel", (selected_index == SETTINGS_MENU_CANCEL) ? '>' : ' ');
    draw_content_line(9, line);

    draw_content_line(bottom_line, status && status[0] ? status : "Adjust values, then select Save and Back.");
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
            settings_selected_index--;
            if(settings_selected_index < 0)
                settings_selected_index = SETTINGS_MENU_ITEM_COUNT - 1;
            settings_status[0] = '\0';
            continue;
        }

        if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
        {
            settings_selected_index++;
            if(settings_selected_index >= SETTINGS_MENU_ITEM_COUNT)
                settings_selected_index = 0;
            settings_status[0] = '\0';
            continue;
        }

        if((key == 'a' || key == 'A' || key == INPUT_KEY_LEFT) && settings_menu_is_adjustable(settings_selected_index))
        {
            int changed = settings_adjust_value(&working_settings, settings_selected_index, -1);
            apply_layout_from_settings(&working_settings);
            snprintf(settings_status, sizeof(settings_status), "%s %s.", settings_key_label(settings_selected_index), changed ? "updated" : "already at minimum");
            continue;
        }

        if((key == 'd' || key == 'D' || key == INPUT_KEY_RIGHT) && settings_menu_is_adjustable(settings_selected_index))
        {
            int changed = settings_adjust_value(&working_settings, settings_selected_index, 1);
            apply_layout_from_settings(&working_settings);
            snprintf(settings_status, sizeof(settings_status), "%s %s.", settings_key_label(settings_selected_index), changed ? "updated" : "already at maximum");
            continue;
        }

        if(key != 13)
            continue;

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
                    status[0] = '\0';
                    break;
                }

                if(key == 13)  // Enter - confirm
                {
                    if(name_len > 0)
                    {
                        strcpy(settings->player_name, name_draft);
                        return STARTUP_ACTION_START_GAME;
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

        if(state == STARTUP_STATE_MENU)
        {
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

            if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
            {
                selected_index++;
                if(selected_index >= STARTUP_MENU_ITEM_COUNT)
                    selected_index = 0;
                status[0] = '\0';
                continue;
            }

            if(key != 13)
                continue;

            if(selected_index == 0)
            {
                state = STARTUP_STATE_CHARACTER_CREATOR;
                status[0] = '\0';
                continue;
            }
            if(selected_index == 1)
            {
                if(savegame_exists(SAVEGAME_FILE))
                    return STARTUP_ACTION_CONTINUE_GAME;
                state = STARTUP_STATE_CONTINUE_STUB;
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
    status[0] = '\0';
    return startup_run_settings_menu_loop(settings, status, sizeof(status));
}