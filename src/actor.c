#include "actor.h"

#define ACTOR_ATTR_MIN 1
#define ACTOR_ATTR_MAX 100
#define ACTOR_ATTR_BASELINE 20

static int actor_attr_or_default(int value)
{
    if(value <= 0)
        return ACTOR_ATTR_BASELINE;
    return actor_attr_clamp(value);
}

/*
 * Purpose:
 *   Implements small Actor-level utility operations.
 *
 * Functions:
 *   - damage_actor: subtracts damage and clamps health to zero.
 */

// Apply damage to an actor and clamp health at zero.
void damage_actor(Actor* a, int dmg)
{
    if(!a)
        return;

    a->health -= dmg;

    if(a->health < 0)
        a->health = 0;
}

int actor_attr_clamp(int value)
{
    if(value < ACTOR_ATTR_MIN)
        return ACTOR_ATTR_MIN;
    if(value > ACTOR_ATTR_MAX)
        return ACTOR_ATTR_MAX;
    return value;
}

void actor_ensure_base_attributes(Actor* actor)
{
    if(!actor)
        return;

    actor->strength = actor_attr_or_default(actor->strength);
    actor->constitution = actor_attr_or_default(actor->constitution);
    actor->endurance = actor_attr_or_default(actor->endurance);
    actor->agility = actor_attr_or_default(actor->agility);
    actor->dexterity = actor_attr_or_default(actor->dexterity);
    actor->speed = actor_attr_or_default(actor->speed);
    actor->intellect = actor_attr_or_default(actor->intellect);
    actor->wisdom = actor_attr_or_default(actor->wisdom);
    actor->resolve = actor_attr_or_default(actor->resolve);
    actor->composure = actor_attr_or_default(actor->composure);
    actor->charisma = actor_attr_or_default(actor->charisma);
    actor->beauty = actor_attr_or_default(actor->beauty);
    actor->perception = actor_attr_or_default(actor->perception);
    actor->wits = actor_attr_or_default(actor->wits);
}

int actor_derived_max_willpower(const Actor* actor)
{
    int wisdom;
    int endurance;
    int resolve;
    int composure;

    if(!actor)
        return 1;

    wisdom = actor_attr_or_default(actor->wisdom);
    endurance = actor_attr_or_default(actor->endurance);
    resolve = actor_attr_or_default(actor->resolve);
    composure = actor_attr_or_default(actor->composure);

    // Resolve is weighted highest, with wisdom/endurance/composure supporting.
    return 4 + (resolve / 2) + (wisdom / 3) + (endurance / 4) + (composure / 5);
}

int actor_speed_hit_bonus(const Actor* actor)
{
    int speed;

    if(!actor)
        return 0;

    speed = actor_attr_or_default(actor->speed);
    return (speed - ACTOR_ATTR_BASELINE) / 8;
}

int actor_speed_block_bonus(const Actor* actor)
{
    int speed;

    if(!actor)
        return 0;

    speed = actor_attr_or_default(actor->speed);
    return (speed - ACTOR_ATTR_BASELINE) / 10;
}

int actor_speed_parry_bonus(const Actor* actor)
{
    int speed;

    if(!actor)
        return 0;

    speed = actor_attr_or_default(actor->speed);
    return (speed - ACTOR_ATTR_BASELINE) / 10;
}

int actor_dodge_attribute_bonus(const Actor* actor)
{
    int agility;
    int speed;
    int dexterity;
    int weighted_delta;

    if(!actor)
        return 0;

    agility = actor_attr_or_default(actor->agility);
    speed = actor_attr_or_default(actor->speed);
    dexterity = actor_attr_or_default(actor->dexterity);

    // AGI/SPD are primary contributors, DEX is secondary.
    weighted_delta =
        ((agility - ACTOR_ATTR_BASELINE) * 3) +
        ((speed - ACTOR_ATTR_BASELINE) * 3) +
        (dexterity - ACTOR_ATTR_BASELINE);

    return weighted_delta / 12;
}

int actor_derived_max_health(const Actor* actor)
{
    int constitution;
    int strength;
    int endurance;

    if(!actor)
        return 20;

    constitution = actor_attr_or_default(actor->constitution);
    strength = actor_attr_or_default(actor->strength);
    endurance = actor_attr_or_default(actor->endurance);
    return 20
        + ((constitution - ACTOR_ATTR_BASELINE) / 4)
        + ((strength - ACTOR_ATTR_BASELINE) / 8)
        + ((endurance - ACTOR_ATTR_BASELINE) / 10);
}

int actor_derived_max_stamina(const Actor* actor)
{
    int endurance;
    int constitution;
    int strength;

    if(!actor)
        return 20;

    endurance = actor_attr_or_default(actor->endurance);
    constitution = actor_attr_or_default(actor->constitution);
    strength = actor_attr_or_default(actor->strength);
    return 20
        + ((endurance - ACTOR_ATTR_BASELINE) / 4)
        + ((constitution - ACTOR_ATTR_BASELINE) / 8)
        + ((strength - ACTOR_ATTR_BASELINE) / 10);
}

int actor_derived_max_mana(const Actor* actor)
{
    int intellect;
    int wisdom;
    int wits;

    if(!actor)
        return 8;

    intellect = actor_attr_or_default(actor->intellect);
    wisdom = actor_attr_or_default(actor->wisdom);
    wits = actor_attr_or_default(actor->wits);
    // INT is primary driver; WIS provides a smaller secondary contribution.
    return 8
        + ((intellect - ACTOR_ATTR_BASELINE) / 4)
        + ((wisdom - ACTOR_ATTR_BASELINE) / 6)
        + ((wits - ACTOR_ATTR_BASELINE) / 8);
}

int actor_strength_melee_bonus(const Actor* actor)
{
    int strength;

    if(!actor)
        return 0;

    strength = actor_attr_or_default(actor->strength);
    return (strength - ACTOR_ATTR_BASELINE) / 6;
}

int actor_dexterity_crit_bonus(const Actor* actor)
{
    int dexterity;

    if(!actor)
        return 0;

    dexterity = actor_attr_or_default(actor->dexterity);
    return (dexterity - ACTOR_ATTR_BASELINE) / 8;
}

int actor_perception_detection_range(const Actor* actor)
{
    int perception;

    if(!actor)
        return 5;

    perception = actor_attr_or_default(actor->perception);
    return 5 + ((perception - ACTOR_ATTR_BASELINE) / 4);
}

int actor_area_vision_range(const Actor* actor)
{
    int perception;
    int wits;
    int wisdom;
    int intellect;
    int range;

    if(!actor)
        return 20;

    perception = actor_attr_or_default(actor->perception);
    wits = actor_attr_or_default(actor->wits);
    wisdom = actor_attr_or_default(actor->wisdom);
    intellect = actor_attr_or_default(actor->intellect);

    // Baseline 20 on core mental stats yields vision range 20.
    range = (perception + wits + wisdom + intellect) / 4;
    if(range < 1)
        range = 1;
    return range;
}

int actor_overworld_vision_range(const Actor* actor)
{
    int perception;
    int wits;
    int range;

    if(!actor)
        return 5;

    perception = actor_attr_or_default(actor->perception);
    wits = actor_attr_or_default(actor->wits);

    range = 5
        + ((perception - ACTOR_ATTR_BASELINE) / 4)
        + ((wits - ACTOR_ATTR_BASELINE) / 8);

    if(range < 1)
        range = 1;

    return range;
}

int actor_wits_initiative_bonus(const Actor* actor)
{
    int wits;

    if(!actor)
        return 0;

    wits = actor_attr_or_default(actor->wits);
    return (wits - ACTOR_ATTR_BASELINE) / 6;
}

