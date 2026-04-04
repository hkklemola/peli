#include "combat.h"

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
#define BASE_ATTACK_STAMINA_COST 2

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
        default: return ATTACK_MODE_FLAG_NONE;
    }
}

/**
 * @brief Get the default/primary damage type for a given attack mode.
 * @param mode The AttackMode (STAB, CUT, SMASH, PUNCH, KICK, etc.).
 * @return A DamageType value (PIERCING, SLASHING, CRUSHING, or NONE).
 * @note Stab=piercing, Cut=slashing, Punch/Kick/Smash=crushing.
 */
static int attack_mode_default_damage_type(AttackMode mode)
{
    switch(mode)
    {
        case ATTACK_MODE_STAB: return DAMAGE_TYPE_PIERCING;
        case ATTACK_MODE_CUT: return DAMAGE_TYPE_SLASHING;
        case ATTACK_MODE_SMASH:
        case ATTACK_MODE_PUNCH:
        case ATTACK_MODE_KICK:
            return DAMAGE_TYPE_CRUSHING;
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
    return DAMAGE_TYPE_NONE;
}

/**
 * @brief Get the default damage type mask for a weapon skill category.
 * @param skill_type The WeaponSkillType (SWORD, AXE, DAGGER, etc.).
 * @return A bitmask of supported DamageType flags for that weapon family.
 * @note Swords support both pierce and slash; daggers/spears pierce only; axes slash; maces crush.
 */
static int default_damage_mask_for_skill(WeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case WEAPON_SKILL_DAGGER:
        case WEAPON_SKILL_SPEAR:
            return DAMAGE_TYPE_PIERCING;
        case WEAPON_SKILL_SWORD:
            return DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_SLASHING;
        case WEAPON_SKILL_AXE:
            return DAMAGE_TYPE_SLASHING;
        case WEAPON_SKILL_MACE:
            return DAMAGE_TYPE_CRUSHING;
        case WEAPON_SKILL_STAFF:
        case WEAPON_SKILL_POLEARM:
            return DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_CRUSHING;
        case WEAPON_SKILL_UNARMED:
        default:
            return DAMAGE_TYPE_CRUSHING;
    }
}

/**
 * @brief Get the default attack mode mask for a weapon skill category.
 * @param skill_type The WeaponSkillType (SWORD, AXE, DAGGER, etc.).
 * @return A bitmask of supported AttackModeFlag values for that weapon family.
 * @note Swords can stab and cut; axes cut only; daggers/spears stab; maces smash; unarmed punches/kicks.
 */
static int default_attack_mode_mask_for_skill(WeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case WEAPON_SKILL_DAGGER:
        case WEAPON_SKILL_SPEAR:
            return ATTACK_MODE_FLAG_STAB;
        case WEAPON_SKILL_SWORD:
            return ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_CUT;
        case WEAPON_SKILL_AXE:
            return ATTACK_MODE_FLAG_CUT;
        case WEAPON_SKILL_MACE:
            return ATTACK_MODE_FLAG_SMASH;
        case WEAPON_SKILL_STAFF:
        case WEAPON_SKILL_POLEARM:
            return ATTACK_MODE_FLAG_STAB | ATTACK_MODE_FLAG_SMASH;
        case WEAPON_SKILL_UNARMED:
        default:
            return ATTACK_MODE_FLAG_PUNCH | ATTACK_MODE_FLAG_KICK;
    }
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

AttackMode attack_mode_first_from_mask(int attack_mode_mask)
{
    if(attack_mode_mask & ATTACK_MODE_FLAG_PUNCH) return ATTACK_MODE_PUNCH;
    if(attack_mode_mask & ATTACK_MODE_FLAG_KICK) return ATTACK_MODE_KICK;
    if(attack_mode_mask & ATTACK_MODE_FLAG_STAB) return ATTACK_MODE_STAB;
    if(attack_mode_mask & ATTACK_MODE_FLAG_CUT) return ATTACK_MODE_CUT;
    if(attack_mode_mask & ATTACK_MODE_FLAG_SMASH) return ATTACK_MODE_SMASH;
    return ATTACK_MODE_NONE;
}

AttackMode attack_mode_next_from_mask(int attack_mode_mask, AttackMode current_mode)
{
    static const AttackMode ordered_modes[] = {
        ATTACK_MODE_PUNCH,
        ATTACK_MODE_KICK,
        ATTACK_MODE_STAB,
        ATTACK_MODE_CUT,
        ATTACK_MODE_SMASH,
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

static void combat_profile_apply_mode(CombatProfile* profile, AttackMode requested_mode)
{
    AttackMode selected_mode;
    int damage_type;

    if(!profile)
        return;

    if(profile->attack_mode_mask == ATTACK_MODE_FLAG_NONE)
        profile->attack_mode_mask = default_attack_mode_mask_for_skill(profile->skill_type);
    if(profile->damage_type_mask == DAMAGE_TYPE_NONE)
        profile->damage_type_mask = default_damage_mask_for_skill(profile->skill_type);

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
}

// Build default unarmed combat profile.
static CombatProfile combat_unarmed_profile(void)
{
    CombatProfile profile;

    memset(&profile, 0, sizeof(profile));
    profile.skill_type = WEAPON_SKILL_UNARMED;
    strncpy(profile.weapon_name, "Fists", sizeof(profile.weapon_name) - 1);
    profile.power = DEFAULT_UNARMED_POWER;
    profile.damage_type_mask = DAMAGE_TYPE_CRUSHING;
    profile.attack_mode_mask = ATTACK_MODE_FLAG_PUNCH | ATTACK_MODE_FLAG_KICK;
    profile.attack_mode = ATTACK_MODE_PUNCH;
    profile.active_damage_type = DAMAGE_TYPE_CRUSHING;
    profile.is_armed = 0;
    return profile;
}

// Build combat profile from equipped item, falling back to unarmed.
static CombatProfile combat_profile_from_item(const Item* item)
{
    CombatProfile profile;

    if(!item || !item_is_weapon(item))
        return combat_unarmed_profile();

    memset(&profile, 0, sizeof(profile));
    profile.skill_type = item->weapon_skill_type;
    strncpy(profile.weapon_name, item->name, sizeof(profile.weapon_name) - 1);
    profile.power = item->power > 0 ? item->power : DEFAULT_UNARMED_POWER;
    profile.accuracy_bonus = item->accuracy_bonus;
    profile.crit_bonus = item->crit_bonus;
    profile.parry_bonus = item->parry_bonus;
    profile.block_bonus = item->block_bonus;
    profile.can_parry = item->can_parry;
    profile.damage_type_mask = item->damage_type_mask;
    profile.attack_mode_mask = item->attack_mode_mask;
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

// Compute raw attack value before mitigation.
int combat_attack_value(const Actor* attacker, const CombatProfile* attack_profile)
{
    int skill_bonus;
    int strength_bonus;
    int base_value;

    if(!attack_profile)
        return 1;

    skill_bonus = attacker ? (actor_get_weapon_skill(attacker, attack_profile->skill_type) / 2) : 0;
    strength_bonus = attacker ? actor_strength_melee_bonus(attacker) : 0;
    base_value = 1 + attack_profile->power + skill_bonus + strength_bonus;

    if(attack_profile->skill_type == WEAPON_SKILL_UNARMED)
    {
        switch(attack_profile->attack_mode)
        {
            case ATTACK_MODE_PUNCH:
                base_value -= 1;
                break;
            case ATTACK_MODE_KICK:
                base_value += 1;
                break;
            default:
                break;
        }
    }

    if(base_value < 1)
        base_value = 1;
    return base_value;
}

// Apply final damage to defender after armor and return dealt damage.
static int combat_apply_damage(Actor* defender, int attack_value, int armor_penetration)
{
    int damage;
    int effective_armor;

    if(!defender)
        return 0;

    effective_armor = defender->armor_rating - armor_penetration;
    if(effective_armor < 0)
        effective_armor = 0;

    damage = attack_value - effective_armor;
    if(damage < 1)
        damage = 1;

    defender->health -= damage;
    if(defender->health < 0)
        defender->health = 0;
    return damage;
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

    if(combat_profile_is_ranged(profile))
    {
        cost = 3;
    }
    else
    {
        switch(profile->attack_mode)
        {
            case ATTACK_MODE_PUNCH:
                cost = 1;
                break;
            case ATTACK_MODE_STAB:
                cost = 2;
                break;
            case ATTACK_MODE_CUT:
                cost = 3;
                break;
            case ATTACK_MODE_KICK:
                cost = 2;
                break;
            case ATTACK_MODE_SMASH:
                cost = 4;
                break;
            case ATTACK_MODE_NONE:
            default:
                cost = 2;
                break;
        }
    }

    if(profile->stamina_cost_mod >= 2)
        cost += 1;
    else if(profile->stamina_cost_mod <= -2)
        cost -= 1;

    return clamp_int(cost, 1, 4);
}

// Return full name for a weapon-skill type.
const char* weapon_skill_name(WeaponSkillType skill_type)
{
    switch(skill_type)
    {
        case WEAPON_SKILL_UNARMED: return "Unarmed";
        case WEAPON_SKILL_DAGGER: return "Dagger";
        case WEAPON_SKILL_SWORD: return "Sword";
        case WEAPON_SKILL_AXE: return "Axe";
        case WEAPON_SKILL_MACE: return "Mace";
        case WEAPON_SKILL_SPEAR: return "Spear";
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
        case WEAPON_SKILL_SWORD: return "Swd";
        case WEAPON_SKILL_AXE: return "Axe";
        case WEAPON_SKILL_MACE: return "Mac";
        case WEAPON_SKILL_SPEAR: return "Spr";
        case WEAPON_SKILL_STAFF: return "Stf";
        case WEAPON_SKILL_POLEARM: return "Pol";
        case WEAPON_SKILL_THROWN: return "Thr";
        case WEAPON_SKILL_BOW: return "Bow";
        case WEAPON_SKILL_CROSSBOW: return "Xbw";
        default: return "?";
    }
}

const char* damage_type_name(int damage_type)
{
    switch(damage_type)
    {
        case DAMAGE_TYPE_PIERCING: return "Piercing";
        case DAMAGE_TYPE_SLASHING: return "Slashing";
        case DAMAGE_TYPE_CRUSHING: return "Crushing";
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

// Build attack profile from character equipment.
CombatProfile combat_profile_for_character_attack(const Character* character, AttackMode requested_mode)
{
    CombatProfile profile;

    if(!character)
        return combat_unarmed_profile();

    // Use slot-based logic for hands
    const Item* right = &character->equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    const Item* left = &character->equipment_slots[EQUIP_SLOT_OFF_HAND].item;
    if(right->type == ITEM_TYPE_WEAPON_TWO_HANDED)
        profile = combat_profile_from_item(right);
    else if(left->type == ITEM_TYPE_WEAPON_TWO_HANDED)
        profile = combat_profile_from_item(left);
    else if(item_is_weapon(right))
        profile = combat_profile_from_item(right);
    else if(item_is_weapon(left))
        profile = combat_profile_from_item(left);
    else
        profile = combat_unarmed_profile();

    combat_profile_apply_mode(&profile, requested_mode);
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

    if(!character)
        return combat_unarmed_profile();

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
    summary.damage = combat_attack_value(&character->actor, &attack_profile);
    summary.is_armed = attack_profile.is_armed;
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
        return result;

    if((rand() % 100) < result.block_chance)
    {
        result.blocked = 1;
        return result;
    }

    if(result.parry_chance > 0 && (rand() % 100) < result.parry_chance)
    {
        result.parried = 1;
        result.defender_levels_gained = actor_gain_weapon_skill_xp(defender, result.parry_skill_type, 3);
        result.parry_skill_level = actor_get_weapon_skill(defender, result.parry_skill_type);
        return result;
    }

    result.hit = 1;
    result.critical = (rand() % 100) < result.crit_chance;

    attack_value = combat_attack_value(attacker, attack_profile);
    if(result.critical)
        attack_value += (attack_value + 1) / 2;

    result.damage = combat_apply_damage(defender, attack_value, attack_profile->armor_penetration);

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
        if(defender->stamina > 0)
            defender->stamina -= 1;
        result.slow_applied = 1;
    }

    result.attacker_levels_gained = actor_gain_weapon_skill_xp(attacker, result.attack_skill_type, result.critical ? 3 : 2);
    result.attack_skill_level = actor_get_weapon_skill(attacker, result.attack_skill_type);
    return result;
}