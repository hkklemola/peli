#include <stdio.h>
#include <stdlib.h>
#include <conio.h> // _getch()
#include <stddef.h>

#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/hud.h"
#include "d:/projekti/peli/include/menu.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/input.h"
#include "d:/projekti/peli/include/spawn.h"
#include "d:/projekti/peli/include/draw.h"
#include "d:/projekti/peli/include/tile.h"
#include "d:/projekti/peli/include/tileset.h"

int main() {
    start_screen();
    // Initialize systems
    atlas_init();      // setup atlas and areas
    generate_map();    // generate dungeon rooms + corridors
    bestiary_init();   // clear creatures
    hud_init();        // HUD setup
    log_init();        // message log setup

    // Character creation
    Creature* player = NULL;
    character_creator(&player); // sets player name, stats, etc.

    // Spawn player in the center of first room
    player = spawn_player(MAP_WIDTH/2, MAP_HEIGHT/2, player->name);

    // Spawn some enemies
    spawn_goblin(10, 10);
    spawn_goblin(15, 7);

    // Game loop
    int game_running = 1;
    while(game_running) {
        draw(player);      // draw map, creatures, HUD, log
        input_handle(player, &game_running); // handle input & movement
    }

    return 0;
}