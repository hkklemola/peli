#include "entity.h"
#include "actor.h"
#include "character.h"
#include "player.h"
#include "bestiary.h"
#include "log.h"
#include "tile.h"
#include "tileset.h"
#include "map.h"
#include "movement.h"
#include "collision.h" 


#include <string.h>
#include <stdio.h>

/*
 * Purpose:
 *   Owns runtime creature storage and default creature templates.
 *
 * Functions:
 *   - get_free_creature_slot: returns the next available creature slot.
 *   - bestiary_init: clears alive flags for all creature slots.
 *   - bestiary_creature_at: returns alive creature at map coordinates.
 */

// Storage
Creature creatures[MAX_CREATURES];

// Return the first non-alive creature slot for spawning.
Creature* get_free_creature_slot(void)
{
    for(int i = 0; i < MAX_CREATURES; i++)
    {
        if(!creatures[i].alive)
            return &creatures[i];
    }
    return NULL;
}

// Make templates global
CreatureTemplate goblin_template = {
    .name = "Goblin",
    .symbol = 'g',
    .color = RENDER_COLOR_LIGHT_GREEN,
    .is_hostile = 1,
    .actor = {
        .health = 8,
        .max_health = 8,
        .stamina = 6,
        .max_stamina = 6,
        .willpower = 4,
        .max_willpower = 4,
        .mana = 0,
        .max_mana = 0,
        .weapon_skill = {
            [WEAPON_SKILL_UNARMED] = 3,
            [WEAPON_SKILL_DAGGER] = 2,
            [WEAPON_SKILL_SWORD] = 2,
            [WEAPON_SKILL_AXE] = 1,
            [WEAPON_SKILL_MACE] = 1,
            [WEAPON_SKILL_SPEAR] = 2,
            [WEAPON_SKILL_STAFF] = 1,
            [WEAPON_SKILL_POLEARM] = 1,
        },
        .armor_rating = 1,
        .dodge = 8,
        .block = 5,
        .parry = 2
    }
};

CreatureTemplate skeleton_template = {
    .name = "Skeleton",
    .symbol = 's',
    .color = RENDER_COLOR_LIGHT_GRAY,
    .is_hostile = 1,
    .actor = {
        .health = 10,
        .max_health = 10,
        .stamina = 8,
        .max_stamina = 8,
        .willpower = 6,
        .max_willpower = 6,
        .mana = 0,
        .max_mana = 0,
        .weapon_skill = {
            [WEAPON_SKILL_UNARMED] = 4,
            [WEAPON_SKILL_DAGGER] = 2,
            [WEAPON_SKILL_SWORD] = 3,
            [WEAPON_SKILL_AXE] = 2,
            [WEAPON_SKILL_MACE] = 3,
            [WEAPON_SKILL_SPEAR] = 2,
            [WEAPON_SKILL_STAFF] = 1,
            [WEAPON_SKILL_POLEARM] = 2,
        },
        .armor_rating = 2,
        .dodge = 5,
        .block = 8,
        .parry = 6
    }
};

CreatureTemplate dog_template = {
    .name = "Dog",
    .symbol = 'd',
    .color = RENDER_COLOR_BROWN,
    .is_hostile = 0,
    .actor = {
        .health = 10, .max_health = 10,
        .stamina = 10, .max_stamina = 10,
        .willpower = 4, .max_willpower = 4,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 4 },
        .armor_rating = 1, .dodge = 10, .block = 3, .parry = 2
    }
};

CreatureTemplate cat_template = {
    .name = "Cat",
    .symbol = 'c',
    .color = RENDER_COLOR_LIGHT_GRAY,
    .is_hostile = 0,
    .actor = {
        .health = 6, .max_health = 6,
        .stamina = 12, .max_stamina = 12,
        .willpower = 3, .max_willpower = 3,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 3 },
        .armor_rating = 0, .dodge = 14, .block = 0, .parry = 1
    }
};

CreatureTemplate bat_template = {
    .name = "Bat",
    .symbol = 'b',
    .color = RENDER_COLOR_DARK_GRAY,
    .is_hostile = 1,
    .actor = {
        .health = 5, .max_health = 5,
        .stamina = 11, .max_stamina = 11,
        .willpower = 2, .max_willpower = 2,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 4 },
        .armor_rating = 0, .dodge = 15, .block = 0, .parry = 1
    }
};

CreatureTemplate rat_template = {
    .name = "Rat",
    .symbol = 'r',
    .color = RENDER_COLOR_BROWN,
    .is_hostile = 1,
    .actor = {
        .health = 6, .max_health = 6,
        .stamina = 10, .max_stamina = 10,
        .willpower = 2, .max_willpower = 2,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 4 },
        .armor_rating = 0, .dodge = 12, .block = 0, .parry = 1
    }
};

CreatureTemplate snake_template = {
    .name = "Snake",
    .symbol = 'n',
    .color = RENDER_COLOR_LIGHT_GREEN,
    .is_hostile = 1,
    .actor = {
        .health = 7, .max_health = 7,
        .stamina = 10, .max_stamina = 10,
        .willpower = 3, .max_willpower = 3,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 5 },
        .armor_rating = 0, .dodge = 11, .block = 0, .parry = 0
    }
};

CreatureTemplate wolf_template = {
    .name = "Wolf",
    .symbol = 'w',
    .color = RENDER_COLOR_LIGHT_GRAY,
    .is_hostile = 1,
    .actor = {
        .health = 14, .max_health = 14,
        .stamina = 12, .max_stamina = 12,
        .willpower = 5, .max_willpower = 5,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 6 },
        .armor_rating = 1, .dodge = 12, .block = 2, .parry = 1
    }
};

CreatureTemplate horse_template = {
    .name = "Horse",
    .symbol = 'h',
    .color = RENDER_COLOR_BROWN,
    .is_hostile = 0,
    .actor = {
        .health = 18, .max_health = 18,
        .stamina = 14, .max_stamina = 14,
        .willpower = 6, .max_willpower = 6,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 3 },
        .armor_rating = 2, .dodge = 8, .block = 2, .parry = 1
    }
};

CreatureTemplate mouse_template = {
    .name = "Mouse",
    .symbol = 'm',
    .color = RENDER_COLOR_LIGHT_GRAY,
    .is_hostile = 0,
    .actor = {
        .health = 3, .max_health = 3,
        .stamina = 8, .max_stamina = 8,
        .willpower = 1, .max_willpower = 1,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 1 },
        .armor_rating = 0, .dodge = 13, .block = 0, .parry = 0
    }
};

CreatureTemplate bird_template = {
    .name = "Bird",
    .symbol = 'B',
    .color = RENDER_COLOR_LIGHT_CYAN,
    .is_hostile = 0,
    .actor = {
        .health = 4, .max_health = 4,
        .stamina = 9, .max_stamina = 9,
        .willpower = 2, .max_willpower = 2,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 2 },
        .armor_rating = 0, .dodge = 14, .block = 0, .parry = 0
    }
};

CreatureTemplate rabbit_template = {
    .name = "Rabbit",
    .symbol = 'R',
    .color = RENDER_COLOR_WHITE,
    .is_hostile = 0,
    .actor = {
        .health = 5, .max_health = 5,
        .stamina = 10, .max_stamina = 10,
        .willpower = 2, .max_willpower = 2,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 2 },
        .armor_rating = 0, .dodge = 13, .block = 0, .parry = 0
    }
};

CreatureTemplate sheep_template = {
    .name = "Sheep",
    .symbol = 'S',
    .color = RENDER_COLOR_WHITE,
    .is_hostile = 0,
    .actor = {
        .health = 11, .max_health = 11,
        .stamina = 8, .max_stamina = 8,
        .willpower = 4, .max_willpower = 4,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 2 },
        .armor_rating = 1, .dodge = 7, .block = 1, .parry = 0
    }
};

CreatureTemplate goat_template = {
    .name = "Goat",
    .symbol = 'G',
    .color = RENDER_COLOR_LIGHT_YELLOW,
    .is_hostile = 0,
    .actor = {
        .health = 12, .max_health = 12,
        .stamina = 9, .max_stamina = 9,
        .willpower = 4, .max_willpower = 4,
        .mana = 0, .max_mana = 0,
        .weapon_skill = { [WEAPON_SKILL_UNARMED] = 3 },
        .armor_rating = 1, .dodge = 8, .block = 1, .parry = 0
    }
};

// Reset all creature slots to an unused state.
void bestiary_init()
{
    for(int i=0; i<MAX_CREATURES; i++)
    {
        creatures[i].alive = 0;
        creatures[i].template = NULL;
        creatures[i].move_state = CREATURE_STATE_WANDER;
        creatures[i].state_turns = 0;
        creatures[i].move_dx = 0;
        creatures[i].move_dy = 0;
    }
}

// Look up an alive creature at the requested map coordinate.
Creature* bestiary_creature_at(int x, int y)
{
    for(int i=0; i<MAX_CREATURES; i++)
        if(creatures[i].alive &&
           creatures[i].actor.entity.x == x &&
           creatures[i].actor.entity.y == y)
            return &creatures[i];
    return NULL;
}

int bestiary_index_of(const Creature* creature)
{
    if(!creature)
        return -1;

    for(int i = 0; i < MAX_CREATURES; i++)
    {
        if(&creatures[i] == creature)
            return i;
    }

    return -1;
}

CreatureTemplate* bestiary_template_by_name(const char* name)
{
    static CreatureTemplate* templates[] = {
        &goblin_template,
        &skeleton_template,
        &dog_template,
        &cat_template,
        &bat_template,
        &rat_template,
        &snake_template,
        &wolf_template,
        &horse_template,
        &mouse_template,
        &bird_template,
        &rabbit_template,
        &sheep_template,
        &goat_template,
    };

    if(!name)
        return NULL;

    for(int i = 0; i < (int)(sizeof(templates) / sizeof(templates[0])); i++)
    {
        if(strcmp(templates[i]->name, name) == 0)
            return templates[i];
    }

    return NULL;
}

