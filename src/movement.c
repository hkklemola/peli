#include "movement.h"
#include "atlas.h"
#include "combat.h"
#include "collision.h"
#include "bestiary.h"
#include "npc.h"
#include "log.h"
#include "tile.h"
#include "tileset.h"
#include "map.h"
#include "player.h"
#include "character.h"
#include "item.h"
#include "item_data.h"
#include "draw.h"
#include "world_items.h"
#include "furniture.h"
#include "interact.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
 *   - player_quickstep: one-step no-turn movement helper.
 *   - player_sprint: multi-step action-based sprint movement.
 */

typedef enum MoveStepResult {
    MOVE_STEP_BLOCKED = 0,
    MOVE_STEP_MOVED,
    MOVE_STEP_COMBAT,
    MOVE_STEP_INTERACT,
    MOVE_STEP_STAIR_PROMPT
} MoveStepResult;

static void movement_spawn_character_corpse(Character* character, const char* display_name)
{
    WorldCorpse corpse;
    char label[64];
    int container_index;

    if(!character || !display_name || !display_name[0] || !current_area)
        return;

    snprintf(label, sizeof(label), "Corpse of %s", display_name);
    container_index = world_container_spawn_3d(current_area->name,
                                               character->actor.entity.x,
                                               character->actor.entity.y,
                                               character->actor.entity.z,
                                               label);
    if(container_index < 0)
    {
        log_add("No room to leave behind %s's corpse.", display_name);
        return;
    }

    for(int i = 0; i < character->equipment_slot_count; i++)
    {
        Item moved_item = character->equipment_slots[i].item;

        if(moved_item.type == ITEM_TYPE_NONE)
            continue;

        moved_item.slot_type = EQUIP_SLOT_NONE;
        if(!world_container_add_item(container_index, &moved_item))
            (void)world_item_drop_3d(&moved_item,
                                     current_area->name,
                                     character->actor.entity.x,
                                     character->actor.entity.y,
                                     character->actor.entity.z);

        item_init(&character->equipment_slots[i].item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
    }

    memset(&corpse, 0, sizeof(corpse));
    corpse.active = 1;
    corpse.type = WORLD_CORPSE_CHARACTER;
    snprintf(corpse.area_name, sizeof(corpse.area_name), "%s", current_area->name);
    snprintf(corpse.source_name, sizeof(corpse.source_name), "%s", display_name);
    corpse.x = character->actor.entity.x;
    corpse.y = character->actor.entity.y;
    corpse.z = character->actor.entity.z;
    corpse.world_container_index = container_index;
    (void)world_corpse_spawn(&corpse);
}

static void movement_sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((unsigned int)(ms * 1000));
#endif
}

static int movement_tile_is_staircase(const Tile* tile)
{
    if(!tile)
        return 0;

    if(strcmp(tile->name, "Staircase") == 0 ||
       strcmp(tile->name, "Stairs Up") == 0 ||
       strcmp(tile->name, "Stairs Down") == 0)
        return 1;

    return tile->symbol == '<' || tile->symbol == '>';
}

static int movement_try_auto_stair_prompt(Player* p)
{
    const Tile* tile;

    if(!p || !current_area)
        return 0;

    tile = map_top_visible_tile(current_area,
                                p->character.actor.entity.x,
                                p->character.actor.entity.y,
                                NULL);
    if(!movement_tile_is_staircase(tile))
        return 0;

    return interact_at(p,
                       p->character.actor.entity.x,
                       p->character.actor.entity.y);
}

static int player_item_available_quantity(const Item* item)
{
    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;

    if(item->stackable)
        return (item->quantity > 0) ? item->quantity : 0;

    return 1;
}

static int movement_primary_damage_type_from_mask(int damage_type_mask)
{
    if(damage_type_mask & DAMAGE_TYPE_PIERCING)
        return DAMAGE_TYPE_PIERCING;
    if(damage_type_mask & DAMAGE_TYPE_SLASHING)
        return DAMAGE_TYPE_SLASHING;
    if(damage_type_mask & DAMAGE_TYPE_CRUSHING)
        return DAMAGE_TYPE_CRUSHING;
    if(damage_type_mask & DAMAGE_TYPE_RANGED)
        return DAMAGE_TYPE_RANGED;
    return DAMAGE_TYPE_NONE;
}

static const Item* player_find_carried_item(const Player* p, const char* item_name, int require_ammo)
{
    if(!p || !item_name || !item_name[0])
        return NULL;

    for(int i = 0; i < p->character.equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &p->character.equipment_slots[i];
        const Item* item = &slot->item;

        if(slot->slot_type != EQUIP_SLOT_NONE || item->type == ITEM_TYPE_NONE)
            continue;
        if(require_ammo && !item->is_ammo)
            continue;
        if(strcmp(item->name, item_name) != 0)
            continue;
        if(player_item_available_quantity(item) <= 0)
            continue;

        return item;
    }

    return NULL;
}

static int player_ammo_cost_per_shot(const CombatProfile* profile)
{
    if(!profile || profile->ammo_item_name[0] == '\0')
        return 0;

    return (profile->ammo_per_shot > 0) ? profile->ammo_per_shot : 1;
}

static int player_count_carried_item_quantity(const Player* p, const char* item_name, int require_ammo)
{
    int total = 0;

    if(!p || !item_name || !item_name[0])
        return 0;

    for(int i = 0; i < p->character.equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &p->character.equipment_slots[i];
        const Item* item = &slot->item;
        int available;

        if(slot->slot_type != EQUIP_SLOT_NONE || item->type == ITEM_TYPE_NONE)
            continue;
        if(require_ammo && !item->is_ammo)
            continue;
        if(strcmp(item->name, item_name) != 0)
            continue;

        available = player_item_available_quantity(item);
        if(available <= 0)
            continue;

        total += available;
    }

    return total;
}

static int player_consume_carried_item_quantity(Player* p, const char* item_name, int amount, int require_ammo)
{
    if(!p || !item_name || !item_name[0] || amount <= 0)
        return 0;

    for(int i = 0; i < p->character.equipment_slot_count && amount > 0; ++i)
    {
        EquipmentSlot* slot = &p->character.equipment_slots[i];
        Item* item = &slot->item;
        int available;
        int consume_amount;

        if(slot->slot_type != EQUIP_SLOT_NONE || item->type == ITEM_TYPE_NONE)
            continue;
        if(require_ammo && !item->is_ammo)
            continue;
        if(strcmp(item->name, item_name) != 0)
            continue;

        available = player_item_available_quantity(item);
        if(available <= 0)
            continue;

        consume_amount = (available < amount) ? available : amount;
        available -= consume_amount;
        amount -= consume_amount;

        if(item->stackable)
        {
            if(available > 0)
                item->quantity = available;
            else
                item_init(item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
        }
        else
        {
            item_init(item, "None", '?', -1, -1, ITEM_TYPE_NONE, 0, 0);
        }
    }

    return amount == 0;
}

static void mark_attack_animation_dirty(Player* p, int target_x, int target_y)
{
    int origin_x;
    int origin_y;
    int min_x;
    int min_y;
    int max_x;
    int max_y;

    if(!p)
        return;

    origin_x = p->character.actor.entity.x;
    origin_y = p->character.actor.entity.y;
    min_x = (origin_x < target_x) ? origin_x : target_x;
    min_y = (origin_y < target_y) ? origin_y : target_y;
    max_x = (origin_x > target_x) ? origin_x : target_x;
    max_y = (origin_y > target_y) ? origin_y : target_y;

    draw_mark_world_rect_dirty(min_x - 1, min_y - 1, max_x + 1, max_y + 1);
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
        mark_attack_animation_dirty(p, target_x, target_y);
        draw_world_viewport_only(p);
        movement_sleep_ms(35);
        player_attack_animation_advance(p);
    }

    mark_attack_animation_dirty(p, target_x, target_y);
    draw_world_viewport_only(p);
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

        if(actor_is_unconscious(&creature->actor))
        {
            creature->actor.stamina = actor_clamp_stamina_value(&creature->actor, creature->actor.stamina + 1);
            continue;
        }

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

    npcs_take_turns(p);
    interact_process_station_turn(p);

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

    if(result->no_damage_hit)
    {
        if(attacker_is_player)
            log_add("You %s%s %s, but the blow fails to penetrate armor.", result->critical ? "critically " : "", attack_mode_verb(result->attack_mode), defender_name);
        else if(defender_is_player)
            log_add("%s %s you, but the blow fails to penetrate your armor.", attacker_name, result->critical ? "critically hits" : attack_mode_verb(result->attack_mode));
        else
            log_add("%s %s %s, but the blow fails to penetrate armor.", attacker_name, result->critical ? "critically hits" : attack_mode_verb(result->attack_mode), defender_name);
    }
    else if(attacker_is_player)
        log_add("You %s%s %s for %d damage (%s).", result->critical ? "critically " : "", attack_mode_verb(result->attack_mode), defender_name, result->damage, damage_type_name(result->damage_type));
    else if(defender_is_player)
        log_add("%s %s you for %d damage.", attacker_name, result->critical ? "critically hits" : attack_mode_verb(result->attack_mode), result->damage);
    else
        log_add("%s %s %s for %d damage.", attacker_name, result->critical ? "critically hits" : attack_mode_verb(result->attack_mode), defender_name, result->damage);

    if(result->stamina_damage > 0)
    {
        if(defender_is_player)
            log_add("%d damage is converted into stamina shock.", result->stamina_damage);
        else
            log_add("%s absorbs %d damage as stamina shock.", defender_name, result->stamina_damage);
    }

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
    int levels_gained = 0;

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
    raw_damage = combat_roll_attack_value(&p->character.actor, attack_profile);

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

    if(furniture->type == FURNITURE_TARGET_DUMMY)
    {
        int regular_xp = combat_weapon_skill_xp_for_hit(0, damage_dealt <= 0);
        int training_xp = regular_xp / 2;

        levels_gained = actor_gain_weapon_skill_xp(&p->character.actor,
                                                   attack_profile->skill_type,
                                                   training_xp);
        log_skill_gain(p->character.name, 1, attack_profile->skill_type, levels_gained);
    }

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

static const Item* player_active_melee_weapon(const Character* c)
{
    const Item* right;
    const Item* left;

    if(!c)
        return NULL;

    right = &c->equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    left = &c->equipment_slots[EQUIP_SLOT_OFF_HAND].item;

    if(right->type == ITEM_TYPE_WEAPON_TWO_HANDED)
        return right;
    if(left->type == ITEM_TYPE_WEAPON_TWO_HANDED)
        return left;
    if(item_is_weapon(right))
        return right;
    if(item_is_weapon(left))
        return left;

    return NULL;
}

static int movement_is_tree_tile(const Tile* tile)
{
    return tile_is_tree(tile);
}

static Tile* find_tree_in_direction(const Player* p, int dx, int dy, int max_range, int* out_x, int* out_y)
{
    int x;
    int y;

    if(out_x)
        *out_x = 0;
    if(out_y)
        *out_y = 0;

    if(!p || !current_area || max_range < 1)
        return NULL;

    x = p->character.actor.entity.x;
    y = p->character.actor.entity.y;

    for(int step = 1; step <= max_range; ++step)
    {
        int tx = x + (dx * step);
        int ty = y + (dy * step);
        Tile* wall_tile;

        if(area_bounds_blocked(tx, ty))
            break;

        wall_tile = map_tile_at_layer_z(current_area, tx, ty, p->character.actor.entity.z, TILE_LAYER_WALL);
        if(movement_is_tree_tile(wall_tile))
        {
            if(out_x)
                *out_x = tx;
            if(out_y)
                *out_y = ty;
            return wall_tile;
        }

        if(is_blocked_3d(tx, ty, p->character.actor.entity.z, 1))
            break;
    }

    return NULL;
}

static TreeDurabilityState* movement_tree_state_at(Area* area,
                                                    int x,
                                                    int y,
                                                    int z,
                                                    TreeSpecies species,
                                                    int create)
{
    TreeDurabilityState* free_entry = NULL;
    const TreeSpeciesInfo* species_info;

    if(!area)
        return NULL;

    if(species <= TREE_SPECIES_NONE || species >= TREE_SPECIES_COUNT)
        species = TREE_SPECIES_OAK;
    species_info = tree_species_info(species);

    for(int i = 0; i < MAX_AREA_TREE_STATES; ++i)
    {
        TreeDurabilityState* entry = &area->tree_states[i];

        if(!entry->active)
        {
            if(!free_entry)
                free_entry = entry;
            continue;
        }

        if(entry->x == x && entry->y == y && entry->z == z)
        {
            if(entry->species <= TREE_SPECIES_NONE || entry->species >= TREE_SPECIES_COUNT)
                entry->species = species;
            return entry;
        }
    }

    if(!create || !free_entry)
        return NULL;

    free_entry->active = 1;
    free_entry->x = x;
    free_entry->y = y;
    free_entry->z = z;
    free_entry->species = species;
    free_entry->structure_points = species_info->max_structure_points;
    area->tree_state_count++;
    return free_entry;
}

static void movement_clear_tree_state(Area* area, TreeDurabilityState* tree_state)
{
    if(!area || !tree_state || !tree_state->active)
        return;

    memset(tree_state, 0, sizeof(*tree_state));
    if(area->tree_state_count > 0)
        area->tree_state_count--;
}

static int movement_find_adjacent_open_tile(int center_x, int center_y, int z, int* out_x, int* out_y)
{
    static const int offsets[8][2] = {
        { 0, -1 },
        { 1, 0 },
        { 0, 1 },
        { -1, 0 },
        { 1, -1 },
        { 1, 1 },
        { -1, 1 },
        { -1, -1 }
    };

    if(out_x)
        *out_x = center_x;
    if(out_y)
        *out_y = center_y;

    if(!current_area)
        return 0;

    for(int i = 0; i < 8; ++i)
    {
        int tx = center_x + offsets[i][0];
        int ty = center_y + offsets[i][1];

        if(area_bounds_blocked(tx, ty))
            continue;
        if(is_blocked_3d(tx, ty, z, 0))
            continue;

        if(out_x)
            *out_x = tx;
        if(out_y)
            *out_y = ty;
        return 1;
    }

    return 0;
}

static int player_attack_profile_can_chop_tree(const Character* c, const CombatProfile* attack_profile)
{
    const Item* active_item = player_active_melee_weapon(c);

    if(!active_item || !attack_profile)
        return 0;

    if(item_tool_non_weapon_skill(active_item) != NON_WEAPON_SKILL_LUMBERJACKING)
        return 0;

    return attack_profile->skill_type == WEAPON_SKILL_AXE || attack_profile->skill_type == WEAPON_SKILL_AXE_2H;
}

static int player_chop_tree(Player* p,
                            int target_x,
                            int target_y,
                            const CombatProfile* attack_profile,
                            int animation_frames)
{
    const ItemTemplate* lumber_template;
    const TreeSpeciesInfo* species_info;
    TreeDurabilityState* tree_state;
    Tile* target_tile;
    Item dropped_item;
    int attack_action_point_cost;
    int raw_damage;
    int damage_dealt;
    int drop_x = target_x;
    int drop_y = target_y;
    int z;
    int levels_gained;
    TreeSpecies species;

    if(!p || !attack_profile || !current_area)
        return 0;

    z = p->character.actor.entity.z;
    target_tile = map_tile_at_layer_z(current_area, target_x, target_y, z, TILE_LAYER_WALL);
    if(!movement_is_tree_tile(target_tile))
        return 0;

    species = tile_tree_species(target_tile);
    if(species == TREE_SPECIES_NONE)
        species = TREE_SPECIES_OAK;
    species_info = tree_species_info(species);

    if(!player_attack_profile_can_chop_tree(&p->character, attack_profile))
    {
        log_add("You need an axe tool to cut down trees.");
        return 0;
    }

    attack_action_point_cost = combat_profile_attack_action_point_cost(attack_profile);
    if(p->character.actor.action_points < attack_action_point_cost)
    {
        log_add("Not enough action points to chop with %s.", attack_profile->weapon_name);
        return 0;
    }

    tree_state = movement_tree_state_at(current_area, target_x, target_y, z, species, 1);
    if(!tree_state)
    {
        log_add("You cannot get a solid bite into this tree right now.");
        return 0;
    }

    player_apply_action_point_cost(p, attack_action_point_cost);
    raw_damage = combat_roll_attack_value(&p->character.actor, attack_profile);
    damage_dealt = raw_damage - species_info->hardness;
    if(damage_dealt < 0)
        damage_dealt = 0;

    play_player_attack_animation(p,
                                 ATTACK_ANIM_MELEE,
                                 target_x,
                                 target_y,
                                 z,
                                 animation_frames);

    if(damage_dealt > 0)
    {
        tree_state->structure_points -= damage_dealt;
        if(tree_state->structure_points < 0)
            tree_state->structure_points = 0;
    }

    levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor,
                                                   NON_WEAPON_SKILL_LUMBERJACKING,
                                                   (damage_dealt > 0) ? 2 : 1);

    if(tree_state->structure_points <= 0)
    {
        tree_state->structure_points = 0;
        tree_state->species = species;

        if(!atlas_set_tile_mutation_at_z(current_area, target_x, target_y, z, TILE_MUTATION_STATE_TREE_STUMP))
        {
            tree_state->structure_points = 1;
            log_add("You crack the trunk of the %s, but it stays standing for now.", species_info->tree_name);
            return 0;
        }

        lumber_template = item_template_by_name(species_info->log_name);
        if(!lumber_template)
            lumber_template = item_template_by_name("Log");
        if(!lumber_template)
            lumber_template = item_template_by_name("Wood Log");

        if(lumber_template && movement_find_adjacent_open_tile(target_x, target_y, z, &drop_x, &drop_y))
        {
            item_init_from_template(&dropped_item, lumber_template, drop_x, drop_y);
            if(world_item_drop_3d(&dropped_item, current_area->name, drop_x, drop_y, z))
                log_add("You strike the %s for %d damage and fell it! %s falls nearby.", species_info->tree_name, damage_dealt, dropped_item.name);
            else
                log_add("You strike the %s for %d damage and fell it, but can't drop the log here.", species_info->tree_name, damage_dealt);
        }
        else
        {
            log_add("You strike the %s for %d damage and fell it, but there is no free space nearby for the log.", species_info->tree_name, damage_dealt);
        }
    }
    else if(damage_dealt > 0)
    {
        log_add("You strike the %s for %d damage. (%d/%d SP, Hardness %d)",
                species_info->tree_name,
                damage_dealt,
                tree_state->structure_points,
                species_info->max_structure_points,
                species_info->hardness);
    }
    else
    {
        log_add("The %s shrugs off the blow. (Hardness %d, %d/%d SP)",
                species_info->tree_name,
                species_info->hardness,
                tree_state->structure_points,
                species_info->max_structure_points);
    }

    if(levels_gained > 0)
    {
        log_add("Your %s increases to %d.",
                non_weapon_skill_name(NON_WEAPON_SKILL_LUMBERJACKING),
                actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_LUMBERJACKING));
    }

    return 1;
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

    if(*out_uses_ranged_weapon && out_profile->ammo_item_name[0])
    {
        const Item* loaded_ammo = player_find_carried_item(p, out_profile->ammo_item_name, 1);
        const ItemTemplate* ammo_template = NULL;
        int ammo_cost = player_ammo_cost_per_shot(out_profile);
        int available_ammo = player_count_carried_item_quantity(p, out_profile->ammo_item_name, 1);
        int ammo_damage_mask = DAMAGE_TYPE_NONE;

        if(available_ammo < ammo_cost)
        {
            log_add("Not enough %s.", out_profile->ammo_item_name);
            return 0;
        }

        if(loaded_ammo)
            ammo_damage_mask = loaded_ammo->damage_type_mask;
        if(ammo_damage_mask == DAMAGE_TYPE_NONE)
        {
            ammo_template = item_template_by_name(out_profile->ammo_item_name);
            if(ammo_template)
                ammo_damage_mask = ammo_template->damage_type_mask;
        }

        if(ammo_damage_mask != DAMAGE_TYPE_NONE)
        {
            out_profile->damage_type_mask = ammo_damage_mask;
            out_profile->active_damage_type = movement_primary_damage_type_from_mask(ammo_damage_mask);
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

    if(uses_ranged_weapon && attack_profile->ammo_item_name[0])
    {
        int ammo_cost = player_ammo_cost_per_shot(attack_profile);
        if(!player_consume_carried_item_quantity(p,
                                                 attack_profile->ammo_item_name,
                                                 ammo_cost,
                                                 1))
        {
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

    animation_frames = (dx > dy) ? dx : dy;
    if(animation_frames < 1)
        animation_frames = 1;

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
        log_attack_result("You", 1, target->template->name, 0, &player_attack);
        log_skill_gain(p->character.name, 1, player_attack.attack_skill_type, player_attack.attacker_levels_gained);
        log_skill_gain(target->template->name, 0, player_attack.parry_skill_type, player_attack.defender_levels_gained);
        if(target->actor.health <= 0)
        {
            log_add("You killed %s!", target->template->name);
            creature_handle_death(target);
        }

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

    if(actor_is_unconscious(&p->character.actor))
    {
        log_add("You are unconscious and cannot attack.");
        return 0;
    }

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
        log_add("You killed %s!", target->template->name);
        creature_handle_death(target);
    }
    else
    {
        if(actor_is_unconscious(&target->actor))
        {
            log_add("%s is unconscious and cannot retaliate.", target->template->name);
            return 1;
        }

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

                target->actor.stamina = actor_clamp_stamina_value(&target->actor,
                                                                   target->actor.stamina - retaliation_stamina_cost);
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
                    movement_spawn_character_corpse(&p->character, p->character.name);
                    log_add("You died! Game over.");
                    exit(0);
                }
                if(actor_is_unconscious(&p->character.actor))
                    log_add("You collapse unconscious from exhaustion!");
            }
        }
    }

    return 1;
}

int player_attack_direction(Player* p, int dx, int dy, AttackMode requested_mode)
{
    Creature* target;
    Furniture* furniture_target;
    Tile* tree_target;
    CombatProfile attack_profile;
    int max_range;
    int attack_action_point_cost;
    int flash_x;
    int flash_y;
    int tree_x = 0;
    int tree_y = 0;
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

    tree_target = find_tree_in_direction(p, dx, dy, max_range, &tree_x, &tree_y);
    if(tree_target)
        return player_chop_tree(p, tree_x, tree_y, &attack_profile, animation_frames);

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

static void movement_update_dragged_world_item(Player* p, int previous_x, int previous_y, int previous_z)
{
    WorldItem* dragged_item;

    if(!p)
        return;

    if(p->dragged_world_item_index < 0 || p->dragged_world_item_index >= MAX_WORLD_ITEMS)
    {
        p->dragged_world_item_index = -1;
        return;
    }

    if(!current_area)
    {
        p->dragged_world_item_index = -1;
        return;
    }

    dragged_item = &world_items[p->dragged_world_item_index];
    if(!dragged_item->active || strcmp(dragged_item->area_name, current_area->name) != 0)
    {
        p->dragged_world_item_index = -1;
        return;
    }

    dragged_item->item.object.base.x = previous_x;
    dragged_item->item.object.base.y = previous_y;
    dragged_item->item.object.base.z = previous_z;
}

// Attempt one movement step by delta, treating occupied tiles as non-combat bumps.
static MoveStepResult player_move_step(Player* p, int dx, int dy)
{
    int old_x = p->character.actor.entity.x;
    int old_y = p->character.actor.entity.y;
    int old_z = p->character.actor.entity.z;
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

    {
        NPC* npc = npc_at_3d(nx, ny, p->character.actor.entity.z);
        if(npc)
        {
            log_add("You bump into %s.", npc_display_name(npc));
            return MOVE_STEP_INTERACT;
        }
    }

    // Tile is free → move player
    p->character.actor.entity.x = nx;
    p->character.actor.entity.y = ny;
    movement_update_dragged_world_item(p, old_x, old_y, old_z);

    {
        const Tile* tile = map_top_visible_tile(current_area, nx, ny, NULL);
        if(movement_tile_is_staircase(tile))
            return MOVE_STEP_STAIR_PROMPT;
    }

    return MOVE_STEP_MOVED;
}

// Attempt to move player by delta, resolving combat when target tile is occupied.
void player_move(Player* p, int dx, int dy)
{
    MoveStepResult result;

    if(!p)
        return;

    result = player_move_step(p, dx, dy);
    if(result == MOVE_STEP_MOVED || result == MOVE_STEP_STAIR_PROMPT)
        player_add_exhaustion(p, 1);
    if(result == MOVE_STEP_STAIR_PROMPT && movement_try_auto_stair_prompt(p))
        return;

    creatures_take_turns(p);
}

// Attempt one-step quickstep movement without advancing turns.
void player_quickstep(Player* p, int dx, int dy)
{
    MoveStepResult step;

    if(!p)
        return;

    step = player_move_step(p, dx, dy);
    if(step == MOVE_STEP_MOVED || step == MOVE_STEP_STAIR_PROMPT)
        player_add_exhaustion(p, 1);
    if(step == MOVE_STEP_STAIR_PROMPT)
        (void)movement_try_auto_stair_prompt(p);
}

// Attempt sprint movement, spending action points and advancing turns afterward.
void player_sprint(Player* p, int dx, int dy, int action_point_cost, int step_count)
{
    MoveStepResult step;
    int moved_steps = 0;

    if(!p)
        return;

    if(action_point_cost < 0)
        action_point_cost = 0;
    if(step_count < 1)
        step_count = 1;

    if(p->character.actor.action_points < action_point_cost)
    {
        log_add("Not enough action points to sprint.");
        return;
    }

    player_apply_action_point_cost(p, action_point_cost);

    for(int i = 0; i < step_count; i++)
    {
        step = player_move_step(p, dx, dy);
        if(step == MOVE_STEP_BLOCKED)
        {
            if(i == 0)
            {
                p->character.actor.action_points += action_point_cost;
                if(p->character.actor.action_points > p->character.actor.max_action_points)
                    p->character.actor.action_points = p->character.actor.max_action_points;
                log_add("Sprint blocked.");
                return;
            }
            break;
        }

        if(step == MOVE_STEP_INTERACT)
        {
            if(i == 0)
            {
                p->character.actor.action_points += action_point_cost;
                if(p->character.actor.action_points > p->character.actor.max_action_points)
                    p->character.actor.action_points = p->character.actor.max_action_points;
                return;
            }
            break;
        }

        if(step == MOVE_STEP_MOVED || step == MOVE_STEP_STAIR_PROMPT)
        {
            player_add_exhaustion(p, 1);
            moved_steps++;
        }

        if(step == MOVE_STEP_STAIR_PROMPT)
        {
            (void)movement_try_auto_stair_prompt(p);
            break;
        }

        if(step == MOVE_STEP_COMBAT)
            break;
    }

    if(moved_steps > 0)
        creatures_take_turns(p);
}

