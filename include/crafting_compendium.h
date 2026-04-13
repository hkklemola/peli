#ifndef CRAFTING_COMPENDIUM_H
#define CRAFTING_COMPENDIUM_H

#include "actor.h"

#define CRAFTING_COMPENDIUM_MAX_ENTRIES 256
#define CRAFTING_COMPENDIUM_ID_LENGTH 48
#define CRAFTING_COMPENDIUM_STATION_LENGTH 24
#define CRAFTING_COMPENDIUM_HINT_MAX 16
#define CRAFTING_COMPENDIUM_HINT_LENGTH 128

typedef enum CraftingDiscoveryTier {
    CRAFTING_DISCOVERY_UNKNOWN = 0,
    CRAFTING_DISCOVERY_RUMORED,
    CRAFTING_DISCOVERY_RECORDED,
    CRAFTING_DISCOVERY_ATTEMPTED,
    CRAFTING_DISCOVERY_MASTERED,
} CraftingDiscoveryTier;

typedef struct CraftingCompendiumEntry {
    int active;
    char recipe_id[CRAFTING_COMPENDIUM_ID_LENGTH];
    char station[CRAFTING_COMPENDIUM_STATION_LENGTH];
    NonWeaponSkillType skill;
    int difficulty;
    CraftingDiscoveryTier tier;
    int attempts;
    int successes;
    int hint_count;
    char hints[CRAFTING_COMPENDIUM_HINT_MAX][CRAFTING_COMPENDIUM_HINT_LENGTH];
} CraftingCompendiumEntry;

extern CraftingCompendiumEntry crafting_compendium[CRAFTING_COMPENDIUM_MAX_ENTRIES];
extern int crafting_compendium_count;

void crafting_compendium_init(void);
int crafting_compendium_register_recipe(const char* recipe_id,
                                        const char* station,
                                        NonWeaponSkillType skill,
                                        int difficulty);
const CraftingCompendiumEntry* crafting_compendium_entry(const char* recipe_id);
CraftingDiscoveryTier crafting_compendium_tier(const char* recipe_id);
int crafting_compendium_upgrade_tier(const char* recipe_id, CraftingDiscoveryTier tier);
int crafting_compendium_add_hint(const char* recipe_id, const char* hint);
int crafting_compendium_mark_attempt(const char* recipe_id, int success);
int crafting_compendium_mastery_success_bonus(const char* recipe_id);

#endif
