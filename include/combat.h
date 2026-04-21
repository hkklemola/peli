#ifndef COMBAT_H
#define COMBAT_H

#include "actor.h"
#include "character.h"

/*
 * Purpose:
 *   Declares combat stat views and melee-resolution APIs.
 *
 * Functions:
 *   - weapon_skill_name / weapon_skill_short_name: UI naming helpers.
 *   - non_weapon_skill_name / non_weapon_skill_save_key: profession-skill naming helpers.
 *   - actor_get_*skill* / actor_gain_*skill*_xp: skill progression APIs.
 *   - combat_profile_for_*: derives attack/parry profiles from equipped gear.
 *   - combat_summary_for_character: produces HUD-friendly combat summary values.
 *   - combat_resolve_melee_attack: executes one melee attack exchange.
 */

typedef struct CombatProfile {
    WeaponSkillType skill_type;
    char weapon_name[32];
    int damage_type_mask;
    int attack_mode_mask;
    int attack_pool_mask;
    int one_hand_attack_mode_mask;
    int two_hand_attack_mode_mask;
    int active_damage_type;
    AttackMode attack_mode;
    int is_two_hand_mode;
    int can_toggle_grip;
    AttackMode next_unlock_mode;
    int next_unlock_skill_level;
    int power;
    int damage_min;
    int damage_max;
    int stab_damage_min;
    int stab_damage_max;
    int cut_damage_min;
    int cut_damage_max;
    int smash_damage_min;
    int smash_damage_max;
    int punch_damage_min;
    int punch_damage_max;
    int kick_damage_min;
    int kick_damage_max;
    int accuracy_bonus;
    int crit_bonus;
    int parry_bonus;
    int block_bonus;
    int can_parry;
    int reach_bonus;
    int armor_penetration;
    int stamina_cost_mod;
    int status_bleed_chance;
    int status_stun_chance;
    int status_slow_chance;
    RangedWeaponType ranged_type;
    int ranged_range;
    char ammo_item_name[32];
    int ammo_per_shot;
    int is_armed;
} CombatProfile;

typedef struct CombatSummary {
    WeaponSkillType skill_type;
    char weapon_name[32];
    int active_damage_type;
    AttackMode attack_mode;
    int skill_level;
    int hit_chance;
    int crit_chance;
    int parry_chance;
    int damage_min;
    int damage_max;
    int damage;
    int is_armed;
    int is_two_hand_mode;
    int can_toggle_grip;
} CombatSummary;

typedef struct MeleeAttackResult {
    WeaponSkillType attack_skill_type;
    WeaponSkillType parry_skill_type;
    int damage_type;
    AttackMode attack_mode;
    int attack_skill_level;
    int parry_skill_level;
    int hit_chance;
    int crit_chance;
    int block_chance;
    int parry_chance;
    int hit;
    int blocked;
    int parried;
    int critical;
    int damage;
    int direct_damage;
    int armor_absorbed;
    int stamina_damage;
    int no_damage_hit;
    int bleed_applied;
    int stun_applied;
    int slow_applied;
    int bonus_damage;
    int attacker_levels_gained;
    int defender_levels_gained;
    ActorBodyPart target_body_part;
} MeleeAttackResult;

// Return a user-facing full name for a weapon skill.
const char* weapon_skill_name(WeaponSkillType skill_type);

// Return a short label for a weapon skill (HUD-friendly).
const char* weapon_skill_short_name(WeaponSkillType skill_type);

// Return a user-facing full name for a non-weapon skill.
const char* non_weapon_skill_name(NonWeaponSkillType skill_type);

// Return stable save-key stem for a non-weapon skill.
const char* non_weapon_skill_save_key(NonWeaponSkillType skill_type);

// Return user-facing name for one damage type.
const char* damage_type_name(int damage_type);

// Return user-facing name for one attack mode.
const char* attack_mode_name(AttackMode mode);

// Return attack verb for combat log text.
const char* attack_mode_verb(AttackMode mode);

// Return first valid mode from a mask.
AttackMode attack_mode_first_from_mask(int attack_mode_mask);

// Return next valid mode from a mask, cycling around.
AttackMode attack_mode_next_from_mask(int attack_mode_mask, AttackMode current_mode);

// Return valid selected mode for character's current attack setup.
AttackMode combat_valid_attack_mode_for_character(const Character* character, AttackMode requested_mode);

// Return effective melee range in tiles for this attack profile.
int combat_profile_melee_range(const CombatProfile* profile);

// Return 1 when profile weapon behaves as ranged.
int combat_profile_is_ranged(const CombatProfile* profile);

// Return effective ranged range in tiles for this attack profile.
int combat_profile_ranged_range(const CombatProfile* profile);

// Return stamina cost for one attack with this profile.
int combat_profile_attack_stamina_cost(const CombatProfile* profile);

// Return action-point cost for one attack with this profile.
int combat_profile_attack_action_point_cost(const CombatProfile* profile);

// Return effective pre-mitigation min/max attack values for a specific attacker/profile pair.
void combat_attack_value_range(const Actor* attacker, const CombatProfile* attack_profile, int* out_min_value, int* out_max_value);

// Return a representative pre-mitigation attack value for a specific attacker/profile pair.
int combat_attack_value(const Actor* attacker, const CombatProfile* attack_profile);

// Roll one pre-mitigation attack value from the current effective damage range.
int combat_roll_attack_value(const Actor* attacker, const CombatProfile* attack_profile);

// Read an actor's current level for a weapon-skill family.
int actor_get_weapon_skill(const Actor* actor, WeaponSkillType skill_type);

// Read accumulated XP for a weapon-skill family.
int actor_get_weapon_skill_xp(const Actor* actor, WeaponSkillType skill_type);

// Add weapon-skill XP and return number of levels gained.
int actor_gain_weapon_skill_xp(Actor* actor, WeaponSkillType skill_type, int amount);

// Return the standard attacker XP for a landed hit outcome.
int combat_weapon_skill_xp_for_hit(int critical, int no_damage_hit);

// Read an actor's current level for a non-weapon skill.
int actor_get_non_weapon_skill(const Actor* actor, NonWeaponSkillType skill_type);

// Read accumulated XP for a non-weapon skill.
int actor_get_non_weapon_skill_xp(const Actor* actor, NonWeaponSkillType skill_type);

// Add non-weapon-skill XP and return number of levels gained.
int actor_gain_non_weapon_skill_xp(Actor* actor, NonWeaponSkillType skill_type, int amount);

// Build the active attack profile from a character's equipped weapon setup.
CombatProfile combat_profile_for_character_attack(const Character* character, AttackMode requested_mode);

// Build the active parry profile from a character's equipped defensive option.
CombatProfile combat_profile_for_character_parry(const Character* character);

// Build an unarmed profile for generic actors.
CombatProfile combat_profile_for_actor_unarmed(const Actor* actor);

// Build a summary snapshot of combat stats for UI display.
CombatSummary combat_summary_for_character(const Character* character, AttackMode requested_mode);

// Resolve one melee attack from attacker to defender.
MeleeAttackResult combat_resolve_melee_attack(
    Actor* attacker,
    const CombatProfile* attack_profile,
    Actor* defender,
    const CombatProfile* defense_profile
);

#endif