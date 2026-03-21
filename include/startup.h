#ifndef STARTUP_H
#define STARTUP_H

/*
 * Purpose:
 *   Declares startup flow results, startup settings persistence, and startup UI entry point.
 */

#define STARTUP_SETTINGS_FILE "settings.ini"

typedef struct StartupSettings
{
    int viewport_width;
    int viewport_height;
    int hud_height;
    int log_height;
} StartupSettings;

typedef enum StartupSettingsResult
{
    STARTUP_SETTINGS_RESULT_OK = 0,
    STARTUP_SETTINGS_RESULT_MISSING,
    STARTUP_SETTINGS_RESULT_INVALID,
    STARTUP_SETTINGS_RESULT_IO_ERROR
} StartupSettingsResult;

typedef enum StartupAction
{
    STARTUP_ACTION_START_GAME = 0,
    STARTUP_ACTION_CONTINUE_GAME,
    STARTUP_ACTION_QUIT
} StartupAction;

// Fill settings with default values.
void startup_settings_defaults(StartupSettings* out);

// Clamp settings to supported ranges.
void startup_settings_sanitize(StartupSettings* settings);

// Load settings from disk into out (always sanitized on return).
StartupSettingsResult startup_settings_load(const char* path, StartupSettings* out);

// Save settings to disk.
StartupSettingsResult startup_settings_save(const char* path, const StartupSettings* settings);

// Run splash/menu flow and return selected action for main loop.
StartupAction startup_run(StartupSettings* settings);

#endif