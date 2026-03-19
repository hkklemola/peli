#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/player.h"

#include <string.h>

Player player;

void character_create(const char* name, int x, int y)
{
    memset(&player, 0, sizeof(Player));

    // Identity
    strncpy(player.character.name, name, 31);
    player.character.name[31] = '\0';

    // Position + visuals
    player.character.actor.entity.x = x;
    player.character.actor.entity.y = y;
    player.character.actor.entity.symbol = '@';
    player.character.actor.entity.blocks = 1;

    // Stats
    player.character.actor.max_hp = 20;
    player.character.actor.hp = 20;
    player.character.actor.attack = 5;
    player.character.actor.defense = 2;

    // Progression
    player.level = 1;
    player.experience = 0;
    player.gold = 0;
}

int character_x() { return player.character.actor.entity.x; }
int character_y() { return player.character.actor.entity.y; }