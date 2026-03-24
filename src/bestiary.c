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


#include <ctype.h>
#include <stdlib.h>
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

static CreatureTemplate* creature_templates = NULL;
static int creature_template_count = 0;
static int creature_template_capacity = 0;

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
CreatureTemplate goblin_template = {0};
CreatureTemplate skeleton_template = {0};
CreatureTemplate dog_template = {0};
CreatureTemplate cat_template = {0};
CreatureTemplate bat_template = {0};
CreatureTemplate rat_template = {0};
CreatureTemplate snake_template = {0};
CreatureTemplate wolf_template = {0};
CreatureTemplate horse_template = {0};
CreatureTemplate mouse_template = {0};
CreatureTemplate bird_template = {0};
CreatureTemplate rabbit_template = {0};
CreatureTemplate sheep_template = {0};
CreatureTemplate goat_template = {0};

static char* creature_strdup(const char* text)
{
    size_t length;
    char* copy;

    if(!text)
        return NULL;

    length = strlen(text);
    copy = (char*)malloc(length + 1);
    if(!copy)
        return NULL;

    memcpy(copy, text, length + 1);
    return copy;
}

static void trim_in_place(char* text)
{
    char* start;
    char* end;

    if(!text)
        return;

    start = text;
    while(*start && isspace((unsigned char)*start))
        start++;

    if(start != text)
        memmove(text, start, strlen(start) + 1);

    end = text + strlen(text);
    while(end > text && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
}

static int equals_ignore_case(const char* left, const char* right)
{
    if(!left || !right)
        return 0;

    while(*left && *right)
    {
        if(tolower((unsigned char)*left) != tolower((unsigned char)*right))
            return 0;
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static int starts_with_ignore_case(const char* text, const char* prefix)
{
    if(!text || !prefix)
        return 0;

    while(*prefix)
    {
        if(*text == '\0')
            return 0;

        if(tolower((unsigned char)*text) != tolower((unsigned char)*prefix))
            return 0;

        text++;
        prefix++;
    }

    return 1;
}

static int parse_color_index(const char* value, int* out)
{
    char* end = NULL;
    long parsed;

    if(!value || !out)
        return 0;

    while(*value && isspace((unsigned char)*value))
        value++;

    if(starts_with_ignore_case(value, "idx:"))
    {
        value += 4;
    }
    else if(starts_with_ignore_case(value, "256:"))
    {
        value += 4;
    }

    parsed = strtol(value, &end, 10);
    if(end == value)
        return 0;

    while(end && *end && isspace((unsigned char)*end))
        end++;

    if(end && *end != '\0')
        return 0;

    if(parsed < 0 || parsed > 255)
        return 0;

    *out = (int)parsed;
    return 1;
}

static int parse_render_color(const char* value, int* out)
{
    static const struct {
        const char* name;
        int color;
    } mappings[] = {
        { "BLACK", RENDER_COLOR_BLACK },
        { "RED", RENDER_COLOR_RED },
        { "GREEN", RENDER_COLOR_GREEN },
        { "BROWN", RENDER_COLOR_BROWN },
        { "BLUE", RENDER_COLOR_BLUE },
        { "MAGENTA", RENDER_COLOR_MAGENTA },
        { "CYAN", RENDER_COLOR_CYAN },
        { "LIGHT_GRAY", RENDER_COLOR_LIGHT_GRAY },
        { "DEFAULT", RENDER_COLOR_DEFAULT },
        { "DARK_GRAY", RENDER_COLOR_DARK_GRAY },
        { "LIGHT_RED", RENDER_COLOR_LIGHT_RED },
        { "LIGHT_GREEN", RENDER_COLOR_LIGHT_GREEN },
        { "LIGHT_YELLOW", RENDER_COLOR_LIGHT_YELLOW },
        { "LIGHT_BLUE", RENDER_COLOR_LIGHT_BLUE },
        { "LIGHT_MAGENTA", RENDER_COLOR_LIGHT_MAGENTA },
        { "LIGHT_CYAN", RENDER_COLOR_LIGHT_CYAN },
        { "WHITE", RENDER_COLOR_WHITE },
    };

    if(parse_color_index(value, out))
        return 1;

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(value, mappings[i].name))
        {
            *out = mappings[i].color;
            return 1;
        }
    }

    return 0;
}

static int parse_skill_key(const char* key, WeaponSkillType* out)
{
    static const struct {
        const char* key;
        WeaponSkillType skill;
    } mappings[] = {
        { "skill_unarmed", WEAPON_SKILL_UNARMED },
        { "skill_dagger", WEAPON_SKILL_DAGGER },
        { "skill_sword", WEAPON_SKILL_SWORD },
        { "skill_axe", WEAPON_SKILL_AXE },
        { "skill_mace", WEAPON_SKILL_MACE },
        { "skill_spear", WEAPON_SKILL_SPEAR },
        { "skill_staff", WEAPON_SKILL_STAFF },
        { "skill_polearm", WEAPON_SKILL_POLEARM },
    };

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(key, mappings[i].key))
        {
            *out = mappings[i].skill;
            return 1;
        }
    }

    return 0;
}

static void free_creature_template(CreatureTemplate* tmpl)
{
    if(!tmpl)
        return;

    free((void*)tmpl->name);
    tmpl->name = NULL;
}

static void clear_creature_templates(void)
{
    for(int i = 0; i < creature_template_count; i++)
        free_creature_template(&creature_templates[i]);

    free(creature_templates);
    creature_templates = NULL;
    creature_template_count = 0;
    creature_template_capacity = 0;
}

static int append_creature_template(const CreatureTemplate* source)
{
    CreatureTemplate* resized;

    if(creature_template_count >= creature_template_capacity)
    {
        int new_capacity = creature_template_capacity > 0 ? creature_template_capacity * 2 : 16;
        resized = (CreatureTemplate*)realloc(creature_templates, (size_t)new_capacity * sizeof(CreatureTemplate));
        if(!resized)
            return 0;

        creature_templates = resized;
        creature_template_capacity = new_capacity;
    }

    creature_templates[creature_template_count++] = *source;
    return 1;
}

static int bind_required_template(CreatureTemplate* out, const char* name)
{
    CreatureTemplate* tmpl = bestiary_template_by_name(name);
    if(!tmpl)
        return 0;

    *out = *tmpl;
    return 1;
}

static int bind_required_templates(void)
{
    return bind_required_template(&goblin_template, "Goblin") &&
           bind_required_template(&skeleton_template, "Skeleton") &&
           bind_required_template(&dog_template, "Dog") &&
           bind_required_template(&cat_template, "Cat") &&
           bind_required_template(&bat_template, "Bat") &&
           bind_required_template(&rat_template, "Rat") &&
           bind_required_template(&snake_template, "Snake") &&
           bind_required_template(&wolf_template, "Wolf") &&
           bind_required_template(&horse_template, "Horse") &&
           bind_required_template(&mouse_template, "Mouse") &&
           bind_required_template(&bird_template, "Bird") &&
           bind_required_template(&rabbit_template, "Rabbit") &&
           bind_required_template(&sheep_template, "Sheep") &&
           bind_required_template(&goat_template, "Goat");
}

static int finalize_creature_template(CreatureTemplate* tmpl)
{
    if(!tmpl->name || tmpl->name[0] == '\0' || tmpl->symbol == '\0')
        return 0;

    if(bestiary_template_by_name(tmpl->name))
        return 0;

    /* Seed base_disposition from is_hostile when not explicitly set. */
    if(tmpl->base_disposition < -100)
        tmpl->base_disposition = tmpl->is_hostile ? -80 : 0;

    actor_ensure_base_attributes(&tmpl->actor);
    if(!append_creature_template(tmpl))
        return 0;

    tmpl->name = NULL;
    return 1;
}

int bestiary_templates_load(const char* path)
{
    FILE* file;
    char line[256];
    CreatureTemplate current;
    int have_current = 0;
    int loaded = 0;

    if(!path)
        return 0;

    file = fopen(path, "r");
    if(!file)
        return 0;

    clear_creature_templates();
    memset(&current, 0, sizeof(current));
    current.hide_below = 0;
    current.base_disposition = -999;  /* sentinel: will be derived from is_hostile in finalize */

    while(fgets(line, sizeof(line), file))
    {
        char* equals;
        WeaponSkillType skill_type;

        trim_in_place(line);
        if(line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        if(line[0] == '[')
        {
            if(have_current)
            {
                if(!finalize_creature_template(&current))
                    goto fail;
                loaded++;
                memset(&current, 0, sizeof(current));
                current.hide_below = 0;
            }

            have_current = equals_ignore_case(line, "[creature]");
            continue;
        }

        if(!have_current)
            continue;

        equals = strchr(line, '=');
        if(!equals)
            goto fail;

        *equals = '\0';
        trim_in_place(line);
        trim_in_place(equals + 1);

        if(equals_ignore_case(line, "name"))
        {
            free((void*)current.name);
            current.name = creature_strdup(equals + 1);
            if(!current.name)
                goto fail;
        }
        else if(equals_ignore_case(line, "symbol"))
            current.symbol = (equals[1] != '\0') ? equals[1] : '\0';
        else if(equals_ignore_case(line, "color"))
        {
            if(!parse_render_color(equals + 1, &current.color))
                goto fail;
        }
        else if(equals_ignore_case(line, "hostile"))
            current.is_hostile = atoi(equals + 1);
        else if(equals_ignore_case(line, "tamable"))
            current.tamable = atoi(equals + 1) ? 1 : 0;
        else if(equals_ignore_case(line, "base_disposition"))
        {
            int v = atoi(equals + 1);
            if(v < -100) v = -100;
            if(v >  100) v =  100;
            current.base_disposition = v;
        }
        else if(equals_ignore_case(line, "aggression_profile"))
        {
            if(equals_ignore_case(equals + 1, "aggressive"))   current.aggression_profile = AGGRESSION_AGGRESSIVE;
            else if(equals_ignore_case(equals + 1, "defensive")) current.aggression_profile = AGGRESSION_DEFENSIVE;
            else if(equals_ignore_case(equals + 1, "skittish"))  current.aggression_profile = AGGRESSION_SKITTISH;
            else if(equals_ignore_case(equals + 1, "docile"))    current.aggression_profile = AGGRESSION_DOCILE;
        }
        else if(equals_ignore_case(line, "hide_below"))
            current.hide_below = atoi(equals + 1) ? 1 : 0;
        else if(equals_ignore_case(line, "health"))
            current.actor.health = atoi(equals + 1);
        else if(equals_ignore_case(line, "max_health"))
            current.actor.max_health = atoi(equals + 1);
        else if(equals_ignore_case(line, "stamina"))
            current.actor.stamina = atoi(equals + 1);
        else if(equals_ignore_case(line, "max_stamina"))
            current.actor.max_stamina = atoi(equals + 1);
        else if(equals_ignore_case(line, "willpower"))
            current.actor.willpower = atoi(equals + 1);
        else if(equals_ignore_case(line, "max_willpower"))
            current.actor.max_willpower = atoi(equals + 1);
        else if(equals_ignore_case(line, "mana"))
            current.actor.mana = atoi(equals + 1);
        else if(equals_ignore_case(line, "max_mana"))
            current.actor.max_mana = atoi(equals + 1);
        else if(equals_ignore_case(line, "armor_rating"))
            current.actor.armor_rating = atoi(equals + 1);
        else if(equals_ignore_case(line, "dodge"))
            current.actor.dodge = atoi(equals + 1);
        else if(equals_ignore_case(line, "block"))
            current.actor.block = atoi(equals + 1);
        else if(equals_ignore_case(line, "parry"))
            current.actor.parry = atoi(equals + 1);
        else if(equals_ignore_case(line, "strength"))
            current.actor.strength = atoi(equals + 1);
        else if(equals_ignore_case(line, "constitution"))
            current.actor.constitution = atoi(equals + 1);
        else if(equals_ignore_case(line, "endurance"))
            current.actor.endurance = atoi(equals + 1);
        else if(equals_ignore_case(line, "agility"))
            current.actor.agility = atoi(equals + 1);
        else if(equals_ignore_case(line, "dexterity"))
            current.actor.dexterity = atoi(equals + 1);
        else if(equals_ignore_case(line, "speed"))
            current.actor.speed = atoi(equals + 1);
        else if(equals_ignore_case(line, "intellect"))
            current.actor.intellect = atoi(equals + 1);
        else if(equals_ignore_case(line, "wisdom"))
            current.actor.wisdom = atoi(equals + 1);
        else if(equals_ignore_case(line, "resolve"))
            current.actor.resolve = atoi(equals + 1);
        else if(equals_ignore_case(line, "composure"))
            current.actor.composure = atoi(equals + 1);
        else if(equals_ignore_case(line, "charisma"))
            current.actor.charisma = atoi(equals + 1);
        else if(equals_ignore_case(line, "beauty"))
            current.actor.beauty = atoi(equals + 1);
        else if(equals_ignore_case(line, "perception"))
            current.actor.perception = atoi(equals + 1);
        else if(equals_ignore_case(line, "wits"))
            current.actor.wits = atoi(equals + 1);
        else if(parse_skill_key(line, &skill_type))
            current.actor.weapon_skill[skill_type] = atoi(equals + 1);
        else
            goto fail;
    }

    if(have_current)
    {
        if(!finalize_creature_template(&current))
            goto fail;
        loaded++;
    }

    fclose(file);
    if(loaded == 0 || !bind_required_templates())
    {
        clear_creature_templates();
        return 0;
    }

    return 1;

fail:
    free_creature_template(&current);
    fclose(file);
    clear_creature_templates();
    return 0;
}

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
Creature* bestiary_creature_at_3d(int x, int y, int z)
{
    for(int i=0; i<MAX_CREATURES; i++)
        if(creatures[i].alive &&
           creatures[i].actor.entity.x == x &&
           creatures[i].actor.entity.y == y &&
           creatures[i].actor.entity.z == z)
            return &creatures[i];
    return NULL;
}

Creature* bestiary_creature_at(int x, int y)
{
    return bestiary_creature_at_3d(x, y, 0);
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
    if(!name)
        return NULL;

    for(int i = 0; i < creature_template_count; i++)
    {
        if(strcmp(creature_templates[i].name, name) == 0)
            return &creature_templates[i];
    }

    return NULL;
}

// ---- Disposition helpers ----

CreatureDispositionBand creature_disposition_band(const Creature* creature)
{
    int d;
    if(!creature) return DISP_BAND_HOSTILE;
    d = creature->disposition;
    if(d <= -50) return DISP_BAND_HOSTILE;
    if(d <    0) return DISP_BAND_WARY;
    if(d <   30) return DISP_BAND_NEUTRAL;
    if(d <   70) return DISP_BAND_FRIENDLY;
    return DISP_BAND_BONDED;
}

int creature_is_hostile(const Creature* creature)
{
    if(!creature || !creature->alive) return 0;
    return creature_disposition_band(creature) == DISP_BAND_HOSTILE;
}

int creature_is_friendly(const Creature* creature)
{
    CreatureDispositionBand band;
    if(!creature || !creature->alive) return 0;
    band = creature_disposition_band(creature);
    return band == DISP_BAND_FRIENDLY || band == DISP_BAND_BONDED;
}

void creature_apply_disposition_delta(Creature* creature, int delta)
{
    if(!creature) return;
    creature->disposition += delta;
    if(creature->disposition < -100) creature->disposition = -100;
    if(creature->disposition >  100) creature->disposition =  100;
}

void creature_provoke_by_attack(Creature* creature)
{
    int delta;
    if(!creature || !creature->template) return;
    if(creature_is_hostile(creature)) return;

    switch(creature->template->aggression_profile)
    {
        case AGGRESSION_DOCILE:   delta = -30; break;
        case AGGRESSION_SKITTISH: delta = -40; break;
        default:                  delta = -60; break;
    }

    creature_apply_disposition_delta(creature, delta);

    if(creature_is_hostile(creature))
        log_add("%s turns hostile!", creature->template->name);
    else if(creature->template->aggression_profile == AGGRESSION_SKITTISH)
        log_add("%s flees in panic!", creature->template->name);
    else
        log_add("%s snarls at you warily.", creature->template->name);
}

void creature_apply_pet_event(Creature* creature, int husbandry_skill)
{
    int delta;
    if(!creature || !creature->template) return;
    if(creature_is_hostile(creature)) return;

    delta = 5 + (husbandry_skill / 5);
    if(delta > 15) delta = 15;

    creature_apply_disposition_delta(creature, delta);
    creature_update_taming_stage(creature, husbandry_skill);
}

void creature_update_taming_stage(Creature* creature, int husbandry_skill)
{
    int d;
    if(!creature || !creature->template || !creature->template->tamable) return;

    d = creature->disposition;
    if(d >= 80 && husbandry_skill >= 10)
        creature->taming_stage = TAMING_BONDED;
    else if(d >= 60 && husbandry_skill >= 5)
        creature->taming_stage = TAMING_TAME;
    else if(d >= 30)
        creature->taming_stage = TAMING_FAMILIAR;
    else if(d >= 10)
        creature->taming_stage = TAMING_WARY;
    else
        creature->taming_stage = TAMING_WILD;
}

const char* taming_stage_name(CreatureTamingStage stage)
{
    switch(stage)
    {
        case TAMING_WARY:     return "wary";
        case TAMING_FAMILIAR: return "familiar";
        case TAMING_TAME:     return "tame";
        case TAMING_BONDED:   return "bonded";
        default:              return "wild";
    }
}

