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
#define MAX_CREATURE_LOOT_ENTRIES 8
#define MAX_CREATURE_FOOD_PREFERENCES 8

typedef struct Item Item;

typedef enum CreatureFoodReaction {
    CREATURE_FOOD_REACTION_NONE = 0,
    CREATURE_FOOD_REACTION_LIKED,
    CREATURE_FOOD_REACTION_TOLERATED,
    CREATURE_FOOD_REACTION_HATED,
} CreatureFoodReaction;

typedef struct CreatureFoodPreference {
    char token[32];
} CreatureFoodPreference;

typedef enum CreatureMoveState {
    CREATURE_STATE_WANDER = 0,
    CREATURE_STATE_REST,
    CREATURE_STATE_FLEE,
} CreatureMoveState;

// Disposition score bands.  Thresholds: hostile<=-50, wary -49..-1, neutral 0..29,
//                                        friendly 30..69, bonded>=70.
typedef enum CreatureDispositionBand {
    DISP_BAND_HOSTILE = 0,
    DISP_BAND_WARY,
    DISP_BAND_NEUTRAL,
    DISP_BAND_FRIENDLY,
    DISP_BAND_BONDED,
} CreatureDispositionBand;

// Taming progression stages (only advances for tamable creatures).
typedef enum CreatureTamingStage {
    TAMING_WILD = 0,
    TAMING_WARY,
    TAMING_FAMILIAR,
    TAMING_TAME,
    TAMING_BONDED,
} CreatureTamingStage;

// How quickly a species turns hostile under provocation.
typedef enum CreatureAggressionProfile {
    AGGRESSION_AGGRESSIVE = 0,  // Hostile templates; instant flip on any strike.
    AGGRESSION_DEFENSIVE,       // Neutral species; standard provocation threshold.
    AGGRESSION_SKITTISH,        // Prey animals; flee-first, but still turn hostile.
    AGGRESSION_DOCILE,          // Domesticated; needs repeated strikes to turn hostile.
} CreatureAggressionProfile;

typedef struct CreatureLootEntry {
    char item_name[32];
    int chance_percent;
    int min_quantity;
    int max_quantity;
} CreatureLootEntry;

// Template for creature stats
typedef struct CreatureTemplate {
    const char* name;
    char symbol;
    int color;
    int is_hostile;                          // Legacy baseline: seeds initial disposition
    int hide_below;
    int tamable;                             // 1 if this species can be tamed
    int base_disposition;                    // Starting score (-100..100)
    CreatureAggressionProfile aggression_profile;
    Actor actor;
    CreatureLootEntry skinning_loot_entries[MAX_CREATURE_LOOT_ENTRIES];
    int skinning_loot_count;
    CreatureLootEntry butchering_loot_entries[MAX_CREATURE_LOOT_ENTRIES];
    int butchering_loot_count;
    CreatureFoodPreference liked_foods[MAX_CREATURE_FOOD_PREFERENCES];
    int liked_food_count;
    CreatureFoodPreference tolerated_foods[MAX_CREATURE_FOOD_PREFERENCES];
    int tolerated_food_count;
    CreatureFoodPreference hated_foods[MAX_CREATURE_FOOD_PREFERENCES];
    int hated_food_count;
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
    int disposition;                // Runtime disposition score (-100..100)
    CreatureTamingStage taming_stage;
} Creature;

// Storage for all creatures
extern Creature creatures[MAX_CREATURES];

// Return an available creature slot, or NULL if all slots are occupied.
Creature* get_free_creature_slot(void);

// Reset all creature slots to an unused state.
void bestiary_init();

// Return the alive creature at (x, y), or NULL.
Creature* bestiary_creature_at(int x, int y);

// Return the alive creature at (x, y, z), or NULL.
Creature* bestiary_creature_at_3d(int x, int y, int z);

// Return slot index for the creature pointer, or -1 when invalid.
int bestiary_index_of(const Creature* creature);

// Return a template by display name, or NULL when not found.
CreatureTemplate* bestiary_template_by_name(const char* name);

// Load creature templates from an external text file.
int bestiary_templates_load(const char* path);

// Mark a creature as dead and roll any configured loot drops.
void creature_handle_death(Creature* creature);

// --- Disposition helpers ---

// Return the current disposition band for this creature.
CreatureDispositionBand creature_disposition_band(const Creature* creature);

// Return 1 if the creature is currently hostile (will attack the player).
int creature_is_hostile(const Creature* creature);

// Return 1 if the creature is currently friendly or bonded.
int creature_is_friendly(const Creature* creature);

// Apply a signed disposition delta, clamping the score to [-100, 100].
void creature_apply_disposition_delta(Creature* creature, int delta);

// Apply a provocation event from a player attack.  Logs the reaction.
// Called before striking a non-hostile creature; may flip it to hostile.
void creature_provoke_by_attack(Creature* creature);

// Apply a petting interaction.  husbandry_skill scales the friendliness gain.
void creature_apply_pet_event(Creature* creature, int husbandry_skill);

// Classify how a creature species reacts to the offered item.
CreatureFoodReaction creature_template_food_reaction(const CreatureTemplate* tmpl, const struct Item* item);

// Apply a feeding interaction and disposition change for the offered item.
CreatureFoodReaction creature_apply_feed_event(Creature* creature, const struct Item* item, int husbandry_skill);

// Advance taming stage if disposition and husbandry thresholds are satisfied.
void creature_update_taming_stage(Creature* creature, int husbandry_skill);

// Return 1 when the item is edible and explicitly liked/tolerated/hated by the creature.
int creature_can_eat_item(const Creature* creature, const struct Item* item);

// Return a display label for a taming stage.
const char* taming_stage_name(CreatureTamingStage stage);

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

