#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "atlas.h"
#include "player.h"
#include "bestiary.h"
#include "spawn.h"
#include "draw.h"
#include "map.h"
#include "movement.h"
#include "combat.h"
#include "overlay_nav.h"
#include "input.h"
#include "log.h"
#include "savegame.h"
#include "startup.h"
#include "target_lock.h"
#include "interact.h"
#include "world_items.h"
#include "world_map.h"

/*
 * Purpose:
 *   Hosts program entry point, startup/menu flow, and main gameplay loop.
 *
 * Functions:
 *   - initialize_game: initializes systems, creates player, and spawns monsters.
 *   - main: runs startup menu loop and in-game input/render loop.
 */

// Place the player at the handcrafted starter spawn or on a random valid tile elsewhere.
static int place_player_for_current_area(void)
{
    if(current_area && current_area->type == LOCATION_STARTER)
    {
        player_place(&player, current_area->width / 2, current_area->height / 2);
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
    log_add("Inspect mode: move cursor with arrows/WASD, Enter inspect, E interact, 1..9 attack mode, L lock/unlock, q cancel");

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
                Creature* c = bestiary_creature_at(tx, ty);
                WorldItem* world_item = world_item_at(tx, ty);

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
                    int index = world_item_index_of(world_item);
                    if(index >= 0 && current_area)
                    {
                        if(target_lock_matches_world_item(p, index, current_area->name))
                        {
                            target_lock_clear(p);
                            log_add("Target lock cleared.");
                        }
                        else
                        {
                            target_lock_set_world_item(p, index, current_area->name);
                            log_add("Target locked: %s at %d,%d", world_item->item.name, world_item->item.entity.x, world_item->item.entity.y);
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
                Tile* tile = &current_area->map[ty][tx];
                Creature* c = bestiary_creature_at(tx, ty);
                int visible = map_has_line_of_sight(px, py, tx, ty);

                if(!visible)
                {
                    snprintf(result_text, sizeof(result_text), "Tile %d,%d is not in sight", tx, ty);
                }
                else if(c)
                {
                    snprintf(result_text, sizeof(result_text), "Tile %d,%d: %s (%c), Creature: %s, blocks_sight=%d, movement=%d, projectile=%d",
                             tx, ty, tile->name, tile->symbol, c->template->name,
                             tile->blocks_sight, tile->blocks_movement, tile->blocks_projectile);
                }
                else
                {
                    snprintf(result_text, sizeof(result_text), "Tile %d,%d: %s (%c), blocks_sight=%d, movement=%d, projectile=%d",
                             tx, ty, tile->name, tile->symbol,
                             tile->blocks_sight, tile->blocks_movement, tile->blocks_projectile);
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
                Creature* target = bestiary_creature_at(tx, ty);

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
                savegame_save(SAVEGAME_FILE, p);
                break;
            }
            case 'e': case 'E':
                if(inspect_interact_at(p, tx, ty))
                    savegame_save(SAVEGAME_FILE, p);
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
static int initialize_game(void)
{
    // Initialize systems
    atlas_init();
    bestiary_init();
    log_init();
    world_map_init();
    world_items_init();

    // Create player
    player_create(&player, "Hero");

    if(!place_player_for_current_area())
        return 0;

    spawn_initial_monsters();

    return 1;
}

static int initialize_loaded_game(void)
{
    atlas_init();
    bestiary_init();
    log_init();
    world_map_init();
    world_items_init();
    player_create(&player, "Hero");

    if(!savegame_load(SAVEGAME_FILE, &player))
        return 0;

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
        if(action == STARTUP_ACTION_QUIT)
        {
            startup_settings_save(STARTUP_SETTINGS_FILE, &settings);
            printf("Goodbye!\n");
            return 0;
        }

        if((action == STARTUP_ACTION_START_GAME && !initialize_game()) ||
           (action == STARTUP_ACTION_CONTINUE_GAME && !initialize_loaded_game()))
        {
            printf("\x1b[2J\x1b[H");
            printf("Failed to initialize game state. Press any key to return to menu.\n");
            read_input_key();
            continue;
        }

        if(action == STARTUP_ACTION_START_GAME)
            savegame_save(SAVEGAME_FILE, &player);

        // =====================
        // Main game loop
        // =====================
        while(1)
        {
            player.selected_attack_mode = combat_valid_attack_mode_for_character(&player.character, player.selected_attack_mode);

            // Draw everything
            draw_world(&player);

            // Handle input
            int c = read_input_key();

            switch(c)
            {
                case 'w': case INPUT_KEY_UP:
                    player_move(&player, 0, -1);
                    savegame_save(SAVEGAME_FILE, &player);
                    break; // up
                case 'W':
                    player_sprint(&player, 0, -1, 2);
                    savegame_save(SAVEGAME_FILE, &player);
                    break; // sprint up
                case 's': case INPUT_KEY_DOWN:
                    player_move(&player, 0, 1);
                    savegame_save(SAVEGAME_FILE, &player);
                    break; // down
                case 'S':
                    player_sprint(&player, 0, 1, 2);
                    savegame_save(SAVEGAME_FILE, &player);
                    break; // sprint down
                case 'a': case INPUT_KEY_LEFT:
                    player_move(&player, -1, 0);
                    savegame_save(SAVEGAME_FILE, &player);
                    break; // left
                case 'A':
                    player_sprint(&player, -1, 0, 2);
                    savegame_save(SAVEGAME_FILE, &player);
                    break; // sprint left
                case 'd': case INPUT_KEY_RIGHT:
                    player_move(&player, 1, 0);
                    savegame_save(SAVEGAME_FILE, &player);
                    break; // right
                case 'D':
                    player_sprint(&player, 1, 0, 2);
                    savegame_save(SAVEGAME_FILE, &player);
                    break; // sprint right
                case ' ':
                    log_add("You wait.");
                    savegame_save(SAVEGAME_FILE, &player);
                    break;

                case 'i': case 'I':
                    overlay_open(OVERLAY_TYPE_INVENTORY, &player);
                    savegame_save(SAVEGAME_FILE, &player);
                    break;
                case 'e': case 'E':
                    inventory_quick_equip(&player.character);
                    savegame_save(SAVEGAME_FILE, &player);
                    break;
                case 't': case 'T':
                    inspect_tile_mode(&player);
                    break;
                case 'm': case 'M':
                    overlay_open(OVERLAY_TYPE_LOG, &player);
                    savegame_save(SAVEGAME_FILE, &player);
                    break;
                case 'c': case 'C':
                    overlay_open(OVERLAY_TYPE_CHARACTER, &player);
                    savegame_save(SAVEGAME_FILE, &player);
                    break;
                case 'j': case 'J':
                    overlay_open(OVERLAY_TYPE_JOURNAL, &player);
                    savegame_save(SAVEGAME_FILE, &player);
                    break;
                case 'o': case 'O':
                    overlay_open(OVERLAY_TYPE_ATLAS, &player);
                    savegame_save(SAVEGAME_FILE, &player);
                    break;
                case 'f': case 'F':
                {
                    CombatProfile attack_profile = combat_profile_for_character_attack(&player.character, player.selected_attack_mode);
                    AttackMode next_mode = attack_mode_next_from_mask(attack_profile.attack_mode_mask, attack_profile.attack_mode);

                    if(next_mode != ATTACK_MODE_NONE)
                    {
                        player.selected_attack_mode = next_mode;
                        attack_profile = combat_profile_for_character_attack(&player.character, player.selected_attack_mode);
                        log_add("Attack mode: %s (%s)", attack_mode_name(attack_profile.attack_mode), damage_type_name(attack_profile.active_damage_type));
                        savegame_save(SAVEGAME_FILE, &player);
                    }
                    break;
                }

                case 'q': case 'Q':
                    savegame_save(SAVEGAME_FILE, &player);
                    startup_settings_save(STARTUP_SETTINGS_FILE, &settings);
                    printf("Goodbye!\n");
                    return 0;
                default:
                    // ignore unknown input
                    break;
            }
        }
    }

    return 0;
}

