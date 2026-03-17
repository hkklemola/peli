#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/player.h"
#include <string.h>

Player player;

void character_create(const char* name, int x, int y)
{
    memset(&player, 0, sizeof(Player));

    // Identity
    strncpy(player.name, name, 31);
    player.name[31] = '\0';

    // Position + visuals
    player.entity.x = x;
    player.entity.y = y;
    player.entity.symbol = '@';
    player.entity.blocks = 1;

    // Stats
    player.actor.max_hp = 20;
    player.actor.hp = 20;
    player.actor.attack = 5;
    player.actor.defense = 2;

    // Progression
    player.level = 1;
    player.xp = 0;
    player.gold = 0;
}

int character_x() { return player.entity.x; }
int character_y() { return player.entity.y; }