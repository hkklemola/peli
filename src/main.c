#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>   // _getch()

#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/player.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/spawn.h"
#include "d:/projekti/peli/include/draw.h"
#include "d:/projekti/peli/include/movement.h"
#include "d:/projekti/peli/include/log.h"


int main()
{
    // Seed RNG
    srand(time(NULL));

    // Initialize systems
    atlas_init();
    bestiary_init();
    log_init();

    // Create player
    player_create(&player, "Hero");

    if(!player_place_random(&player))
    {
        printf("Failed to place player!\n");
        return 1;
    }

    // Spawn some monsters
    for(int i = 0; i < 5; i++)
        spawn_monster(-1, -1, &goblin_template);

    // =====================
    // Main game loop
    // =====================
    while(1)
    {
        // Draw everything
        draw_world(&player);

        // Handle input
        char c = _getch();

        switch(c)
        {
            case 'w': player_move(&player, 0, -1); break;
            case 's': player_move(&player, 0, 1); break;
            case 'a': player_move(&player, -1, 0); break;
            case 'd': player_move(&player, 1, 0); break;

            case 'q':
                printf("Goodbye!\n");
                return 0;
        }
    }

    return 0;
}