#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

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

static void spawn_initial_monsters(void);
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

        ui_overlay_draw_line(0, "Esc close | W/S move | Enter select");

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

        if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
        {
            selected++;
            if(selected >= menu_item_count)
                selected = 0;
            continue;
        }

        if(key != 13)
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

static int place_player_for_current_area(void)
{
    if(current_area && current_area == &atlas[4])
    {
        int stairs_x;
        int stairs_y;

        if(find_stairs_up_position(&stairs_x, &stairs_y))
        {
            player_place(&player, stairs_x, stairs_y);
            return 1;
        }
    }

    if(current_area && current_area->type == LOCATION_STARTER)
    {
        player_place(&player, STARTER_PLAYER_START_X, STARTER_PLAYER_START_Y);
        return 1;
    }

    return player_place_random(&player);
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
        { ATTACK_MODE_FLAG_STAB, ATTACK_MODE_STAB },
        { ATTACK_MODE_FLAG_CUT, ATTACK_MODE_CUT },
        { ATTACK_MODE_FLAG_SMASH, ATTACK_MODE_SMASH },
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
            return "Heavy unarmed kick. Slower but stronger than a punch.";
        case ATTACK_MODE_STAB:
            return "Precise thrust. Strong versus gaps and vulnerable targets.";
        case ATTACK_MODE_CUT:
            return "Sweeping slash. Balanced accuracy and sustained wound pressure.";
        case ATTACK_MODE_SMASH:
            return "Crushing blow. Highest impact, best against armor.";
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

static int open_melee_attack_menu(Player* p, AttackMode* out_mode)
{
    AttackMode options[9];
    int option_count;
    int selected = 0;

    if(!p || !out_mode)
        return 0;

    option_count = collect_attack_modes_for_character(&p->character, p->selected_attack_mode, options);
    if(option_count <= 0)
    {
        log_add("No attack modes available for current weapon.");
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
        int line_i = 0;
        int key;
        char line[192];

        ui_overlay_draw_frame("Melee Attack");

        total_options = option_count + 1;
        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        list_limit = status_line - 5;
        if(list_limit < 4)
            list_limit = 4;

        ui_overlay_draw_line(line_i++, "Choose an attack action:");
        ui_overlay_draw_line(line_i++, "");

        for(int i = 0; i < option_count && line_i < list_limit - 1; i++)
        {
            CombatSummary row_summary = combat_summary_for_character(&p->character, options[i]);
            CombatProfile row_profile = combat_profile_for_character_attack(&p->character, options[i]);
            int ap_cost = combat_profile_attack_action_point_cost(&row_profile);

            snprintf(line,
                     sizeof(line),
                     "%c %d. %-6s Hit:%3d%% Dmg:%2d AP:%2d",
                     (i == selected) ? '>' : ' ',
                     i + 1,
                     attack_mode_name(options[i]),
                     row_summary.hit_chance,
                     row_summary.damage,
                     ap_cost);
            ui_overlay_draw_line(line_i++, line);
        }

        if(line_i < list_limit)
        {
            snprintf(line,
                     sizeof(line),
                     "%c R. Recover action points (+2 AP for 1 Sta)",
                     (selected == option_count) ? '>' : ' ');
            ui_overlay_draw_line(line_i++, line);
        }

        while(line_i < list_limit)
            ui_overlay_draw_line(line_i++, "");

        if(selected < option_count)
        {
            CombatSummary selected_summary = combat_summary_for_character(&p->character, options[selected]);
            CombatProfile selected_profile = combat_profile_for_character_attack(&p->character, options[selected]);

            ui_overlay_draw_line(line_i++, "Attack Details:");
            snprintf(line,
                     sizeof(line),
                     "Mode: %s   Damage Type: %s",
                     attack_mode_name(selected_summary.attack_mode),
                     damage_type_name(selected_summary.active_damage_type));
            ui_overlay_draw_line(line_i++, line);
            ui_overlay_draw_line(line_i++, attack_mode_description(options[selected]));
            snprintf(line,
                     sizeof(line),
                     "Reach: %d   AP Cost: %d   Current AP: %d/%d",
                     combat_profile_melee_range(&selected_profile),
                     combat_profile_attack_action_point_cost(&selected_profile),
                     p->character.actor.action_points,
                     p->character.actor.max_action_points);
            ui_overlay_draw_line(line_i++, line);
        }
        else
        {
            ui_overlay_draw_line(line_i++, "Recover Action Points:");
            ui_overlay_draw_line(line_i++, "Spend 1 stamina to recover 2 action points.");
            snprintf(line,
                     sizeof(line),
                     "Current AP: %d/%d   Stamina: %d/%d",
                     p->character.actor.action_points,
                     p->character.actor.max_action_points,
                     p->character.actor.stamina,
                     p->character.actor.max_stamina);
            ui_overlay_draw_line(line_i++, line);
        }

        ui_overlay_draw_line(status_line, "Enter confirm | W/S move | 1-9 select | R recover | Q cancel");
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
        if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
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
            if(option >= 0 && option < option_count)
                selected = option;
            continue;
        }
        if(key == 'r' || key == 'R')
        {
            selected = option_count;
            continue;
        }
        if(key == 13)
        {
            *out_mode = (selected < option_count) ? options[selected] : ATTACK_MODE_NONE;
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
        ui_overlay_draw_line(3, "Choose direction with WASD or arrow keys.");
        ui_overlay_draw_line(4, "Reach weapons can hit farther in chosen direction.");
        ui_overlay_draw_line(ui_overlay_content_lines() - 2, "Q cancel");
        ui_overlay_draw_global_hotkeys();

        key = read_input_key();
        switch(key)
        {
            case INPUT_KEY_UP: case 'w': case 'W': dy = -1; dx = 0; break;
            case INPUT_KEY_DOWN: case 's': case 'S': dy = 1; dx = 0; break;
            case INPUT_KEY_LEFT: case 'a': case 'A': dx = -1; dy = 0; break;
            case INPUT_KEY_RIGHT: case 'd': case 'D': dx = 1; dy = 0; break;
            case 'q': case 'Q': case 27:
                log_add("Melee attack canceled.");
                return 0;
            default:
                continue;
        }

        return player_attack_direction(p, dx, dy, selected_mode);
    }
}

static int melee_attack_mode(Player* p)
{
    AttackMode selected_mode;

    if(!p)
        return 0;

    if(!open_melee_attack_menu(p, &selected_mode))
        return 0;

    if(selected_mode == ATTACK_MODE_NONE)
    {
        if(player_recover_action_points_from_stamina(p, 1, 2))
        {
            creatures_take_turns(p);
            return 1;
        }
        return 0;
    }

    p->selected_attack_mode = selected_mode;
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

static int complete_travel_to_index(Player* p, int area_index)
{
    int previous_area_index = -1;
    int previous_x;
    int previous_y;
    int previous_z;

    if(!p || area_index < 0 || area_index >= MAX_AREAS)
        return 0;

    if(current_area == &atlas[area_index])
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

    if(!place_player_for_current_area())
    {
        if(previous_area_index >= 0 && previous_area_index < MAX_AREAS)
        {
            atlas_travel(previous_area_index);
            bestiary_init();
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

    p->travelling = 0;
    spawn_initial_monsters();
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

    return complete_travel_to_index(p, selected);
}

static int open_world_map_exploration(Player* p)
{
    int selected = -1;
    int previous_x;
    int previous_y;
    int previous_z;

    if(!p)
        return 0;

    if(current_area && (current_area->type == LOCATION_CRYPT || current_area->type == LOCATION_CAVERN || current_area->type == LOCATION_DUNGEON))
    {
        log_add("Overworld map unavailable in underground areas.");
        ui_overlay_show_mini_prompt("Travel Unavailable",
                                    "You are underground.",
                                    "Reach the surface to use world map travel.");
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

    previous_x = p->character.actor.entity.x;
    previous_y = p->character.actor.entity.y;
    previous_z = p->character.actor.entity.z;

    p->travelling = 1;
    p->character.actor.entity.x = -1;
    p->character.actor.entity.y = -1;
    p->character.actor.entity.z = AREA_GROUND_Z;

    (void)world_map_show_overlay(p);
    selected = world_map_overlay_take_selected_area();
    if(selected < 0)
    {
        p->travelling = 0;
        p->character.actor.entity.x = previous_x;
        p->character.actor.entity.y = previous_y;
        p->character.actor.entity.z = previous_z;

        OverlayType next = overlay_take_request();
        if(next != OVERLAY_TYPE_NONE)
            overlay_open(next, p);
        return 0;
    }

    return complete_travel_to_index(p, selected);
}

static int try_edge_travel(Player* p)
{
    if(!p)
        return 0;

    log_add("Edge reached. [T]ravel or [S]tay?");
    draw_world(p);

    while(1)
    {
        int key = read_input_key();

        if(key == 's' || key == 'S' || key == 'o' || key == 'O' || key == 'q' || key == 'Q' || key == 27)
        {
            log_add("You stay where you are.");
            return 0;
        }

        if(key == 't' || key == 'T')
        {
            if(has_adjacent_hostile(p))
            {
                log_add("Travel blocked: hostile enemy adjacent.");
                ui_overlay_show_mini_prompt("Travel Blocked",
                                            "A hostile enemy is adjacent.",
                                            "Move away before attempting travel.");
                return 0;
            }

            return open_world_map_exploration(p);
        }
    }
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
                    log_add("No lockable entity at %d,%d", tx, ty);
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

    log_add("Rest/Camp menu: 1) Rest  2) Camp setup  3) Sleep  Q) Cancel");

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
        spawn_monster(STARTER_PLAYER_START_X - 18, STARTER_PLAYER_START_Y, &snake_template);
        spawn_monster(STARTER_PLAYER_START_X + 18, STARTER_PLAYER_START_Y, &rat_template);
        spawn_monster(STARTER_PLAYER_START_X, STARTER_PLAYER_START_Y + 18, &bat_template);
        spawn_monster(STARTER_PLAYER_START_X, STARTER_PLAYER_START_Y - 12, &dog_template);
        spawn_monster(STARTER_PLAYER_START_X - 12, STARTER_PLAYER_START_Y, &cat_template);
        spawn_monster(STARTER_PLAYER_START_X + 12, STARTER_PLAYER_START_Y, &rabbit_template);
        spawn_monster(STARTER_PLAYER_START_X, STARTER_PLAYER_START_Y + 12, &sheep_template);

        for(int i = 0; i < 4; i++)
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

// Initialize gameplay systems and one fresh run state.
static void furniture_sync_container_links(void)
{
    for(int area_i = 0; area_i < atlas_location_count; area_i++)
    {
        Area* area = &atlas[area_i];
        for(int i = 0; i < area->furniture_count; i++)
        {
            Furniture* f = &area->furniture[i];
            WorldContainer* container = NULL;

            if(!f || !furniture_uses_container_type(f->type))
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
                container = candidate;
                break;
            }

            if(container)
            {
                f->world_container_index = world_container_index_of(container);
                f->interactable = 1;
            }
            else
            {
                f->world_container_index = -1;
                f->interactable = 0;
            }
        }
    }
}

static int initialize_game(const char* player_name)
{
    if(!template_content_load_all())
        return 0;

    // Initialize systems
    world_items_init();
    atlas_init();
    bestiary_init();
    log_init();
    world_map_init();
    atlas_sync_world_map();
    world_map_load_biomes("data/templates/maps/world_biomes.txt");

    furniture_sync_container_links();

    // Create player
    player_create(&player, player_name);
    apply_debug_mode_flags(&player);

    if(!place_player_for_current_area())
        return 0;


    spawn_initial_monsters();

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
    atlas_sync_world_map();
    world_map_load_biomes("data/templates/maps/world_biomes.txt");
    player_create(&player, player_name);

    savegame_resolve_slot_path(selected_slot, load_path, sizeof(load_path));
    active_save_set_slot(selected_slot);

    if(!savegame_load(load_path, &player))
        return 0;

    furniture_sync_container_links();
    apply_debug_mode_flags(&player);


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

        if((action == STARTUP_ACTION_START_GAME && !initialize_game(settings.player_name)) ||
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
                map_reveal_from_point(current_area,
                                      player.character.actor.entity.x,
                                      player.character.actor.entity.y,
                                      vision_range);
            }

            // Draw everything
            draw_world(&player);

            // Handle input
            int c = read_input_key();

            switch(c)
            {
                case 'w': case INPUT_KEY_UP:
                    if(movement_attempt_exits_area(&player, 0, -1))
                    {
                        if(try_edge_travel(&player))
                            save_active_game(&player);
                        break;
                    }
                    player_move(&player, 0, -1);
                    save_active_game(&player);
                    break; // up
                case 'W':
                    if(movement_attempt_exits_area(&player, 0, -1))
                    {
                        if(try_edge_travel(&player))
                            save_active_game(&player);
                        break;
                    }
                    player_sprint(&player, 0, -1, 3);
                    save_active_game(&player);
                    break; // sprint up
                case 's': case INPUT_KEY_DOWN:
                    if(movement_attempt_exits_area(&player, 0, 1))
                    {
                        if(try_edge_travel(&player))
                            save_active_game(&player);
                        break;
                    }
                    player_move(&player, 0, 1);
                    save_active_game(&player);
                    break; // down
                case 'S':
                    if(movement_attempt_exits_area(&player, 0, 1))
                    {
                        if(try_edge_travel(&player))
                            save_active_game(&player);
                        break;
                    }
                    player_sprint(&player, 0, 1, 3);
                    save_active_game(&player);
                    break; // sprint down
                case 'a': case INPUT_KEY_LEFT:
                    if(movement_attempt_exits_area(&player, -1, 0))
                    {
                        if(try_edge_travel(&player))
                            save_active_game(&player);
                        break;
                    }
                    player_move(&player, -1, 0);
                    save_active_game(&player);
                    break; // left
                case 'A':
                    if(movement_attempt_exits_area(&player, -1, 0))
                    {
                        if(try_edge_travel(&player))
                            save_active_game(&player);
                        break;
                    }
                    player_sprint(&player, -1, 0, 3);
                    save_active_game(&player);
                    break; // sprint left
                case 'd': case INPUT_KEY_RIGHT:
                    if(movement_attempt_exits_area(&player, 1, 0))
                    {
                        if(try_edge_travel(&player))
                            save_active_game(&player);
                        break;
                    }
                    player_move(&player, 1, 0);
                    save_active_game(&player);
                    break; // right
                case 'D':
                    if(movement_attempt_exits_area(&player, 1, 0))
                    {
                        if(try_edge_travel(&player))
                            save_active_game(&player);
                        break;
                    }
                    player_sprint(&player, 1, 0, 3);
                    save_active_game(&player);
                    break; // sprint right
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
                    player_wait(&player, in_combat);
                    save_active_game(&player);
                    break;
                case 'r': case 'R':
                    if(ranged_attack_mode(&player))
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
                case 'e': case 'E':
                    quick_interact(&player);
                    save_active_game(&player);
                    break;
                case 'u': case 'U':
                    save_active_game(&player);
                    break;
                case 't': case 'T':
                    inspect_tile_mode(&player);
                    break;
                case 'm': case 'M':
                    overlay_open(OVERLAY_TYPE_LOG, &player);
                    save_active_game(&player);
                    break;
                case 'c': case 'C':
                    overlay_open(OVERLAY_TYPE_CHARACTER, &player);
                    save_active_game(&player);
                    break;
                case 'j': case 'J':
                    overlay_open(OVERLAY_TYPE_JOURNAL, &player);
                    save_active_game(&player);
                    break;
                case 'o': case 'O':
                    (void)open_atlas_for_travel(&player, ATLAS_OVERLAY_MODE_VIEW);
                    save_active_game(&player);
                    break;
                case 'p': case 'P':
                    log_add("Press Esc to open the game menu.");
                    break;
                case 9: // Tab
                    (void)open_world_map_exploration(&player);
                    save_active_game(&player);
                    break;
                case 'f': case 'F':
                {
                    if(melee_attack_mode(&player))
                        save_active_game(&player);
                    break;
                }

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

                case 'q': case 'Q':
                    log_add("Press Esc to open the game menu.");
                    break;
                default:
                    // ignore unknown input
                    break;
            }
        }
    }

    return 0;
}

