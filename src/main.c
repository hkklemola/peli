#include <stdio.h>
#include <stdlib.h>
#include <conio.h> // _getch()
#include <stddef.h>
#include <time.h>

#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/input.h"
#include "d:/projekti/peli/include/spawn.h"
#include "d:/projekti/peli/include/draw.h"
#include "d:/projekti/peli/include/tile.h"
#include "d:/projekti/peli/include/tileset.h"
#include "d:/projekti/peli/include/player.h"
#include "d:/projekti/peli/include/hud.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/collision.h"
int main()
{
   
    atlas_init();
    bestiary_init();
    hud_init();
    log_init();

    // Spawn player
   spawn_player(&player, MAP_WIDTH/2, MAP_HEIGHT/2, "Hero");
   // Spawn monsters
    spawn_monster(10, 10, &goblin_template);
    spawn_monster(15, 7, &skeleton_template);
    while(1)
    {
        draw(&player);
        player_handle_input();
    }

    return 0;
}