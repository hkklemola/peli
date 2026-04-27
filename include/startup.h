#ifndef STARTUP_H
#define STARTUP_H

#include "actor.h"

/*
 * Purpose:
 *   Declares startup flow results, startup settings persistence, and startup UI entry point.
 */

#define STARTUP_SETTINGS_FILE "settings.ini"

/** @struct StartupSettings
 *  @brief Display and UI layout configuration.
 */
typedef struct StartupSettings
{
    /** @brief Width in character cells for the main viewport (game world view). */
    int viewport_width;
    /** @brief Height in character cells for the main viewport (game world view). */
    int viewport_height;
    /** @brief Height in character cells for the HUD panel (stats, equipment, etc.). */
    int hud_height;
    /** @brief Height in character cells for the log/message panel. */
    int log_height;
    /** @brief Color palette mode used for ANSI rendering. */
    int color_palette_mode;
    /** @brief 1 enables menu music, 0 disables it. */
    int music_enabled;
    /** @brief Music volume from 0 (mute) to 10 (max). */
    int music_volume;
    /** @brief 1 enables full dev test loot in Starter Hut chests, 0 keeps one random item per chest. */
    int dev_test_loot;
    /** @brief Active save slot index (1..SAVEGAME_SLOT_COUNT). */
    int selected_save_slot;
    /** @brief Player name for new game character creation. */
    char player_name[64];
    /** @brief Selected player race id for new game character creation. */
    char player_race_id[ACTOR_RACE_ID_LENGTH];
    /** @brief Optional rolled starting attributes finalized in character creator. */
    Actor player_starting_attributes;
    /** @brief 1 if player_starting_attributes should override race baseline on new game start. */
    int has_player_starting_attributes;
} StartupSettings;

/** @enum StartupSettingsResult
 *  @brief Return status for settings file operations.
 */
typedef enum StartupSettingsResult
{
    /** Settings loaded/saved successfully. */
    STARTUP_SETTINGS_RESULT_OK = 0,
    /** Settings file was not found; defaults were used. */
    STARTUP_SETTINGS_RESULT_MISSING,
    /** Settings file was found but contained invalid values. */
    STARTUP_SETTINGS_RESULT_INVALID,
    /** I/O error occurred while reading/writing settings file. */
    STARTUP_SETTINGS_RESULT_IO_ERROR
} StartupSettingsResult;

/** @enum StartupAction
 *  @brief User action selected at startup menu.
 */
typedef enum StartupAction
{
    /** User selected 'Start New Game'. */
    STARTUP_ACTION_START_GAME = 0,
    /** User selected 'Continue Game' (load save). */
    STARTUP_ACTION_CONTINUE_GAME,
    /** User selected 'Quit' or closed the game. */
    STARTUP_ACTION_QUIT
} StartupAction;

/**
 * @brief Fill settings structure with default values.
 * @param out Pointer to StartupSettings to populate with defaults.
 */
void startup_settings_defaults(StartupSettings* out);

/**
 * @brief Clamp and validate settings to supported ranges.
 * @param settings Pointer to StartupSettings to sanitize in-place.
 */
void startup_settings_sanitize(StartupSettings* settings);

/**
 * @brief Load settings from disk into the output structure.
 *        If load fails, defaults are used and structure is sanitized.
 * @param path File path to read settings from (e.g., "build/settings.ini").
 * @param out Pointer to StartupSettings to populate (always sanitized on return).
 * @return StartupSettingsResult indicating success or failure reason.
 */
StartupSettingsResult startup_settings_load(const char* path, StartupSettings* out);

/**
 * @brief Save settings to disk in INI format.
 * @param path File path to write settings to (e.g., "build/settings.ini").
 * @param settings Pointer to StartupSettings to save.
 * @return StartupSettingsResult indicating success or failure reason.
 */
StartupSettingsResult startup_settings_save(const char* path, const StartupSettings* settings);

/**
 * @brief Run the startup splash screen, menu flow, and return selected action.
 *        Updates settings from user input (e.g., display preferences).
 * @param settings Pointer to StartupSettings (may be modified from menu choices).
 * @return The startup action selected by the user (new game, continue, quit).
 */
StartupAction startup_run(StartupSettings* settings);

/**
 * @brief Open the settings menu directly during gameplay.
 *        Applies layout changes immediately; saves only on explicit save action.
 * @param settings Pointer to StartupSettings (updated in-place when saved).
 * @return 1 if settings were saved, 0 if canceled.
 */
int startup_open_settings_menu(StartupSettings* settings);

#endif