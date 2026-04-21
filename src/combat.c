#include "combat.h"
#include "player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "item.h"

/*
 * Purpose:
 *   Implements melee combat math, attack resolution, and weapon-skill progression.
 *
 * Functions:
 *   - clamp_int / weapon_skill_xp_required: local math helpers.
 *   - combat_unarmed_profile / combat_profile_from_item: profile builders.
 *   - combat_hit/crit/parry/attack/apply helpers: core combat calculations.
 *   - weapon_skill_* and actor_get/gain functions: progression and naming APIs.
 *   - combat_profile_for_* / combat_summary_for_character: combat state views.
 *   - combat_resolve_melee_attack: full melee exchange resolver.
 */

#define BASE_HIT_CHANCE 52
#define HIT_SKILL_STEP 4
#define MIN_HIT_CHANCE 15
#define MAX_HIT_CHANCE 95

#define BASE_CRIT_CHANCE 3
#define CRIT_SKILL_STEP 1
#define MAX_CRIT_CHANCE 60

#define PARRY_SKILL_STEP 2
#define MAX_PARRY_CHANCE 70

#define DEFAULT_UNARMED_POWER 1
#define MAX_WEAPON_SKILL_LEVEL 99
#define MAX_NON_WEAPON_SKILL_LEVEL 99
#define BASE_ATTACK_STAMINA_COST 2

#define WEAPON_SKILL_XP_MISS 1
#define WEAPON_SKILL_XP_NO_DAMAGE_HIT 2
#define WEAPON_SKILL_XP_DAMAGE_HIT 3
#define WEAPON_SKILL_XP_CRITICAL_HIT 5
#define WEAPON_SKILL_XP_SUCCESSFUL_PARRY 5

#define TWO_HAND_DAMAGE_MULTIPLIER_NUM 3
#define TWO_HAND_DAMAGE_MULTIPLIER_DEN 2

/**
 * @brief Convert an AttackMode enum value to its corresponding bit flag.
 * @param mode The AttackMode to convert (STAB, CUT, SMASH, PUNCH, KICK).
 * @return The equivalent AttackModeFlag, or ATTACK_MODE_FLAG_NONE if invalid.
 * @note Used to check if a weapon supports a requested attack technique.
 */
static int attack_mode_to_flag(AttackMode mode)
{
    switch(mode)
    {
        case ATTACK_MODE_PUNCH: return ATTACK_MODE_FLAG_PUNCH;
        case ATTACK_MODE_KICK: return ATTACK_MODE_FLAG_KICK;
        case ATTACK_MODE_STAB: return ATTACK_MODE_FLAG_STAB;
        case ATTACK_MODE_CUT: return ATTACK_MODE_FLAG_CUT;
        case ATTACK_MODE_SMASH: return ATTACK_MODE_FLAG_SMASH;
        case ATTACK_MODE_THRUST: return ATTACK_MODE_FLAG_THRUST;
        case ATTACK_MODE_SLASH: return ATTACK_MODE_FLAG_SLASH;
        case ATTACK_MODE_BASH: return ATTACK_MODE_FLAG_BASH;
        case ATTACK_MODE_SHOT: return ATTACK_MODE_FLAG_SHOT;
        case ATTACK_MODE_AIMED_SHOT: return ATTACK_MODE_FLAG_AIMED_SHOT;
        case ATTACK_MODE_HAYMAKER: return ATTACK_MODE_FLAG_HAYMAKER;
        case ATTACK_MODE_FEINT: return ATTACK_MODE_FLAG_FEINT;
        case ATTACK_MODE_LUNGE: return ATTACK_MODE_FLAG_LUNGE;
        case ATTACK_MODE_CLEAVE: return ATTACK_MODE_FLAG_CLEAVE;
        case ATTACK_MODE_SHATTER: return ATTACK_MODE_FLAG_SHATTER;
        case ATTACK_MODE_IMPALE: return ATTACK_MODE_FLAG_IMPALE;
        case ATTACK_MODE_SWEEP: return ATTACK_MODE_FLAG_SWEEP;
        case ATTACK_MODE_HOOK: return ATTACK_MODE_FLAG_HOOK;
        case ATTACK_MODE_VOLLEY: return ATTACK_MODE_FLAG_VOLLEY;
        case ATTACK_MODE_PIN_SHOT: return ATTACK_MODE_FLAG_PIN_SHOT;
        case ATTACK_MODE_DEADEYE: return ATTACK_MODE_FLAG_DEADEYE;
        default: return ATTACK_MODE_FLAG_NONE;
    }
}

/**
 * @brief Get the default/primary damage type for a given attack mode.
 * @param mode The AttackMode (STAB, CUT, SMASH, PUNCH, KICK, etc.).
 * @return A DamageType value (PIERCING, SLASHING, CRUSHING, RANGED, or NONE).
 */
static int attack_mode_default_damage_type(AttackMode mode)
{
    switch(mode)
    {
        case ATTACK_MODE_STAB:
        case ATTACK_MODE_THRUST:
        case ATTACK_MODE_FEINT:
        case ATTACK_MODE_LUNGE:
        case ATTACK_MODE_IMPALE:
            return DAMAGE_TYPE_PIERCING;
        case ATTACK_MODE_CUT:
        case ATTACK_MODE_SLASH:
        case ATTACK_MODE_CLEAVE:
        case ATTACK_MODE_HOOK:
            return DAMAGE_TYPE_SLASHING;
        case ATTACK_MODE_SMASH:
        case ATTACK_MODE_BASH:
        case ATTACK_MODE_PUNCH:
        case ATTACK_MODE_KICK:
        case ATTACK_MODE_HAYMAKER:
        case ATTACK_MODE_SHATTER:
        case ATTACK_MODE_SWEEP:
            return DAMAGE_TYPE_CRUSHING;
        case ATTACK_MODE_SHOT:
        case ATTACK_MODE_AIMED_SHOT:
        case ATTACK_MODE_VOLLEY:
        case ATTACK_MODE_PIN_SHOT:
        case ATTACK_MODE_DEADEYE:
            return DAMAGE_TYPE_RANGED;
        default:
            return DAMAGE_TYPE_NONE;
    }
}

/**
 * @brief Extract the primary damage type from a damage type bitmask.
 * @param damage_type_mask Bitmask of DamageType flags.
 * @return The first set damage type in priority order: PIERCING, SLASHING, CRUSHING, or NONE.
 */
static int damage_type_primary_from_mask(int damage_type_mask)
{
    if(damage_type_mask & DAMAGE_TYPE_PIERCING) return DAMAGE_TYPE_PIERCING;
    if(damage_type_mask & DAMAGE_TYPE_SLASHING) return DAMAGE_TYPE_SLASHING;
    if(damage_type_mask & DAMAGE_TYPE_CRUSHING) return DAMAGE_TYPE_CRUSHING;
    if(damage_type_mask & DAMAGE_TYPE_RANGED) return DAMAGE_TYPE_RANGED;
    return DAMAGE_TYPE_NONE;
}

static WeaponSkillType weapon_skill_for_grip(WeaponSkillType skill_type, int is_two_hand_mode)
{
    switch(skill_type)
    {
        case WEAPON_SKILL_SWORD:
        case WEAPON_SKILL_SWORD_2H:
            return is_two_hand_mode ? WEAPON_SKILL_SWORD_2H : WEAPON_SKILL_SWORD;
        case WEAPON_SKILL_AXE:
        case WEAPON_SKILL_AXE_2H:
            return is_two_hand_mode ? WEAPON_SKILL_AXE_2H : WEAPON_SKILL_AXE;
        case WEAPON_SKILL_MACE:
        case WEAPON_SKILL_MACE_2H:
            return is_two_hand_mode ? WEAPON_SKILL_MACE_2H : WEAPON_SKILL_MACE;
        case WEAPON_SKILL_SPEAR:
        case WEAPON_SKILL_SPEAR_2H:
            return is_two_hand_mode ? WEAPON_SKILL_SPEAR_2H : WEAPON_SKILL_SPEAR;
        default:
            return skill_type;
    }
}

/**
 * @brief Get the default damage type mask for a weapon skill category.
 * @param skill_type The WeaponSkillType (SWORD, AXE, DAGGER, etc.).
 * @return A bitmask of supported DamageType flags for that weapon family.
 */
static int default_damage_mask_for_skill(WeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case WEAPON_SKILL_DAGGER:
        case WEAPON_SKILL_SPEAR:
        case WEAPON_SKILL_SPEAR_2H:
            return DAMAGE_TYPE_PIERCING;
        case WEAPON_SKILL_SWORD:
        case WEAPON_SKILL_SWORD_2H:
            return DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_SLASHING;
        case WEAPON_SKILL_AXE:
        case WEAPON_SKILL_AXE_2H:
            return DAMAGE_TYPE_SLASHING;
        case WEAPON_SKILL_MACE:
        case WEAPON_SKILL_MACE_2H:
            return DAMAGE_TYPE_CRUSHING;
        case WEAPON_SKILL_STAFF:
            return DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_CRUSHING;
        case WEAPON_SKILL_POLEARM:
            return DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_SLASHING;
        case WEAPON_SKILL_THROWN:
            return DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_SLASHING | DAMAGE_TYPE_CRUSHING;
        case WEAPON_SKILL_BOW:
        case WEAPON_SKILL_CROSSBOW:
            return DAMAGE_TYPE_RANGED;
        case WEAPON_SKILL_UNARMED:
        default:
            return DAMAGE_TYPE_CRUSHING;
    }
}

static AttackMode special_attack_mode_for_skill(WeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case WEAPON_SKILL_UNARMED: return ATTACK_MODE_HAYMAKER;
        case WEAPON_SKILL_DAGGER: return ATTACK_MODE_FEINT;
        case WEAPON_SKILL_SWORD:
        case WEAPON_SKILL_SWORD_2H:
            return ATTACK_MODE_LUNGE;
        case WEAPON_SKILL_AXE:
        case WEAPON_SKILL_AXE_2H:
            return ATTACK_MODE_CLEAVE;
        case WEAPON_SKILL_MACE:
        case WEAPON_SKILL_MACE_2H:
            return ATTACK_MODE_SHATTER;
        case WEAPON_SKILL_SPEAR:
        case WEAPON_SKILL_SPEAR_2H:
            return ATTACK_MODE_IMPALE;
        case WEAPON_SKILL_STAFF: return ATTACK_MODE_SWEEP;
        case WEAPON_SKILL_POLEARM: return ATTACK_MODE_HOOK;
        case WEAPON_SKILL_THROWN: return ATTACK_MODE_VOLLEY;
        case WEAPON_SKILL_BOW: return ATTACK_MODE_PIN_SHOT;
        case WEAPON_SKILL_CROSSBOW: return ATTACK_MODE_DEADEYE;
        default: return ATTACK_MODE_NONE;
    }
}

/**
 * @brief Get the default attack mode mask for a weapon skill category.
 * @param skill_type The WeaponSkillType (SWORD, AXE, DAGGER, etc.).
 * @return A bitmask of supported AttackModeFlag values for that weapon family.
 */
static int default_attack_mode_mask_for_skill(WeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case WEAPON_SKILL_DAGGER:
            return ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_THRUST | ATTACK_MODE_FLAG_FEINT;
        case WEAPON_SKILL_SWORD:
        case WEAPON_SKILL_SWORD_2H:
            return ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_THRUST | ATTACK_MODE_FLAG_CUT | ATTACK_MODE_FLAG_SLASH | ATTACK_MODE_FLAG_LUNGE;
        case WEAPON_SKILL_AXE:
        case WEAPON_SKILL_AXE_2H:
            return ATTACK_MODE_FLAG_CUT | ATTACK_MODE_FLAG_SLASH | ATTACK_MODE_FLAG_CLEAVE;
        case WEAPON_SKILL_MACE:
        case WEAPON_SKILL_MACE_2H:
            return ATTACK_MODE_FLAG_SMASH | ATTACK_MODE_FLAG_BASH | ATTACK_MODE_FLAG_SHATTER;
        case WEAPON_SKILL_SPEAR:
        case WEAPON_SKILL_SPEAR_2H:
            return ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_THRUST | ATTACK_MODE_FLAG_IMPALE;
        case WEAPON_SKILL_STAFF:
            return ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_THRUST | ATTACK_MODE_FLAG_SMASH | ATTACK_MODE_FLAG_BASH | ATTACK_MODE_FLAG_SWEEP;
        case WEAPON_SKILL_POLEARM:
            return ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_THRUST | ATTACK_MODE_FLAG_CUT | ATTACK_MODE_FLAG_SLASH | ATTACK_MODE_FLAG_HOOK;
        case WEAPON_SKILL_THROWN:
            return ATTACK_MODE_FLAG_SHOT | ATTACK_MODE_FLAG_AIMED_SHOT | ATTACK_MODE_FLAG_VOLLEY;
        case WEAPON_SKILL_BOW:
            return ATTACK_MODE_FLAG_SHOT | ATTACK_MODE_FLAG_AIMED_SHOT | ATTACK_MODE_FLAG_PIN_SHOT;
        case WEAPON_SKILL_CROSSBOW:
            return ATTACK_MODE_FLAG_SHOT | ATTACK_MODE_FLAG_AIMED_SHOT | ATTACK_MODE_FLAG_DEADEYE;
        case WEAPON_SKILL_UNARMED:
        default:
            return ATTACK_MODE_FLAG_PUNCH | ATTACK_MODE_FLAG_KICK | ATTACK_MODE_FLAG_HAYMAKER;
    }
}

static int attack_pool_mask_from_damage_types(int damage_type_mask, int is_armed, int is_ranged_weapon)
{
    int attack_pool_mask = ATTACK_MODE_FLAG_NONE;

    if(!is_armed)
        return ATTACK_MODE_FLAG_PUNCH | ATTACK_MODE_FLAG_KICK;

    if(damage_type_mask & DAMAGE_TYPE_PIERCING)
        attack_pool_mask |= ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_THRUST;
    if(damage_type_mask & DAMAGE_TYPE_SLASHING)
        attack_pool_mask |= ATTACK_MODE_FLAG_CUT | ATTACK_MODE_FLAG_SLASH;
    if(damage_type_mask & DAMAGE_TYPE_CRUSHING)
        attack_pool_mask |= ATTACK_MODE_FLAG_SMASH | ATTACK_MODE_FLAG_BASH;
    if(is_ranged_weapon || (damage_type_mask & DAMAGE_TYPE_RANGED))
        attack_pool_mask |= ATTACK_MODE_FLAG_SHOT | ATTACK_MODE_FLAG_AIMED_SHOT;

    return attack_pool_mask;
}

static int damage_type_mask_from_attack_mode_mask(int attack_mode_mask)
{
    int damage_mask = DAMAGE_TYPE_NONE;

    if(attack_mode_mask & (ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_THRUST | ATTACK_MODE_FLAG_FEINT | ATTACK_MODE_FLAG_LUNGE | ATTACK_MODE_FLAG_IMPALE))
        damage_mask |= DAMAGE_TYPE_PIERCING;
    if(attack_mode_mask & (ATTACK_MODE_FLAG_CUT | ATTACK_MODE_FLAG_SLASH | ATTACK_MODE_FLAG_CLEAVE | ATTACK_MODE_FLAG_HOOK))
        damage_mask |= DAMAGE_TYPE_SLASHING;
    if(attack_mode_mask & (ATTACK_MODE_FLAG_PUNCH | ATTACK_MODE_FLAG_KICK | ATTACK_MODE_FLAG_SMASH | ATTACK_MODE_FLAG_BASH | ATTACK_MODE_FLAG_HAYMAKER | ATTACK_MODE_FLAG_SHATTER | ATTACK_MODE_FLAG_SWEEP))
        damage_mask |= DAMAGE_TYPE_CRUSHING;
    if(attack_mode_mask & (ATTACK_MODE_FLAG_SHOT | ATTACK_MODE_FLAG_AIMED_SHOT | ATTACK_MODE_FLAG_VOLLEY | ATTACK_MODE_FLAG_PIN_SHOT | ATTACK_MODE_FLAG_DEADEYE))
        damage_mask |= DAMAGE_TYPE_RANGED;

    return damage_mask;
}

typedef struct AttackUnlockRule {
    AttackMode mode;
    int required_damage_type;
    WeaponSkillType required_skill_type;
    int minimum_skill_level;
    int requires_armed;
    int requires_unarmed;
} AttackUnlockRule;

static const AttackUnlockRule attack_unlock_rules[] = {
    { ATTACK_MODE_PUNCH, DAMAGE_TYPE_NONE, WEAPON_SKILL_UNARMED, 0, 0, 1 },
    { ATTACK_MODE_KICK, DAMAGE_TYPE_NONE, WEAPON_SKILL_UNARMED, 0, 0, 1 },
    { ATTACK_MODE_HAYMAKER, DAMAGE_TYPE_NONE, WEAPON_SKILL_UNARMED, 6, 0, 1 },

    { ATTACK_MODE_STAB, DAMAGE_TYPE_PIERCING, WEAPON_SKILL_COUNT, 0, 1, 0 },
    { ATTACK_MODE_THRUST, DAMAGE_TYPE_PIERCING, WEAPON_SKILL_COUNT, 3, 1, 0 },
    { ATTACK_MODE_FEINT, DAMAGE_TYPE_NONE, WEAPON_SKILL_DAGGER, 6, 1, 0 },
    { ATTACK_MODE_LUNGE, DAMAGE_TYPE_NONE, WEAPON_SKILL_SWORD, 6, 1, 0 },
    { ATTACK_MODE_LUNGE, DAMAGE_TYPE_NONE, WEAPON_SKILL_SWORD_2H, 6, 1, 0 },
    { ATTACK_MODE_IMPALE, DAMAGE_TYPE_NONE, WEAPON_SKILL_SPEAR, 6, 1, 0 },
    { ATTACK_MODE_IMPALE, DAMAGE_TYPE_NONE, WEAPON_SKILL_SPEAR_2H, 6, 1, 0 },

    { ATTACK_MODE_CUT, DAMAGE_TYPE_SLASHING, WEAPON_SKILL_COUNT, 0, 1, 0 },
    { ATTACK_MODE_SLASH, DAMAGE_TYPE_SLASHING, WEAPON_SKILL_COUNT, 3, 1, 0 },
    { ATTACK_MODE_CLEAVE, DAMAGE_TYPE_NONE, WEAPON_SKILL_AXE, 6, 1, 0 },
    { ATTACK_MODE_CLEAVE, DAMAGE_TYPE_NONE, WEAPON_SKILL_AXE_2H, 6, 1, 0 },
    { ATTACK_MODE_HOOK, DAMAGE_TYPE_NONE, WEAPON_SKILL_POLEARM, 6, 1, 0 },

    { ATTACK_MODE_SMASH, DAMAGE_TYPE_CRUSHING, WEAPON_SKILL_COUNT, 0, 1, 0 },
    { ATTACK_MODE_BASH, DAMAGE_TYPE_CRUSHING, WEAPON_SKILL_COUNT, 3, 1, 0 },
    { ATTACK_MODE_SHATTER, DAMAGE_TYPE_NONE, WEAPON_SKILL_MACE, 6, 1, 0 },
    { ATTACK_MODE_SHATTER, DAMAGE_TYPE_NONE, WEAPON_SKILL_MACE_2H, 6, 1, 0 },
    { ATTACK_MODE_SWEEP, DAMAGE_TYPE_NONE, WEAPON_SKILL_STAFF, 6, 1, 0 },

    { ATTACK_MODE_SHOT, DAMAGE_TYPE_NONE, WEAPON_SKILL_COUNT, 0, 1, 0 },
    { ATTACK_MODE_AIMED_SHOT, DAMAGE_TYPE_NONE, WEAPON_SKILL_COUNT, 3, 1, 0 },
    { ATTACK_MODE_VOLLEY, DAMAGE_TYPE_NONE, WEAPON_SKILL_THROWN, 6, 1, 0 },
    { ATTACK_MODE_PIN_SHOT, DAMAGE_TYPE_NONE, WEAPON_SKILL_BOW, 6, 1, 0 },
    { ATTACK_MODE_DEADEYE, DAMAGE_TYPE_NONE, WEAPON_SKILL_CROSSBOW, 6, 1, 0 },
};

static void combat_profile_resolve_attack_modes(CombatProfile* profile, const Actor* actor)
{
    int configured_pool_mask;
    int restricted_damage_mask = DAMAGE_TYPE_NONE;
    int effective_damage_mask;
    int derived_pool_mask;
    int special_flag;
    int unlocked_mask = ATTACK_MODE_FLAG_NONE;
    int skill_level = 0;
    AttackMode next_unlock_mode = ATTACK_MODE_NONE;
    int next_unlock_skill_level = 0;

    if(!profile)
        return;

    if(profile->damage_type_mask == DAMAGE_TYPE_NONE)
        profile->damage_type_mask = default_damage_mask_for_skill(profile->skill_type);

    configured_pool_mask = profile->attack_pool_mask;
    effective_damage_mask = profile->damage_type_mask;

    if(configured_pool_mask != ATTACK_MODE_FLAG_NONE)
    {
        restricted_damage_mask = damage_type_mask_from_attack_mode_mask(configured_pool_mask);
        if((restricted_damage_mask != DAMAGE_TYPE_NONE) && (effective_damage_mask & restricted_damage_mask))
            effective_damage_mask &= restricted_damage_mask;
    }

    profile->damage_type_mask = effective_damage_mask;
    derived_pool_mask = attack_pool_mask_from_damage_types(effective_damage_mask,
                                                           profile->is_armed,
                                                           profile->ranged_type != RANGED_WEAPON_NONE);
    special_flag = attack_mode_to_flag(special_attack_mode_for_skill(profile->skill_type));
    if(special_flag != ATTACK_MODE_FLAG_NONE)
        derived_pool_mask |= special_flag;

    if(derived_pool_mask != ATTACK_MODE_FLAG_NONE)
        profile->attack_pool_mask = derived_pool_mask;
    else
        profile->attack_pool_mask = default_attack_mode_mask_for_skill(profile->skill_type);

    if(actor)
        skill_level = actor_get_weapon_skill(actor, profile->skill_type);

    for(int i = 0; i < (int)(sizeof(attack_unlock_rules) / sizeof(attack_unlock_rules[0])); i++)
    {
        int flag = attack_mode_to_flag(attack_unlock_rules[i].mode);

        if(flag == ATTACK_MODE_FLAG_NONE || !(profile->attack_pool_mask & flag))
            continue;
        if(attack_unlock_rules[i].requires_armed && !profile->is_armed)
            continue;
        if(attack_unlock_rules[i].requires_unarmed && profile->is_armed)
            continue;
        if((attack_unlock_rules[i].required_skill_type != WEAPON_SKILL_COUNT)
            && (profile->skill_type != attack_unlock_rules[i].required_skill_type))
            continue;
        if(attack_unlock_rules[i].required_damage_type != DAMAGE_TYPE_NONE
            && !(effective_damage_mask & attack_unlock_rules[i].required_damage_type))
            continue;

        if(skill_level >= attack_unlock_rules[i].minimum_skill_level)
        {
            unlocked_mask |= flag;
        }
        else if(next_unlock_mode == ATTACK_MODE_NONE
                || attack_unlock_rules[i].minimum_skill_level < next_unlock_skill_level)
        {
            next_unlock_mode = attack_unlock_rules[i].mode;
            next_unlock_skill_level = attack_unlock_rules[i].minimum_skill_level;
        }
    }

    if(unlocked_mask == ATTACK_MODE_FLAG_NONE && profile->attack_pool_mask != ATTACK_MODE_FLAG_NONE)
    {
        AttackMode fallback_mode = attack_mode_first_from_mask(profile->attack_pool_mask);
        int fallback_flag = attack_mode_to_flag(fallback_mode);
        if(fallback_flag != ATTACK_MODE_FLAG_NONE)
            unlocked_mask = fallback_flag;
    }

    profile->attack_mode_mask = unlocked_mask;
    profile->next_unlock_mode = next_unlock_mode;
    profile->next_unlock_skill_level = next_unlock_skill_level;
}

/**
 * @brief Clamp an integer value into an inclusive range.
 * @param value The value to clamp.
 * @param min_value The minimum allowed value (inclusive).
 * @param max_value The maximum allowed value (inclusive).
 * @return The clamped value.
 */
static int clamp_int(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

static void combat_normalize_damage_range(int* min_value, int* max_value, int fallback)
{
    if(!min_value || !max_value)
        return;

    if(*min_value < 0 && *max_value < 0)
    {
        *min_value = fallback;
        *max_value = fallback;
    }
    else
    {
        if(*min_value < 0)
            *min_value = (*max_value >= 0) ? *max_value : fallback;
        if(*max_value < 0)
            *max_value = (*min_value >= 0) ? *min_value : fallback;
    }

    if(*min_value < 0)
        *min_value = 0;
    if(*max_value < *min_value)
        *max_value = *min_value;
}

static void combat_profile_select_damage_range(CombatProfile* profile)
{
    int min_value;
    int max_value;
    int mode_min = -1;
    int mode_max = -1;
    int fallback;

    if(!profile)
        return;

    fallback = profile->power;
    if(fallback < 0)
        fallback = 0;

    min_value = profile->damage_min;
    max_value = profile->damage_max;
    combat_normalize_damage_range(&min_value, &max_value, fallback);

    switch(profile->attack_mode)
    {
        case ATTACK_MODE_STAB:
        case ATTACK_MODE_THRUST:
        case ATTACK_MODE_FEINT:
        case ATTACK_MODE_LUNGE:
        case ATTACK_MODE_IMPALE:
            mode_min = profile->stab_damage_min;
            mode_max = profile->stab_damage_max;
            break;
        case ATTACK_MODE_CUT:
        case ATTACK_MODE_SLASH:
        case ATTACK_MODE_CLEAVE:
        case ATTACK_MODE_HOOK:
            mode_min = profile->cut_damage_min;
            mode_max = profile->cut_damage_max;
            break;
        case ATTACK_MODE_SMASH:
        case ATTACK_MODE_BASH:
        case ATTACK_MODE_HAYMAKER:
        case ATTACK_MODE_SHATTER:
        case ATTACK_MODE_SWEEP:
            mode_min = profile->smash_damage_min;
            mode_max = profile->smash_damage_max;
            break;
        case ATTACK_MODE_PUNCH:
            mode_min = profile->punch_damage_min;
            mode_max = profile->punch_damage_max;
            break;
        case ATTACK_MODE_KICK:
            mode_min = profile->kick_damage_min;
            mode_max = profile->kick_damage_max;
            break;
        case ATTACK_MODE_SHOT:
        case ATTACK_MODE_AIMED_SHOT:
        case ATTACK_MODE_VOLLEY:
        case ATTACK_MODE_PIN_SHOT:
        case ATTACK_MODE_DEADEYE:
        case ATTACK_MODE_NONE:
        default:
            break;
    }

    if(mode_min >= 0 || mode_max >= 0)
    {
        combat_normalize_damage_range(&mode_min, &mode_max, min_value);
        min_value = mode_min;
        max_value = mode_max;
    }

    profile->damage_min = min_value;
    profile->damage_max = max_value;
}

static int roll_percent(int chance)
{
    if(chance <= 0)
        return 0;
    if(chance >= 100)
        return 1;
    return (rand() % 100) < chance;
}

/**
 * @brief Compute XP threshold required to advance from a given skill level.
 * @param skill_level The current skill level (0-99).
 * @return The XP required to advance to the next level.
 * @note Formula: 8 + (skill_level * 4), so level 0 can progress normally into level 1.
 */
static int weapon_skill_xp_required(int skill_level)
{
    if(skill_level < 0)
        skill_level = 0;
    return 8 + (skill_level * 4);
}

static int non_weapon_skill_xp_required(int skill_level)
{
    if(skill_level < 0)
        skill_level = 0;
    return 100 * (skill_level + 1);
}

int combat_weapon_skill_xp_for_hit(int critical, int no_damage_hit)
{
    if(critical)
        return WEAPON_SKILL_XP_CRITICAL_HIT;
    if(no_damage_hit)
        return WEAPON_SKILL_XP_NO_DAMAGE_HIT;
    return WEAPON_SKILL_XP_DAMAGE_HIT;
}

AttackMode attack_mode_first_from_mask(int attack_mode_mask)
{
    static const AttackMode ordered_modes[] = {
        ATTACK_MODE_PUNCH,
        ATTACK_MODE_KICK,
        ATTACK_MODE_HAYMAKER,
        ATTACK_MODE_STAB,
        ATTACK_MODE_THRUST,
        ATTACK_MODE_FEINT,
        ATTACK_MODE_LUNGE,
        ATTACK_MODE_IMPALE,
        ATTACK_MODE_CUT,
        ATTACK_MODE_SLASH,
        ATTACK_MODE_CLEAVE,
        ATTACK_MODE_HOOK,
        ATTACK_MODE_SMASH,
        ATTACK_MODE_BASH,
        ATTACK_MODE_SHATTER,
        ATTACK_MODE_SWEEP,
        ATTACK_MODE_SHOT,
        ATTACK_MODE_AIMED_SHOT,
        ATTACK_MODE_VOLLEY,
        ATTACK_MODE_PIN_SHOT,
        ATTACK_MODE_DEADEYE,
    };

    for(int i = 0; i < (int)(sizeof(ordered_modes) / sizeof(ordered_modes[0])); i++)
    {
        int flag = attack_mode_to_flag(ordered_modes[i]);
        if(flag != ATTACK_MODE_FLAG_NONE && (attack_mode_mask & flag))
            return ordered_modes[i];
    }

    return ATTACK_MODE_NONE;
}

AttackMode attack_mode_next_from_mask(int attack_mode_mask, AttackMode current_mode)
{
    static const AttackMode ordered_modes[] = {
        ATTACK_MODE_PUNCH,
        ATTACK_MODE_KICK,
        ATTACK_MODE_HAYMAKER,
        ATTACK_MODE_STAB,
        ATTACK_MODE_THRUST,
        ATTACK_MODE_FEINT,
        ATTACK_MODE_LUNGE,
        ATTACK_MODE_IMPALE,
        ATTACK_MODE_CUT,
        ATTACK_MODE_SLASH,
        ATTACK_MODE_CLEAVE,
        ATTACK_MODE_HOOK,
        ATTACK_MODE_SMASH,
        ATTACK_MODE_BASH,
        ATTACK_MODE_SHATTER,
        ATTACK_MODE_SWEEP,
        ATTACK_MODE_SHOT,
        ATTACK_MODE_AIMED_SHOT,
        ATTACK_MODE_VOLLEY,
        ATTACK_MODE_PIN_SHOT,
        ATTACK_MODE_DEADEYE,
    };
    int start_index = -1;
    int mode_count = (int)(sizeof(ordered_modes) / sizeof(ordered_modes[0]));

    if(attack_mode_mask == ATTACK_MODE_FLAG_NONE)
        return ATTACK_MODE_NONE;

    for(int i = 0; i < mode_count; i++)
    {
        if(ordered_modes[i] == current_mode)
        {
            start_index = i;
            break;
        }
    }

    for(int step = 1; step <= mode_count; step++)
    {
        int idx = (start_index + step + mode_count) % mode_count;
        int flag = attack_mode_to_flag(ordered_modes[idx]);
        if(flag != ATTACK_MODE_FLAG_NONE && (attack_mode_mask & flag))
            return ordered_modes[idx];
    }

    return attack_mode_first_from_mask(attack_mode_mask);
}

static void combat_profile_apply_mode(CombatProfile* profile, const Actor* actor, AttackMode requested_mode)
{
    AttackMode selected_mode;
    int damage_type;

    if(!profile)
        return;

    profile->skill_type = weapon_skill_for_grip(profile->skill_type, profile->is_two_hand_mode);

    if(profile->damage_type_mask == DAMAGE_TYPE_NONE)
        profile->damage_type_mask = default_damage_mask_for_skill(profile->skill_type);

    if(profile->can_toggle_grip)
    {
        int grip_pool_mask = (profile->is_two_hand_mode && profile->two_hand_attack_mode_mask != ATTACK_MODE_FLAG_NONE)
            ? profile->two_hand_attack_mode_mask
            : profile->one_hand_attack_mode_mask;

        if(grip_pool_mask != ATTACK_MODE_FLAG_NONE)
        {
            profile->attack_pool_mask = grip_pool_mask;
            profile->attack_mode_mask = grip_pool_mask;
        }
    }

    combat_profile_resolve_attack_modes(profile, actor);

    selected_mode = attack_mode_first_from_mask(profile->attack_mode_mask);
    if(requested_mode != ATTACK_MODE_NONE)
    {
        int requested_flag = attack_mode_to_flag(requested_mode);
        if((requested_flag != ATTACK_MODE_FLAG_NONE) && (profile->attack_mode_mask & requested_flag))
            selected_mode = requested_mode;
    }

    damage_type = attack_mode_default_damage_type(selected_mode);
    if((damage_type == DAMAGE_TYPE_NONE) || !(profile->damage_type_mask & damage_type))
        damage_type = damage_type_primary_from_mask(profile->damage_type_mask);

    profile->attack_mode = selected_mode;
    profile->active_damage_type = damage_type;
    combat_profile_select_damage_range(profile);
}

// Build default unarmed combat profile.
static CombatProfile combat_unarmed_profile(void)
{
    CombatProfile profile;

    memset(&profile, 0, sizeof(profile));
    profile.skill_type = WEAPON_SKILL_UNARMED;
    strncpy(profile.weapon_name, "Fists", sizeof(profile.weapon_name) - 1);
    profile.power = DEFAULT_UNARMED_POWER;
    profile.damage_min = DEFAULT_UNARMED_POWER;
    profile.damage_max = DEFAULT_UNARMED_POWER;
    profile.stab_damage_min = -1;
    profile.stab_damage_max = -1;
    profile.cut_damage_min = -1;
    profile.cut_damage_max = -1;
    profile.smash_damage_min = 1;
    profile.smash_damage_max = 3;
    profile.punch_damage_min = 0;
    profile.punch_damage_max = 1;
    profile.kick_damage_min = 1;
    profile.kick_damage_max = 2;
    profile.damage_type_mask = DAMAGE_TYPE_CRUSHING;
    profile.attack_mode_mask = ATTACK_MODE_FLAG_PUNCH | ATTACK_MODE_FLAG_KICK;
    profile.attack_mode = ATTACK_MODE_PUNCH;
    profile.active_damage_type = DAMAGE_TYPE_CRUSHING;
    profile.is_armed = 0;
    return profile;
}

static int combat_is_baseline_unarmed_mode(AttackMode mode)
{
    return mode == ATTACK_MODE_PUNCH || mode == ATTACK_MODE_KICK;
}

// Build combat profile from equipped item, falling back to unarmed.
static CombatProfile combat_profile_from_item(const Item* item)
{
    CombatProfile profile;

    if(!item || (!item_is_weapon(item) && !item_is_ranged_weapon(item)))
        return combat_unarmed_profile();

    memset(&profile, 0, sizeof(profile));
    profile.skill_type = weapon_skill_for_grip(item->weapon_skill_type, item->type == ITEM_TYPE_WEAPON_TWO_HANDED);
    item_format_display_name(item, profile.weapon_name, sizeof(profile.weapon_name));
    profile.power = item->power > 0 ? item->power : DEFAULT_UNARMED_POWER;
    profile.damage_min = item->damage_min;
    profile.damage_max = item->damage_max;
    profile.stab_damage_min = item->stab_damage_min;
    profile.stab_damage_max = item->stab_damage_max;
    profile.cut_damage_min = item->cut_damage_min;
    profile.cut_damage_max = item->cut_damage_max;
    profile.smash_damage_min = item->smash_damage_min;
    profile.smash_damage_max = item->smash_damage_max;
    profile.punch_damage_min = item->punch_damage_min;
    profile.punch_damage_max = item->punch_damage_max;
    profile.kick_damage_min = item->kick_damage_min;
    profile.kick_damage_max = item->kick_damage_max;
    profile.accuracy_bonus = item->accuracy_bonus;
    profile.crit_bonus = item->crit_bonus;
    profile.parry_bonus = item->parry_bonus;
    profile.block_bonus = item->block_bonus;
    profile.can_parry = item->can_parry;
    profile.damage_type_mask = item->damage_type_mask;
    profile.attack_mode_mask = item->attack_mode_mask;
    profile.attack_pool_mask = item->attack_mode_mask;
    profile.one_hand_attack_mode_mask = item->attack_mode_mask;
    profile.two_hand_attack_mode_mask = item->two_hand_attack_mode_mask;
    profile.is_two_hand_mode = item->type == ITEM_TYPE_WEAPON_TWO_HANDED;
    profile.can_toggle_grip = item->type == ITEM_TYPE_WEAPON_VERSATILE;
    profile.reach_bonus = item->reach_bonus;
    profile.armor_penetration = item->armor_penetration;
    profile.stamina_cost_mod = item->stamina_cost_mod;
    profile.status_bleed_chance = item->status_bleed_chance;
    profile.status_stun_chance = item->status_stun_chance;
    profile.status_slow_chance = item->status_slow_chance;
    profile.ranged_type = item->ranged_type;
    profile.ranged_range = item->ranged_range;
    snprintf(profile.ammo_item_name, sizeof(profile.ammo_item_name), "%s", item->ammo_item_name);
    profile.ammo_per_shot = item->ammo_per_shot;
    profile.is_armed = 1;
    return profile;
}

// Calculate attacker hit chance versus defender dodge.
static int combat_hit_chance(const Actor* attacker, const CombatProfile* attack_profile, const Actor* defender)
{
    int defender_dodge = 0;
    int speed_hit_bonus = actor_speed_hit_bonus(attacker);

    if(defender)
        defender_dodge = defender->dodge + actor_dodge_attribute_bonus(defender);

    return clamp_int(
        BASE_HIT_CHANCE + (actor_get_weapon_skill(attacker, attack_profile->skill_type) * HIT_SKILL_STEP) +
        attack_profile->accuracy_bonus + speed_hit_bonus - defender_dodge,
        MIN_HIT_CHANCE,
        MAX_HIT_CHANCE
    );
}

// Calculate critical-hit chance for current attack profile.
static int combat_crit_chance(const Actor* attacker, const CombatProfile* attack_profile)
{
    return clamp_int(
        BASE_CRIT_CHANCE + (actor_get_weapon_skill(attacker, attack_profile->skill_type) * CRIT_SKILL_STEP) +
        attack_profile->crit_bonus + actor_dexterity_crit_bonus(attacker),
        1,
        MAX_CRIT_CHANCE
    );
}

// Calculate parry chance for defender profile.
static int combat_parry_chance(const Actor* defender, const CombatProfile* defense_profile)
{
    if(!defender || !defense_profile || !defense_profile->can_parry)
        return 0;

    return clamp_int(
        defender->parry + (actor_get_weapon_skill(defender, defense_profile->skill_type) * PARRY_SKILL_STEP) +
        defense_profile->parry_bonus + actor_speed_parry_bonus(defender),
        0,
        MAX_PARRY_CHANCE
    );
}

static int combat_kick_knockdown_chance(const Actor* attacker)
{
    int strength;
    int constitution;
    int chance;

    if(!attacker)
        return 20;

    strength = attacker->strength;
    constitution = attacker->constitution;
    chance = 20
        + ((strength - 20) / 2)
        + ((constitution - 20) / 4);

    return clamp_int(chance, 5, 60);
}

// Compute effective raw attack-value range before mitigation.
void combat_attack_value_range(const Actor* attacker, const CombatProfile* attack_profile, int* out_min_value, int* out_max_value)
{
    int skill_bonus;
    int strength_bonus;
    int min_value;
    int max_value;
    int fallback;

    if(out_min_value)
        *out_min_value = 1;
    if(out_max_value)
        *out_max_value = 1;
    if(!attack_profile)
        return;

    fallback = attack_profile->power;
    if(fallback < 0)
        fallback = 0;

    min_value = attack_profile->damage_min;
    max_value = attack_profile->damage_max;
    combat_normalize_damage_range(&min_value, &max_value, fallback);

    skill_bonus = attacker ? (actor_get_weapon_skill(attacker, attack_profile->skill_type) / 2) : 0;
    strength_bonus = attacker ? actor_strength_melee_bonus(attacker) : 0;
    min_value = 1 + min_value + skill_bonus + strength_bonus;
    max_value = 1 + max_value + skill_bonus + strength_bonus;

    if(attack_profile->skill_type == WEAPON_SKILL_UNARMED)
    {
        switch(attack_profile->attack_mode)
        {
            case ATTACK_MODE_PUNCH:
                min_value -= 1;
                max_value -= 1;
                break;
            case ATTACK_MODE_KICK:
                min_value += 1;
                max_value += 1;
                break;
            case ATTACK_MODE_HAYMAKER:
                min_value += 2;
                max_value += 2;
                break;
            default:
                break;
        }
    }

    if(attack_profile->can_toggle_grip && attack_profile->is_two_hand_mode && attack_profile->skill_type != WEAPON_SKILL_UNARMED)
    {
        min_value = (min_value * TWO_HAND_DAMAGE_MULTIPLIER_NUM) / TWO_HAND_DAMAGE_MULTIPLIER_DEN;
        max_value = (max_value * TWO_HAND_DAMAGE_MULTIPLIER_NUM) / TWO_HAND_DAMAGE_MULTIPLIER_DEN;
    }

    if(min_value < 1)
        min_value = 1;
    if(max_value < min_value)
        max_value = min_value;

    if(out_min_value)
        *out_min_value = min_value;
    if(out_max_value)
        *out_max_value = max_value;
}

int combat_attack_value(const Actor* attacker, const CombatProfile* attack_profile)
{
    int min_value;
    int max_value;

    combat_attack_value_range(attacker, attack_profile, &min_value, &max_value);
    return (min_value + max_value) / 2;
}

int combat_roll_attack_value(const Actor* attacker, const CombatProfile* attack_profile)
{
    int min_value;
    int max_value;

    combat_attack_value_range(attacker, attack_profile, &min_value, &max_value);
    if(max_value <= min_value)
        return min_value;
    return min_value + (rand() % (max_value - min_value + 1));
}

// Apply final damage to defender after armor and return dealt damage.
static int combat_apply_damage(Actor* defender,
                               int attack_value,
                               int armor_penetration,
                               int* out_armor_absorbed,
                               int* out_stamina_damage)
{
    int damage;
    int converted_to_stamina;
    int effective_armor;
    int hard_dr;
    int soft_dr;
    int max_stamina_absorb;
    int stamina_floor;

    if(out_armor_absorbed)
        *out_armor_absorbed = 0;
    if(out_stamina_damage)
        *out_stamina_damage = 0;
    if(!defender)
        return 0;

    hard_dr = defender->hard_damage_reduction;
    if(hard_dr < 0)
        hard_dr = 0;

    effective_armor = hard_dr - armor_penetration;
    if(effective_armor < 0)
        effective_armor = 0;

    damage = attack_value - effective_armor;
    if(damage < 0)
        damage = 0;

    if(out_armor_absorbed)
    {
        int absorbed = attack_value - damage;
        if(absorbed < 0)
            absorbed = 0;
        *out_armor_absorbed = absorbed;
    }

    soft_dr = defender->soft_damage_reduction;

    converted_to_stamina = 0;
    if(soft_dr > 0 && damage > 0)
    {
        stamina_floor = actor_stamina_floor(defender);
        max_stamina_absorb = defender->stamina - stamina_floor;
        if(max_stamina_absorb < 0)
            max_stamina_absorb = 0;

        converted_to_stamina = damage;
        if(converted_to_stamina > soft_dr)
            converted_to_stamina = soft_dr;
        if(converted_to_stamina > max_stamina_absorb)
            converted_to_stamina = max_stamina_absorb;

        if(converted_to_stamina > 0)
        {
            if(defender == &player.character.actor && defender->stamina >= 0)
            {
                int below_zero = converted_to_stamina - defender->stamina;
                if(below_zero > 0)
                {
                    int prevented = below_zero;
                    if(prevented > defender->willpower)
                        prevented = defender->willpower;
                    converted_to_stamina -= prevented;
                    defender->willpower -= prevented;
                }
            }

            defender->stamina -= converted_to_stamina;
            defender->stamina = actor_clamp_stamina_value(defender, defender->stamina);
            damage -= converted_to_stamina;
        }
    }

    if(out_stamina_damage)
        *out_stamina_damage = converted_to_stamina;

    if(defender == &player.character.actor && damage > 0 && defender->health > 0 && defender->health - damage < 0)
    {
        int below_zero = damage - defender->health;
        int max_preventable = defender->willpower / 2;
        if(max_preventable > 0)
        {
            int prevented = below_zero;
            if(prevented > max_preventable)
                prevented = max_preventable;
            damage -= prevented;
            defender->willpower -= prevented * 2;
        }
    }

    if(damage > 0)
    {
        defender->health -= damage;
        if(defender->health < 0)
            defender->health = 0;
    }
    return damage;
}

static int combat_apply_stamina_only_damage(Actor* defender,
                                            int attack_value,
                                            int armor_penetration,
                                            int* out_armor_absorbed,
                                            int* out_stamina_damage)
{
    int damage;
    int effective_armor;
    int hard_dr;

    if(out_armor_absorbed)
        *out_armor_absorbed = 0;
    if(out_stamina_damage)
        *out_stamina_damage = 0;
    if(!defender)
        return 0;

    hard_dr = defender->hard_damage_reduction;
    if(hard_dr < 0)
        hard_dr = 0;

    effective_armor = hard_dr - armor_penetration;
    if(effective_armor < 0)
        effective_armor = 0;

    damage = attack_value - effective_armor;
    if(damage < 0)
        damage = 0;

    if(out_armor_absorbed)
    {
        int absorbed = attack_value - damage;
        if(absorbed < 0)
            absorbed = 0;
        *out_armor_absorbed = absorbed;
    }

    if(damage > 0)
    {
        int stamina_damage = damage;
        int stamina_floor = actor_stamina_floor(defender);
        int max_stamina_damage = defender->stamina - stamina_floor;

        if(max_stamina_damage < 0)
            max_stamina_damage = 0;
        if(stamina_damage > max_stamina_damage)
            stamina_damage = max_stamina_damage;

        if(defender == &player.character.actor && defender->stamina >= 0)
        {
            int below_zero = stamina_damage - defender->stamina;
            if(below_zero > 0)
            {
                int prevented = below_zero;
                if(prevented > defender->willpower)
                    prevented = defender->willpower;
                stamina_damage -= prevented;
                defender->willpower -= prevented;
            }
        }

        if(stamina_damage > 0)
        {
            defender->stamina -= stamina_damage;
            defender->stamina = actor_clamp_stamina_value(defender, defender->stamina);
            if(out_stamina_damage)
                *out_stamina_damage = stamina_damage;
        }
    }

    return 0;
}

int combat_profile_melee_range(const CombatProfile* profile)
{
    if(!profile)
        return 1;
    return 1 + ((profile->reach_bonus > 0) ? profile->reach_bonus : 0);
}

int combat_profile_is_ranged(const CombatProfile* profile)
{
    return profile && profile->ranged_type != RANGED_WEAPON_NONE;
}

int combat_profile_ranged_range(const CombatProfile* profile)
{
    if(!profile || profile->ranged_range <= 0)
        return 0;
    return profile->ranged_range;
}

int combat_profile_attack_stamina_cost(const CombatProfile* profile)
{
    int cost = BASE_ATTACK_STAMINA_COST;

    if(profile)
        cost += profile->stamina_cost_mod;
    if(cost < 0)
        cost = 0;
    return cost;
}

int combat_profile_attack_action_point_cost(const CombatProfile* profile)
{
    int cost = 2;

    if(!profile)
        return cost;

    switch(profile->attack_mode)
    {
        case ATTACK_MODE_PUNCH:
            cost = 1;
            break;
        case ATTACK_MODE_KICK:
            cost = 2;
            break;
        case ATTACK_MODE_STAB:
        case ATTACK_MODE_CUT:
        case ATTACK_MODE_SMASH:
        case ATTACK_MODE_SHOT:
            cost = 2;
            break;
        case ATTACK_MODE_BASH:
        case ATTACK_MODE_THRUST:
        case ATTACK_MODE_SLASH:
        case ATTACK_MODE_AIMED_SHOT:
        case ATTACK_MODE_FEINT:
        case ATTACK_MODE_LUNGE:
        case ATTACK_MODE_HOOK:
        case ATTACK_MODE_SWEEP:
        case ATTACK_MODE_VOLLEY:
        case ATTACK_MODE_PIN_SHOT:
            cost = 3;
            break;
        case ATTACK_MODE_HAYMAKER:
        case ATTACK_MODE_CLEAVE:
        case ATTACK_MODE_SHATTER:
        case ATTACK_MODE_IMPALE:
        case ATTACK_MODE_DEADEYE:
            cost = 4;
            break;
        case ATTACK_MODE_NONE:
        default:
            cost = combat_profile_is_ranged(profile) ? 3 : 2;
            break;
    }

    if(profile->stamina_cost_mod >= 2)
        cost += 1;
    else if(profile->stamina_cost_mod <= -2)
        cost -= 1;

    if(profile->skill_type == WEAPON_SKILL_DAGGER)
        cost -= 1;

    if(profile->is_two_hand_mode)
        cost += 1;

    return clamp_int(cost, 1, 6);
}

// Return full name for a weapon-skill type.
const char* weapon_skill_name(WeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case WEAPON_SKILL_UNARMED: return "Unarmed";
        case WEAPON_SKILL_DAGGER: return "Dagger";
        case WEAPON_SKILL_SWORD: return "Sword 1H";
        case WEAPON_SKILL_SWORD_2H: return "Sword 2H";
        case WEAPON_SKILL_AXE: return "Axe 1H";
        case WEAPON_SKILL_AXE_2H: return "Axe 2H";
        case WEAPON_SKILL_MACE: return "Mace 1H";
        case WEAPON_SKILL_MACE_2H: return "Mace 2H";
        case WEAPON_SKILL_SPEAR: return "Spear 1H";
        case WEAPON_SKILL_SPEAR_2H: return "Spear 2H";
        case WEAPON_SKILL_STAFF: return "Staff";
        case WEAPON_SKILL_POLEARM: return "Polearm";
        case WEAPON_SKILL_THROWN: return "Thrown";
        case WEAPON_SKILL_BOW: return "Bow";
        case WEAPON_SKILL_CROSSBOW: return "Crossbow";
        default: return "Unknown";
    }
}

// Return short label for a weapon-skill type.
const char* weapon_skill_short_name(WeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case WEAPON_SKILL_UNARMED: return "Un";
        case WEAPON_SKILL_DAGGER: return "Dag";
        case WEAPON_SKILL_SWORD: return "Sw1";
        case WEAPON_SKILL_SWORD_2H: return "Sw2";
        case WEAPON_SKILL_AXE: return "Ax1";
        case WEAPON_SKILL_AXE_2H: return "Ax2";
        case WEAPON_SKILL_MACE: return "Mc1";
        case WEAPON_SKILL_MACE_2H: return "Mc2";
        case WEAPON_SKILL_SPEAR: return "Sp1";
        case WEAPON_SKILL_SPEAR_2H: return "Sp2";
        case WEAPON_SKILL_STAFF: return "Stf";
        case WEAPON_SKILL_POLEARM: return "Pol";
        case WEAPON_SKILL_THROWN: return "Thr";
        case WEAPON_SKILL_BOW: return "Bow";
        case WEAPON_SKILL_CROSSBOW: return "Xbw";
        default: return "?";
    }
}

const char* non_weapon_skill_name(NonWeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case NON_WEAPON_SKILL_ANIMAL_HANDLING: return "Animal Handling";
        case NON_WEAPON_SKILL_MINING: return "Mining";
        case NON_WEAPON_SKILL_SMELTING: return "Smelting";
        case NON_WEAPON_SKILL_BLACKSMITHING: return "Blacksmithing";
        case NON_WEAPON_SKILL_LUMBERJACKING: return "Lumberjacking";
        case NON_WEAPON_SKILL_CARPENTRY: return "Carpentry";
        case NON_WEAPON_SKILL_COOKING: return "Cooking";
        case NON_WEAPON_SKILL_HERBALISM: return "Herbalism";
        case NON_WEAPON_SKILL_FISHING: return "Fishing";
        case NON_WEAPON_SKILL_ALCHEMY: return "Alchemy";
        case NON_WEAPON_SKILL_COAL_BURNING: return "Coal Burning";
        case NON_WEAPON_SKILL_TAILORING: return "Tailoring";
        case NON_WEAPON_SKILL_LEATHERWORKING: return "Leatherworking";
        case NON_WEAPON_SKILL_SKINNING: return "Skinning";
        case NON_WEAPON_SKILL_TANNING: return "Tanning";
        default: return "Unknown";
    }
}

const char* non_weapon_skill_save_key(NonWeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case NON_WEAPON_SKILL_ANIMAL_HANDLING: return "animal_handling";
        case NON_WEAPON_SKILL_MINING: return "mining";
        case NON_WEAPON_SKILL_SMELTING: return "smelting";
        case NON_WEAPON_SKILL_BLACKSMITHING: return "blacksmithing";
        case NON_WEAPON_SKILL_LUMBERJACKING: return "lumberjacking";
        case NON_WEAPON_SKILL_CARPENTRY: return "carpentry";
        case NON_WEAPON_SKILL_COOKING: return "cooking";
        case NON_WEAPON_SKILL_HERBALISM: return "herbalism";
        case NON_WEAPON_SKILL_FISHING: return "fishing";
        case NON_WEAPON_SKILL_ALCHEMY: return "alchemy";
        case NON_WEAPON_SKILL_COAL_BURNING: return "coal_burning";
        case NON_WEAPON_SKILL_TAILORING: return "tailoring";
        case NON_WEAPON_SKILL_LEATHERWORKING: return "leatherworking";
        case NON_WEAPON_SKILL_SKINNING: return "skinning";
        case NON_WEAPON_SKILL_TANNING: return "tanning";
        default: return "unknown";
    }
}

const char* damage_type_name(int damage_type)
{
    switch(damage_type)
    {
        case DAMAGE_TYPE_PIERCING: return "Piercing";
        case DAMAGE_TYPE_SLASHING: return "Slashing";
        case DAMAGE_TYPE_CRUSHING: return "Crushing";
        case DAMAGE_TYPE_RANGED: return "Ranged";
        default: return "None";
    }
}

const char* attack_mode_name(AttackMode mode)
{
    switch(mode)
    {
        case ATTACK_MODE_PUNCH: return "Punch";
        case ATTACK_MODE_KICK: return "Kick";
        case ATTACK_MODE_STAB: return "Stab";
        case ATTACK_MODE_CUT: return "Cut";
        case ATTACK_MODE_SMASH: return "Smash";
        case ATTACK_MODE_THRUST: return "Thrust";
        case ATTACK_MODE_SLASH: return "Slash";
        case ATTACK_MODE_BASH: return "Bash";
        case ATTACK_MODE_SHOT: return "Shot";
        case ATTACK_MODE_AIMED_SHOT: return "Aimed Shot";
        case ATTACK_MODE_HAYMAKER: return "Haymaker";
        case ATTACK_MODE_FEINT: return "Feint";
        case ATTACK_MODE_LUNGE: return "Lunge";
        case ATTACK_MODE_CLEAVE: return "Cleave";
        case ATTACK_MODE_SHATTER: return "Shatter";
        case ATTACK_MODE_IMPALE: return "Impale";
        case ATTACK_MODE_SWEEP: return "Sweep";
        case ATTACK_MODE_HOOK: return "Hook";
        case ATTACK_MODE_VOLLEY: return "Volley";
        case ATTACK_MODE_PIN_SHOT: return "Pin Shot";
        case ATTACK_MODE_DEADEYE: return "Deadeye";
        default: return "None";
    }
}

const char* attack_mode_verb(AttackMode mode)
{
    switch(mode)
    {
        case ATTACK_MODE_PUNCH: return "punch";
        case ATTACK_MODE_KICK: return "kick";
        case ATTACK_MODE_STAB: return "stab";
        case ATTACK_MODE_CUT: return "cut";
        case ATTACK_MODE_SMASH: return "smash";
        case ATTACK_MODE_THRUST: return "thrust at";
        case ATTACK_MODE_SLASH: return "slash";
        case ATTACK_MODE_BASH: return "bash";
        case ATTACK_MODE_SHOT: return "shoot";
        case ATTACK_MODE_AIMED_SHOT: return "aim at";
        case ATTACK_MODE_HAYMAKER: return "haymaker";
        case ATTACK_MODE_FEINT: return "feint at";
        case ATTACK_MODE_LUNGE: return "lunge at";
        case ATTACK_MODE_CLEAVE: return "cleave";
        case ATTACK_MODE_SHATTER: return "shatter";
        case ATTACK_MODE_IMPALE: return "impale";
        case ATTACK_MODE_SWEEP: return "sweep at";
        case ATTACK_MODE_HOOK: return "hook";
        case ATTACK_MODE_VOLLEY: return "volley at";
        case ATTACK_MODE_PIN_SHOT: return "pin";
        case ATTACK_MODE_DEADEYE: return "snipe";
        default: return "hit";
    }
}

// Return current skill level in the requested weapon family.
int actor_get_weapon_skill(const Actor* actor, WeaponSkillType skill_type)
{
    int skill_level;

    if(!actor)
        return 0;
    if(skill_type < 0 || skill_type >= WEAPON_SKILL_COUNT)
        skill_type = WEAPON_SKILL_UNARMED;

    skill_level = actor->weapon_skill[skill_type];
    if(skill_level < 0)
        skill_level = 0;
    return skill_level;
}

// Return current accumulated XP in a weapon skill.
int actor_get_weapon_skill_xp(const Actor* actor, WeaponSkillType skill_type)
{
    if(!actor)
        return 0;
    if(skill_type < 0 || skill_type >= WEAPON_SKILL_COUNT)
        return 0;
    return actor->weapon_skill_xp[skill_type];
}

// Add XP and process level-ups for weapon skill progression.
int actor_gain_weapon_skill_xp(Actor* actor, WeaponSkillType skill_type, int amount)
{
    int levels_gained = 0;

    if(!actor || amount <= 0)
        return 0;
    if(skill_type < 0 || skill_type >= WEAPON_SKILL_COUNT)
        skill_type = WEAPON_SKILL_UNARMED;

    actor->weapon_skill_xp[skill_type] += amount;
    if(actor->weapon_skill[skill_type] < 0)
        actor->weapon_skill[skill_type] = 0;

    while(actor->weapon_skill[skill_type] < MAX_WEAPON_SKILL_LEVEL)
    {
        int xp_required = weapon_skill_xp_required(actor->weapon_skill[skill_type]);
        if(actor->weapon_skill_xp[skill_type] < xp_required)
            break;

        actor->weapon_skill_xp[skill_type] -= xp_required;
        actor->weapon_skill[skill_type]++;
        levels_gained++;
    }

    return levels_gained;
}

int actor_get_non_weapon_skill(const Actor* actor, NonWeaponSkillType skill_type)
{
    int skill_level;

    if(!actor)
        return 0;
    if(skill_type < 0 || skill_type >= NON_WEAPON_SKILL_COUNT)
        skill_type = NON_WEAPON_SKILL_ANIMAL_HANDLING;

    skill_level = actor->non_weapon_skill[skill_type];
    if(skill_level < 0)
        skill_level = 0;
    return skill_level;
}

int actor_get_non_weapon_skill_xp(const Actor* actor, NonWeaponSkillType skill_type)
{
    if(!actor)
        return 0;
    if(skill_type < 0 || skill_type >= NON_WEAPON_SKILL_COUNT)
        return 0;
    return actor->non_weapon_skill_xp[skill_type];
}

int actor_gain_non_weapon_skill_xp(Actor* actor, NonWeaponSkillType skill_type, int amount)
{
    int levels_gained = 0;

    if(!actor || amount <= 0)
        return 0;
    if(skill_type < 0 || skill_type >= NON_WEAPON_SKILL_COUNT)
        skill_type = NON_WEAPON_SKILL_ANIMAL_HANDLING;

    actor->non_weapon_skill_xp[skill_type] += amount;
    if(actor->non_weapon_skill[skill_type] < 0)
        actor->non_weapon_skill[skill_type] = 0;

    while(actor->non_weapon_skill[skill_type] < MAX_NON_WEAPON_SKILL_LEVEL)
    {
        int xp_required = non_weapon_skill_xp_required(actor->non_weapon_skill[skill_type]);
        if(actor->non_weapon_skill_xp[skill_type] < xp_required)
            break;

        actor->non_weapon_skill_xp[skill_type] -= xp_required;
        actor->non_weapon_skill[skill_type]++;
        levels_gained++;
    }

    return levels_gained;
}

static const Item* combat_character_select_attack_item(const Character* character, int* out_is_two_hand_mode)
{
    const Item* right;
    const Item* left;
    const Item* selected = NULL;
    int is_two_hand_mode = 0;

    if(out_is_two_hand_mode)
        *out_is_two_hand_mode = 0;
    if(!character)
        return NULL;

    right = &character->equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    left = &character->equipment_slots[EQUIP_SLOT_OFF_HAND].item;

    if(right->type == ITEM_TYPE_WEAPON_TWO_HANDED)
    {
        selected = right;
        is_two_hand_mode = 1;
    }
    else if(left->type == ITEM_TYPE_WEAPON_TWO_HANDED)
    {
        selected = left;
        is_two_hand_mode = 1;
    }
    else if(item_is_weapon(right))
    {
        selected = right;
        if(right->type == ITEM_TYPE_WEAPON_VERSATILE && character->versatile_grip_mode == WEAPON_GRIP_TWO_HANDED)
            is_two_hand_mode = 1;
    }
    else if(item_is_ranged_weapon(right))
    {
        selected = right;
    }
    else if(item_is_weapon(left))
    {
        selected = left;
        if(left->type == ITEM_TYPE_WEAPON_VERSATILE && character->versatile_grip_mode == WEAPON_GRIP_TWO_HANDED)
            is_two_hand_mode = 1;
    }
    else if(item_is_ranged_weapon(left))
    {
        selected = left;
    }

    if(out_is_two_hand_mode)
        *out_is_two_hand_mode = is_two_hand_mode;
    return selected;
}

// Build attack profile from character equipment.
CombatProfile combat_profile_for_character_attack(const Character* character, AttackMode requested_mode)
{
    CombatProfile profile;
    CombatProfile available_profile;
    const Item* active_item;
    int is_two_hand_mode = 0;
    int available_mask = ATTACK_MODE_FLAG_PUNCH | ATTACK_MODE_FLAG_KICK;

    if(!character)
        return combat_unarmed_profile();

    active_item = combat_character_select_attack_item(character, &is_two_hand_mode);
    if(active_item)
        available_profile = combat_profile_from_item(active_item);
    else
        available_profile = combat_unarmed_profile();

    if(available_profile.can_toggle_grip)
        available_profile.is_two_hand_mode = is_two_hand_mode ? 1 : 0;

    combat_profile_apply_mode(&available_profile, &character->actor, ATTACK_MODE_NONE);
    available_mask |= available_profile.attack_mode_mask;

    if(active_item && combat_is_baseline_unarmed_mode(requested_mode))
    {
        profile = combat_unarmed_profile();
        combat_profile_apply_mode(&profile, &character->actor, requested_mode);
        profile.attack_mode_mask = available_mask;
        return profile;
    }

    profile = available_profile;
    if(requested_mode != ATTACK_MODE_NONE)
        combat_profile_apply_mode(&profile, &character->actor, requested_mode);

    profile.attack_mode_mask = available_mask;
    return profile;
}

AttackMode combat_valid_attack_mode_for_character(const Character* character, AttackMode requested_mode)
{
    CombatProfile profile = combat_profile_for_character_attack(character, requested_mode);
    return profile.attack_mode;
}

// Build parry profile from character equipment.
CombatProfile combat_profile_for_character_parry(const Character* character)
{
    CombatProfile left_profile;
    CombatProfile right_profile;
    const Item* active_item;
    int is_two_hand_mode = 0;

    if(!character)
        return combat_unarmed_profile();

    active_item = combat_character_select_attack_item(character, &is_two_hand_mode);
    if(active_item && is_two_hand_mode)
    {
        CombatProfile active_profile = combat_profile_from_item(active_item);
        active_profile.is_two_hand_mode = 1;
        active_profile.skill_type = weapon_skill_for_grip(active_profile.skill_type, active_profile.is_two_hand_mode);
        return active_profile;
    }

    const Item* right = &character->equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    const Item* left = &character->equipment_slots[EQUIP_SLOT_OFF_HAND].item;
    right_profile = combat_profile_from_item(right);
    left_profile = combat_profile_from_item(left);

    if(right_profile.can_parry && (!left_profile.can_parry || right_profile.parry_bonus >= left_profile.parry_bonus))
        return right_profile;
    if(left_profile.can_parry)
        return left_profile;
    return combat_unarmed_profile();
}

// Build generic unarmed profile for non-character actors.
CombatProfile combat_profile_for_actor_unarmed(const Actor* actor)
{
    (void)actor;
    return combat_unarmed_profile();
}

// Build HUD summary values for a character's current combat setup.
CombatSummary combat_summary_for_character(const Character* character, AttackMode requested_mode)
{
    CombatSummary summary;
    CombatProfile attack_profile;
    CombatProfile parry_profile;

    memset(&summary, 0, sizeof(summary));
    if(!character)
        return summary;

    attack_profile = combat_profile_for_character_attack(character, requested_mode);
    parry_profile = combat_profile_for_character_parry(character);

    summary.skill_type = attack_profile.skill_type;
    summary.active_damage_type = attack_profile.active_damage_type;
    summary.attack_mode = attack_profile.attack_mode;
    summary.skill_level = actor_get_weapon_skill(&character->actor, attack_profile.skill_type);
    summary.hit_chance = combat_hit_chance(&character->actor, &attack_profile, NULL);
    summary.crit_chance = combat_crit_chance(&character->actor, &attack_profile);
    summary.parry_chance = combat_parry_chance(&character->actor, &parry_profile);
    combat_attack_value_range(&character->actor, &attack_profile, &summary.damage_min, &summary.damage_max);
    summary.damage = (summary.damage_min + summary.damage_max) / 2;
    summary.is_armed = attack_profile.is_armed;
    summary.is_two_hand_mode = attack_profile.is_two_hand_mode;
    summary.can_toggle_grip = attack_profile.can_toggle_grip;
    strncpy(summary.weapon_name, attack_profile.weapon_name, sizeof(summary.weapon_name) - 1);
    return summary;
}

// Resolve one melee attack attempt including hit, block, parry, and damage.
MeleeAttackResult combat_resolve_melee_attack(
    Actor* attacker,
    const CombatProfile* attack_profile,
    Actor* defender,
    const CombatProfile* defense_profile
)
{
    MeleeAttackResult result;
    int attack_value;
    int attacker_skill_xp = 0;

    memset(&result, 0, sizeof(result));
    if(!attacker || !attack_profile || !defender)
        return result;

    result.attack_skill_type = attack_profile->skill_type;
    result.parry_skill_type = defense_profile ? defense_profile->skill_type : WEAPON_SKILL_UNARMED;
    result.damage_type = attack_profile->active_damage_type;
    result.attack_mode = attack_profile->attack_mode;
    result.attack_skill_level = actor_get_weapon_skill(attacker, attack_profile->skill_type);
    result.parry_skill_level = defense_profile ? actor_get_weapon_skill(defender, defense_profile->skill_type) : 0;
    result.hit_chance = combat_hit_chance(attacker, attack_profile, defender);
    result.crit_chance = combat_crit_chance(attacker, attack_profile);
    result.block_chance = clamp_int(
        defender->block + actor_speed_block_bonus(defender) + (defense_profile ? defense_profile->block_bonus : 0),
        0,
        85
    );
    result.parry_chance = combat_parry_chance(defender, defense_profile);

    if((rand() % 100) >= result.hit_chance)
    {
        result.attacker_levels_gained = actor_gain_weapon_skill_xp(attacker, result.attack_skill_type, WEAPON_SKILL_XP_MISS);
        result.attack_skill_level = actor_get_weapon_skill(attacker, result.attack_skill_type);
        return result;
    }

    if((rand() % 100) < result.block_chance)
    {
        result.blocked = 1;
        return result;
    }

    if(result.parry_chance > 0 && (rand() % 100) < result.parry_chance)
    {
        result.parried = 1;
        result.defender_levels_gained = actor_gain_weapon_skill_xp(defender, result.parry_skill_type, WEAPON_SKILL_XP_SUCCESSFUL_PARRY);
        result.parry_skill_level = actor_get_weapon_skill(defender, result.parry_skill_type);
        return result;
    }

    result.hit = 1;
    result.critical = (rand() % 100) < result.crit_chance;

    attack_value = combat_roll_attack_value(attacker, attack_profile);
    if(result.critical)
        attack_value += (attack_value + 1) / 2;

    if(combat_is_baseline_unarmed_mode(attack_profile->attack_mode))
    {
        result.direct_damage = combat_apply_stamina_only_damage(defender,
                                                                attack_value,
                                                                attack_profile->armor_penetration,
                                                                &result.armor_absorbed,
                                                                &result.stamina_damage);
    }
    else
    {
        result.direct_damage = combat_apply_damage(defender,
                                                   attack_value,
                                                   attack_profile->armor_penetration,
                                                   &result.armor_absorbed,
                                                   &result.stamina_damage);
    }
    result.no_damage_hit = (result.direct_damage <= 0 && result.stamina_damage <= 0);
    result.damage = result.direct_damage;

    if(roll_percent(attack_profile->status_bleed_chance))
    {
        int bleed_damage = 1 + (attack_profile->power / 4);
        if(bleed_damage < 1)
            bleed_damage = 1;
        defender->health -= bleed_damage;
        if(defender->health < 0)
            defender->health = 0;
        result.damage += bleed_damage;
        result.bonus_damage += bleed_damage;
        result.bleed_applied = 1;
    }

    if(attack_profile->attack_mode == ATTACK_MODE_KICK)
    {
        if(roll_percent(combat_kick_knockdown_chance(attacker)))
            result.stun_applied = 1;
    }
    else if(roll_percent(attack_profile->status_stun_chance))
    {
        result.stun_applied = 1;
    }

    if(roll_percent(attack_profile->status_slow_chance))
    {
        if(defender->stamina > actor_stamina_floor(defender))
            defender->stamina = actor_clamp_stamina_value(defender, defender->stamina - 1);
        result.slow_applied = 1;
    }

    attacker_skill_xp = combat_weapon_skill_xp_for_hit(result.critical, result.no_damage_hit);

    result.attacker_levels_gained = actor_gain_weapon_skill_xp(attacker, result.attack_skill_type, attacker_skill_xp);
    result.attack_skill_level = actor_get_weapon_skill(attacker, result.attack_skill_type);
    return result;
}