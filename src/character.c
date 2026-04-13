#include "entity.h"
#include "actor.h"
#include "character.h"
#include "player.h"
#include "map.h"

#include <string.h>

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

void character_clear_recipe_unlocks(Character* c)
{
    if(!c)
        return;

    c->unlocked_recipe_count = 0;
    memset(c->unlocked_recipe_ids, 0, sizeof(c->unlocked_recipe_ids));
}

int character_has_recipe_unlock(const Character* c, const char* recipe_id)
{
    if(!c || !recipe_id || recipe_id[0] == '\0')
        return 0;

    for(int i = 0; i < c->unlocked_recipe_count; i++)
    {
        if(strcmp(c->unlocked_recipe_ids[i], recipe_id) == 0)
            return 1;
    }

    return 0;
}

int character_add_recipe_unlock(Character* c, const char* recipe_id)
{
    if(!c || !recipe_id || recipe_id[0] == '\0')
        return 0;

    if(character_has_recipe_unlock(c, recipe_id))
        return 1;

    if(c->unlocked_recipe_count >= CHARACTER_MAX_UNLOCKED_RECIPES)
        return 0;

    snprintf(c->unlocked_recipe_ids[c->unlocked_recipe_count],
             CHARACTER_RECIPE_ID_LENGTH,
             "%s",
             recipe_id);
    c->unlocked_recipe_count++;
    return 1;
}

