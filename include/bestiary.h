#ifndef BESTIARY_H
#define BESTIARY_H

#include "entity.h" 
#include "actor.h"   // Actor struct for stats
 // Entity struct for position & symbol

/*
 * Purpose:
 *   Declares creature templates, creature storage, and creature lookup helpers.
 *
 * Functions:
 *   - get_free_creature_slot: returns an unused creature slot.
 *   - bestiary_init: resets creature storage for a new game.
 *   - bestiary_creature_at: finds an alive creature at map coordinates.
 */

#define MAX_CREATURES 128

typedef enum CreatureMoveState {
    CREATURE_STATE_WANDER = 0,
    CREATURE_STATE_REST,
    CREATURE_STATE_FLEE,
} CreatureMoveState;

// Template for creature stats
typedef struct CreatureTemplate {
    const char* name;
    char symbol;
    int color;
    int is_hostile;
    int hide_below;
    Actor actor;
} CreatureTemplate;

// Actual creature instance
typedef struct Creature {
    Actor actor;            // inherit stats
    struct CreatureTemplate* template;  // pointer to base stats template
    int alive;
    CreatureMoveState move_state;
    int state_turns;
    int move_dx;
    int move_dy;
} Creature;

// Storage for all creatures
extern Creature creatures[MAX_CREATURES];

// Return an available creature slot, or NULL if all slots are occupied.
Creature* get_free_creature_slot(void);

// Reset all creature slots to an unused state.
void bestiary_init();

// Return the alive creature at (x, y), or NULL.
Creature* bestiary_creature_at(int x, int y);

// Return slot index for the creature pointer, or -1 when invalid.
int bestiary_index_of(const Creature* creature);

// Return a template by display name, or NULL when not found.
CreatureTemplate* bestiary_template_by_name(const char* name);

// Load creature templates from an external text file.
int bestiary_templates_load(const char* path);

// Predefined creature templates
extern CreatureTemplate goblin_template;
extern CreatureTemplate skeleton_template;
extern CreatureTemplate dog_template;
extern CreatureTemplate cat_template;
extern CreatureTemplate bat_template;
extern CreatureTemplate rat_template;
extern CreatureTemplate snake_template;
extern CreatureTemplate wolf_template;
extern CreatureTemplate horse_template;
extern CreatureTemplate mouse_template;
extern CreatureTemplate bird_template;
extern CreatureTemplate rabbit_template;
extern CreatureTemplate sheep_template;
extern CreatureTemplate goat_template;

#endif

