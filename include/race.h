#ifndef RACE_H
#define RACE_H

#include "actor.h"

#define MAX_RACE_TEMPLATES 32
#define RACE_TEMPLATE_NAME_LENGTH 64
#define RACE_TEMPLATE_DESC_LENGTH 160

typedef enum RaceAttributeBand {
    RACE_ATTRIBUTE_POOR = -2,
    RACE_ATTRIBUTE_BELOW_AVERAGE = -1,
    RACE_ATTRIBUTE_AVERAGE = 0,
    RACE_ATTRIBUTE_ABOVE_AVERAGE = 1,
    RACE_ATTRIBUTE_EXCEPTIONAL = 2,
} RaceAttributeBand;

typedef struct RaceTemplate {
    char id[ACTOR_RACE_ID_LENGTH];
    char name[RACE_TEMPLATE_NAME_LENGTH];
    char description[RACE_TEMPLATE_DESC_LENGTH];
    Actor baseline;
    int average_deviation;
    int above_average_delta;
    int below_average_delta;
} RaceTemplate;

void clear_race_templates(void);
int race_templates_load(const char* path);
const char* race_templates_last_error(void);
const RaceTemplate* race_template_by_id(const char* id);
const RaceTemplate* race_template_by_name(const char* name);
const RaceTemplate* race_default_template(void);
void race_apply_base_attributes(Actor* actor, const RaceTemplate* race);
int race_baseline_attribute_value(const RaceTemplate* race, const char* attribute_name);
RaceAttributeBand race_attribute_band_for_value(const RaceTemplate* race, const char* attribute_name, int value);
const char* race_attribute_band_name(RaceAttributeBand band);

#endif
