#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/player.h"
#include "d:/projekti/peli/include/atlas.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/tile.h"
#include "d:/projekti/peli/include/tileset.h"
#include "d:/projekti/peli/include/map.h"
#include "d:/projekti/peli/include/movement.h"
#include "d:/projekti/peli/include/collision.h"

#include <stdio.h>
#include <string.h>
#include <conio.h> // for _getch()
#include <stdlib.h> // optional, for exit()

// Initialize player stats and name
void player_create(Player* p, const char* name)
{
    strcpy(p->character.name, name);

    // Base stats
    p->character.actor.hp = 20;
    p->character.actor.max_hp = 20;
    p->character.actor.attack = 5;
    p->character.actor.defense = 2;

    // Map symbol and blocking
    p->character.actor.entity.symbol = '@';
    p->character.actor.entity.blocks = 1;

    // Default position
    p->character.actor.entity.x = 0;
    p->character.actor.entity.y = 0;

    // Player-specific fields
    p->level = 1;
    p->experience = 0;
}

// Place the player at a specific x,y
void player_place(Player* p, int x, int y)
{
    p->character.actor.entity.x = x;
    p->character.actor.entity.y = y;
}

// Place the player on a random free tile
int player_place_random(Player* p)
{
    int attempts = 100;
    while(attempts--)
    {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;

        // Check if tile is free
        if(!is_blocked(x, y, 0) && !bestiary_creature_at(x, y))
        {
            player_place(p, x, y);
            return 1; // success
        }
    }

    log_add("Failed to place player on a free tile!");
    return 0; // failed
}

void player_handle_input()
{
    int nx = player.character.actor.entity.x;
    int ny = player.character.actor.entity.y;

    char c = _getch();
    switch(c)
    {
        case 'w': ny--; break;
        case 's': ny++; break;
        case 'a': nx--; break;
        case 'd': nx++; break;
        case 'q': exit(0); break;
    }

    if(is_blocked(nx, ny, 0))
        return;

    player.character.actor.entity.x = nx;
    player.character.actor.entity.y = ny;
}