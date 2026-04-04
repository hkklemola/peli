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
#include "draw.h"
#include "furniture.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

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

static void movement_sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((unsigned int)(ms * 1000));
#endif
}

static void play_player_attack_animation(Player* p,
                                         AttackAnimationType type,
                                         int target_x,
                                         int target_y,
                                         int target_z,
                                         int frame_max)
{
    if(!p)
        return;

    if(target_z != p->character.actor.entity.z)
        return;

    if(frame_max < 1)
        frame_max = 1;

    player_attack_animation_start(p,
                                  type,
                                  p->character.actor.entity.x,
                                  p->character.actor.entity.y,
                                  p->character.actor.entity.z,
                                  target_x,
                                  target_y,
                                  target_z,
                                  frame_max);

    while(player_attack_animation_active(p))
    {
        draw_force_full_redraw();
        draw_world(p);
        movement_sleep_ms(35);
        player_attack_animation_advance(p);
    }

    draw_force_full_redraw();
}

/**
 * @brief Check if coordinates are outside the current area's bounds.
 * @param x The x-coordinate to check.
 * @param y The y-coordinate to check.
 * @return 1 if out of bounds or no area is loaded, 0 if within valid area bounds.
 */
static int area_bounds_blocked(int x, int y)
{
    if(!current_area)
        return 1;

    return x < 0 || x >= current_area->width || y < 0 || y >= current_area->height;
}

/**
 * @brief Determine if a creature should flee from the player.
 *        Flees when health is below 35% AND within 8 tiles of the player.
 * @param creature The creature to evaluate.
 * @param p The player to measure distance from.
 * @return 1 if creature should flee, 0 otherwise.
 */
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

/**
 * @brief Attempt to move a creature by a delta in one of the four cardinal directions.
 * @param creature The creature to move.
 * @param dx Change in x (-1, 0, or 1).
 * @param dy Change in y (-1, 0, or 1).
 * @return 1 if movement succeeded (creature was alive and moved), 0 if blocked or creature dead.
 * @note Updates creature position in-place if successful.
 */
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
    if(is_blocked_3d(nx, ny, creature->actor.entity.z, 0))
        return 0;

    creature->actor.entity.x = nx;
    creature->actor.entity.y = ny;
    return 1;
}

/**
 * @brief Transition a creature into a wander state with random movement direction.
 *        Sets creature movement to wander for 2-4 turns in a cardinal direction.
 * @param creature The creature to start wandering.
 */
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

/**
 * @brief Execute one turn of creature wander behavior.
 *        Moves in current direction; may transition to rest or choose new wander direction.
 *        Complex state machine: wander -> rest -> wander cycle.
 * @param creature The creature taking its turn.
 */
static void creature_take_wander_turn(Creature* creature)
{
    if(!creature)
        return;

    /* State timeout: decide next state when current wander/rest period ends. */
    if(creature->state_turns <= 0)
    {
        /* 25% chance to rest instead of continuing to wander. */
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

    /* Attempt to move in current wander direction; choose new direction if blocked. */
    if(!creature_try_move(creature, creature->move_dx, creature->move_dy))
        creature_begin_wander(creature);

    creature->state_turns--;
}

/**
 * @brief Execute one turn of creature rest behavior.
 *        Creature stands still and decrements rest timer until it transitions back to wandering.
 * @param creature The creature taking its turn.
 */
static void creature_take_rest_turn(Creature* creature)
{
    if(!creature)
        return;

    if(creature->state_turns > 0)
        creature->state_turns--;

    if(creature->state_turns <= 0)
        creature_begin_wander(creature);
}

/**
 * @brief Execute one turn of creature flee behavior.
 *        Uses pathfinding (8 directions) to maximize distance from player.
 *        Transitions back to wander when health stabilizes above flee threshold.
 * @param creature The creature taking its turn.
 * @param p The player the creature is fleeing from.
 * @note Evaluates all 8 cardinal+diagonal directions and picks the one farthest from player.
 */
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

    /* Evaluate all adjacent tiles (including diagonals) and pick the one farthest from player. */
    for(int i = 0; i < 8; i++)
    {
        int nx = creature->actor.entity.x + dirs[i][0];
        int ny = creature->actor.entity.y + dirs[i][1];
        int score;

        if(area_bounds_blocked(nx, ny))
            continue;
        if(is_blocked_3d(nx, ny, creature->actor.entity.z, 0))
            continue;

        /* Score = Manhattan distance from player at new position. */
        score = abs(nx - p->character.actor.entity.x) + abs(ny - p->character.actor.entity.y);
        if(score > best_score)
        {
            best_score = score;
            best_dx = dirs[i][0];
            best_dy = dirs[i][1];
        }
    }

    /* Move toward safest position if one was found. */
    if(best_score > -99999)
        creature_try_move(creature, best_dx, best_dy);

    /* Stop fleeing when health stabilizes (threshold check). */
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

    if(p->skip_action_point_regen_turn)
    {
        p->skip_action_point_regen_turn = 0;
    }
    else
    {
        (void)player_recover_action_points(p, player_action_point_regen_per_turn(p));
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

    if(result->bleed_applied)
        log_add("Bleed opens additional wounds.");
    if(result->slow_applied)
        log_add("The strike slows the target's tempo.");
    if(result->stun_applied)
        log_add("The target is stunned!");
}

static Creature* find_reach_target_in_direction(const Player* p, int dx, int dy, int max_range)
{
    int x;
    int y;

    if(!p || max_range <= 1)
        return NULL;

    x = p->character.actor.entity.x;
    y = p->character.actor.entity.y;

    for(int step = 2; step <= max_range; step++)
    {
        int tx = x + (dx * step);
        int ty = y + (dy * step);
        Creature* target;

        if(area_bounds_blocked(tx, ty))
            break;
        if(is_blocked_3d(tx, ty, p->character.actor.entity.z, 1))
            break;

        target = bestiary_creature_at_3d(tx, ty, p->character.actor.entity.z);
        if(target)
            return target;
    }

    return NULL;
}

static Furniture* find_attackable_furniture_in_direction(const Player* p, int dx, int dy, int max_range)
{
    int x;
    int y;

    if(!p || max_range < 1)
        return NULL;

    x = p->character.actor.entity.x;
    y = p->character.actor.entity.y;

    for(int step = 1; step <= max_range; step++)
    {
        int tx = x + (dx * step);
        int ty = y + (dy * step);
        Furniture* furniture;

        if(area_bounds_blocked(tx, ty))
            break;

        furniture = furniture_at_3d(current_area, tx, ty, p->character.actor.entity.z);
        if(furniture && furniture_is_destructible(furniture))
            return furniture;

        if(is_blocked_3d(tx, ty, p->character.actor.entity.z, 1))
            break;
    }

    return NULL;
}

static int player_resolve_furniture_attack(Player* p,
                                           Furniture* furniture,
                                           const CombatProfile* attack_profile,
                                           AttackAnimationType animation_type,
                                           int animation_frames)
{
    char furniture_name[64];
    int hardness;
    int max_structure;
    int target_x;
    int target_y;
    int target_z;
    int raw_damage;
    int damage_dealt = 0;
    int destroyed = 0;

    if(!p || !furniture || !attack_profile)
        return 0;

    if(!furniture_is_destructible(furniture))
    {
        log_add("%s cannot be meaningfully damaged.", furniture_display_name(furniture));
        return 0;
    }

    snprintf(furniture_name, sizeof(furniture_name), "%s", furniture_display_name(furniture));
    hardness = furniture_hardness(furniture);
    max_structure = furniture_max_structure_points(furniture);
    target_x = furniture->base.base.x;
    target_y = furniture->base.base.y;
    target_z = furniture->base.base.z;
    raw_damage = combat_attack_value(&p->character.actor, attack_profile);

    (void)furniture_apply_damage(furniture, raw_damage, &damage_dealt, &destroyed);

    play_player_attack_animation(p,
                                 animation_type,
                                 target_x,
                                 target_y,
                                 target_z,
                                 animation_frames);

    if(damage_dealt > 0)
    {
        if(destroyed)
            log_add("You strike %s for %d damage and destroy it!", furniture_name, damage_dealt);
        else
            log_add("You strike %s for %d damage. (%d/%d SP)",
                    furniture_name,
                    damage_dealt,
                    furniture_current_structure_points(furniture),
                    max_structure);
    }
    else
    {
        log_add("%s shrugs off the blow. (Hardness %d)", furniture_name, hardness);
    }

    creatures_take_turns(p);
    return 1;
}

static const Item* player_active_ranged_weapon(const Character* c)
{
    if(!c)
        return NULL;

    const Item* right = &c->equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    const Item* left = &c->equipment_slots[EQUIP_SLOT_OFF_HAND].item;
    if(right->type == ITEM_TYPE_WEAPON_TWO_HANDED && item_is_ranged_weapon(right))
        return right;
    if(left->type == ITEM_TYPE_WEAPON_TWO_HANDED && item_is_ranged_weapon(left))
        return left;
    if(item_is_ranged_weapon(right))
        return right;
    if(item_is_ranged_weapon(left))
        return left;
    return NULL;
}

static Item* player_active_throw_item(Character* c)
{
    if(!c)
        return NULL;

    Item* right = &c->equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    Item* left = &c->equipment_slots[EQUIP_SLOT_OFF_HAND].item;
    if(right->type != ITEM_TYPE_NONE)
        return right;
    if(left->type != ITEM_TYPE_NONE)
        return left;
    return NULL;
}

int player_ranged_attack_creature(Player* p, Creature* target, AttackMode requested_mode)
{
    if(!p || !target || !target->alive || !target->template)
        return 0;

    return player_ranged_attack_tile(p,
                                     target->actor.entity.x,
                                     target->actor.entity.y,
                                     target->actor.entity.z,
                                     requested_mode);
}

static int player_prepare_ranged_attack(Player* p,
                                        AttackMode requested_mode,
                                        CombatProfile* out_profile,
                                        int* out_max_range,
                                        int* out_attack_action_point_cost,
                                        int* out_uses_ranged_weapon)
{
    const Item* ranged_weapon;
    Item* throw_item;

    if(!p || !out_profile || !out_max_range || !out_attack_action_point_cost || !out_uses_ranged_weapon)
        return 0;

    ranged_weapon = player_active_ranged_weapon(&p->character);
    if(ranged_weapon)
    {
        *out_profile = combat_profile_for_character_attack(&p->character, requested_mode);
        if(!combat_profile_is_ranged(out_profile))
        {
            log_add("Current weapon cannot perform ranged attacks.");
            return 0;
        }

        *out_max_range = combat_profile_ranged_range(out_profile);
        if(*out_max_range <= 0)
        {
            log_add("Current ranged weapon has no valid range.");
            return 0;
        }

        *out_uses_ranged_weapon = 1;
    }
    else
    {
        throw_item = player_active_throw_item(&p->character);
        if(!throw_item)
        {
            log_add("No item in hand to throw.");
            return 0;
        }

        *out_profile = combat_profile_for_character_attack(&p->character, requested_mode);
        if(!item_is_weapon(throw_item))
        {
            snprintf(out_profile->weapon_name, sizeof(out_profile->weapon_name), "%s", throw_item->name);
            out_profile->power = 1;
            out_profile->accuracy_bonus = 0;
            out_profile->crit_bonus = 0;
            out_profile->skill_type = WEAPON_SKILL_THROWN;
        }

        *out_max_range = 4;
        if(out_profile->reach_bonus > 0)
            *out_max_range += out_profile->reach_bonus;

        if(!throw_item->throwable)
        {
            out_profile->accuracy_bonus -= 15;
            out_profile->crit_bonus -= 10;
            out_profile->power -= 2;
            if(out_profile->power < 1)
                out_profile->power = 1;
        }

        *out_uses_ranged_weapon = 0;
    }

    if(*out_uses_ranged_weapon && out_profile->ammo_per_shot > 0 && out_profile->ammo_item_name[0])
    {

        {
            log_add("Not enough %s.", out_profile->ammo_item_name);
            return 0;
        }
    }

    *out_attack_action_point_cost = combat_profile_attack_action_point_cost(out_profile);
    if(p->character.actor.action_points < *out_attack_action_point_cost)
    {
        log_add("Not enough action points to fire %s.", out_profile->weapon_name);
        return 0;
    }

    return 1;
}

static int player_consume_ranged_attack_resources(Player* p,
                                                  const CombatProfile* attack_profile,
                                                  int uses_ranged_weapon,
                                                  int attack_action_point_cost)
{
    Item* throw_item;

    if(!p || !attack_profile)
        return 0;

    if(uses_ranged_weapon && attack_profile->ammo_per_shot > 0 && attack_profile->ammo_item_name[0])
    {
        // Slot-based ammo consumption
        int consumed = 0;
        for(int i = 0; i < p->character.equipment_slot_count; i++) {
            if(p->character.equipment_slots[i].slot_type == EQUIP_SLOT_NONE &&
               strcmp(p->character.equipment_slots[i].item.name, attack_profile->ammo_item_name) == 0 &&
               p->character.equipment_slots[i].item.type != ITEM_TYPE_NONE) {
                p->character.equipment_slots[i].item.type = ITEM_TYPE_NONE;
                p->character.equipment_slots[i].item.name[0] = '\0';
                consumed = 1;
                break;
            }
        }
        if(!consumed) {
            log_add("Failed to load required ammunition.");
            return 0;
        }
    }
    else if(!uses_ranged_weapon)
    {
        throw_item = player_active_throw_item(&p->character);
        if(!throw_item)
        {
            log_add("No item in hand to throw.");
            return 0;
        }

        if(throw_item->stackable && throw_item->quantity > 0)
        {
            throw_item->quantity--;
            if(throw_item->quantity <= 0)
                item_init(throw_item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
        }
        else
        {
            item_init(throw_item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
        }
    }

    player_apply_action_point_cost(p, attack_action_point_cost);
    return 1;
}

int player_ranged_attack_tile(Player* p, int target_x, int target_y, int target_z, AttackMode requested_mode)
{
    int dx;
    int dy;
    int max_range;
    int attack_action_point_cost;
    int animation_frames;
    int uses_ranged_weapon;
    CombatProfile player_attack_profile;
    CombatProfile creature_profile;
    MeleeAttackResult player_attack;
    Creature* target;
    Furniture* target_furniture;

    if(!p || !current_area)
        return 0;

    if(target_z != p->character.actor.entity.z)
    {
        log_add("Target is on a different layer.");
        return 0;
    }

    if(area_bounds_blocked(target_x, target_y))
    {
        log_add("Target tile is out of bounds.");
        return 0;
    }

    if(!player_prepare_ranged_attack(p,
                                     requested_mode,
                                     &player_attack_profile,
                                     &max_range,
                                     &attack_action_point_cost,
                                     &uses_ranged_weapon))
        return 0;

    dx = target_x - p->character.actor.entity.x;
    if(dx < 0) dx = -dx;
    dy = target_y - p->character.actor.entity.y;
    if(dy < 0) dy = -dy;

    if(dx > max_range || dy > max_range)
    {
        log_add("Target out of range for ranged attack.");
        return 0;
    }

    if(!map_has_projectile_path(p->character.actor.entity.x,
                                p->character.actor.entity.y,
                                target_x,
                                target_y))
    {
        log_add("Shot blocked by terrain.");
        return 0;
    }

    target = bestiary_creature_at_3d(target_x, target_y, target_z);
    target_furniture = furniture_at_3d(current_area, target_x, target_y, target_z);
    if(target && target->alive && target->template && !creature_is_hostile(target))
    {
        creature_provoke_by_attack(target);
        if(!creature_is_hostile(target))
            return 0;
    }

    if(!player_consume_ranged_attack_resources(p,
                                               &player_attack_profile,
                                               uses_ranged_weapon,
                                               attack_action_point_cost))
        return 0;

    animation_frames = combat_profile_ranged_range(&player_attack_profile);
    if(animation_frames < 1)
        animation_frames = max_range;

    if(target && target->alive && target->template)
    {
        creature_profile = combat_profile_for_actor_unarmed(&target->actor);
        player_attack = combat_resolve_melee_attack(
            &p->character.actor,
            &player_attack_profile,
            &target->actor,
            &creature_profile
        );

        play_player_attack_animation(p,
                                     ATTACK_ANIM_RANGED,
                                     target->actor.entity.x,
                                     target->actor.entity.y,
                                     target->actor.entity.z,
                                     animation_frames);
        if(target->actor.health <= 0)
        {
            target->alive = 0;
            log_add("You killed %s!", target->template->name);
        }

        creatures_take_turns(p);
        return 1;
    }
    else if(target_furniture && furniture_is_destructible(target_furniture))
    {
        return player_resolve_furniture_attack(p,
                                               target_furniture,
                                               &player_attack_profile,
                                               ATTACK_ANIM_RANGED,
                                               animation_frames);
    }
    else
    {
        play_player_attack_animation(p,
                                     ATTACK_ANIM_RANGED,
                                     target_x,
                                     target_y,
                                     target_z,
                                     animation_frames);
        log_add("You fire into the empty ground.");
    }

    creatures_take_turns(p);
    return 1;
}

int player_attack_creature(Player* p, Creature* target, AttackMode requested_mode)
{
    int dx;
    int dy;
    int max_range;
    int attack_action_point_cost;
    int animation_frames;
    CombatProfile player_attack_profile;
    CombatProfile creature_profile;
    MeleeAttackResult player_attack;

    if(!p || !target || !target->alive || !target->template)
        return 0;

    player_attack_profile = combat_profile_for_character_attack(&p->character, requested_mode);
    max_range = combat_profile_melee_range(&player_attack_profile);

    dx = target->actor.entity.x - p->character.actor.entity.x;
    if(dx < 0) dx = -dx;
    dy = target->actor.entity.y - p->character.actor.entity.y;
    if(dy < 0) dy = -dy;

    if(dx > max_range || dy > max_range)
    {
        log_add("Target out of range for melee attack.");
        return 0;
    }

    if(!creature_is_hostile(target))
    {
        creature_provoke_by_attack(target);
        if(!creature_is_hostile(target))
            return 0;  /* Not provoked enough yet; attack blocked this turn. */
    }

    attack_action_point_cost = combat_profile_attack_action_point_cost(&player_attack_profile);
    if(p->character.actor.action_points < attack_action_point_cost)
    {
        log_add("Not enough action points to attack with %s.", player_attack_profile.weapon_name);
        return 0;
    }
    player_apply_action_point_cost(p, attack_action_point_cost);

    creature_profile = combat_profile_for_actor_unarmed(&target->actor);
    player_attack = combat_resolve_melee_attack(
        &p->character.actor,
        &player_attack_profile,
        &target->actor,
        &creature_profile
    );

    animation_frames = combat_profile_melee_range(&player_attack_profile);
    play_player_attack_animation(p,
                                 ATTACK_ANIM_MELEE,
                                 target->actor.entity.x,
                                 target->actor.entity.y,
                                 target->actor.entity.z,
                                 animation_frames);

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
        if(player_attack.stun_applied)
        {
            log_add("%s cannot retaliate while stunned.", target->template->name);
        }
        else
        {
            int retaliation_stamina_cost = combat_profile_attack_stamina_cost(&creature_profile);

            if(target->actor.stamina < retaliation_stamina_cost)
            {
                log_add("%s is too exhausted to retaliate.", target->template->name);
            }
            else
            {
                CombatProfile player_parry_profile = combat_profile_for_character_parry(&p->character);
                MeleeAttackResult retaliation;

                target->actor.stamina -= retaliation_stamina_cost;
                retaliation = combat_resolve_melee_attack(
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
        }
    }

    creatures_take_turns(p);
    return 1;
}

int player_attack_direction(Player* p, int dx, int dy, AttackMode requested_mode)
{
    Creature* target;
    Furniture* furniture_target;
    CombatProfile attack_profile;
    int max_range;
    int attack_action_point_cost;
    int flash_x;
    int flash_y;
    int animation_frames;

    if(!p)
        return 0;

    if(dx == 0 && dy == 0)
        return 0;

    target = bestiary_creature_at_3d(p->character.actor.entity.x + dx,
                                     p->character.actor.entity.y + dy,
                                     p->character.actor.entity.z);
    if(target)
        return player_attack_creature(p, target, requested_mode);

    attack_profile = combat_profile_for_character_attack(&p->character, requested_mode);
    max_range = combat_profile_melee_range(&attack_profile);
    if(max_range < 1)
        max_range = 1;

    flash_x = p->character.actor.entity.x + (dx * max_range);
    flash_y = p->character.actor.entity.y + (dy * max_range);
    animation_frames = max_range;
    if(max_range > 1)
    {
        Creature* reach_target = find_reach_target_in_direction(p, dx, dy, max_range);
        if(reach_target)
            return player_attack_creature(p, reach_target, requested_mode);
    }

    furniture_target = find_attackable_furniture_in_direction(p, dx, dy, max_range);
    if(furniture_target)
    {
        attack_action_point_cost = combat_profile_attack_action_point_cost(&attack_profile);
        if(p->character.actor.action_points < attack_action_point_cost)
        {
            log_add("Not enough action points to attack with %s.", attack_profile.weapon_name);
            return 0;
        }

        player_apply_action_point_cost(p, attack_action_point_cost);
        return player_resolve_furniture_attack(p,
                                               furniture_target,
                                               &attack_profile,
                                               ATTACK_ANIM_MELEE,
                                               animation_frames);
    }

    /* Even without a target, show a brief directional swing flash for feedback. */
    play_player_attack_animation(p,
                                 ATTACK_ANIM_MELEE,
                                 flash_x,
                                 flash_y,
                                 p->character.actor.entity.z,
                                 animation_frames);
    log_add("No creature to attack in that direction.");
    return 0;
}

// Attempt one movement step by delta, treating occupied tiles as non-combat bumps.
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

    if(is_blocked_3d(nx, ny, p->character.actor.entity.z, 1))
        return MOVE_STEP_BLOCKED;

    // Occupied tile: harmless bump, no auto-attack.
    Creature* target = bestiary_creature_at_3d(nx, ny, p->character.actor.entity.z);
    if(target)
    {
        log_add("You bump into %s.", target->template->name);
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

// Attempt sprint movement, spending action points and handling blocked second-step refund.
void player_sprint(Player* p, int dx, int dy, int action_point_cost)
{
    MoveStepResult first_step;
    MoveStepResult second_step;

    if(!p)
        return;

    if(action_point_cost < 0)
        action_point_cost = 0;

    if(p->character.actor.action_points < action_point_cost)
    {
        log_add("Not enough action points to sprint.");
        return;
    }

    player_apply_action_point_cost(p, action_point_cost);

    first_step = player_move_step(p, dx, dy);
    if(first_step == MOVE_STEP_BLOCKED)
    {
        p->character.actor.action_points += action_point_cost;
        if(p->character.actor.action_points > p->character.actor.max_action_points)
            p->character.actor.action_points = p->character.actor.max_action_points;
        log_add("Sprint blocked.");
        return;
    }

    if(first_step == MOVE_STEP_INTERACT)
    {
        p->character.actor.action_points += action_point_cost;
        if(p->character.actor.action_points > p->character.actor.max_action_points)
            p->character.actor.action_points = p->character.actor.max_action_points;
        return;
    }

    if(first_step == MOVE_STEP_COMBAT)
        return;

    second_step = player_move_step(p, dx, dy);
    if(second_step == MOVE_STEP_BLOCKED)
    {
        p->character.actor.action_points += 1;
        if(p->character.actor.action_points > p->character.actor.max_action_points)
            p->character.actor.action_points = p->character.actor.max_action_points;
        log_add("Sprint clipped by terrain. You recover 1 action point.");
    }

    creatures_take_turns(p);
}

