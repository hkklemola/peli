#include "crafting_compendium.h"

#include <stdio.h>
#include <string.h>

CraftingCompendiumEntry crafting_compendium[CRAFTING_COMPENDIUM_MAX_ENTRIES];
int crafting_compendium_count = 0;

static int crafting_compendium_find_index(const char* recipe_id)
{
    if(!recipe_id || recipe_id[0] == '\0')
        return -1;

    for(int i = 0; i < crafting_compendium_count; i++)
    {
        if(crafting_compendium[i].active && strcmp(crafting_compendium[i].recipe_id, recipe_id) == 0)
            return i;
    }

    return -1;
}

void crafting_compendium_init(void)
{
    memset(crafting_compendium, 0, sizeof(crafting_compendium));
    crafting_compendium_count = 0;
}

int crafting_compendium_register_recipe(const char* recipe_id,
                                        const char* station,
                                        NonWeaponSkillType skill,
                                        int difficulty)
{
    int index;

    if(!recipe_id || recipe_id[0] == '\0')
        return -1;

    index = crafting_compendium_find_index(recipe_id);
    if(index >= 0)
    {
        if(station && station[0] != '\0' && crafting_compendium[index].station[0] == '\0')
            snprintf(crafting_compendium[index].station, sizeof(crafting_compendium[index].station), "%s", station);
        if(skill >= 0)
            crafting_compendium[index].skill = skill;
        if(difficulty > 0)
            crafting_compendium[index].difficulty = difficulty;
        return index;
    }

    if(crafting_compendium_count >= CRAFTING_COMPENDIUM_MAX_ENTRIES)
        return -1;

    index = crafting_compendium_count++;
    crafting_compendium[index].active = 1;
    snprintf(crafting_compendium[index].recipe_id, sizeof(crafting_compendium[index].recipe_id), "%s", recipe_id);
    if(station && station[0] != '\0')
        snprintf(crafting_compendium[index].station, sizeof(crafting_compendium[index].station), "%s", station);
    crafting_compendium[index].skill = skill;
    crafting_compendium[index].difficulty = difficulty;
    crafting_compendium[index].tier = CRAFTING_DISCOVERY_UNKNOWN;
    return index;
}

const CraftingCompendiumEntry* crafting_compendium_entry(const char* recipe_id)
{
    int index = crafting_compendium_find_index(recipe_id);
    if(index < 0)
        return NULL;
    return &crafting_compendium[index];
}

CraftingDiscoveryTier crafting_compendium_tier(const char* recipe_id)
{
    const CraftingCompendiumEntry* entry = crafting_compendium_entry(recipe_id);
    return entry ? entry->tier : CRAFTING_DISCOVERY_UNKNOWN;
}

int crafting_compendium_upgrade_tier(const char* recipe_id, CraftingDiscoveryTier tier)
{
    int index = crafting_compendium_register_recipe(recipe_id, "", NON_WEAPON_SKILL_COUNT, 0);

    if(index < 0)
        return 0;

    if(tier > crafting_compendium[index].tier)
        crafting_compendium[index].tier = tier;

    return 1;
}

int crafting_compendium_add_hint(const char* recipe_id, const char* hint)
{
    int index;

    if(!hint || hint[0] == '\0')
        return 0;

    index = crafting_compendium_register_recipe(recipe_id, "", NON_WEAPON_SKILL_COUNT, 0);
    if(index < 0)
        return 0;

    for(int i = 0; i < crafting_compendium[index].hint_count; i++)
    {
        if(strcmp(crafting_compendium[index].hints[i], hint) == 0)
            return 1;
    }

    if(crafting_compendium[index].hint_count >= CRAFTING_COMPENDIUM_HINT_MAX)
        return 0;

    snprintf(crafting_compendium[index].hints[crafting_compendium[index].hint_count],
             CRAFTING_COMPENDIUM_HINT_LENGTH,
             "%s",
             hint);
    crafting_compendium[index].hint_count++;
    return 1;
}

int crafting_compendium_mark_attempt(const char* recipe_id, int success)
{
    int index = crafting_compendium_register_recipe(recipe_id, "", NON_WEAPON_SKILL_COUNT, 0);

    if(index < 0)
        return 0;

    crafting_compendium[index].attempts++;
    if(crafting_compendium[index].tier < CRAFTING_DISCOVERY_ATTEMPTED)
        crafting_compendium[index].tier = CRAFTING_DISCOVERY_ATTEMPTED;

    if(success)
        crafting_compendium[index].successes++;

    if(crafting_compendium[index].successes >= 5)
        crafting_compendium[index].tier = CRAFTING_DISCOVERY_MASTERED;

    return 1;
}

int crafting_compendium_mastery_success_bonus(const char* recipe_id)
{
    const CraftingCompendiumEntry* entry = crafting_compendium_entry(recipe_id);

    if(!entry)
        return 0;
    if(entry->tier < CRAFTING_DISCOVERY_MASTERED)
        return 0;

    return 5;
}
