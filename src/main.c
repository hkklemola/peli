#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>

#include "atlas.h"
#include "player.h"
#include "bestiary.h"
#include "spawn.h"
#include "draw.h"
#include "map.h"
#include "movement.h"
#include "collision.h"
#include "combat.h"
#include "atlas_overlay.h"
#include "overlay_nav.h"
#include "input.h"
#include "log.h"
#include "layout.h"
#include "savegame.h"
#include "startup.h"
#include "target_lock.h"
#include "interact.h"
#include "template_content.h"
#include "item_data.h"
#include "ui_frame.h"
#include "ui_overlay.h"
#include "world_items.h"
#include "world_map.h"
#include "world_map_overlay.h"
#include "keybind_helpers.h"
#include "npc.h"

static void spawn_initial_monsters(void);
static void spawn_initial_npcs(void);
static int ranged_attack_mode(Player* p);
static char g_active_save_path[SAVEGAME_SLOT_PATH_LENGTH] = "savegame_slot_1.ini";
static time_t g_session_start_time = 0;
static unsigned long long g_session_base_playtime = 0ULL;

#define CAMP_OPTION_MAX 9

typedef enum InGameSystemMenuAction
{
    INGAME_SYSTEM_MENU_RESUME = 0,
    INGAME_SYSTEM_MENU_QUIT
} InGameSystemMenuAction;

static void active_save_set_slot(int slot_index)
{
    savegame_build_slot_path(slot_index, g_active_save_path, sizeof(g_active_save_path));
}

static void game_session_begin(const Player* p)
{
    g_session_start_time = time(NULL);
    g_session_base_playtime = p ? p->playtime_seconds : 0ULL;
}

static void game_session_sync_playtime(Player* p)
{
    time_t now;

    if(!p || g_session_start_time <= 0)
        return;

    now = time(NULL);
    if(now < g_session_start_time)
        return;

    p->playtime_seconds = g_session_base_playtime + (unsigned long long)(now - g_session_start_time);
}

static int save_active_game(Player* p)
{
    game_session_sync_playtime(p);
    return savegame_save(g_active_save_path, p);
}

static int file_exists_on_disk(const char* path)
{
    FILE* file;

    if(!path || path[0] == '\0')
        return 0;

    file = fopen(path, "rb");
    if(!file)
        return 0;

    fclose(file);
    return 1;
}

static int file_last_write_time(const char* path, time_t* out_time)
{
    struct stat info;

    if(!path || !out_time || stat(path, &info) != 0)
        return 0;

    *out_time = info.st_mtime;
    return 1;
}

static int select_newest_existing_path(const char* const* candidates,
                                      int candidate_count,
                                      char* out_path,
                                      size_t out_size,
                                      time_t* out_time)
{
    time_t best_time = 0;
    int found = 0;

    if(!candidates || candidate_count <= 0 || !out_path || out_size == 0)
        return 0;

    out_path[0] = '\0';
    if(out_time)
        *out_time = 0;

    for(int i = 0; i < candidate_count; i++)
    {
        time_t candidate_time = 0;

        if(!file_last_write_time(candidates[i], &candidate_time))
            continue;

        if(!found || candidate_time > best_time)
        {
            best_time = candidate_time;
            snprintf(out_path, out_size, "%s", candidates[i]);
            found = 1;
        }
    }

    if(found && out_time)
        *out_time = best_time;

    return found;
}

static int try_run_world_map_sync_launcher(const char* launcher,
                                           const char* script_path,
                                           const char* csv_path,
                                           const char* ods_path,
                                           const char* fods_path)
{
    char command[1024];

    if(!launcher || !script_path || !csv_path || !ods_path || !fods_path)
        return 0;

#ifdef _WIN32
    snprintf(command,
             sizeof(command),
             "%s \"%s\" --prefer-spreadsheet --csv \"%s\" --ods \"%s\" --fods \"%s\" >NUL 2>NUL",
             launcher,
             script_path,
             csv_path,
             ods_path,
             fods_path);
#else
    snprintf(command,
             sizeof(command),
             "%s \"%s\" --prefer-spreadsheet --csv \"%s\" --ods \"%s\" --fods \"%s\" >/dev/null 2>&1",
             launcher,
             script_path,
             csv_path,
             ods_path,
             fods_path);
#endif

    return system(command) == 0;
}

static void refresh_world_map_csv_from_spreadsheet(void)
{
    static const char* csv_path = "data/templates/maps/world_map_tiles.csv";
    static const char* launcher_candidates[] = { "python", "py -3", "py" };
    static const char* script_candidates[] = {
        "tools/generate_world_map_sheet.py",
        "../tools/generate_world_map_sheet.py"
    };
    static const char* ods_candidates[] = {
        "data/templates/maps/world_map_tiles.ods",
        "../data/templates/maps/world_map_tiles.ods",
        "master_data/templates/maps/world_map_tiles.ods",
        "../master_data/templates/maps/world_map_tiles.ods",
        "tools/world_map_tiles.ods",
        "../tools/world_map_tiles.ods"
    };
    static const char* fods_candidates[] = {
        "data/templates/maps/world_map_tiles.fods",
        "../data/templates/maps/world_map_tiles.fods",
        "master_data/templates/maps/world_map_tiles.fods",
        "../master_data/templates/maps/world_map_tiles.fods",
        "tools/world_map_tiles.fods",
        "../tools/world_map_tiles.fods"
    };
    char script_path[260] = "";
    char ods_path[260] = "data/templates/maps/world_map_tiles.ods";
    char fods_path[260] = "data/templates/maps/world_map_tiles.fods";
    time_t csv_time = 0;
    time_t ods_time = 0;
    time_t fods_time = 0;
    int has_csv = file_last_write_time(csv_path, &csv_time);
    int has_ods = select_newest_existing_path(ods_candidates,
                                              (int)(sizeof(ods_candidates) / sizeof(ods_candidates[0])),
                                              ods_path,
                                              sizeof(ods_path),
                                              &ods_time);
    int has_fods = select_newest_existing_path(fods_candidates,
                                               (int)(sizeof(fods_candidates) / sizeof(fods_candidates[0])),
                                               fods_path,
                                               sizeof(fods_path),
                                               &fods_time);

    if(!has_ods && !has_fods)
        return;

    if(has_csv
       && (!has_ods || ods_time <= csv_time)
       && (!has_fods || fods_time <= csv_time))
        return;

    for(int i = 0; i < (int)(sizeof(script_candidates) / sizeof(script_candidates[0])); i++)
    {
        if(file_exists_on_disk(script_candidates[i]))
        {
            snprintf(script_path, sizeof(script_path), "%s", script_candidates[i]);
            break;
        }
    }

    if(script_path[0] == '\0')
    {
        fprintf(stderr, "[world-map] Spreadsheet sync skipped: generator script not found.\n");
        return;
    }

    for(int i = 0; i < (int)(sizeof(launcher_candidates) / sizeof(launcher_candidates[0])); i++)
    {
        if(try_run_world_map_sync_launcher(launcher_candidates[i],
                                           script_path,
                                           csv_path,
                                           ods_path,
                                           fods_path))
            return;
    }

    fprintf(stderr, "[world-map] Spreadsheet sync skipped: could not launch Python.\n");
}

static InGameSystemMenuAction open_in_game_system_menu(StartupSettings* settings, Player* p)
{
    static const char* menu_items[] = {
        "Resume Game",
        "Settings",
        "Save & Quit"
    };
    const int menu_item_count = (int)(sizeof(menu_items) / sizeof(menu_items[0]));
    int selected = 0;
    int needs_world_redraw = 1;

    if(!settings || !p)
        return INGAME_SYSTEM_MENU_RESUME;

    while(1)
    {
        char line[128];
        int content_lines;
        int drawn_menu_items;
        int first_clear_line;
        int menu_limit_line;
        int menu_last_line;
        int show_status_line;
        int status_line;
        int key;

        if(needs_world_redraw)
        {
            draw_world(p);
            ui_overlay_invalidate_cache();
            needs_world_redraw = 0;
        }
        ui_overlay_draw_frame("Game Menu");
        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        menu_last_line = 1 + menu_item_count - 1;
        show_status_line = content_lines > 2 && menu_last_line < status_line;
        menu_limit_line = show_status_line ? status_line : (content_lines - 1);
        if(menu_limit_line < 0)
            menu_limit_line = 0;

        ui_overlay_draw_line(0, "Esc close | W/X move | S/Enter select");

        drawn_menu_items = 0;
        for(int i = 0; i < menu_item_count && (1 + i) < menu_limit_line; i++)
        {
            snprintf(line, sizeof(line), "%c %s", (i == selected) ? '>' : ' ', menu_items[i]);
            ui_overlay_draw_line(1 + i, line);
            drawn_menu_items++;
        }

        first_clear_line = 1 + drawn_menu_items;
        for(int row = first_clear_line; row < menu_limit_line; row++)
            ui_overlay_draw_line(row, "");

        if(show_status_line)
            ui_overlay_draw_line(status_line, "Open settings, or save before returning/quitting.");
        ui_overlay_draw_global_hotkeys();

        key = read_input_key();
        if(key == 27)
            return INGAME_SYSTEM_MENU_RESUME;

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            selected--;
            if(selected < 0)
                selected = menu_item_count - 1;
            continue;
        }

        if(KEYBIND_DOWN(key))
        {
            selected++;
            if(selected >= menu_item_count)
                selected = 0;
            continue;
        }

        if(!KEYBIND_SELECT(key))
            continue;

        if(selected == 0)
            return INGAME_SYSTEM_MENU_RESUME;

        if(selected == 1)
        {
            if(startup_open_settings_menu(settings))
                startup_settings_save(STARTUP_SETTINGS_FILE, settings);
            draw_invalidate_viewport_cache();
            needs_world_redraw = 1;
            continue;
        }

        (void)save_active_game(p);
        startup_settings_save(STARTUP_SETTINGS_FILE, settings);
        return INGAME_SYSTEM_MENU_QUIT;
    }
}

static int equals_ignore_case_ascii(const char* left, const char* right)
{
    if(!left || !right)
        return 0;

    while(*left && *right)
    {
        int lc = tolower((unsigned char)*left);
        int rc = tolower((unsigned char)*right);
        if(lc != rc)
            return 0;
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static void trim_ascii_whitespace(char* text)
{
    char* start;
    char* end;

    if(!text)
        return;

    start = text;
    while(*start && isspace((unsigned char)*start))
        start++;
    if(start != text)
        memmove(text, start, strlen(start) + 1);

    end = text + strlen(text);
    while(end > text && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
}

static int parse_road_tier_value(const char* text)
{
    char normalized[32];
    int numeric_value;

    if(!text || text[0] == '\0')
        return WORLD_MAP_ROAD_TIER_TRAIL;

    snprintf(normalized, sizeof(normalized), "%s", text);
    trim_ascii_whitespace(normalized);
    if(normalized[0] == '\0')
        return WORLD_MAP_ROAD_TIER_TRAIL;

    if(equals_ignore_case_ascii(normalized, "none"))
        return WORLD_MAP_ROAD_TIER_NONE;
    if(equals_ignore_case_ascii(normalized, "trail"))
        return WORLD_MAP_ROAD_TIER_TRAIL;
    if(equals_ignore_case_ascii(normalized, "paved"))
        return WORLD_MAP_ROAD_TIER_PAVED;
    if(equals_ignore_case_ascii(normalized, "highway"))
        return WORLD_MAP_ROAD_TIER_HIGHWAY;

    numeric_value = atoi(normalized);
    if(numeric_value < WORLD_MAP_ROAD_TIER_NONE)
        return WORLD_MAP_ROAD_TIER_NONE;
    if(numeric_value > WORLD_MAP_MAX_ROAD_TIER)
        return WORLD_MAP_MAX_ROAD_TIER;
    return numeric_value;
}

static int load_world_roads_from_csv(const char* path)
{
    FILE* file;
    char line[256];
    int loaded = 0;

    if(!path || path[0] == '\0')
        return 0;

    file = fopen(path, "r");
    if(!file)
        return 0;

    while(fgets(line, sizeof(line), file))
    {
        char* cursor = line;
        char from_name[64] = "";
        char to_name[64] = "";
        char tier_text[32] = "";
        char* fields[] = { from_name, to_name, tier_text };
        size_t field_sizes[] = { sizeof(from_name), sizeof(to_name), sizeof(tier_text) };

        trim_ascii_whitespace(line);
        if(line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        for(int column = 0; column < 3; column++)
        {
            int i = 0;
            while(*cursor && *cursor != ',' && *cursor != ';' && *cursor != '\n' && *cursor != '\r')
            {
                if(i + 1 < (int)field_sizes[column])
                    fields[column][i++] = *cursor;
                cursor++;
            }
            fields[column][i] = '\0';
            trim_ascii_whitespace(fields[column]);
            if(*cursor == ',' || *cursor == ';')
                cursor++;
        }

        if(equals_ignore_case_ascii(from_name, "from_name")
           && equals_ignore_case_ascii(to_name, "to_name"))
            continue;

        if(from_name[0] != '\0' && to_name[0] != '\0')
        {
            int from_index = atlas_find_location(from_name);
            int to_index = atlas_find_location(to_name);

            if(from_index >= 0 && to_index >= 0)
            {
                world_map_draw_road(atlas[from_index].world_x,
                                    atlas[from_index].world_y,
                                    atlas[to_index].world_x,
                                    atlas[to_index].world_y,
                                    parse_road_tier_value(tier_text));
                loaded++;
            }
        }
    }

    fclose(file);
    return loaded;
}

static void apply_debug_mode_flags(Player* p)
{
    int name_debug;
    int debug_enabled;

    if(!p)
        return;

    name_debug = equals_ignore_case_ascii(p->character.name, "godmode=666");
    debug_enabled = name_debug;

    p->godmode = debug_enabled ? 1 : 0;

    if(name_debug)
        log_add("[DEBUG] Godmode enabled from player name. Dev chest loot enabled.");
}



/*
 * Purpose:
 *   Hosts program entry point, startup/menu flow, and main gameplay loop.
 *
 * Functions:
 *   - initialize_game: initializes systems, creates player, and spawns monsters.
 *   - main: runs startup menu loop and in-game input/render loop.
 */

// Place the player at the handcrafted starter spawn or on a random valid tile elsewhere.
static int find_stairs_up_position(int* out_x, int* out_y)
{
    if(!current_area || !out_x || !out_y)
        return 0;

    for(int y = 0; y < current_area->height; y++)
    {
        for(int x = 0; x < current_area->width; x++)
        {
            Tile* tile = map_tile_at_layer(current_area, x, y, TILE_LAYER_WALL);
            if(tile && tile->symbol == '<')
            {
                *out_x = x;
                *out_y = y;
                return 1;
            }
        }
    }

    return 0;
}

typedef struct TravelArrivalContext {
    int enabled;
    int exit_dx;
    int exit_dy;
    int source_x;
    int source_y;
    int source_width;
    int source_height;
} TravelArrivalContext;

typedef struct RoadSeedSpec {
    const char* from_name;
    const char* to_name;
    int road_tier;
} RoadSeedSpec;

static int world_map_has_authored_roads(void)
{
    for(int y = 0; y < WORLD_MAP_HEIGHT; y++)
    {
        for(int x = 0; x < WORLD_MAP_WIDTH; x++)
        {
            if(world_map_get_road_tier(x, y) > WORLD_MAP_ROAD_TIER_NONE)
                return 1;
        }
    }

    return 0;
}

static void seed_default_world_roads(void)
{
    static const RoadSeedSpec road_seeds[] = {
        { "The Glade of Beginnings", "Village", WORLD_MAP_ROAD_TIER_TRAIL },
    };

    if(world_map_has_authored_roads())
        return;

    if(load_world_roads_from_csv("data/templates/maps/world_roads.csv") > 0)
        return;

    for(int i = 0; i < (int)(sizeof(road_seeds) / sizeof(road_seeds[0])); i++)
    {
        int from_index = atlas_find_location(road_seeds[i].from_name);
        int to_index = atlas_find_location(road_seeds[i].to_name);

        if(from_index < 0 || to_index < 0)
            continue;

        world_map_draw_road(atlas[from_index].world_x,
                            atlas[from_index].world_y,
                            atlas[to_index].world_x,
                            atlas[to_index].world_y,
                            road_seeds[i].road_tier);
    }
}

static int clamp_int_value(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

static int scale_edge_coordinate(int value, int old_extent, int new_extent)
{
    if(new_extent <= 1)
        return 0;

    if(old_extent <= 1)
        return clamp_int_value(value, 0, new_extent - 1);

    value = clamp_int_value(value, 0, old_extent - 1);
    return (value * (new_extent - 1) + (old_extent - 1) / 2) / (old_extent - 1);
}

static int try_place_player_from_edge_transition(Player* p, const TravelArrivalContext* arrival)
{
    int width;
    int height;
    int start_x;
    int start_y;
    int step_x = 0;
    int step_y = 0;
    int max_steps;

    if(!p || !arrival || !arrival->enabled || !current_area)
        return 0;

    width = current_area->width;
    height = current_area->height;
    if(width <= 0 || height <= 0)
        return 0;

    start_x = clamp_int_value(width / 2, 0, width - 1);
    start_y = clamp_int_value(height / 2, 0, height - 1);

    if(arrival->exit_dy != 0)
    {
        int min_x = (width > 2) ? 1 : 0;
        int max_x = (width > 2) ? (width - 2) : (width - 1);
        int mapped_x = scale_edge_coordinate(arrival->source_x, arrival->source_width, width);

        start_x = clamp_int_value(mapped_x, min_x, max_x);
        start_y = (arrival->exit_dy < 0)
            ? ((height > 1) ? (height - 2) : 0)
            : ((height > 1) ? 1 : 0);
        step_y = (arrival->exit_dy < 0) ? -1 : 1;
        max_steps = height;
    }
    else if(arrival->exit_dx != 0)
    {
        int min_y = (height > 2) ? 1 : 0;
        int max_y = (height > 2) ? (height - 2) : (height - 1);
        int mapped_y = scale_edge_coordinate(arrival->source_y, arrival->source_height, height);

        start_y = clamp_int_value(mapped_y, min_y, max_y);
        start_x = (arrival->exit_dx < 0)
            ? ((width > 1) ? (width - 2) : 0)
            : ((width > 1) ? 1 : 0);
        step_x = (arrival->exit_dx < 0) ? -1 : 1;
        max_steps = width;
    }
    else
    {
        return 0;
    }

    p->character.actor.entity.z = AREA_GROUND_Z;
    for(int step = 0; step < max_steps; step++)
    {
        int x = start_x + (step * step_x);
        int y = start_y + (step * step_y);

        if(x < 0 || x >= width || y < 0 || y >= height)
            break;

        if(!is_blocked_3d(x, y, AREA_GROUND_Z, 0) && !bestiary_creature_at_3d(x, y, AREA_GROUND_Z))
        {
            player_place(p, x, y);
            return 1;
        }
    }

    return 0;
}

static int place_player_for_current_area(Player* p, const TravelArrivalContext* arrival)
{
    if(!p)
        return 0;

    if(try_place_player_from_edge_transition(p, arrival))
        return 1;

    if(current_area && current_area == &atlas[4])
    {
        int stairs_x;
        int stairs_y;

        if(find_stairs_up_position(&stairs_x, &stairs_y))
        {
            player_place(p, stairs_x, stairs_y);
            return 1;
        }
    }

    if(current_area && current_area->type == LOCATION_STARTER)
    {
        player_place(p, STARTER_PLAYER_START_X, STARTER_PLAYER_START_Y);
        return 1;
    }

    return player_place_random(p);
}

// Resolve 1-based mode option to concrete attack mode from current mode mask.
static AttackMode inspect_mode_from_option_index(int attack_mode_mask, int option_index, int* out_count)
{
    static const struct {
        int flag;
        AttackMode mode;
    } ordered_modes[] = {
        { ATTACK_MODE_FLAG_PUNCH, ATTACK_MODE_PUNCH },
        { ATTACK_MODE_FLAG_KICK, ATTACK_MODE_KICK },
        { ATTACK_MODE_FLAG_HAYMAKER, ATTACK_MODE_HAYMAKER },
        { ATTACK_MODE_FLAG_STAB, ATTACK_MODE_STAB },
        { ATTACK_MODE_FLAG_THRUST, ATTACK_MODE_THRUST },
        { ATTACK_MODE_FLAG_FEINT, ATTACK_MODE_FEINT },
        { ATTACK_MODE_FLAG_LUNGE, ATTACK_MODE_LUNGE },
        { ATTACK_MODE_FLAG_IMPALE, ATTACK_MODE_IMPALE },
        { ATTACK_MODE_FLAG_CUT, ATTACK_MODE_CUT },
        { ATTACK_MODE_FLAG_SLASH, ATTACK_MODE_SLASH },
        { ATTACK_MODE_FLAG_CLEAVE, ATTACK_MODE_CLEAVE },
        { ATTACK_MODE_FLAG_HOOK, ATTACK_MODE_HOOK },
        { ATTACK_MODE_FLAG_SMASH, ATTACK_MODE_SMASH },
        { ATTACK_MODE_FLAG_BASH, ATTACK_MODE_BASH },
        { ATTACK_MODE_FLAG_SHATTER, ATTACK_MODE_SHATTER },
        { ATTACK_MODE_FLAG_SWEEP, ATTACK_MODE_SWEEP },
        { ATTACK_MODE_FLAG_SHOT, ATTACK_MODE_SHOT },
        { ATTACK_MODE_FLAG_AIMED_SHOT, ATTACK_MODE_AIMED_SHOT },
        { ATTACK_MODE_FLAG_VOLLEY, ATTACK_MODE_VOLLEY },
        { ATTACK_MODE_FLAG_PIN_SHOT, ATTACK_MODE_PIN_SHOT },
        { ATTACK_MODE_FLAG_DEADEYE, ATTACK_MODE_DEADEYE },
    };
    int count = 0;

    for(int i = 0; i < (int)(sizeof(ordered_modes) / sizeof(ordered_modes[0])); i++)
    {
        if(!(attack_mode_mask & ordered_modes[i].flag))
            continue;

        count++;
        if(count == option_index)
        {
            if(out_count) *out_count = count;
            return ordered_modes[i].mode;
        }
    }

    if(out_count) *out_count = count;
    return ATTACK_MODE_NONE;
}

static const char* attack_mode_description(AttackMode mode)
{
    switch(mode)
    {
        case ATTACK_MODE_PUNCH:
            return "Quick unarmed jab. Low damage, very reliable baseline strike.";
        case ATTACK_MODE_KICK:
            return "Driving kick. Stronger than a punch and available from the start.";
        case ATTACK_MODE_HAYMAKER:
            return "All-in unarmed power shot. High impact, slower to commit.";
        case ATTACK_MODE_STAB:
            return "Basic piercing attack. Accurate and efficient at skill level 0.";
        case ATTACK_MODE_THRUST:
            return "Longer committed pierce. A stronger follow-up learned with practice.";
        case ATTACK_MODE_FEINT:
            return "Dagger specialty. A deceptive setup strike for close knife work.";
        case ATTACK_MODE_LUNGE:
            return "Sword specialty. Extend into the target with a committed attack.";
        case ATTACK_MODE_IMPALE:
            return "Spear specialty. A deep, punishing piercing finish.";
        case ATTACK_MODE_CUT:
            return "Basic slashing attack. Balanced and reliable at skill level 0.";
        case ATTACK_MODE_SLASH:
            return "Broader slashing arc. Better follow-through once the skill improves.";
        case ATTACK_MODE_CLEAVE:
            return "Axe specialty. A heavy chop built to tear through targets.";
        case ATTACK_MODE_HOOK:
            return "Polearm specialty. Use the head to catch and drag through the target.";
        case ATTACK_MODE_SMASH:
            return "Basic crushing blow. High impact and good against armour.";
        case ATTACK_MODE_BASH:
            return "A quicker blunt follow-up that keeps pressure on the target.";
        case ATTACK_MODE_SHATTER:
            return "Mace specialty. A punishing strike meant to break defenses.";
        case ATTACK_MODE_SWEEP:
            return "Staff specialty. A wide control attack that batters with reach.";
        case ATTACK_MODE_SHOT:
            return "Standard ranged shot. The basic ranged attack for bows and thrown weapons.";
        case ATTACK_MODE_AIMED_SHOT:
            return "Carefully lined-up ranged attack. Slower but more deliberate.";
        case ATTACK_MODE_VOLLEY:
            return "Thrown specialty. Rapid release technique for skilled throwers.";
        case ATTACK_MODE_PIN_SHOT:
            return "Bow specialty. A precise shot intended to pin the target down.";
        case ATTACK_MODE_DEADEYE:
            return "Crossbow specialty. Extreme precision with a committed shot.";
        default:
            return "Standard melee attack.";
    }
}

static int collect_attack_modes_for_character(const Character* c, AttackMode requested_mode, AttackMode out_modes[9])
{
    CombatProfile attack_profile;
    int count = 0;

    if(!c || !out_modes)
        return 0;

    attack_profile = combat_profile_for_character_attack(c, requested_mode);
    for(int option_index = 1; option_index <= 9; option_index++)
    {
        int available_count = 0;
        AttackMode mode = inspect_mode_from_option_index(attack_profile.attack_mode_mask, option_index, &available_count);
        if(mode == ATTACK_MODE_NONE)
            break;
        out_modes[count++] = mode;
    }

    return count;
}

static int player_toggle_versatile_grip(Player* p)
{
    CombatProfile attack_profile;

    if(!p)
        return 0;

    attack_profile = combat_profile_for_character_attack(&p->character, p->selected_attack_mode);
    if(!attack_profile.can_toggle_grip)
    {
        log_add("No active versatile weapon is ready for a grip change.");
        return 0;
    }

    p->character.versatile_grip_mode = (p->character.versatile_grip_mode == WEAPON_GRIP_TWO_HANDED)
        ? WEAPON_GRIP_ONE_HANDED
        : WEAPON_GRIP_TWO_HANDED;
    p->selected_attack_mode = combat_valid_attack_mode_for_character(&p->character, p->selected_attack_mode);
    attack_profile = combat_profile_for_character_attack(&p->character, p->selected_attack_mode);

    log_add("You shift %s to a %s grip.",
            attack_profile.weapon_name,
            attack_profile.is_two_hand_mode ? "two-handed" : "one-handed");
    if(attack_profile.is_two_hand_mode)
        log_add("Your off-hand gear stays equipped, but it no longer helps in active combat.");

    return 1;
}

typedef enum ActionMenuToolAction {
    TOOL_ACTION_NONE = 0,
    TOOL_ACTION_FISH,
    TOOL_ACTION_DRAW_WEAPON,
    TOOL_ACTION_SHEATHE_WEAPON,
} ActionMenuToolAction;

static int player_has_tool_for_skill(const Player* p, NonWeaponSkillType skill_type)
{
    if(!p)
        return 0;

    for(int i = 0; i < p->character.equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &p->character.equipment_slots[i];
        if(slot->item.type == ITEM_TYPE_NONE)
            continue;
        if(item_tool_non_weapon_skill(&slot->item) == skill_type)
            return 1;
    }

    return 0;
}

static const Item* player_find_tool_for_skill(const Player* p, NonWeaponSkillType skill_type)
{
    if(!p)
        return NULL;

    for(int i = 0; i < p->character.equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &p->character.equipment_slots[i];
        if(slot->item.type == ITEM_TYPE_NONE)
            continue;
        if(item_tool_non_weapon_skill(&slot->item) == skill_type)
            return &slot->item;
    }

    return NULL;
}

static int player_tool_reach_for_skill(const Player* p, NonWeaponSkillType skill_type)
{
    int max_reach = 0;

    if(!p)
        return 0;

    for(int i = 0; i < p->character.equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &p->character.equipment_slots[i];
        if(slot->item.type == ITEM_TYPE_NONE)
            continue;
        if(item_tool_non_weapon_skill(&slot->item) != skill_type)
            continue;
        if(slot->item.reach_bonus > max_reach)
            max_reach = slot->item.reach_bonus;
    }

    return max_reach;
}

static int player_has_fishable_tile_nearby(const Player* p)
{
    if(!p || !current_area)
        return 0;

    int px = p->character.actor.entity.x;
    int py = p->character.actor.entity.y;

    for(int dy = -1; dy <= 1; ++dy)
    {
        for(int dx = -1; dx <= 1; ++dx)
        {
            int tx = px + dx;
            int ty = py + dy;
            const Tile* tile;

            if(tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
                continue;

            tile = map_top_visible_tile(current_area, tx, ty, NULL);
            if(tile && tile_is_fishable(tile))
                return 1;
        }
    }

    return 0;
}

static int action_menu(Player* p, AttackMode* out_mode, int* out_use_ranged, int* out_tool_action)
{
    AttackMode options[9];
    int option_count;
    int selected = 0;
    CombatProfile base_profile;
    int has_ranged_option;
    int tool_action_index = -1;
    int has_fishing_tool_option = 0;
    const Item* fishing_tool = NULL;

    if(!p || !out_mode)
        return 0;

    if(out_use_ranged)
        *out_use_ranged = 0;

    base_profile = combat_profile_for_character_attack(&p->character, p->selected_attack_mode);
    has_ranged_option = combat_profile_is_ranged(&base_profile);

    option_count = collect_attack_modes_for_character(&p->character, p->selected_attack_mode, options);
    has_fishing_tool_option = player_has_tool_for_skill(p, NON_WEAPON_SKILL_FISHING) && player_has_fishable_tile_nearby(p);
    if(has_fishing_tool_option)
        fishing_tool = player_find_tool_for_skill(p, NON_WEAPON_SKILL_FISHING);

    int has_draw_option = interact_can_draw_weapon(p);
    int has_sheathe_option = interact_can_sheathe_weapon(p);

    if(option_count <= 0 && !has_ranged_option && !has_fishing_tool_option && !has_draw_option && !has_sheathe_option)
    {
        log_add("No attack or tool actions available for current weapon.");
        return 0;
    }

    for(int i = 0; i < option_count; i++)
    {
        if(options[i] == p->selected_attack_mode)
        {
            selected = i;
            break;
        }
    }

    draw_world(p);

    while(1)
    {
        int content_lines;
        int status_line;
        int list_limit;
        int total_options;
        int ranged_option_index;
        int line_i = 0;
        int key;
        char line[192];
        char damage_text[32];
        AttackMode preview_mode = (p->selected_attack_mode != ATTACK_MODE_NONE)
            ? p->selected_attack_mode
            : ((option_count > 0) ? options[0] : ATTACK_MODE_NONE);
        CombatSummary ranged_summary = combat_summary_for_character(&p->character, preview_mode);
        CombatProfile ranged_profile = combat_profile_for_character_attack(&p->character, preview_mode);
        has_ranged_option = combat_profile_is_ranged(&ranged_profile);
        int ranged_ap_cost = combat_profile_attack_action_point_cost(&ranged_profile);
        int ranged_range = combat_profile_ranged_range(&ranged_profile);

        draw_world(p);
        ui_overlay_draw_frame("Action");
        ui_overlay_invalidate_cache();

        ranged_option_index = option_count;
        tool_action_index = option_count + (has_ranged_option ? 1 : 0);
        int draw_action_index = tool_action_index + (has_fishing_tool_option ? 1 : 0);
        int sheathe_action_index = draw_action_index + (has_draw_option ? 1 : 0);
        total_options = option_count + 1 + (has_ranged_option ? 1 : 0) + (has_fishing_tool_option ? 1 : 0) + (has_draw_option ? 1 : 0) + (has_sheathe_option ? 1 : 0);
        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        list_limit = status_line - 5;
        if(list_limit < 4)
            list_limit = 4;

        ui_overlay_draw_line(line_i++, "Choose an action:");
        ui_overlay_draw_line(line_i++, "");

        for(int i = 0; i < option_count && line_i < list_limit - 1; i++)
        {
            CombatSummary row_summary = combat_summary_for_character(&p->character, options[i]);
            CombatProfile row_profile = combat_profile_for_character_attack(&p->character, options[i]);
            const char* weapon_name = (row_profile.weapon_name[0] != '\0') ? row_profile.weapon_name : "weapon";
            int ap_cost = combat_profile_attack_action_point_cost(&row_profile);

            if(row_summary.damage_min == row_summary.damage_max)
                snprintf(damage_text, sizeof(damage_text), "%d", row_summary.damage_min);
            else
                snprintf(damage_text, sizeof(damage_text), "%d-%d", row_summary.damage_min, row_summary.damage_max);

            snprintf(line,
                     sizeof(line),
                     "%c %d. Attack - %-12s %-10s Hit:%3d%% Dmg:%-5s AP:%2d",
                     (i == selected) ? '>' : ' ',
                     i + 1,
                     weapon_name,
                     attack_mode_name(options[i]),
                     row_summary.hit_chance,
                     damage_text,
                     ap_cost);
            ui_overlay_draw_line(line_i++, line);
        }

        if(ranged_summary.damage_min == ranged_summary.damage_max)
            snprintf(damage_text, sizeof(damage_text), "%d", ranged_summary.damage_min);
        else
            snprintf(damage_text, sizeof(damage_text), "%d-%d", ranged_summary.damage_min, ranged_summary.damage_max);

        if(has_ranged_option && line_i < list_limit)
        {
            const char* weapon_name = (ranged_profile.weapon_name[0] != '\0') ? ranged_profile.weapon_name : "weapon";
            snprintf(line,
                     sizeof(line),
                     "%c 0. Attack - %-12s %-10s Hit:%3d%% Dmg:%-5s AP:%2d",
                     (selected == ranged_option_index) ? '>' : ' ',
                     weapon_name,
                     "ranged",
                     ranged_summary.hit_chance,
                     damage_text,
                     ranged_ap_cost);
            ui_overlay_draw_line(line_i++, line);
        }

        if(has_fishing_tool_option && line_i < list_limit)
        {
            const char* tool_name = (fishing_tool && fishing_tool->name[0]) ? fishing_tool->name : "tool";
            snprintf(line,
                     sizeof(line),
                     "%c %d. Use tool - %s",
                     (selected == tool_action_index) ? '>' : ' ',
                     tool_action_index + 1,
                     tool_name);
            ui_overlay_draw_line(line_i++, line);
        }
        if(has_draw_option && line_i < list_limit)
        {
            snprintf(line,
                     sizeof(line),
                     "%c %d. Draw weapon (AP: 1)",
                     (selected == draw_action_index) ? '>' : ' ',
                     draw_action_index + 1);
            ui_overlay_draw_line(line_i++, line);
        }
        if(has_sheathe_option && line_i < list_limit)
        {
            snprintf(line,
                     sizeof(line),
                     "%c %d. Sheathe weapon (AP: 1)",
                     (selected == sheathe_action_index) ? '>' : ' ',
                     sheathe_action_index + 1);
            ui_overlay_draw_line(line_i++, line);
        }

        while(line_i < list_limit)
            ui_overlay_draw_line(line_i++, "");

        if(selected < option_count)
        {
            CombatSummary selected_summary = combat_summary_for_character(&p->character, options[selected]);
            CombatProfile selected_profile = combat_profile_for_character_attack(&p->character, options[selected]);

            ui_overlay_draw_line(line_i++, "Attack Details:");
            if(selected_summary.damage_min == selected_summary.damage_max)
                snprintf(damage_text, sizeof(damage_text), "%d", selected_summary.damage_min);
            else
                snprintf(damage_text, sizeof(damage_text), "%d-%d", selected_summary.damage_min, selected_summary.damage_max);

            snprintf(line,
                     sizeof(line),
                     "Mode: %s   Damage: %s   Type: %s",
                     attack_mode_name(selected_summary.attack_mode),
                     damage_text,
                     damage_type_name(selected_summary.active_damage_type));
            ui_overlay_draw_line(line_i++, line);
            ui_overlay_draw_line(line_i++, attack_mode_description(options[selected]));
            if(selected_profile.next_unlock_mode != ATTACK_MODE_NONE)
            {
                snprintf(line,
                         sizeof(line),
                         "Next unlock: %s at %s %d",
                         attack_mode_name(selected_profile.next_unlock_mode),
                         weapon_skill_name(selected_profile.skill_type),
                         selected_profile.next_unlock_skill_level);
                ui_overlay_draw_line(line_i++, line);
            }
            snprintf(line,
                     sizeof(line),
                     "Reach: %d   AP Cost: %d   Current AP: %d/%d",
                     combat_profile_melee_range(&selected_profile),
                     combat_profile_attack_action_point_cost(&selected_profile),
                     p->character.actor.action_points,
                     p->character.actor.max_action_points);
            ui_overlay_draw_line(line_i++, line);
        }
        else if(has_ranged_option && selected == ranged_option_index)
        {
            ui_overlay_draw_line(line_i++, "Ranged Details:");
            snprintf(line,
                     sizeof(line),
                     "Weapon: %s   Damage: %s   Range: %d   AP: %d",
                     ranged_profile.weapon_name[0] ? ranged_profile.weapon_name : "Ranged weapon",
                     damage_text,
                     ranged_range,
                     ranged_ap_cost);
            ui_overlay_draw_line(line_i++, line);
            ui_overlay_draw_line(line_i++, "Aim with cursor, or fire immediately at a locked target.");
            snprintf(line,
                     sizeof(line),
                     "Current AP: %d/%d   Selected mode: %s",
                     p->character.actor.action_points,
                     p->character.actor.max_action_points,
                     attack_mode_name(preview_mode));
            ui_overlay_draw_line(line_i++, line);
        }
        else if(has_fishing_tool_option && selected == tool_action_index)
        {
            const char* tool_name = (fishing_tool && fishing_tool->name[0]) ? fishing_tool->name : "tool";
            ui_overlay_draw_line(line_i++, "Tool Action:");
            snprintf(line, sizeof(line), "Use %s for fishing.", tool_name);
            ui_overlay_draw_line(line_i++, line);
        }
        else if(has_draw_option && selected == draw_action_index)
        {
            ui_overlay_draw_line(line_i++, "Draw Weapon:");
            ui_overlay_draw_line(line_i++, "Draw a sheathed weapon into a free hand.");
        }
        else if(has_sheathe_option && selected == sheathe_action_index)
        {
            ui_overlay_draw_line(line_i++, "Sheathe Weapon:");
            ui_overlay_draw_line(line_i++, "Sheathe a wielded weapon into an empty scabbard.");
        }
        else
        {
            ui_overlay_draw_line(line_i++, "Select an action from the menu.");
        }

        while(line_i < status_line)
            ui_overlay_draw_line(line_i++, "");

        ui_overlay_draw_line(status_line,
                             has_ranged_option
                                 ? ((option_count > 0)
                                     ? "Enter confirm | W/X move | 1-9 attack/tool | 0 ranged | D draw | S sheathe | Q cancel"
                                     : "Enter confirm | W/X move | 0 ranged | D draw | S sheathe | Q cancel")
                                 : "Enter confirm | W/X move | 1-9 attack/tool | D draw | S sheathe | Q cancel");
        ui_overlay_draw_global_hotkeys();

        key = read_input_key();
        if(key == 'q' || key == 'Q' || key == 27)
            return 0;
        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            if(selected > 0)
                selected--;
            else
                selected = total_options - 1;
            continue;
        }
        if(KEYBIND_DOWN(key))
        {
            if(selected < total_options - 1)
                selected++;
            else
                selected = 0;
            continue;
        }
        if(key >= '1' && key <= '9')
        {
            int option = key - '1';
            if(option < 0 || option >= total_options)
                continue;

            selected = option;

            if(selected < option_count)
            {
                *out_mode = options[selected];
                if(out_use_ranged)
                    *out_use_ranged = 0;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_NONE;
            }
            else if(has_ranged_option && selected == ranged_option_index)
            {
                *out_mode = preview_mode;
                if(out_use_ranged)
                    *out_use_ranged = 1;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_NONE;
            }
            else if(has_fishing_tool_option && selected == tool_action_index)
            {
                *out_mode = ATTACK_MODE_NONE;
                if(out_use_ranged)
                    *out_use_ranged = 0;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_FISH;
            }
            else if(has_draw_option && selected == draw_action_index)
            {
                *out_mode = ATTACK_MODE_NONE;
                if(out_use_ranged)
                    *out_use_ranged = 0;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_DRAW_WEAPON;
            }
            else if(has_sheathe_option && selected == sheathe_action_index)
            {
                *out_mode = ATTACK_MODE_NONE;
                if(out_use_ranged)
                    *out_use_ranged = 0;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_SHEATHE_WEAPON;
            }
            else
            {
                continue;
            }
            return 1;
        }
        if(key == '0')
        {
            if(has_ranged_option)
            {
                selected = ranged_option_index;
                *out_mode = preview_mode;
                if(out_use_ranged)
                    *out_use_ranged = 1;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_NONE;
                return 1;
            }
            continue;
        }
        if((key == 'd' || key == 'D') && has_draw_option)
        {
            selected = draw_action_index;
            *out_mode = ATTACK_MODE_NONE;
            if(out_use_ranged)
                *out_use_ranged = 0;
            if(out_tool_action)
                *out_tool_action = TOOL_ACTION_DRAW_WEAPON;
            return 1;
        }
        if((key == 's' || key == 'S') && has_sheathe_option)
        {
            selected = sheathe_action_index;
            *out_mode = ATTACK_MODE_NONE;
            if(out_use_ranged)
                *out_use_ranged = 0;
            if(out_tool_action)
                *out_tool_action = TOOL_ACTION_SHEATHE_WEAPON;
            return 1;
        }
        if(KEYBIND_SELECT(key))
        {
            if(selected < option_count)
            {
                *out_mode = options[selected];
                if(out_use_ranged)
                    *out_use_ranged = 0;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_NONE;
            }
            else if(has_ranged_option && selected == ranged_option_index)
            {
                *out_mode = preview_mode;
                if(out_use_ranged)
                    *out_use_ranged = 1;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_NONE;
            }
            else if(has_fishing_tool_option && selected == tool_action_index)
            {
                *out_mode = ATTACK_MODE_NONE;
                if(out_use_ranged)
                    *out_use_ranged = 0;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_FISH;
            }
            else if(has_draw_option && selected == draw_action_index)
            {
                *out_mode = ATTACK_MODE_NONE;
                if(out_use_ranged)
                    *out_use_ranged = 0;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_DRAW_WEAPON;
            }
            else if(has_sheathe_option && selected == sheathe_action_index)
            {
                *out_mode = ATTACK_MODE_NONE;
                if(out_use_ranged)
                    *out_use_ranged = 0;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_SHEATHE_WEAPON;
            }
            else
            {
                *out_mode = ATTACK_MODE_NONE;
                if(out_use_ranged)
                    *out_use_ranged = 0;
                if(out_tool_action)
                    *out_tool_action = TOOL_ACTION_NONE;
            }
            return 1;
        }
    }
}

static int open_melee_direction_prompt(Player* p, AttackMode selected_mode)
{
    int key;
    int dx = 0;
    int dy = 0;

    if(!p)
        return 0;

    draw_world(p);

    while(1)
    {
        char line[128];
        CombatSummary summary = combat_summary_for_character(&p->character, selected_mode);

        ui_overlay_draw_frame("Melee Attack - Direction");
        snprintf(line, sizeof(line), "Mode: %s (%s)",
                 attack_mode_name(summary.attack_mode),
                 damage_type_name(summary.active_damage_type));
        ui_overlay_draw_line(0, line);
        ui_overlay_draw_line(1, attack_mode_description(selected_mode));
        ui_overlay_draw_line(2, "");
        ui_overlay_draw_line(3, "Choose direction with WAXD or qezc diagonals.");
        ui_overlay_draw_line(4, "Reach weapons can hit farther in chosen direction.");
        ui_overlay_draw_line(ui_overlay_content_lines() - 2, "Esc cancel");
        ui_overlay_draw_global_hotkeys();

        key = read_input_key();
        switch(key)
        {
            case INPUT_KEY_UP: case 'w': case 'W': dy = -1; dx = 0; break;
            case INPUT_KEY_DOWN: case 'x': case 'X': dy = 1; dx = 0; break;
            case INPUT_KEY_LEFT: case 'a': case 'A': dx = -1; dy = 0; break;
            case INPUT_KEY_RIGHT: case 'd': case 'D': dx = 1; dy = 0; break;
            case 'q': case 'Q': dy = -1; dx = -1; break;
            case 'e': case 'E': dy = -1; dx = 1; break;
            case 'z': case 'Z': dy = 1; dx = -1; break;
            case 'c': case 'C': dy = 1; dx = 1; break;
            case 27:
                log_add("Melee attack canceled.");
                return 0;
            default:
                continue;
        }

        return player_attack_direction(p, dx, dy, selected_mode);
    }
}

static int fish_tool_mode(Player* p, const Item* tool)
{
    if(!p)
        return 0;

    int tx = p->character.actor.entity.x;
    int ty = p->character.actor.entity.y;
    const char* tool_name = (tool && tool->name[0]) ? tool->name : "tool";

    draw_set_inspect_cursor(tx, ty);
    log_add("Fishing mode: move cursor, Enter fish with %s, q cancel", tool_name);

    while(1)
    {
        int key;
        int px = p->character.actor.entity.x;
        int py = p->character.actor.entity.y;
        const Tile* tile;
        int dx;
        int dy;

        if(tx < 0) tx = 0;
        if(tx >= current_area->width) tx = current_area->width - 1;
        if(ty < 0) ty = 0;
        if(ty >= current_area->height) ty = current_area->height - 1;

        draw_set_inspect_cursor(tx, ty);
        draw_world(p);

        key = read_input_key();
        switch(key)
        {
            case INPUT_KEY_UP: case 'w': case 'W': ty--; break;
            case INPUT_KEY_DOWN: case 's': case 'S': ty++; break;
            case INPUT_KEY_LEFT: case 'a': case 'A': tx--; break;
            case INPUT_KEY_RIGHT: case 'd': case 'D': tx++; break;
            case 'q': case 'Q': case 27:
                draw_clear_inspect_cursor();
                log_add("Fishing canceled.");
                return 0;
            case 13:
                dx = abs(px - tx);
                dy = abs(py - ty);
                {
                    int max_range = 1 + player_tool_reach_for_skill(p, NON_WEAPON_SKILL_FISHING);
                    if(dx > max_range || dy > max_range)
                    {
                        log_add("You are too far away to fish there.");
                        break;
                    }
                }
                if(!map_has_line_of_sight(px, py, tx, ty))
                {
                    log_add("You cannot see that tile clearly from here.");
                    break;
                }
                tile = map_top_visible_tile(current_area, tx, ty, NULL);
                if(!tile || !tile_is_fishable(tile))
                {
                    log_add("There is no fishable water at %d,%d.", tx, ty);
                    break;
                }
                draw_clear_inspect_cursor();
                return interact_fish_at(p, tx, ty);
            default:
                break;
        }
    }
}

static int attack_action_mode(Player* p)
{
    AttackMode selected_mode;
    int use_ranged = 0;
    int tool_action = TOOL_ACTION_NONE;

    if(!p)
        return 0;

    if(actor_is_unconscious(&p->character.actor))
    {
        log_add("You are unconscious and cannot attack.");
        return 0;
    }

    if(!action_menu(p, &selected_mode, &use_ranged, &tool_action))
        return 0;

    if(tool_action == TOOL_ACTION_FISH)
    {
        const Item* fishing_tool = player_find_tool_for_skill(p, NON_WEAPON_SKILL_FISHING);
        return fish_tool_mode(p, fishing_tool);
    }

    if(tool_action == TOOL_ACTION_DRAW_WEAPON)
    {
        if(p->character.actor.action_points < 1)
        {
            log_add("Not enough action points.");
            return 0;
        }

        int result = interact_draw_weapon(p);
        if(result)
        {
            player_apply_action_point_cost(p, 1);
            // Quick action: do not advance creature turns.
        }
        return result;
    }

    if(tool_action == TOOL_ACTION_SHEATHE_WEAPON)
    {
        if(p->character.actor.action_points < 1)
        {
            log_add("Not enough action points.");
            return 0;
        }

        int result = interact_sheathe_weapon(p);
        if(result)
        {
            player_apply_action_point_cost(p, 1);
            // Quick action: do not advance creature turns.
        }
        return result;
    }

    if(use_ranged)
    {
        if(selected_mode != ATTACK_MODE_NONE)
            p->selected_attack_mode = selected_mode;
        return ranged_attack_mode(p);
    }

    if(selected_mode == ATTACK_MODE_NONE)
    {
        return 0;
    }

    p->selected_attack_mode = selected_mode;

    {
        TargetLockResolved lock;
        if(target_lock_resolve_live(p, &lock, 0) && lock.kind == TARGET_LOCK_CREATURE)
        {
            int dx = lock.x - p->character.actor.entity.x;
            int dy = lock.y - p->character.actor.entity.y;
            int abs_dx = dx < 0 ? -dx : dx;
            int abs_dy = dy < 0 ? -dy : dy;
            CombatProfile profile = combat_profile_for_character_attack(&p->character, selected_mode);
            int max_range = combat_profile_melee_range(&profile);
            int dir_x = (dx > 0) - (dx < 0);
            int dir_y = (dy > 0) - (dy < 0);

            if(max_range < 1)
                max_range = 1;

            if((dx == 0 || dy == 0 || abs_dx == abs_dy) &&
               abs_dx <= max_range && abs_dy <= max_range &&
               (abs_dx > 0 || abs_dy > 0))
            {
                return player_attack_direction(p, dir_x, dir_y, selected_mode);
            }
        }
    }

    return open_melee_direction_prompt(p, selected_mode);
}

static int has_adjacent_hostile(const Player* p)
{
    int px;
    int py;

    if(!p)
        return 0;

    px = p->character.actor.entity.x;
    py = p->character.actor.entity.y;

    for(int i = 0; i < MAX_CREATURES; i++)
    {
        const Creature* creature = &creatures[i];
        int dx;
        int dy;

        if(!creature->alive || !creature->template || !creature_is_hostile(creature))
            continue;

        dx = abs(creature->actor.entity.x - px);
        dy = abs(creature->actor.entity.y - py);
        if(dx <= 1 && dy <= 1)
            return 1;
    }

    return 0;
}

static int movement_attempt_exits_area(const Player* p, int dx, int dy)
{
    int nx;
    int ny;

    if(!p || !current_area)
        return 0;

    nx = p->character.actor.entity.x + dx;
    ny = p->character.actor.entity.y + dy;
    return nx < 0 || nx >= current_area->width || ny < 0 || ny >= current_area->height;
}

static int complete_travel_to_index(Player* p, int area_index, const TravelArrivalContext* arrival)
{
    int previous_area_index = -1;
    int previous_x;
    int previous_y;
    int previous_z;

    if(!p || area_index < 0 || area_index >= MAX_AREAS)
        return 0;

    if(!arrival && current_area == &atlas[area_index])
    {
        log_add("You are already in %s.", atlas[area_index].name);
        return 0;
    }

    previous_x = p->character.actor.entity.x;
    previous_y = p->character.actor.entity.y;
    previous_z = p->character.actor.entity.z;
    if(current_area)
        previous_area_index = atlas_find_location(current_area->name);

    atlas_travel(area_index);
    bestiary_init();

    if(!place_player_for_current_area(p, arrival))
    {
        if(previous_area_index >= 0 && previous_area_index < MAX_AREAS)
        {
            atlas_travel(previous_area_index);
            bestiary_init();
            world_map_set_overworld_position(atlas[previous_area_index].world_x,
                                             atlas[previous_area_index].world_y);
        }
        p->character.actor.entity.x = previous_x;
        p->character.actor.entity.y = previous_y;
        p->character.actor.entity.z = previous_z;
        p->travelling = 0;
        draw_invalidate_viewport_cache();
        ui_overlay_reset_cache();
        log_add("Travel failed: no safe arrival point.");
        ui_overlay_show_mini_prompt("Travel Failed",
                                    "No safe arrival point was found.",
                                    "Try another destination.");
        return 0;
    }

    world_map_set_overworld_position(atlas[area_index].world_x, atlas[area_index].world_y);
    world_map_mark_discovered(atlas[area_index].world_x, atlas[area_index].world_y);
    world_map_mark_visited(atlas[area_index].world_x, atlas[area_index].world_y);

    p->travelling = 0;
    spawn_initial_monsters();
    spawn_initial_npcs();
    draw_invalidate_viewport_cache();
    ui_overlay_reset_cache();
    return 1;
}

static int open_atlas_for_travel(Player* p, AtlasOverlayMode mode)
{
    int selected = -1;

    if(!p)
        return 0;

    (void)atlas_show_overlay_mode(p, mode);
    selected = atlas_overlay_take_selected_travel();
    if(selected < 0)
    {
        OverlayType next = overlay_take_request();
        if(next != OVERLAY_TYPE_NONE)
            overlay_open(next, p);
        return 0;
    }

    return complete_travel_to_index(p, selected, NULL);
}

static int open_world_map_exploration(Player* p)
{
    if(!p)
        return 0;

    if(current_area && (current_area->type == LOCATION_CRYPT || current_area->type == LOCATION_CAVERN || current_area->type == LOCATION_DUNGEON))
    {
        log_add("Overworld map unavailable in underground areas.");
        ui_overlay_show_mini_prompt("Travel Unavailable",
                                    "You are underground.",
                                    "Reach the surface to use the overland map or atlas fast travel.");
        return 0;
    }

    draw_set_viewport_tab(VIEWPORT_TAB_WORLD);
    (void)world_map_show_overlay(p);

    {
        OverlayType next = overlay_take_request();
        if(next != OVERLAY_TYPE_NONE)
            overlay_open(next, p);
    }

    return 0;
}

static int try_edge_travel(Player* p, int dx, int dy)
{
    int target_world_x;
    int target_world_y;
    int area_index = -1;
    TravelArrivalContext arrival = {0};
    WorldMapTile* tile;

    if(!p || !current_area)
        return 0;

    if(current_area->type == LOCATION_CRYPT || current_area->type == LOCATION_CAVERN || current_area->type == LOCATION_DUNGEON)
    {
        log_add("Overland edge travel unavailable underground.");
        ui_overlay_show_mini_prompt("Travel Unavailable",
                                    "You are underground.",
                                    "Reach the surface to change zones by edge travel.");
        return 0;
    }

    if(has_adjacent_hostile(p))
    {
        log_add("Travel blocked: hostile enemy adjacent.");
        ui_overlay_show_mini_prompt("Travel Blocked",
                                    "A hostile enemy is adjacent.",
                                    "Move away before attempting travel.");
        return 0;
    }

    target_world_x = current_area->world_x + dx;
    target_world_y = current_area->world_y + dy;
    if(target_world_x < 0 || target_world_x >= WORLD_MAP_WIDTH || target_world_y < 0 || target_world_y >= WORLD_MAP_HEIGHT)
    {
        log_add("You cannot travel beyond the edge of the world.");
        ui_overlay_show_mini_prompt("World Boundary",
                                    "No overland zone lies beyond this edge.",
                                    "Turn back or choose another direction.");
        return 0;
    }

    world_map_mark_discovered(target_world_x, target_world_y);
    world_map_mark_visited(target_world_x, target_world_y);

    tile = world_map_get_tile(target_world_x, target_world_y);
    if(tile && tile->zone_index >= 0)
        area_index = tile->zone_index;
    else if(!atlas_prepare_generated_area(target_world_x, target_world_y, &area_index))
    {
        log_add("Travel failed: no adjacent zone could be prepared.");
        ui_overlay_show_mini_prompt("Travel Failed",
                                    "No adjacent overland zone was found.",
                                    "Try another edge.");
        return 0;
    }

    arrival.enabled = 1;
    arrival.exit_dx = dx;
    arrival.exit_dy = dy;
    arrival.source_x = p->character.actor.entity.x;
    arrival.source_y = p->character.actor.entity.y;
    arrival.source_width = current_area->width;
    arrival.source_height = current_area->height;

    return complete_travel_to_index(p, area_index, &arrival);
}

static const char* tile_layer_name(TileLayer layer)
{
    switch(layer)
    {
        case TILE_LAYER_GROUND: return "ground";
        case TILE_LAYER_FLOOR: return "floor";
        case TILE_LAYER_WALL: return "wall";
        case TILE_LAYER_DECOR: return "decor";
        case TILE_LAYER_EFFECT: return "effect";
        default: return "unknown";
    }
}

static void inspect_format_result(char* out, size_t out_size, int tx, int ty)
{
    const Tile* visible_tiles[TILE_LAYER_COUNT];
    TileLayer visible_layers[TILE_LAYER_COUNT];
    int visible_count;
    Creature* creature;
    WorldItem* world_item;
    int world_item_count = 0;
    int offset = 0;
    int should_continue = 1;

    if(!out || out_size == 0 || !current_area)
        return;

    out[0] = '\0';
    creature = bestiary_creature_at_3d(tx, ty, player.character.actor.entity.z);
    world_item = world_item_at_3d(tx, ty, player.character.actor.entity.z);
    world_item_count = world_item_count_at_3d(tx, ty, player.character.actor.entity.z);
    Furniture* furn = furniture_at(current_area, tx, ty);

    if(player.character.actor.entity.x == tx && player.character.actor.entity.y == ty)
    {
        offset += snprintf(out + offset, out_size - (size_t)offset, "You see [unit] Player");
        should_continue = player.character.actor.entity.hide_below ? 0 : 1;
    }
    else if(creature && creature->alive && creature->template)
    {
        offset += snprintf(out + offset, out_size - (size_t)offset, "You see [unit] %s", creature->template->name);
        should_continue = creature->actor.entity.hide_below ? 0 : 1;

        if(should_continue && world_item && world_item->active)
        {
            if(world_item_count > 1)
            {
                offset += snprintf(out + offset,
                                   out_size - (size_t)offset,
                                   ", [unit] %s (+%d more items)",
                                   world_item->item.name,
                                   world_item_count - 1);
            }
            else
            {
                offset += snprintf(out + offset, out_size - (size_t)offset, ", [unit] %s", world_item->item.name);
            }
            should_continue = world_item->item.object.base.hide_below ? 0 : 1;
        }
    }
    else if(world_item && world_item->active)
    {
        if(world_item_count > 1)
        {
            offset += snprintf(out + offset,
                               out_size - (size_t)offset,
                               "You see [unit] %s (+%d more items)",
                               world_item->item.name,
                               world_item_count - 1);
        }
        else
        {
            offset += snprintf(out + offset, out_size - (size_t)offset, "You see [unit] %s", world_item->item.name);
        }
        should_continue = world_item->item.object.base.hide_below ? 0 : 1;
    }
    else if(furn && furn->type != FURNITURE_NONE)
    {
        const char* furn_name = furniture_display_name(furn);

        offset += snprintf(out + offset, out_size - (size_t)offset, "You see %s [furniture]", furn_name);
        should_continue = furn->base.base.hide_below ? 0 : 1;
    }

    visible_count = map_collect_visible_static_layers(current_area, tx, ty, visible_tiles, visible_layers, TILE_LAYER_COUNT);
    for(int i = 0; i < visible_count && should_continue; i++)
    {
        const Tile* tile = visible_tiles[i];
        if(offset > 0)
            offset += snprintf(out + offset, out_size - (size_t)offset, ", ");

        offset += snprintf(out + offset,
                           out_size - (size_t)offset,
                           "[%s] %s",
                           tile_layer_name(visible_layers[i]),
                           tile->name[0] ? tile->name : "Unknown");

        if(tile->hide_below)
            should_continue = 0;
    }

    if(offset <= 0)
        snprintf(out, out_size, "Nothing visible at %d,%d.", tx, ty);
}

int inspect_query_at(Player* p, int tx, int ty, char* out, size_t out_size)
{
    if(!p || !current_area || !out || out_size == 0)
        return 0;

    inspect_format_result(out, out_size, tx, ty);
    return 1;
}

// Interactive mode to inspect a tile within line of sight.
static void inspect_tile_mode(Player* p)
{
    if(!p || !current_area)
        return;

    int px = p->character.actor.entity.x;
    int py = p->character.actor.entity.y;
    int tx = px;
    int ty = py;

    draw_set_inspect_cursor(tx, ty);
    log_add("Inspect mode: move cursor with arrows/WASD, Enter inspect, 1..9 attack mode, L lock/unlock, q cancel");

    char result_text[256] = "";
    int got_result = 0;

    while(1)
    {
        if(tx < 0) tx = 0;
        if(tx >= current_area->width) tx = current_area->width - 1;
        if(ty < 0) ty = 0;
        if(ty >= current_area->height) ty = current_area->height - 1;

        draw_set_inspect_cursor(tx, ty);
        draw_world(p);

        int key = read_input_key();
        switch(key)
        {
            case INPUT_KEY_UP: case 'w': case 'W': ty--; break; // up
            case INPUT_KEY_DOWN: case 's': case 'S': ty++; break; // down
            case INPUT_KEY_LEFT: case 'a': case 'A': tx--; break; // left
            case INPUT_KEY_RIGHT: case 'd': case 'D': tx++; break; // right
            case 'l': case 'L':
            {
                Creature* c = bestiary_creature_at_3d(tx, ty, p->character.actor.entity.z);
                WorldItem* world_item = world_item_at_3d(tx, ty, p->character.actor.entity.z);
                int item_count = world_item_count_at_3d(tx, ty, p->character.actor.entity.z);

                if(c && c->alive)
                {
                    int index = bestiary_index_of(c);
                    if(index >= 0 && current_area)
                    {
                        if(target_lock_matches_creature(p, index, current_area->name))
                        {
                            target_lock_clear(p);
                            log_add("Target lock cleared.");
                        }
                        else
                        {
                            target_lock_set_creature(p, index, current_area->name);
                            log_add("Target locked: %s at %d,%d", c->template->name, c->actor.entity.x, c->actor.entity.y);
                        }
                    }
                }
                else if(world_item)
                {
                    WorldItem* selected_item = world_item;
                    int index;

                    if(item_count > 1 &&
                       p->target_lock.active &&
                       p->target_lock.kind == TARGET_LOCK_WORLD_ITEM &&
                       current_area &&
                       strcmp(p->target_lock.area_name, current_area->name) == 0)
                    {
                        int current_index = p->target_lock.slot_index;
                        if(current_index >= 0 && current_index < MAX_WORLD_ITEMS)
                        {
                            WorldItem* current_locked_item = &world_items[current_index];
                            if(current_locked_item->active &&
                               current_locked_item->item.object.base.x == tx &&
                               current_locked_item->item.object.base.y == ty &&
                               strcmp(current_locked_item->area_name, current_area->name) == 0)
                            {
                                selected_item = world_item_next_at_3d(tx, ty, p->character.actor.entity.z, current_locked_item);
                            }
                        }
                    }

                    index = world_item_index_of(selected_item);
                    if(index >= 0 && current_area)
                    {
                        if(item_count <= 1 && target_lock_matches_world_item(p, index, current_area->name))
                        {
                            target_lock_clear(p);
                            log_add("Target lock cleared.");
                        }
                        else if(target_lock_set_world_item(p, index, current_area->name))
                        {
                            if(item_count > 1)
                            {
                                log_add("Target locked: %s at %d,%d (%d items here, press L again to cycle)",
                                        selected_item->item.name,
                                        selected_item->item.object.base.x,
                                        selected_item->item.object.base.y,
                                        item_count);
                            }
                            else
                            {
                                log_add("Target locked: %s at %d,%d",
                                        selected_item->item.name,
                                        selected_item->item.object.base.x,
                                        selected_item->item.object.base.y);
                            }
                        }
                    }
                }
                else
                {
                    if(p->target_lock.active)
                    {
                        target_lock_clear(p);
                        log_add("Target lock cleared.");
                    }
                    else
                    {
                        log_add("No lockable entity at %d,%d", tx, ty);
                    }
                }
                break;
            }
            case 'q': case 'Q':
                snprintf(result_text, sizeof(result_text), "Inspect canceled.");
                got_result = 1;
                goto inspect_done;
            case 13: // Enter
            {
                int visible = map_has_line_of_sight(px, py, tx, ty);

                if(!visible)
                {
                    snprintf(result_text, sizeof(result_text), "Tile %d,%d is not in sight", tx, ty);
                }
                else
                {
                    inspect_query_at(p, tx, ty, result_text, sizeof(result_text));
                }
                got_result = 1;
                goto inspect_done;
            }
            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':
            {
                int option_index = key - '0';
                int available_modes = 0;
                CombatProfile attack_profile = combat_profile_for_character_attack(&p->character, p->selected_attack_mode);
                AttackMode selected_mode = inspect_mode_from_option_index(attack_profile.attack_mode_mask, option_index, &available_modes);
                Creature* target = bestiary_creature_at_3d(tx, ty, p->character.actor.entity.z);

                if(selected_mode == ATTACK_MODE_NONE)
                {
                    if(available_modes <= 0)
                        log_add("No attack modes available for current weapon.");
                    else
                        log_add("Attack option %d out of range (1-%d).", option_index, available_modes);
                    break;
                }

                if(!target || !target->alive)
                {
                    log_add("No creature at %d,%d to attack.", tx, ty);
                    break;
                }

                p->selected_attack_mode = selected_mode;
                player_attack_creature(p, target, selected_mode);
                save_active_game(p);
                break;
            }
            case 'e': case 'E':
                if(interact_at(p, tx, ty))
                    save_active_game(p);
                break;
            default:
                break;
        }
    }

inspect_done:
    draw_clear_inspect_cursor();
    if(got_result)
        log_add("%s", result_text);
}

static int ranged_attack_mode(Player* p)
{
    TargetLockResolved lock;

    if(!p || !current_area)
        return 0;

    if(target_lock_resolve_live(p, &lock, 1) && lock.kind == TARGET_LOCK_CREATURE)
    {
        if(lock.slot_index >= 0 && lock.slot_index < MAX_CREATURES)
        {
            Creature* target = &creatures[lock.slot_index];
            if(target->alive)
                return player_ranged_attack_creature(p, target, p->selected_attack_mode);
        }
    }

    {
        int tx = p->character.actor.entity.x;
        int ty = p->character.actor.entity.y;

        draw_set_inspect_cursor(tx, ty);
        log_add("Ranged mode: move cursor, Enter fire, L lock/unlock, q cancel");

        while(1)
        {
            int key;

            if(tx < 0) tx = 0;
            if(tx >= current_area->width) tx = current_area->width - 1;
            if(ty < 0) ty = 0;
            if(ty >= current_area->height) ty = current_area->height - 1;

            draw_set_inspect_cursor(tx, ty);
            draw_world(p);

            key = read_input_key();
            switch(key)
            {
                case INPUT_KEY_UP: case 'w': case 'W': ty--; break;
                case INPUT_KEY_DOWN: case 's': case 'S': ty++; break;
                case INPUT_KEY_LEFT: case 'a': case 'A': tx--; break;
                case INPUT_KEY_RIGHT: case 'd': case 'D': tx++; break;
                case 'l': case 'L':
                {
                    Creature* c = bestiary_creature_at_3d(tx, ty, p->character.actor.entity.z);
                    if(c && c->alive)
                    {
                        int index = bestiary_index_of(c);
                        if(index >= 0 && current_area)
                        {
                            if(target_lock_matches_creature(p, index, current_area->name))
                            {
                                target_lock_clear(p);
                                log_add("Target lock cleared.");
                            }
                            else
                            {
                                target_lock_set_creature(p, index, current_area->name);
                                log_add("Target locked: %s at %d,%d", c->template->name, c->actor.entity.x, c->actor.entity.y);
                            }
                        }
                    }
                    else
                    {
                        log_add("No creature to lock at %d,%d", tx, ty);
                    }
                    break;
                }
                case 'q': case 'Q':
                    draw_clear_inspect_cursor();
                    log_add("Ranged mode canceled.");
                    return 0;
                case 13:
                {
                    draw_clear_inspect_cursor();
                    if(player_ranged_attack_tile(p, tx, ty, p->character.actor.entity.z, p->selected_attack_mode))
                        return 1;
                    break;
                }
                default:
                    break;
            }
        }
    }
}

static void camp_collect_option_from_item(const Item* item,
                                          char names[CAMP_OPTION_MAX][32],
                                          int counts[CAMP_OPTION_MAX],
                                          int* option_count)
{
    int slot;

    if(!item || !option_count)
        return;

    if(item->type == ITEM_TYPE_NONE || !item->camp_placeable)
        return;

    for(slot = 0; slot < *option_count; slot++)
    {
        if(strcmp(names[slot], item->name) == 0)
        {
            counts[slot] += (item->quantity > 0) ? item->quantity : 1;
            return;
        }
    }

    if(*option_count >= CAMP_OPTION_MAX)
        return;

    snprintf(names[*option_count], 32, "%s", item->name);
    counts[*option_count] = (item->quantity > 0) ? item->quantity : 1;
    (*option_count)++;
}

static int camp_collect_item_options(const Character* c,
                                     char names[CAMP_OPTION_MAX][32],
                                     int counts[CAMP_OPTION_MAX])
{
    int option_count = 0;

    if(!c)
        return 0;

    for(int i = 0; i < c->equipment_slot_count; i++) {
        if(c->equipment_slots[i].slot_type == EQUIP_SLOT_NONE && c->equipment_slots[i].item.type != ITEM_TYPE_NONE)
            camp_collect_option_from_item(&c->equipment_slots[i].item, names, counts, &option_count);
    }



    return option_count;
}

static int camp_can_place_at(const Player* p, int x, int y, char* reason, size_t reason_size)
{
    int pz;

    if(!p || !current_area)
    {
        snprintf(reason, reason_size, "No active area to place camp item.");
        return 0;
    }

    pz = p->character.actor.entity.z;

    if(x < 0 || y < 0 || x >= current_area->width || y >= current_area->height)
    {
        snprintf(reason, reason_size, "Placement out of bounds.");
        return 0;
    }

    if(map_cell_blocks_movement(current_area, x, y))
    {
        snprintf(reason, reason_size, "Blocked terrain: cannot place camp item here.");
        return 0;
    }

    if(creature_at_3d(x, y, pz))
    {
        snprintf(reason, reason_size, "A creature is occupying that tile.");
        return 0;
    }

    if(world_item_count_at_3d(x, y, pz) > 0)
    {
        snprintf(reason, reason_size, "Ground tile already has an item.");
        return 0;
    }

    return 1;
}

static int camp_setup_mode(Player* p, int in_combat)
{
    char camp_names[CAMP_OPTION_MAX][32] = {{0}};
    int camp_counts[CAMP_OPTION_MAX] = {0};
    int option_count;
    int selected_index = -1;
    int tx;
    int ty;

    if(!p || !current_area)
        return 0;

    if(in_combat)
    {
        log_add("Camp setup is not safe while enemies are nearby.");
        return 0;
    }

    option_count = camp_collect_item_options(&p->character, camp_names, camp_counts);
    if(option_count <= 0)
    {
        log_add("No camp items available. Bring bedrolls, tents, or campfires.");
        return 0;
    }

    log_add("Camp setup: choose item 1-%d, or Q cancel.", option_count);
    for(int i = 0; i < option_count; i++)
        log_add("%d) %s x%d", i + 1, camp_names[i], camp_counts[i]);

    while(1)
    {
        int key;

        draw_world(p);
        key = read_input_key();

        if(KEYBIND_CANCEL(key))
        {
            log_add("Camp setup canceled.");
            return 0;
        }

        if(key >= '1' && key <= '9')
        {
            int pick = key - '1';
            if(pick >= 0 && pick < option_count)
            {
                selected_index = pick;
                break;
            }
        }

        log_add("Invalid camp item selection.");
    }

    tx = p->character.actor.entity.x;
    ty = p->character.actor.entity.y;
    draw_set_inspect_cursor(tx, ty);
    log_add("Place %s: WASD/arrows move, Enter place, Q cancel.", camp_names[selected_index]);

    while(1)
    {
        int key;
        char reason[128] = "";

        if(tx < 0) tx = 0;
        if(tx >= current_area->width) tx = current_area->width - 1;
        if(ty < 0) ty = 0;
        if(ty >= current_area->height) ty = current_area->height - 1;

        draw_set_inspect_cursor(tx, ty);
        draw_world(p);

        key = read_input_key();
        switch(key)
        {
            case INPUT_KEY_UP: case 'w': case 'W': ty--; break;
            case INPUT_KEY_DOWN: case 's': case 'S': ty++; break;
            case INPUT_KEY_LEFT: case 'a': case 'A': tx--; break;
            case INPUT_KEY_RIGHT: case 'd': case 'D': tx++; break;
            case 13:
            {
                const ItemTemplate* tmpl;
                Item placed_item;
                Item refund_item;

                if(!camp_can_place_at(p, tx, ty, reason, sizeof(reason)))
                {
                    log_add("%s", reason);
                    break;
                }

                tmpl = item_template_by_name(camp_names[selected_index]);
                if(!tmpl)
                {
                    log_add("Camp item template missing: %s", camp_names[selected_index]);
                    draw_clear_inspect_cursor();
                    return 0;
                }

                // Slot-based consume logic
                int consumed = 0;
                for(int i = 0; i < p->character.equipment_slot_count; i++) {
                    if(p->character.equipment_slots[i].slot_type == EQUIP_SLOT_NONE &&
                       strcmp(p->character.equipment_slots[i].item.name, camp_names[selected_index]) == 0 &&
                       p->character.equipment_slots[i].item.type != ITEM_TYPE_NONE) {
                        p->character.equipment_slots[i].item.type = ITEM_TYPE_NONE;
                        p->character.equipment_slots[i].item.name[0] = '\0';
                        consumed = 1;
                        break;
                    }
                }
                if(!consumed) {
                    log_add("Camp item no longer available: %s", camp_names[selected_index]);
                    draw_clear_inspect_cursor();
                    return 0;
                }

                item_init_from_template(&placed_item, tmpl, tx, ty);
                placed_item.quantity = 1;
                placed_item.object.base.z = p->character.actor.entity.z;

                if(!world_item_drop_3d(&placed_item,
                                       current_area->name,
                                       tx,
                                       ty,
                                       p->character.actor.entity.z))
                {
                    item_init_from_template(&refund_item, tmpl, -1, -1);
                    refund_item.quantity = 1;
                    (void)inventory_add(&p->character, &refund_item);
                    log_add("Failed to place %s here.", camp_names[selected_index]);
                    draw_clear_inspect_cursor();
                    return 0;
                }

                log_add("You set up %s.", camp_names[selected_index]);
                draw_clear_inspect_cursor();
                return 1;
            }
            case 'q': case 'Q':
                log_add("Camp placement canceled.");
                draw_clear_inspect_cursor();
                return 0;
            default:
                break;
        }
    }
}

static int rest_camp_menu(Player* p, int in_combat)
{
    if(!p)
        return 0;

    log_add("Rest/Camp menu: 1) Rest  2) Camp setup  3) Sleep (bed gives better sleep)  Q) Cancel");

    while(1)
    {
        int key;

        draw_world(p);
        key = read_input_key();

        if(KEYBIND_CANCEL(key))
        {
            log_add("Rest/Camp canceled.");
            return 0;
        }

        if(key == '1')
        {
            if(player_start_rest(p, in_combat))
                return 1;
            continue;
        }

        if(key == '2')
        {
            if(camp_setup_mode(p, in_combat))
                return 1;
            continue;
        }

        if(key == '3')
        {
            if(player_start_sleep(p, in_combat))
                return 1;
            continue;
        }

        log_add("Invalid menu choice. Use 1, 2, 3, or Q.");
    }
}



// Spawn a light goblin presence around the starter glade perimeter.
static void spawn_initial_monsters(void)
{
    static CreatureTemplate* random_pool[] = {
        &goblin_template,
        &skeleton_template,
        &bat_template,
        &rat_template,
        &snake_template,
        &wolf_template,
        &dog_template,
        &cat_template,
        &horse_template,
        &mouse_template,
        &bird_template,
        &rabbit_template,
        &sheep_template,
        &goat_template,
    };

    if(current_area && current_area->type == LOCATION_STARTER)
    {
        // Fixed starter sample set: hostile and passive mix on safe center paths.
        spawn_monster(STARTER_PLAYER_START_X, STARTER_PLAYER_START_Y - 18, &wolf_template);
        spawn_monster(STARTER_PLAYER_START_X + 6, STARTER_PLAYER_START_Y - 18, &wolf_template);
        spawn_monster(STARTER_PLAYER_START_X - 18, STARTER_PLAYER_START_Y, &snake_template);
        spawn_monster(STARTER_PLAYER_START_X - 18, STARTER_PLAYER_START_Y + 6, &snake_template);
        spawn_monster(STARTER_PLAYER_START_X + 18, STARTER_PLAYER_START_Y, &rat_template);
        spawn_monster(STARTER_PLAYER_START_X + 18, STARTER_PLAYER_START_Y + 6, &rat_template);
        spawn_monster(STARTER_PLAYER_START_X, STARTER_PLAYER_START_Y + 18, &bat_template);
        spawn_monster(STARTER_PLAYER_START_X + 6, STARTER_PLAYER_START_Y + 18, &bat_template);
        spawn_monster(STARTER_PLAYER_START_X, STARTER_PLAYER_START_Y - 12, &dog_template);
        spawn_monster(STARTER_PLAYER_START_X + 6, STARTER_PLAYER_START_Y - 12, &dog_template);
        spawn_monster(STARTER_PLAYER_START_X - 12, STARTER_PLAYER_START_Y, &cat_template);
        spawn_monster(STARTER_PLAYER_START_X - 12, STARTER_PLAYER_START_Y + 6, &cat_template);
        spawn_monster(STARTER_PLAYER_START_X + 12, STARTER_PLAYER_START_Y, &rabbit_template);
        spawn_monster(STARTER_PLAYER_START_X + 12, STARTER_PLAYER_START_Y + 6, &rabbit_template);
        spawn_monster(STARTER_PLAYER_START_X, STARTER_PLAYER_START_Y + 12, &sheep_template);
        spawn_monster(STARTER_PLAYER_START_X + 6, STARTER_PLAYER_START_Y + 12, &sheep_template);

        for(int i = 0; i < 32; i++)
        {
            CreatureTemplate* tmpl = random_pool[rand() % (int)(sizeof(random_pool) / sizeof(random_pool[0]))];
            spawn_monster(-1, -1, tmpl);
        }
        return;
    }

    for(int i = 0; i < 8; i++)
    {
        CreatureTemplate* tmpl = random_pool[rand() % (int)(sizeof(random_pool) / sizeof(random_pool[0]))];
        spawn_monster(-1, -1, tmpl);
    }
}

static void spawn_initial_npcs(void)
{
    npc_init();

    if(current_area && current_area->type == LOCATION_STARTER)
        (void)spawn_old_hermit_npc();
}

// Initialize gameplay systems and one fresh run state.
static void furniture_sync_container_links(void)
{
    for(int area_i = 0; area_i < atlas_location_count; area_i++)
    {
        Area* area = &atlas[area_i];
        for(int i = 0; i < area->furniture_count; i++)
        {
            Furniture* f = &area->furniture[i];
            WorldContainer* storage_container = NULL;
            WorldContainer* input_container = NULL;
            WorldContainer* output_container = NULL;

            if(!f)
                continue;

            for(int container_i = 0; container_i < MAX_WORLD_CONTAINERS; container_i++)
            {
                WorldContainer* candidate = &world_containers[container_i];
                if(!candidate->active)
                    continue;
                if(strcmp(candidate->area_name, area->name) != 0)
                    continue;
                if(candidate->x != f->base.base.x || candidate->y != f->base.base.y || candidate->z != f->base.base.z)
                    continue;

                if(furniture_uses_container_type(f->type) &&
                   strcmp(candidate->label, furniture_container_label_for_type(f->type)) == 0)
                    storage_container = candidate;
                if(furniture_has_input_container_type(f->type) &&
                   strcmp(candidate->label, furniture_input_container_label_for_type(f->type)) == 0)
                    input_container = candidate;
                if(furniture_has_output_container_type(f->type) &&
                   strcmp(candidate->label, furniture_output_container_label_for_type(f->type)) == 0)
                    output_container = candidate;
            }

            if(storage_container)
                f->world_container_index = world_container_index_of(storage_container);
            else
                f->world_container_index = -1;

            if(input_container)
                f->input_world_container_index = world_container_index_of(input_container);
            else
                f->input_world_container_index = -1;

            if(output_container)
                f->output_world_container_index = world_container_index_of(output_container);
            else
                f->output_world_container_index = -1;

            if((!furniture_uses_container_type(f->type) || storage_container) &&
               (!furniture_has_input_container_type(f->type) || input_container) &&
               (!furniture_has_output_container_type(f->type) || output_container))
                f->interactable = 1;
            else
                f->interactable = 0;
        }
    }
}

static int initialize_game(const char* player_name, const char* player_race_id, const Actor* rolled_attributes)
{
    if(!template_content_load_all())
        return 0;

    // Initialize systems
    world_items_init();
    atlas_init();
    bestiary_init();
    log_init();
    world_map_init();
    world_map_load_tiles("data/templates/maps/world_map_tiles.csv");
    atlas_sync_world_map();
    seed_default_world_roads();

    furniture_sync_container_links();

    // Create player
    player_create(&player, player_name, player_race_id, rolled_attributes);
    apply_debug_mode_flags(&player);

    if(!place_player_for_current_area(&player, NULL))
        return 0;


    spawn_initial_monsters();
    spawn_initial_npcs();

    return 1;
}

static int initialize_loaded_game(const char* player_name, int selected_slot)
{
    char load_path[SAVEGAME_SLOT_PATH_LENGTH];

    if(!template_content_load_all())
        return 0;

    world_items_init();
    atlas_init();
    bestiary_init();
    log_init();
    world_map_init();
    world_map_load_tiles("data/templates/maps/world_map_tiles.csv");
    atlas_sync_world_map();
    seed_default_world_roads();
    player_create(&player, player_name, NULL, NULL);

    savegame_resolve_slot_path(selected_slot, load_path, sizeof(load_path));
    active_save_set_slot(selected_slot);

    if(!savegame_load(load_path, &player))
        return 0;

    furniture_sync_container_links();
    apply_debug_mode_flags(&player);
    spawn_initial_npcs();

    log_add("Loaded saved game.");
    return 1;
}


// Program entry point.
int main()
{
    StartupSettings settings;
    StartupSettingsResult load_result;

    // Seed RNG
    srand(time(NULL));

    load_result = startup_settings_load(STARTUP_SETTINGS_FILE, &settings);
    if(load_result == STARTUP_SETTINGS_RESULT_IO_ERROR)
        startup_settings_defaults(&settings);

    while(1)
    {
        StartupAction action = startup_run(&settings);
        active_save_set_slot(settings.selected_save_slot);
        if(action == STARTUP_ACTION_QUIT)
        {
            startup_settings_save(STARTUP_SETTINGS_FILE, &settings);
            printf("Goodbye!\n");
            return 0;
        }

        if((action == STARTUP_ACTION_START_GAME &&
            !initialize_game(settings.player_name,
                             settings.player_race_id,
                             settings.has_player_starting_attributes ? &settings.player_starting_attributes : NULL)) ||
           (action == STARTUP_ACTION_CONTINUE_GAME && !initialize_loaded_game(settings.player_name, settings.selected_save_slot)))
        {
            printf("\x1b[2J\x1b[H");
            if(template_content_last_error()[0] != '\0')
                printf("%s\n", template_content_last_error());
            printf("Failed to initialize game state. Press any key to return to menu.\n");
            read_input_key();
            continue;
        }

        game_session_begin(&player);

        int sprint_mode_enabled = 0;

        if(action == STARTUP_ACTION_START_GAME)
            save_active_game(&player);

        // =====================
        // Main game loop
        // =====================
        while(1)
        {

        int in_combat = has_adjacent_hostile(&player);

        if(player.is_resting || player.is_sleeping)
            player_recover_tick(&player, in_combat);

        if(current_area)
        {
            int vision_range = actor_area_vision_range(&player.character.actor);
            int px = player.character.actor.entity.x;
            int py = player.character.actor.entity.y;
            int pz = player.character.actor.entity.z;
            map_reveal_from_point(current_area, px, py, vision_range);

            // Bestiary encounter tracking: scan visible tiles for entities
            for(int dy = -vision_range; dy <= vision_range; dy++) {
                for(int dx = -vision_range; dx <= vision_range; dx++) {
                    int x = px + dx;
                    int y = py + dy;
                    if(x < 0 || y < 0 || x >= current_area->width || y >= current_area->height)
                        continue;
                    if((dx * dx) + (dy * dy) > (vision_range * vision_range))
                        continue;
                    if(!map_is_tile_discovered(current_area, x, y))
                        continue;

                    // Check for creatures
                    Creature* c = bestiary_creature_at_3d(x, y, pz);
                    if(c && c->alive && c->template && c->template->name[0]) {
                        bestiary_mark_sighted(c->template->name, BESTIARY_ENTRY_TYPE_MONSTER, c->actor.entity.id);
                    }

                    // Check for humanoid NPCs
                    NPC* npc = npc_at_3d(x, y, pz);
                    if(npc && npc->character.actor.body_layout == ACTOR_BODY_LAYOUT_HUMANOID && npc->character.actor.race_id[0]) {
                        bestiary_mark_sighted(npc->character.actor.race_id, BESTIARY_ENTRY_TYPE_RACE, npc->character.actor.entity.id);
                    }
                }
            }
        }

            // Draw everything
            draw_world(&player);

            // Handle input
            int c = read_input_key();

            if(actor_is_unconscious(&player.character.actor))
            {
                int was_unconscious = 1;

                if(c == 27)
                {
                    InGameSystemMenuAction menu_action = open_in_game_system_menu(&settings, &player);
                    if(menu_action == INGAME_SYSTEM_MENU_QUIT)
                    {
                        printf("Goodbye!\n");
                        return 0;
                    }
                }

                player.character.actor.stamina = actor_clamp_stamina_value(&player.character.actor,
                                                                            player.character.actor.stamina + 1);
                if(was_unconscious && !actor_is_unconscious(&player.character.actor))
                    log_add("You regain consciousness.");
                creatures_take_turns(&player);
                save_active_game(&player);
                continue;
            }

            if(draw_get_viewport_tab() == VIEWPORT_TAB_WORLD)
            {
                if(KEYBIND_MATCH_ALPHA(c, 't', 'T'))
                {
                    (void)open_world_map_exploration(&player);
                    save_active_game(&player);
                    continue;
                }

                if(c != 9 &&
                   (KEYBIND_MOVEMENT(c)
                    || KEYBIND_MATCH_ALPHA(c, 'f', 'F')
                    || KEYBIND_MATCH_ALPHA(c, 'e', 'E')
                    || c == ' ' || c == '.' || c == '>'))
                {
                    draw_set_viewport_tab(VIEWPORT_TAB_ZONE);
                }
            }

            switch(c)
            {
                case 'w': case INPUT_KEY_UP:
                    if(movement_attempt_exits_area(&player, 0, -1))
                    {
                        if(try_edge_travel(&player, 0, -1))
                            save_active_game(&player);
                        break;
                    }
                    if(sprint_mode_enabled)
                        player_sprint(&player, 0, -1, 1, 2);
                    else
                        player_move(&player, 0, -1);
                    save_active_game(&player);
                    break; // up
                case 'W':
                    if(movement_attempt_exits_area(&player, 0, -1))
                    {
                        if(try_edge_travel(&player, 0, -1))
                            save_active_game(&player);
                        break;
                    }
                    if(sprint_mode_enabled)
                        player_sprint(&player, 0, -1, 1, 2);
                    else
                        player_quickstep(&player, 0, -1);
                    save_active_game(&player);
                    break; // quickstep up
                case 'x': case INPUT_KEY_DOWN:
                    if(movement_attempt_exits_area(&player, 0, 1))
                    {
                        if(try_edge_travel(&player, 0, 1))
                            save_active_game(&player);
                        break;
                    }
                    if(sprint_mode_enabled)
                        player_sprint(&player, 0, 1, 1, 2);
                    else
                        player_move(&player, 0, 1);
                    save_active_game(&player);
                    break; // down
                case 'X':
                    if(movement_attempt_exits_area(&player, 0, 1))
                    {
                        if(try_edge_travel(&player, 0, 1))
                            save_active_game(&player);
                        break;
                    }
                    if(sprint_mode_enabled)
                        player_sprint(&player, 0, 1, 1, 2);
                    else
                        player_quickstep(&player, 0, 1);
                    save_active_game(&player);
                    break; // quickstep down
                case 'a': case INPUT_KEY_LEFT:
                    if(movement_attempt_exits_area(&player, -1, 0))
                    {
                        if(try_edge_travel(&player, -1, 0))
                            save_active_game(&player);
                        break;
                    }
                    if(sprint_mode_enabled)
                        player_sprint(&player, -1, 0, 1, 2);
                    else
                        player_move(&player, -1, 0);
                    save_active_game(&player);
                    break; // left
                case 'A':
                    if(movement_attempt_exits_area(&player, -1, 0))
                    {
                        if(try_edge_travel(&player, -1, 0))
                            save_active_game(&player);
                        break;
                    }
                    if(sprint_mode_enabled)
                        player_sprint(&player, -1, 0, 1, 2);
                    else
                        player_quickstep(&player, -1, 0);
                    save_active_game(&player);
                    break; // quickstep left
                case 'd': case INPUT_KEY_RIGHT:
                    if(movement_attempt_exits_area(&player, 1, 0))
                    {
                        if(try_edge_travel(&player, 1, 0))
                            save_active_game(&player);
                        break;
                    }
                    if(sprint_mode_enabled)
                        player_sprint(&player, 1, 0, 1, 2);
                    else
                        player_move(&player, 1, 0);
                    save_active_game(&player);
                    break; // right
                case 'D':
                    if(movement_attempt_exits_area(&player, 1, 0))
                    {
                        if(try_edge_travel(&player, 1, 0))
                            save_active_game(&player);
                        break;
                    }
                    if(sprint_mode_enabled)
                        player_sprint(&player, 1, 0, 1, 2);
                    else
                        player_quickstep(&player, 1, 0);
                    save_active_game(&player);
                    break; // quickstep right
                case 'q': case 'Q':
                    if(movement_attempt_exits_area(&player, -1, -1))
                    {
                        if(try_edge_travel(&player, -1, -1))
                            save_active_game(&player);
                        break;
                    }
                    if((c) == 'Q')
                    {
                        if(sprint_mode_enabled)
                            player_sprint(&player, -1, -1, 1, 2);
                        else
                            player_quickstep(&player, -1, -1);
                    }
                    else
                    {
                        if(sprint_mode_enabled)
                            player_sprint(&player, -1, -1, 1, 2);
                        else
                            player_move(&player, -1, -1);
                    }
                    save_active_game(&player);
                    break; // up-left
                case 'e': case 'E':
                    if(movement_attempt_exits_area(&player, 1, -1))
                    {
                        if(try_edge_travel(&player, 1, -1))
                            save_active_game(&player);
                        break;
                    }
                    if((c) == 'E')
                    {
                        if(sprint_mode_enabled)
                            player_sprint(&player, 1, -1, 1, 2);
                        else
                            player_quickstep(&player, 1, -1);
                    }
                    else
                    {
                        if(sprint_mode_enabled)
                            player_sprint(&player, 1, -1, 1, 2);
                        else
                            player_move(&player, 1, -1);
                    }
                    save_active_game(&player);
                    break; // up-right
                case 'z': case 'Z':
                    if(movement_attempt_exits_area(&player, -1, 1))
                    {
                        if(try_edge_travel(&player, -1, 1))
                            save_active_game(&player);
                        break;
                    }
                    if((c) == 'Z')
                    {
                        if(sprint_mode_enabled)
                            player_sprint(&player, -1, 1, 1, 2);
                        else
                            player_quickstep(&player, -1, 1);
                    }
                    else
                    {
                        if(sprint_mode_enabled)
                            player_sprint(&player, -1, 1, 1, 2);
                        else
                            player_move(&player, -1, 1);
                    }
                    save_active_game(&player);
                    break; // down-left
                case 'c': case 'C':
                    if(movement_attempt_exits_area(&player, 1, 1))
                    {
                        if(try_edge_travel(&player, 1, 1))
                            save_active_game(&player);
                        break;
                    }
                    if((c) == 'C')
                    {
                        if(sprint_mode_enabled)
                            player_sprint(&player, 1, 1, 1, 2);
                        else
                            player_quickstep(&player, 1, 1);
                    }
                    else
                    {
                        if(sprint_mode_enabled)
                            player_sprint(&player, 1, 1, 1, 2);
                        else
                            player_move(&player, 1, 1);
                    }
                    save_active_game(&player);
                    break; // down-right
                case INPUT_KEY_PGUP:
                    draw_nudge_view_layer(1, &player);
                    log_add("View layer z=%d/%d (player z=%d)",
                            draw_get_view_layer(&player),
                            map_max_view_floor(current_area),
                            player.character.actor.entity.z);
                    break;
                case INPUT_KEY_PGDN:
                    draw_nudge_view_layer(-1, &player);
                    log_add("View layer z=%d/%d (player z=%d)",
                            draw_get_view_layer(&player),
                            map_max_view_floor(current_area),
                            player.character.actor.entity.z);
                    break;
                case INPUT_KEY_HOME:
                    draw_reset_view_layer_to_player();
                    log_add("View reset to player z=%d", player.character.actor.entity.z);
                    break;
                case ' ':
                    log_add("You pass your turn.");
                    creatures_take_turns(&player);
                    save_active_game(&player);
                    break;
                case 'f': case 'F':
                    if(attack_action_mode(&player))
                        save_active_game(&player);
                    break;
                case '.': case '>':
                    if(rest_camp_menu(&player, in_combat))
                        save_active_game(&player);
                    break;

                case 'i': case 'I':
                    overlay_open(OVERLAY_TYPE_INVENTORY, &player);
                    save_active_game(&player);
                    break;
                case 's': case 'S':
                    quick_interact(&player);
                    save_active_game(&player);
                    break;
                case 'r': case 'R':
                    sprint_mode_enabled = !sprint_mode_enabled;
                    log_add("Movement mode: %s (WAXD uses this mode)",
                            sprint_mode_enabled ? "Sprint" : "Walk");
                    break;
                case 't': case 'T':
                    inspect_tile_mode(&player);
                    break;
                case 'v': case 'V':
                    if(player_toggle_versatile_grip(&player))
                        save_active_game(&player);
                    break;
                case 'l': case 'L':
                    log_add("Open Codex with O, then choose Log.");
                    break;
                case 'u': case 'U':
                    overlay_open(OVERLAY_TYPE_CHARACTER, &player);
                    save_active_game(&player);
                    break;
                case 'j': case 'J':
                    log_add("Open Codex with O, then choose Journal.");
                    break;
                case 'o': case 'O':
                    overlay_open(OVERLAY_TYPE_CODEX, &player);
                    save_active_game(&player);
                    break;
                case 'p': case 'P':
                    log_add("Press Esc to open the game menu.");
                    break;
                case 'm': case 'M':
                    log_add("Open Codex with O, then choose World Map.");
                    break;
                case 27:
                {
                    InGameSystemMenuAction menu_action = open_in_game_system_menu(&settings, &player);
                    if(menu_action == INGAME_SYSTEM_MENU_QUIT)
                    {
                        printf("Goodbye!\n");
                        return 0;
                    }
                    break;
                }
                default:
                    // ignore unknown input
                    break;
            }
        }
    }

    return 0;
}

