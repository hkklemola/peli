#include "race.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RaceTemplate g_race_templates[MAX_RACE_TEMPLATES];
static int g_race_template_count = 0;
static char g_race_template_last_error[256];

static void set_race_template_error(const char* message, const char* detail)
{
    if(detail && detail[0] != '\0')
        snprintf(g_race_template_last_error, sizeof(g_race_template_last_error), "%s: %s", message, detail);
    else
        snprintf(g_race_template_last_error, sizeof(g_race_template_last_error), "%s", message);
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

static void race_template_set_defaults(RaceTemplate* race)
{
    if(!race)
        return;

    memset(race, 0, sizeof(*race));
    snprintf(race->name, sizeof(race->name), "Unknown");
    snprintf(race->description, sizeof(race->description), "No description.");
    race->average_deviation = 2;
    race->above_average_delta = 7;
    race->below_average_delta = 7;

    race->baseline.strength = 20;
    race->baseline.constitution = 20;
    race->baseline.endurance = 20;
    race->baseline.agility = 20;
    race->baseline.dexterity = 20;
    race->baseline.speed = 20;
    race->baseline.intellect = 20;
    race->baseline.wisdom = 20;
    race->baseline.resolve = 20;
    race->baseline.composure = 20;
    race->baseline.charisma = 20;
    race->baseline.beauty = 20;
    race->baseline.perception = 20;
    race->baseline.wits = 20;
}

static int parse_integer(const char* value, int* out)
{
    char* end = NULL;
    long parsed;

    if(!value || !out)
        return 0;

    parsed = strtol(value, &end, 10);
    if(end == value)
        return 0;

    while(end && *end && isspace((unsigned char)*end))
        end++;

    if(end && *end != '\0')
        return 0;

    *out = (int)parsed;
    return 1;
}

static int* race_attribute_ptr(RaceTemplate* race, const char* key)
{
    if(!race || !key)
        return NULL;

    if(equals_ignore_case(key, "strength")) return &race->baseline.strength;
    if(equals_ignore_case(key, "constitution")) return &race->baseline.constitution;
    if(equals_ignore_case(key, "endurance")) return &race->baseline.endurance;
    if(equals_ignore_case(key, "agility")) return &race->baseline.agility;
    if(equals_ignore_case(key, "dexterity")) return &race->baseline.dexterity;
    if(equals_ignore_case(key, "speed")) return &race->baseline.speed;
    if(equals_ignore_case(key, "intellect")) return &race->baseline.intellect;
    if(equals_ignore_case(key, "wisdom")) return &race->baseline.wisdom;
    if(equals_ignore_case(key, "resolve")) return &race->baseline.resolve;
    if(equals_ignore_case(key, "composure")) return &race->baseline.composure;
    if(equals_ignore_case(key, "charisma")) return &race->baseline.charisma;
    if(equals_ignore_case(key, "beauty")) return &race->baseline.beauty;
    if(equals_ignore_case(key, "perception")) return &race->baseline.perception;
    if(equals_ignore_case(key, "wits")) return &race->baseline.wits;

    return NULL;
}

static int finalize_race_template(RaceTemplate* race)
{
    if(!race)
        return 0;

    if(race->id[0] == '\0')
    {
        set_race_template_error("Race template is missing an id", NULL);
        return 0;
    }

    if(race->name[0] == '\0')
        snprintf(race->name, sizeof(race->name), "%s", race->id);

    if(race->average_deviation < 0)
        race->average_deviation = 0;
    if(race->above_average_delta < race->average_deviation)
        race->above_average_delta = race->average_deviation;
    if(race->below_average_delta < race->average_deviation)
        race->below_average_delta = race->average_deviation;

    actor_ensure_base_attributes(&race->baseline);
    snprintf(race->baseline.race_id, sizeof(race->baseline.race_id), "%s", race->id);
    return 1;
}

void clear_race_templates(void)
{
    memset(g_race_templates, 0, sizeof(g_race_templates));
    g_race_template_count = 0;
    g_race_template_last_error[0] = '\0';
}

int race_templates_load(const char* path)
{
    FILE* file;
    char line[256];
    int line_number = 0;
    int in_section = 0;
    RaceTemplate current;

    if(!path || path[0] == '\0')
    {
        set_race_template_error("Missing race template path", NULL);
        return 0;
    }

    file = fopen(path, "r");
    if(!file)
    {
        set_race_template_error("Could not open race template file", path);
        return 0;
    }

    clear_race_templates();
    race_template_set_defaults(&current);

    while(fgets(line, sizeof(line), file))
    {
        char* equals;
        char* key;
        char* value;
        char detail[128];

        line_number++;
        trim_in_place(line);

        if(line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        if(line[0] == '[')
        {
            if(in_section)
            {
                if(!finalize_race_template(&current))
                {
                    fclose(file);
                    return 0;
                }

                if(g_race_template_count >= MAX_RACE_TEMPLATES)
                {
                    fclose(file);
                    set_race_template_error("Too many race templates", path);
                    return 0;
                }

                g_race_templates[g_race_template_count++] = current;
            }

            if(equals_ignore_case(line, "[race]"))
            {
                race_template_set_defaults(&current);
                in_section = 1;
            }
            else
            {
                in_section = 0;
            }
            continue;
        }

        if(!in_section)
            continue;

        equals = strchr(line, '=');
        if(!equals)
        {
            snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
            set_race_template_error("Malformed race template line", detail);
            fclose(file);
            return 0;
        }

        *equals = '\0';
        key = line;
        value = equals + 1;
        trim_in_place(key);
        trim_in_place(value);

        if(equals_ignore_case(key, "id"))
        {
            snprintf(current.id, sizeof(current.id), "%s", value);
        }
        else if(equals_ignore_case(key, "name"))
        {
            snprintf(current.name, sizeof(current.name), "%s", value);
        }
        else if(equals_ignore_case(key, "description"))
        {
            snprintf(current.description, sizeof(current.description), "%s", value);
        }
        else if(equals_ignore_case(key, "average_deviation"))
        {
            if(!parse_integer(value, &current.average_deviation))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_race_template_error("Invalid average_deviation value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "above_average_delta"))
        {
            if(!parse_integer(value, &current.above_average_delta))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_race_template_error("Invalid above_average_delta value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "below_average_delta"))
        {
            if(!parse_integer(value, &current.below_average_delta))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_race_template_error("Invalid below_average_delta value", detail);
                fclose(file);
                return 0;
            }
        }
        else
        {
            int* attr = race_attribute_ptr(&current, key);
            if(attr)
            {
                if(!parse_integer(value, attr))
                {
                    snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                    set_race_template_error("Invalid race attribute value", detail);
                    fclose(file);
                    return 0;
                }
            }
        }
    }

    fclose(file);

    if(in_section)
    {
        if(!finalize_race_template(&current))
            return 0;

        if(g_race_template_count >= MAX_RACE_TEMPLATES)
        {
            set_race_template_error("Too many race templates", path);
            return 0;
        }

        g_race_templates[g_race_template_count++] = current;
    }

    if(g_race_template_count <= 0)
    {
        set_race_template_error("No race templates were loaded", path);
        return 0;
    }

    return 1;
}

const char* race_templates_last_error(void)
{
    return g_race_template_last_error;
}

int race_templates_count(void)
{
    return g_race_template_count;
}

const RaceTemplate* race_template_at(int index)
{
    if(index < 0 || index >= g_race_template_count)
        return NULL;

    return &g_race_templates[index];
}

const RaceTemplate* race_template_by_id(const char* id)
{
    if(!id || id[0] == '\0')
        return NULL;

    for(int i = 0; i < g_race_template_count; i++)
    {
        if(equals_ignore_case(g_race_templates[i].id, id))
            return &g_race_templates[i];
    }

    return NULL;
}

const RaceTemplate* race_template_by_name(const char* name)
{
    if(!name || name[0] == '\0')
        return NULL;

    for(int i = 0; i < g_race_template_count; i++)
    {
        if(equals_ignore_case(g_race_templates[i].name, name))
            return &g_race_templates[i];
    }

    return NULL;
}

const RaceTemplate* race_default_template(void)
{
    const RaceTemplate* human = race_template_by_id("human");
    if(human)
        return human;
    if(g_race_template_count > 0)
        return &g_race_templates[0];
    return NULL;
}

void race_apply_base_attributes(Actor* actor, const RaceTemplate* race)
{
    if(!actor || !race)
        return;

    actor->strength = race->baseline.strength;
    actor->constitution = race->baseline.constitution;
    actor->endurance = race->baseline.endurance;
    actor->agility = race->baseline.agility;
    actor->dexterity = race->baseline.dexterity;
    actor->speed = race->baseline.speed;
    actor->intellect = race->baseline.intellect;
    actor->wisdom = race->baseline.wisdom;
    actor->resolve = race->baseline.resolve;
    actor->composure = race->baseline.composure;
    actor->charisma = race->baseline.charisma;
    actor->beauty = race->baseline.beauty;
    actor->perception = race->baseline.perception;
    actor->wits = race->baseline.wits;
    snprintf(actor->race_id, sizeof(actor->race_id), "%s", race->id);
}

int race_baseline_attribute_value(const RaceTemplate* race, const char* attribute_name)
{
    if(!race || !attribute_name)
        return 20;

    if(equals_ignore_case(attribute_name, "strength")) return race->baseline.strength;
    if(equals_ignore_case(attribute_name, "constitution")) return race->baseline.constitution;
    if(equals_ignore_case(attribute_name, "endurance")) return race->baseline.endurance;
    if(equals_ignore_case(attribute_name, "agility")) return race->baseline.agility;
    if(equals_ignore_case(attribute_name, "dexterity")) return race->baseline.dexterity;
    if(equals_ignore_case(attribute_name, "speed")) return race->baseline.speed;
    if(equals_ignore_case(attribute_name, "intellect")) return race->baseline.intellect;
    if(equals_ignore_case(attribute_name, "wisdom")) return race->baseline.wisdom;
    if(equals_ignore_case(attribute_name, "resolve")) return race->baseline.resolve;
    if(equals_ignore_case(attribute_name, "composure")) return race->baseline.composure;
    if(equals_ignore_case(attribute_name, "charisma")) return race->baseline.charisma;
    if(equals_ignore_case(attribute_name, "beauty")) return race->baseline.beauty;
    if(equals_ignore_case(attribute_name, "perception")) return race->baseline.perception;
    if(equals_ignore_case(attribute_name, "wits")) return race->baseline.wits;

    return 20;
}

RaceAttributeBand race_attribute_band_for_value(const RaceTemplate* race, const char* attribute_name, int value)
{
    int baseline;
    int average_low;
    int average_high;
    int above_high;
    int below_low;

    if(!race)
        return RACE_ATTRIBUTE_AVERAGE;

    baseline = race_baseline_attribute_value(race, attribute_name);
    average_low = baseline - race->average_deviation;
    average_high = baseline + race->average_deviation;
    above_high = baseline + race->above_average_delta;
    below_low = baseline - race->below_average_delta;

    if(value >= average_low && value <= average_high)
        return RACE_ATTRIBUTE_AVERAGE;
    if(value > average_high && value <= above_high)
        return RACE_ATTRIBUTE_ABOVE_AVERAGE;
    if(value < average_low && value >= below_low)
        return RACE_ATTRIBUTE_BELOW_AVERAGE;
    if(value > above_high)
        return RACE_ATTRIBUTE_EXCEPTIONAL;
    return RACE_ATTRIBUTE_POOR;
}

const char* race_attribute_band_name(RaceAttributeBand band)
{
    switch(band)
    {
        case RACE_ATTRIBUTE_POOR: return "poor";
        case RACE_ATTRIBUTE_BELOW_AVERAGE: return "below average";
        case RACE_ATTRIBUTE_AVERAGE: return "average";
        case RACE_ATTRIBUTE_ABOVE_AVERAGE: return "above average";
        case RACE_ATTRIBUTE_EXCEPTIONAL: return "exceptional";
        default: return "unknown";
    }
}
