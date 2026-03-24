#include "entity.h"
#include "actor.h"
#include "character.h"
#include "player.h"
#include "map.h"

/*
 * Purpose:
 *   Provides compatibility wrappers around global player creation and position.
 *
 * Functions:
 *   - character_create: initializes global player and sets starting coordinates.
 *   - character_x / character_y: exposes current global player position.
 */

Player player;

// Create the global player and assign an initial position.
void character_create(const char* name, int x, int y)
{
    player_create(&player, name);
    player.character.actor.entity.x = x;
    player.character.actor.entity.y = y;
    player.character.actor.entity.z = AREA_GROUND_Z;
}

// Return global player x-position.
int character_x() { return player.character.actor.entity.x; }

// Return global player y-position.
int character_y() { return player.character.actor.entity.y; }

// Return global player z-position.
int character_z() { return player.character.actor.entity.z; }

