#include "actor.h"

#include <string.h>

#define ACTOR_ATTR_MIN 1
#define ACTOR_ATTR_MAX 100
#define ACTOR_ATTR_BASELINE 20

static int actor_attr_or_default(int value)
{
    if(value <= 0)
        return ACTOR_ATTR_BASELINE;
    return actor_attr_clamp(value);
}

static int actor_body_layout_part_mask(ActorBodyLayout layout)
{
    switch(layout)
    {
        case ACTOR_BODY_LAYOUT_HUMANOID:
            return (1 << ACTOR_BODY_PART_HEAD)
                | (1 << ACTOR_BODY_PART_LEFT_EYE)
                | (1 << ACTOR_BODY_PART_RIGHT_EYE)
                | (1 << ACTOR_BODY_PART_FACE)
                | (1 << ACTOR_BODY_PART_NECK)
                | (1 << ACTOR_BODY_PART_LEFT_ARM)
                | (1 << ACTOR_BODY_PART_RIGHT_ARM)
                | (1 << ACTOR_BODY_PART_LEFT_HAND)
                | (1 << ACTOR_BODY_PART_RIGHT_HAND)
                | (1 << ACTOR_BODY_PART_LEFT_LEG)
                | (1 << ACTOR_BODY_PART_RIGHT_LEG)
                | (1 << ACTOR_BODY_PART_LEFT_FOOT)
                | (1 << ACTOR_BODY_PART_RIGHT_FOOT)
                | (1 << ACTOR_BODY_PART_TORSO);
        case ACTOR_BODY_LAYOUT_CREATURE_GENERIC:
            return (1 << ACTOR_BODY_PART_HEAD)
                | (1 << ACTOR_BODY_PART_TORSO)
                | (1 << ACTOR_BODY_PART_FORELIMBS)
                | (1 << ACTOR_BODY_PART_HINDLIMBS)
                | (1 << ACTOR_BODY_PART_TAIL);
        case ACTOR_BODY_LAYOUT_NONE:
        default:
            return 0;
    }
}

static int actor_body_part_index_valid(ActorBodyPart part)
{
    return part >= ACTOR_BODY_PART_HEAD && part < ACTOR_BODY_PART_COUNT;
}

static int actor_body_weighted_value(int total, ActorBodyPart part)
{
    if(total <= 0)
        return 0;

    switch(part)
    {
        case ACTOR_BODY_PART_TORSO:
            return total;
        case ACTOR_BODY_PART_LEFT_ARM:
        case ACTOR_BODY_PART_RIGHT_ARM:
        case ACTOR_BODY_PART_LEFT_LEG:
        case ACTOR_BODY_PART_RIGHT_LEG:
        case ACTOR_BODY_PART_FORELIMBS:
        case ACTOR_BODY_PART_HINDLIMBS:
            return total / 2;
        case ACTOR_BODY_PART_HEAD:
        case ACTOR_BODY_PART_FACE:
        case ACTOR_BODY_PART_NECK:
        case ACTOR_BODY_PART_LEFT_HAND:
        case ACTOR_BODY_PART_RIGHT_HAND:
        case ACTOR_BODY_PART_LEFT_FOOT:
        case ACTOR_BODY_PART_RIGHT_FOOT:
        case ACTOR_BODY_PART_TAIL:
            return total / 4;
        case ACTOR_BODY_PART_LEFT_EYE:
        case ACTOR_BODY_PART_RIGHT_EYE:
            return total / 5;
        default:
            return 0;
    }
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

const char* actor_body_layout_name(ActorBodyLayout layout)
{
    switch(layout)
    {
        case ACTOR_BODY_LAYOUT_HUMANOID:
            return "humanoid";
        case ACTOR_BODY_LAYOUT_CREATURE_GENERIC:
            return "creature";
        case ACTOR_BODY_LAYOUT_NONE:
        default:
            return "none";
    }
}

const char* actor_body_part_name(ActorBodyPart part)
{
    switch(part)
    {
        case ACTOR_BODY_PART_HEAD:
            return "Head";
        case ACTOR_BODY_PART_LEFT_EYE:
            return "Left Eye";
        case ACTOR_BODY_PART_RIGHT_EYE:
            return "Right Eye";
        case ACTOR_BODY_PART_FACE:
            return "Face";
        case ACTOR_BODY_PART_NECK:
            return "Throat";
        case ACTOR_BODY_PART_LEFT_ARM:
            return "Left Arm";
        case ACTOR_BODY_PART_RIGHT_ARM:
            return "Right Arm";
        case ACTOR_BODY_PART_LEFT_HAND:
            return "Left Hand";
        case ACTOR_BODY_PART_RIGHT_HAND:
            return "Right Hand";
        case ACTOR_BODY_PART_LEFT_LEG:
            return "Left Leg";
        case ACTOR_BODY_PART_RIGHT_LEG:
            return "Right Leg";
        case ACTOR_BODY_PART_LEFT_FOOT:
            return "Left Foot";
        case ACTOR_BODY_PART_RIGHT_FOOT:
            return "Right Foot";
        case ACTOR_BODY_PART_TORSO:
            return "Torso";
        case ACTOR_BODY_PART_FORELIMBS:
            return "Forelimbs";
        case ACTOR_BODY_PART_HINDLIMBS:
            return "Hindlimbs";
        case ACTOR_BODY_PART_TAIL:
            return "Tail";
        default:
            return "Unknown";
    }
}

void actor_body_reset(Actor* actor)
{
    if(!actor)
        return;

    actor->body_layout = ACTOR_BODY_LAYOUT_NONE;
    memset(actor->body_part_active, 0, sizeof(actor->body_part_active));
    memset(actor->body_part_health, 0, sizeof(actor->body_part_health));
    memset(actor->body_part_max_health, 0, sizeof(actor->body_part_max_health));
    memset(actor->body_part_hard_damage_reduction, 0, sizeof(actor->body_part_hard_damage_reduction));
    memset(actor->body_part_soft_damage_reduction, 0, sizeof(actor->body_part_soft_damage_reduction));
}

void actor_body_set_layout(Actor* actor, ActorBodyLayout layout)
{
    int active_mask;

    if(!actor)
        return;

    actor_body_reset(actor);
    actor->body_layout = layout;
    active_mask = actor_body_layout_part_mask(layout);

    for(int i = 0; i < ACTOR_BODY_PART_COUNT; i++)
    {
        if(active_mask & (1 << i))
            actor->body_part_active[i] = 1;
    }
}

int actor_body_part_is_active(const Actor* actor, ActorBodyPart part)
{
    if(!actor || !actor_body_part_index_valid(part))
        return 0;

    return actor->body_part_active[part] != 0;
}

int actor_body_total_health(const Actor* actor)
{
    int total = 0;

    if(!actor)
        return 0;

    for(int i = 0; i < ACTOR_BODY_PART_COUNT; i++)
    {
        if(actor->body_part_active[i])
            total += actor->body_part_health[i];
    }

    return total;
}

int actor_body_total_max_health(const Actor* actor)
{
    int total = 0;

    if(!actor)
        return 0;

    for(int i = 0; i < ACTOR_BODY_PART_COUNT; i++)
    {
        if(actor->body_part_active[i])
            total += actor->body_part_max_health[i];
    }

    return total;
}

int actor_body_distribute_health(Actor* actor, int current_total, int max_total)
{
    if(!actor)
        return 0;

    if(current_total < 0)
        current_total = 0;
    if(max_total < 0)
        max_total = 0;
    if(current_total > max_total)
        current_total = max_total;

    for(int i = 0; i < ACTOR_BODY_PART_COUNT; i++)
    {
        if(!actor->body_part_active[i])
        {
            actor->body_part_health[i] = 0;
            actor->body_part_max_health[i] = 0;
            continue;
        }

        actor->body_part_health[i] = actor_body_weighted_value(current_total, (ActorBodyPart)i);
        actor->body_part_max_health[i] = actor_body_weighted_value(max_total, (ActorBodyPart)i);

        if(actor->body_part_health[i] > actor->body_part_max_health[i])
            actor->body_part_health[i] = actor->body_part_max_health[i];
    }

    return 1;
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

    if(actor->hard_damage_reduction < 0)
        actor->hard_damage_reduction = 0;
    if(actor->soft_damage_reduction < 0)
        actor->soft_damage_reduction = 0;
    if(actor->armour_rating < 0)
        actor->armour_rating = 0;

    for(int i = 0; i < ACTOR_BODY_PART_COUNT; i++)
    {
        if(actor->body_part_active[i] == 0)
        {
            actor->body_part_health[i] = 0;
            actor->body_part_max_health[i] = 0;
            continue;
        }

        if(actor->body_part_health[i] < 0)
            actor->body_part_health[i] = 0;
        if(actor->body_part_max_health[i] < 0)
            actor->body_part_max_health[i] = 0;
        if(actor->body_part_health[i] > actor->body_part_max_health[i])
            actor->body_part_health[i] = actor->body_part_max_health[i];
    }
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

int actor_derived_max_action_points(const Actor* actor)
{
    int speed;
    int agility;
    int endurance;
    int wits;
    int bonus;
    int max_ap;

    if(!actor)
        return 4;

    speed = actor_attr_or_default(actor->speed);
    agility = actor_attr_or_default(actor->agility);
    endurance = actor_attr_or_default(actor->endurance);
    wits = actor_attr_or_default(actor->wits);

    bonus = ((speed - ACTOR_ATTR_BASELINE)
           + (agility - ACTOR_ATTR_BASELINE)
           + (endurance - ACTOR_ATTR_BASELINE)
           + (wits - ACTOR_ATTR_BASELINE)) / 40;

    max_ap = 4 + bonus;
    if(max_ap < 1)
        max_ap = 1;
    return max_ap;
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
        return 40;

    perception = actor_attr_or_default(actor->perception);
    wits = actor_attr_or_default(actor->wits);
    wisdom = actor_attr_or_default(actor->wisdom);
    intellect = actor_attr_or_default(actor->intellect);

    // Perception and wits are the primary vision drivers; wisdom/intellect are secondary.
    // Baseline 20 on all four stats now yields vision range 40.
    range = ((perception * 2) + (wits * 2) + wisdom + intellect) / 3;
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

int actor_passive_scout_range(const Actor* actor)
{
    int perception;
    int wits;
    int range;

    if(!actor)
        return 2;

    perception = actor_attr_or_default(actor->perception);
    wits = actor_attr_or_default(actor->wits);

    range = 2;
    if(perception > ACTOR_ATTR_BASELINE)
        range += (perception - ACTOR_ATTR_BASELINE) / 4;
    if(wits > ACTOR_ATTR_BASELINE)
        range += (wits - ACTOR_ATTR_BASELINE) / 8;

    if(range < 2)
        range = 2;

    return range;
}

int actor_passive_scout_chance(const Actor* actor, int distance)
{
    int perception;
    int wits;
    int base;
    int chance;

    if(!actor)
        return 0;
    if(distance < 0)
        distance = 0;

    perception = actor_attr_or_default(actor->perception);
    wits = actor_attr_or_default(actor->wits);

    base = 35
        + ((perception - ACTOR_ATTR_BASELINE) / 2)
        + ((wits - ACTOR_ATTR_BASELINE) / 4);

    chance = base - (distance * 10);
    if(chance < 5)
        chance = 5;
    if(chance > 95)
        chance = 95;

    return chance;
}

int actor_wits_initiative_bonus(const Actor* actor)
{
    int wits;

    if(!actor)
        return 0;

    wits = actor_attr_or_default(actor->wits);
    return (wits - ACTOR_ATTR_BASELINE) / 6;
}

int actor_stamina_floor(const Actor* actor)
{
    int max_stamina;

    if(!actor)
        return -20;

    max_stamina = actor->max_stamina;
    if(max_stamina < 1)
        max_stamina = actor_derived_max_stamina(actor);
    if(max_stamina < 1)
        max_stamina = 1;

    return -max_stamina;
}

int actor_clamp_stamina_value(const Actor* actor, int stamina_value)
{
    int max_stamina;
    int min_stamina;

    if(!actor)
        return stamina_value;

    max_stamina = actor->max_stamina;
    if(max_stamina < 1)
        max_stamina = actor_derived_max_stamina(actor);
    if(max_stamina < 1)
        max_stamina = 1;

    min_stamina = -max_stamina;

    if(stamina_value > max_stamina)
        return max_stamina;
    if(stamina_value < min_stamina)
        return min_stamina;
    return stamina_value;
}

int actor_is_unconscious(const Actor* actor)
{
    return actor && actor->stamina < 0;
}

