#ifndef ACTOR_H
#define ACTOR_H

#include "entity.h"   // Entity struct for position & symbol

/*
 * Purpose:
 *   Defines the base Actor data model shared by players and creatures.
 *
 * Types:
 *   - WeaponSkillType: weapon family identifiers used by combat and progression.
 *   - NonWeaponSkillType: general profession and field-skill identifiers.
 *   - Actor: runtime stats, position, and skill progression values.
 */

#define ACTOR_RACE_ID_LENGTH 32

typedef enum ActorBodyLayout {
    ACTOR_BODY_LAYOUT_NONE = 0,
    ACTOR_BODY_LAYOUT_HUMANOID,
    ACTOR_BODY_LAYOUT_CREATURE_GENERIC,
} ActorBodyLayout;

typedef enum ActorBodyPart {
    ACTOR_BODY_PART_HEAD = 0,
    ACTOR_BODY_PART_LEFT_EYE,
    ACTOR_BODY_PART_RIGHT_EYE,
    ACTOR_BODY_PART_FACE,
    ACTOR_BODY_PART_NECK,
    ACTOR_BODY_PART_LEFT_ARM,
    ACTOR_BODY_PART_RIGHT_ARM,
    ACTOR_BODY_PART_LEFT_HAND,
    ACTOR_BODY_PART_RIGHT_HAND,
    ACTOR_BODY_PART_LEFT_LEG,
    ACTOR_BODY_PART_RIGHT_LEG,
    ACTOR_BODY_PART_LEFT_FOOT,
    ACTOR_BODY_PART_RIGHT_FOOT,
    ACTOR_BODY_PART_TORSO,
    ACTOR_BODY_PART_FORELIMBS,
    ACTOR_BODY_PART_HINDLIMBS,
    ACTOR_BODY_PART_TAIL,
    ACTOR_BODY_PART_COUNT,
} ActorBodyPart;

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
    WEAPON_SKILL_THROWN,
    WEAPON_SKILL_BOW,
    WEAPON_SKILL_CROSSBOW,
    WEAPON_SKILL_SWORD_2H,
    WEAPON_SKILL_AXE_2H,
    WEAPON_SKILL_MACE_2H,
    WEAPON_SKILL_SPEAR_2H,
    WEAPON_SKILL_COUNT,
    WEAPON_SKILL_SWORD_1H = WEAPON_SKILL_SWORD,
    WEAPON_SKILL_AXE_1H = WEAPON_SKILL_AXE,
    WEAPON_SKILL_MACE_1H = WEAPON_SKILL_MACE,
    WEAPON_SKILL_SPEAR_1H = WEAPON_SKILL_SPEAR,
} WeaponSkillType;

// Keep non-weapon skills in a separate indexed array so professions can grow over time.
typedef enum NonWeaponSkillType {
    NON_WEAPON_SKILL_ANIMAL_HANDLING = 0,
    NON_WEAPON_SKILL_MINING,
    NON_WEAPON_SKILL_SMELTING,
    NON_WEAPON_SKILL_BLACKSMITHING,
    NON_WEAPON_SKILL_LUMBERJACKING,
    NON_WEAPON_SKILL_CARPENTRY,
    NON_WEAPON_SKILL_COOKING,
    NON_WEAPON_SKILL_HERBALISM,
    NON_WEAPON_SKILL_FISHING,
    NON_WEAPON_SKILL_ALCHEMY,
    NON_WEAPON_SKILL_COAL_BURNING,
    NON_WEAPON_SKILL_TAILORING,
    NON_WEAPON_SKILL_LEATHERWORKING,
    NON_WEAPON_SKILL_SKINNING,
    NON_WEAPON_SKILL_TANNING,
    NON_WEAPON_SKILL_COUNT
} NonWeaponSkillType;

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
    char race_id[ACTOR_RACE_ID_LENGTH];
    int health;
    int max_health;
    int stamina;
    int max_stamina;
    int action_points;
    int max_action_points;
    int willpower;
    int max_willpower;
    int mana;
    int max_mana;
    int weapon_skill[WEAPON_SKILL_COUNT];
    int weapon_skill_xp[WEAPON_SKILL_COUNT];
    int non_weapon_skill[NON_WEAPON_SKILL_COUNT];
    int non_weapon_skill_xp[NON_WEAPON_SKILL_COUNT];
    int hard_damage_reduction;
    int soft_damage_reduction;
    int armour_rating;
    ActorBodyLayout body_layout;
    int body_part_active[ACTOR_BODY_PART_COUNT];
    int body_part_health[ACTOR_BODY_PART_COUNT];
    int body_part_max_health[ACTOR_BODY_PART_COUNT];
    int body_part_hard_damage_reduction[ACTOR_BODY_PART_COUNT];
    int body_part_soft_damage_reduction[ACTOR_BODY_PART_COUNT];
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
int actor_derived_max_action_points(const Actor* actor);
int actor_derived_max_mana(const Actor* actor);
int actor_derived_max_willpower(const Actor* actor);
int actor_speed_hit_bonus(const Actor* actor);
int actor_speed_block_bonus(const Actor* actor);
int actor_speed_parry_bonus(const Actor* actor);
int actor_dodge_attribute_bonus(const Actor* actor);
int actor_strength_melee_bonus(const Actor* actor);
int actor_dexterity_crit_bonus(const Actor* actor);
int actor_perception_detection_range(const Actor* actor);
int actor_area_vision_range(const Actor* actor);
int actor_overworld_vision_range(const Actor* actor);
int actor_wits_initiative_bonus(const Actor* actor);
int actor_stamina_floor(const Actor* actor);
int actor_clamp_stamina_value(const Actor* actor, int stamina_value);
int actor_is_unconscious(const Actor* actor);

const char* actor_body_layout_name(ActorBodyLayout layout);
const char* actor_body_part_name(ActorBodyPart part);
void actor_body_reset(Actor* actor);
void actor_body_set_layout(Actor* actor, ActorBodyLayout layout);
int actor_body_part_is_active(const Actor* actor, ActorBodyPart part);
int actor_body_total_health(const Actor* actor);
int actor_body_total_max_health(const Actor* actor);
int actor_body_distribute_health(Actor* actor, int current_total, int max_total);

#endif

