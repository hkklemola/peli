#include "movement.h"
#include "atlas.h"
#include "combat.h"
#include "collision.h"
#include "bestiary.h"
#include "log.h"
#include "tile.h"
#include "tileset.h"
#include "map.h"
#include "player.h"
#include "character.h"
#include "item.h"
#include <stdlib.h>
#include <stdio.h>

/*
 * Purpose:
 *   Implements player movement and melee engagement behavior.
 *
 * Functions:
 *   - log_skill_gain: emits combat skill progression messages.
 *   - log_attack_result: emits combat outcome messages.
 *   - player_move_step: processes one movement/combat step.
 *   - player_move: one-step movement wrapper.
 *   - player_sprint: multi-step stamina-based sprint movement.
 */

typedef enum MoveStepResult {
    MOVE_STEP_BLOCKED = 0,
    MOVE_STEP_MOVED,
    MOVE_STEP_COMBAT,
    MOVE_STEP_INTERACT
} MoveStepResult;

static int area_bounds_blocked(int x, int y)
{
    if(!current_area)
        return 1;

    return x < 0 || x >= current_area->width || y < 0 || y >= current_area->height;
}

static int creature_should_flee(const Creature* creature, const Player* p)
{
    int dx;
    int dy;

    if(!creature || !p || creature->actor.max_health <= 0)
        return 0;

    dx = abs(creature->actor.entity.x - p->character.actor.entity.x);
    dy = abs(creature->actor.entity.y - p->character.actor.entity.y);

    return (creature->actor.health * 100 <= creature->actor.max_health * 35) && (dx + dy <= 8);
}

static int creature_try_move(Creature* creature, int dx, int dy)
{
    int nx;
    int ny;

    if(!creature || !creature->alive)
        return 0;

    nx = creature->actor.entity.x + dx;
    ny = creature->actor.entity.y + dy;

    if(area_bounds_blocked(nx, ny))
        return 0;
    if(is_blocked(nx, ny, 0))
        return 0;

    creature->actor.entity.x = nx;
    creature->actor.entity.y = ny;
    return 1;
}

static void creature_begin_wander(Creature* creature)
{
    static const int dirs[4][2] = {
        { 0, -1 },
        { 0, 1 },
        { -1, 0 },
        { 1, 0 }
    };
    int choice;

    if(!creature)
        return;

    choice = rand() % 4;
    creature->move_state = CREATURE_STATE_WANDER;
    creature->state_turns = 2 + (rand() % 3);
    creature->move_dx = dirs[choice][0];
    creature->move_dy = dirs[choice][1];
}

static void creature_take_wander_turn(Creature* creature)
{
    if(!creature)
        return;

    if(creature->state_turns <= 0)
    {
        if(rand() % 4 == 0)
        {
            creature->move_state = CREATURE_STATE_REST;
            creature->state_turns = 1 + (rand() % 3);
            creature->move_dx = 0;
            creature->move_dy = 0;
            return;
        }

        creature_begin_wander(creature);
    }

    if(!creature_try_move(creature, creature->move_dx, creature->move_dy))
        creature_begin_wander(creature);

    creature->state_turns--;
}

static void creature_take_rest_turn(Creature* creature)
{
    if(!creature)
        return;

    if(creature->state_turns > 0)
        creature->state_turns--;

    if(creature->state_turns <= 0)
        creature_begin_wander(creature);
}

static void creature_take_flee_turn(Creature* creature, const Player* p)
{
    static const int dirs[8][2] = {
        { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 },
        { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 }
    };
    int best_score = -99999;
    int best_dx = 0;
    int best_dy = 0;

    if(!creature || !p)
        return;

    for(int i = 0; i < 8; i++)
    {
        int nx = creature->actor.entity.x + dirs[i][0];
        int ny = creature->actor.entity.y + dirs[i][1];
        int score;

        if(area_bounds_blocked(nx, ny))
            continue;
        if(is_blocked(nx, ny, 0))
            continue;

        score = abs(nx - p->character.actor.entity.x) + abs(ny - p->character.actor.entity.y);
        if(score > best_score)
        {
            best_score = score;
            best_dx = dirs[i][0];
            best_dy = dirs[i][1];
        }
    }

    if(best_score > -99999)
        creature_try_move(creature, best_dx, best_dy);

    if(!creature_should_flee(creature, p))
        creature_begin_wander(creature);
}

void creatures_take_turns(Player* p)
{
    if(!current_area || !p)
        return;

    for(int i = 0; i < MAX_CREATURES; i++)
    {
        Creature* creature = &creatures[i];

        if(!creature->alive || !creature->template)
            continue;

        if(creature_should_flee(creature, p))
            creature->move_state = CREATURE_STATE_FLEE;

        switch(creature->move_state)
        {
            case CREATURE_STATE_REST:
                creature_take_rest_turn(creature);
                break;
            case CREATURE_STATE_FLEE:
                creature_take_flee_turn(creature, p);
                break;
            case CREATURE_STATE_WANDER:
            default:
                creature_take_wander_turn(creature);
                break;
        }
    }
}

// Log one skill-gain event after combat actions.
static void log_skill_gain(const char* actor_name, int is_player, WeaponSkillType skill_type, int levels_gained)
{
    if(levels_gained <= 0)
        return;

    if(is_player)
        log_add("Your %s skill rises to %d!", weapon_skill_name(skill_type), actor_get_weapon_skill(&player.character.actor, skill_type));
    else
        log_add("%s improves %s by %d level%s.", actor_name, weapon_skill_name(skill_type), levels_gained, levels_gained == 1 ? "" : "s");
}

// Log the textual outcome for one attack resolution.
static void log_attack_result(
    const char* attacker_name,
    int attacker_is_player,
    const char* defender_name,
    int defender_is_player,
    const MeleeAttackResult* result
)
{
    if(!result)
        return;

    if(!result->hit && !result->blocked && !result->parried)
    {
        if(attacker_is_player)
            log_add("You miss %s.", defender_name);
        else if(defender_is_player)
            log_add("%s misses you.", attacker_name);
        else
            log_add("%s misses %s.", attacker_name, defender_name);
        return;
    }

    if(result->blocked)
    {
        if(defender_is_player)
            log_add("You block %s's attack!", attacker_name);
        else if(attacker_is_player)
            log_add("%s blocks your attack!", defender_name);
        else
            log_add("%s blocks %s's attack.", defender_name, attacker_name);
        return;
    }

    if(result->parried)
    {
        if(defender_is_player)
            log_add("You parry %s's attack!", attacker_name);
        else if(attacker_is_player)
            log_add("%s parries your attack!", defender_name);
        else
            log_add("%s parries %s's attack.", defender_name, attacker_name);
        return;
    }

    if(attacker_is_player)
        log_add("You %s%s %s for %d damage (%s).", result->critical ? "critically " : "", attack_mode_verb(result->attack_mode), defender_name, result->damage, damage_type_name(result->damage_type));
    else if(defender_is_player)
        log_add("%s %s you for %d damage.", attacker_name, result->critical ? "critically hits" : attack_mode_verb(result->attack_mode), result->damage);
    else
        log_add("%s %s %s for %d damage.", attacker_name, result->critical ? "critically hits" : attack_mode_verb(result->attack_mode), defender_name, result->damage);
}

// Attempt one direct melee attack against a specific creature target.
int player_attack_creature(Player* p, Creature* target, AttackMode requested_mode)
{
    int dx;
    int dy;
    CombatProfile player_attack_profile;
    CombatProfile creature_profile;
    MeleeAttackResult player_attack;

    if(!p || !target || !target->alive || !target->template)
        return 0;

    dx = target->actor.entity.x - p->character.actor.entity.x;
    if(dx < 0) dx = -dx;
    dy = target->actor.entity.y - p->character.actor.entity.y;
    if(dy < 0) dy = -dy;

    if(dx > 1 || dy > 1)
    {
        log_add("Target out of range for melee attack.");
        return 0;
    }

    if(!target->template->is_hostile)
    {
        log_add("%s watches you warily.", target->template->name);
        return 0;
    }

    player_attack_profile = combat_profile_for_character_attack(&p->character, requested_mode);
    creature_profile = combat_profile_for_actor_unarmed(&target->actor);
    player_attack = combat_resolve_melee_attack(
        &p->character.actor,
        &player_attack_profile,
        &target->actor,
        &creature_profile
    );

    log_attack_result("You", 1, target->template->name, 0, &player_attack);
    log_skill_gain(p->character.name, 1, player_attack.attack_skill_type, player_attack.attacker_levels_gained);
    log_skill_gain(target->template->name, 0, player_attack.parry_skill_type, player_attack.defender_levels_gained);

    if(target->actor.health <= 0)
    {
        target->alive = 0;
        log_add("You killed %s!", target->template->name);
    }
    else
    {
        CombatProfile player_parry_profile = combat_profile_for_character_parry(&p->character);
        MeleeAttackResult retaliation = combat_resolve_melee_attack(
            &target->actor,
            &creature_profile,
            &p->character.actor,
            &player_parry_profile
        );

        log_attack_result(target->template->name, 0, p->character.name, 1, &retaliation);
        log_skill_gain(target->template->name, 0, retaliation.attack_skill_type, retaliation.attacker_levels_gained);
        log_skill_gain(p->character.name, 1, retaliation.parry_skill_type, retaliation.defender_levels_gained);

        if(p->character.actor.health <= 0)
        {
            p->character.actor.health = 0;
            log_add("You died! Game over.");
            exit(0);
        }
    }

    creatures_take_turns(p);
    return 1;
}

// Attempt one movement step by delta, resolving combat when target tile is occupied.
static MoveStepResult player_move_step(Player* p, int dx, int dy)
{
    int nx = p->character.actor.entity.x + dx;
    int ny = p->character.actor.entity.y + dy;

    // Check map bounds
    if(area_bounds_blocked(nx, ny))
        return MOVE_STEP_BLOCKED;

    // Check for tile blocking
    if(!current_area)
        return MOVE_STEP_BLOCKED;

    if(is_blocked(nx, ny, 1))
        return MOVE_STEP_BLOCKED;

    // Combat: attack creature if present
    Creature* target = bestiary_creature_at(nx, ny);
    if(target)
    {
        if(player_attack_creature(p, target, p->selected_attack_mode))
            return MOVE_STEP_COMBAT;
        return MOVE_STEP_INTERACT;
    }

    // Tile is free → move player
    p->character.actor.entity.x = nx;
    p->character.actor.entity.y = ny;
    return MOVE_STEP_MOVED;
}

// Attempt to move player by delta, resolving combat when target tile is occupied.
void player_move(Player* p, int dx, int dy)
{
    if(!p)
        return;

    (void)player_move_step(p, dx, dy);
    creatures_take_turns(p);
}

// Attempt sprint movement, spending stamina and handling blocked second-step refund.
void player_sprint(Player* p, int dx, int dy, int stamina_cost)
{
    MoveStepResult first_step;
    MoveStepResult second_step;

    if(!p)
        return;

    if(stamina_cost < 0)
        stamina_cost = 0;

    if(p->character.actor.stamina < stamina_cost)
    {
        log_add("Too exhausted to sprint.");
        return;
    }

    p->character.actor.stamina -= stamina_cost;

    first_step = player_move_step(p, dx, dy);
    if(first_step == MOVE_STEP_BLOCKED)
    {
        p->character.actor.stamina += stamina_cost;
        if(p->character.actor.stamina > p->character.actor.max_stamina)
            p->character.actor.stamina = p->character.actor.max_stamina;
        log_add("Sprint blocked.");
        return;
    }

    if(first_step == MOVE_STEP_INTERACT)
    {
        p->character.actor.stamina += stamina_cost;
        if(p->character.actor.stamina > p->character.actor.max_stamina)
            p->character.actor.stamina = p->character.actor.max_stamina;
        return;
    }

    if(first_step == MOVE_STEP_COMBAT)
        return;

    second_step = player_move_step(p, dx, dy);
    if(second_step == MOVE_STEP_BLOCKED)
    {
        p->character.actor.stamina += 1;
        if(p->character.actor.stamina > p->character.actor.max_stamina)
            p->character.actor.stamina = p->character.actor.max_stamina;
        log_add("Sprint clipped by terrain. You recover 1 stamina.");
    }

    creatures_take_turns(p);
}

