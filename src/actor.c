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
}

int actor_derived_max_willpower(const Actor* actor)
{
    int wisdom;
    int endurance;
    int resolve;

    if(!actor)
        return 1;

    wisdom = actor_attr_or_default(actor->wisdom);
    endurance = actor_attr_or_default(actor->endurance);
    resolve = actor_attr_or_default(actor->resolve);

    // Resolve is weighted highest, with wisdom/endurance supporting.
    return 4 + (resolve / 2) + (wisdom / 3) + (endurance / 4);
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

