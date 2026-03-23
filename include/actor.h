#ifndef ACTOR_H
#define ACTOR_H

#include "entity.h"   // Entity struct for position & symbol

/*
 * Purpose:
 *   Defines the base Actor data model shared by players and creatures.
 *
 * Types:
 *   - WeaponSkillType: weapon family identifiers used by combat and progression.
 *   - Actor: runtime stats, position, and weapon-skill progression values.
 */

// Keep weapon skills in an indexed array so new weapon categories can be added later.
typedef enum WeaponSkillType {
    WEAPON_SKILL_UNARMED = 0,
    WEAPON_SKILL_DAGGER,
    WEAPON_SKILL_SWORD,
    WEAPON_SKILL_AXE,
    WEAPON_SKILL_MACE,
    WEAPON_SKILL_SPEAR,
    WEAPON_SKILL_STAFF,
    WEAPON_SKILL_POLEARM,
    WEAPON_SKILL_COUNT
} WeaponSkillType;

typedef struct Actor {
    Entity entity;      // composition: every actor **has** an entity
    // Base attributes (1-100, 20 = average baseline)
    int strength;
    int constitution;
    int endurance;
    int agility;
    int dexterity;
    int speed;
    int intellect;
    int wisdom;
    int resolve;
    int composure;
    int charisma;
    int beauty;
    int perception;
    int wits;
    int health;
    int max_health;
    int stamina;
    int max_stamina;
    int willpower;
    int max_willpower;
    int mana;
    int max_mana;
    int weapon_skill[WEAPON_SKILL_COUNT];
    int weapon_skill_xp[WEAPON_SKILL_COUNT];
    int armor_rating;
    int dodge;
    int block;
    int parry;
} Actor;

// Apply damage to an actor and clamp health at zero.
void damage_actor(Actor* a, int dmg);

// Clamp/normalize one base attribute value to valid runtime range.
int actor_attr_clamp(int value);

// Ensure all base attributes are initialized to sane defaults.
void actor_ensure_base_attributes(Actor* actor);

// Derived-stat helpers used by combat and UI.
int actor_derived_max_health(const Actor* actor);
int actor_derived_max_stamina(const Actor* actor);
int actor_derived_max_mana(const Actor* actor);
int actor_derived_max_willpower(const Actor* actor);
int actor_speed_hit_bonus(const Actor* actor);
int actor_speed_block_bonus(const Actor* actor);
int actor_speed_parry_bonus(const Actor* actor);
int actor_dodge_attribute_bonus(const Actor* actor);
int actor_strength_melee_bonus(const Actor* actor);
int actor_dexterity_crit_bonus(const Actor* actor);
int actor_perception_detection_range(const Actor* actor);
int actor_overworld_vision_range(const Actor* actor);
int actor_wits_initiative_bonus(const Actor* actor);

#endif

