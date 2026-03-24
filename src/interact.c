#include "interact.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "atlas.h"
#include "bestiary.h"
#include "draw.h"
#include "input.h"
#include "inventory.h"
#include "item_data.h"
#include "keybind_helpers.h"
#include "log.h"
#include "map.h"
#include "movement.h"
#include "target_lock.h"
#include "ui_overlay.h"
#include "world_items.h"

/**
 * @file interact.c
 * @brief Implementation of world object interaction system (doors, items, creatures).
 *
 * Handles interaction range checking, priority ordering of interactive targets,
 * and execution of interactions like opening doors, picking up items, or examining creatures.
 */

/**
 * @brief Get the maximum interaction range in tiles.
 * @return The default interaction range (currently INTERACT_RANGE_DEFAULT from header).
 */
static int interact_max_range(void)
{
    return INTERACT_RANGE_DEFAULT;
}

/**
 * @brief Check if a target is within interaction range using Chebyshev distance.
 * @param px The player's x-coordinate.
 * @param py The player's y-coordinate.
 * @param tx The target's x-coordinate.
 * @param ty The target's y-coordinate.
 * @param range The maximum interaction range in tiles.
 * @return 1 if target is within range, 0 otherwise.
 * @note Uses Chebyshev distance (max of absolute differences), allowing diagonal interactions.
 */
static int interact_in_range(int px, int py, int tx, int ty, int range)
{
    int dx = abs(tx - px);
    int dy = abs(ty - py);
    return dx <= range && dy <= range;
}

static int interact_current_area_index(void)
{
    if(!current_area)
        return -1;

    for(int i = 0; i < MAX_AREAS; i++)
    {
        if(&atlas[i] == current_area)
            return i;
    }

    return -1;
}

static int tile_is_stairs_up(const Tile* tile)
{
    return tile && strcmp(tile->name, "Stairs Up") == 0;
}

static int tile_is_stairs_down(const Tile* tile)
{
    return tile && strcmp(tile->name, "Stairs Down") == 0;
}

// Resolve user-facing target name in interaction priority order.
static const char* interact_target_name_at(int tx, int ty)
{
    int pz = player.character.actor.entity.z;
    Creature* creature = bestiary_creature_at_3d(tx, ty, pz);
    const Tile* tile;
    WorldItem* world_item;
    WorldContainer* world_container;

    if(creature && creature->alive && creature->template)
        return creature->template->name;

    if(!current_area || tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return NULL;

    tile = map_top_visible_tile(current_area, tx, ty, NULL);
    if(tile->interactable && tile->name[0])
        return tile->name;

    world_item = world_item_at_3d(tx, ty, pz);
    if(world_item && world_item->active)
        return world_item->item.name;

    world_container = world_container_at(tx, ty);
    if(world_container && world_container->active)
        return world_container->label;

    return NULL;
}

// Return 1 when tile behaves like a door in current tile schema.
static int tile_is_door(const Tile* tile)
{
    if(!tile)
        return 0;

    if(strcmp(tile->name, "Door") == 0 || strcmp(tile->name, "Open Door") == 0)
        return 1;

    return 0;
}

typedef enum InteractionActionType {
    INTERACTION_ACTION_OPEN_CONTAINER = 0,
    INTERACTION_ACTION_PICK_UP_ITEM,
    INTERACTION_ACTION_EQUIP_FROM_GROUND,
    INTERACTION_ACTION_EXAMINE_ITEM,
    INTERACTION_ACTION_PET,
    INTERACTION_ACTION_FEED,
    INTERACTION_ACTION_TREAT_INJURY,
    INTERACTION_ACTION_TALK,
    INTERACTION_ACTION_GIVE_ITEM,
    INTERACTION_ACTION_TILE_USE,
} InteractionActionType;

typedef struct InteractionAction {
    InteractionActionType type;
    int enabled;
    char label[80];
    char disabled_reason[80];
    Creature* creature;
    WorldItem* world_item;
    WorldContainer* world_container;
    int tx;
    int ty;
} InteractionAction;

#define INTERACTION_ACTIONS_MAX 24

static int interact_item_type_is_container(ItemType type)
{
    return type == ITEM_TYPE_CONTAINER_BACKPACK || type == ITEM_TYPE_CONTAINER_BELTPOUCH;
}

static void interaction_action_add(InteractionAction* actions,
                                   int* count,
                                   InteractionActionType type,
                                   int enabled,
                                   const char* label,
                                   const char* disabled_reason,
                                   Creature* creature,
                                   WorldItem* world_item,
                                   WorldContainer* world_container,
                                   int tx,
                                   int ty)
{
    InteractionAction* action;

    if(!actions || !count || *count < 0 || *count >= INTERACTION_ACTIONS_MAX)
        return;

    action = &actions[*count];
    action->type = type;
    action->enabled = enabled;
    action->creature = creature;
    action->world_item = world_item;
    action->world_container = world_container;
    action->tx = tx;
    action->ty = ty;

    snprintf(action->label, sizeof(action->label), "%s", label ? label : "Action");
    if(disabled_reason && disabled_reason[0] != '\0')
        snprintf(action->disabled_reason, sizeof(action->disabled_reason), "%s", disabled_reason);
    else
        action->disabled_reason[0] = '\0';

    (*count)++;
}

// Try interacting with a creature first, per inspect interaction priority.
static int interact_creature(Player* p, Creature* creature)
{
    if(!p || !creature || !creature->alive || !creature->template)
        return 0;

    if(creature_is_hostile(creature))
    {
        log_add("%s is hostile. Maybe use attack instead.", creature->template->name);
        return 0;
    }

    {
        int husbandry = p->character.actor.husbandry_skill;
        creature_apply_pet_event(creature, husbandry);

        if(creature->template->tamable)
        {
            log_add("You pet the %s. [%s]", creature->template->name, taming_stage_name(creature->taming_stage));
            /* Grant husbandry XP: 5 XP per pet, next level at 100 * (current_level + 1) */
            p->character.actor.husbandry_skill_xp += 5;
            if(p->character.actor.husbandry_skill_xp >= 100 * (p->character.actor.husbandry_skill + 1))
            {
                p->character.actor.husbandry_skill_xp = 0;
                p->character.actor.husbandry_skill++;
                log_add("Your husbandry skill improved to %d!", p->character.actor.husbandry_skill);
            }
        }
        else
        {
            log_add("You pet the %s.", creature->template->name);
        }
    }

    creatures_take_turns(p);
    return 1;
}

// Open one world container and allow item pickup via overlay.
static int interact_open_container(Player* p, WorldContainer* container)
{
    int selected = 0;
    int scroll_offset = 0;
    int took_any = 0;
    int need_world_redraw = 1;
    char title[96];

    if(!p || !container || !container->active)
        return 0;

    snprintf(title, sizeof(title), "Container - %s", container->label);

    while(1)
    {
        int content_lines;
        int status_line;
        int visible_rows;
        int max_scroll;
        int line_i = 0;

        if(need_world_redraw)
        {
            draw_world(p);
            ui_overlay_draw_frame(title);
            ui_overlay_invalidate_cache();
            need_world_redraw = 0;
        }

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        visible_rows = status_line;
        max_scroll = container->item_count - visible_rows;
        if(max_scroll < 0)
            max_scroll = 0;

        if(container->item_count <= 0)
        {
            selected = 0;
            scroll_offset = 0;
            if(line_i < status_line) ui_overlay_draw_line(line_i++, "This container is empty.");
            while(line_i < status_line) ui_overlay_draw_line(line_i++, "");
            ui_overlay_draw_line(status_line, "Esc/Q close | Enter take selected | W/S move");
            ui_overlay_draw_global_hotkeys();
        }
        else
        {
            if(selected < 0) selected = 0;
            if(selected >= container->item_count) selected = container->item_count - 1;
            if(selected < scroll_offset)
                scroll_offset = selected;
            if(visible_rows > 0 && selected >= scroll_offset + visible_rows)
                scroll_offset = selected - visible_rows + 1;
            if(scroll_offset < 0)
                scroll_offset = 0;
            if(scroll_offset > max_scroll)
                scroll_offset = max_scroll;

            for(int i = scroll_offset; i < container->item_count && line_i < status_line; i++)
            {
                char line[128];
                snprintf(line, sizeof(line), "%c %2d. %-28s x%d",
                         (i == selected) ? '>' : ' ',
                         i + 1,
                         container->items[i].name,
                         container->items[i].quantity > 0 ? container->items[i].quantity : 1);
                ui_overlay_draw_line(line_i++, line);
            }

            while(line_i < status_line)
                ui_overlay_draw_line(line_i++, "");

            ui_overlay_draw_line(status_line, "Esc/Q close | Enter take | W/S move | PgUp/PgDn jump | Home/End");
            ui_overlay_draw_global_hotkeys();
        }

        {
            int key = read_input_key();

            if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
                break;

            if(container->item_count <= 0)
                continue;

            if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
            {
                if(selected > 0) selected--;
                continue;
            }

            if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
            {
                if(selected < container->item_count - 1) selected++;
                continue;
            }

            if(key == INPUT_KEY_PGUP)
            {
                selected -= (visible_rows > 0) ? visible_rows : 1;
                if(selected < 0)
                    selected = 0;
                continue;
            }

            if(key == INPUT_KEY_PGDN)
            {
                selected += (visible_rows > 0) ? visible_rows : 1;
                if(selected >= container->item_count)
                    selected = container->item_count - 1;
                continue;
            }

            if(key == INPUT_KEY_HOME)
            {
                selected = 0;
                continue;
            }

            if(key == INPUT_KEY_END)
            {
                selected = container->item_count - 1;
                continue;
            }

            if(key == 13)
            {
                Item picked_item;
                int container_index = world_container_index_of(container);

                if(container_index < 0)
                    continue;

                if(world_container_remove_item(container_index, selected, &picked_item))
                {
                    if(inventory_add(&p->character, &picked_item))
                    {
                        log_add("You take %s from %s.", picked_item.name, container->label);
                        took_any = 1;
                        need_world_redraw = 1;

                        if(container->item_count <= 0)
                        {
                            (void)world_container_remove(container_index);
                            break;
                        }

                        if(selected >= container->item_count)
                            selected = container->item_count - 1;
                        if(selected < 0)
                            selected = 0;
                        max_scroll = container->item_count - visible_rows;
                        if(max_scroll < 0)
                            max_scroll = 0;
                        if(scroll_offset > max_scroll)
                            scroll_offset = max_scroll;
                    }
                    else
                    {
                        log_add("No space in inventory for %s.", picked_item.name);
                        (void)world_container_add_item(container_index, &picked_item);
                        need_world_redraw = 1;
                    }
                }
            }
        }
    }

    return took_any;
}

static int interact_tile(Player* p, int tx, int ty);

static int interaction_show_menu(Player* p, const char* target_name, InteractionAction* actions, int action_count)
{
    int selected = 0;
    int scroll_offset = 0;
    char title[96];

    if(!p || !actions || action_count <= 0)
        return -1;

    snprintf(title, sizeof(title), "Interact - %s", (target_name && target_name[0]) ? target_name : "Target");

    while(1)
    {
        int content_lines;
        int status_line;
        int visible_rows;
        int max_scroll;
        int line_i = 0;
        int key;

        draw_world(p);
        ui_overlay_draw_frame(title);
        ui_overlay_invalidate_cache();

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        visible_rows = status_line;
        max_scroll = action_count - visible_rows;
        if(max_scroll < 0)
            max_scroll = 0;

        if(selected < 0)
            selected = 0;
        if(selected >= action_count)
            selected = action_count - 1;

        if(selected < scroll_offset)
            scroll_offset = selected;
        if(visible_rows > 0 && selected >= scroll_offset + visible_rows)
            scroll_offset = selected - visible_rows + 1;
        if(scroll_offset < 0)
            scroll_offset = 0;
        if(scroll_offset > max_scroll)
            scroll_offset = max_scroll;

        for(int i = scroll_offset; i < action_count && line_i < status_line; i++)
        {
            char line[128];
            const char* state_tag = actions[i].enabled ? "" : " [disabled]";

            snprintf(line, sizeof(line), "%c %s%s",
                     (i == selected) ? '>' : ' ',
                     actions[i].label,
                     state_tag);
            ui_overlay_draw_line(line_i++, line);
        }

        while(line_i < status_line)
            ui_overlay_draw_line(line_i++, "");

        if(actions[selected].enabled)
            ui_overlay_draw_line(status_line, "Enter confirm | W/S move | Q/Esc cancel");
        else
        {
            char status[128];
            snprintf(status, sizeof(status), "Unavailable: %s | W/S move | Q/Esc cancel",
                     actions[selected].disabled_reason[0] ? actions[selected].disabled_reason : "Not implemented yet");
            ui_overlay_draw_line(status_line, status);
        }

        ui_overlay_draw_global_hotkeys();

        key = read_input_key();

        if(KEYBIND_CANCEL(key) || key == 'e' || key == 'E')
            return -1;

        if(KEYBIND_UP(key))
        {
            if(selected > 0)
                selected--;
            continue;
        }

        if(KEYBIND_DOWN(key))
        {
            if(selected < action_count - 1)
                selected++;
            continue;
        }

        if(KEYBIND_CONFIRM(key))
        {
            if(!actions[selected].enabled)
            {
                log_add("%s", actions[selected].disabled_reason[0] ? actions[selected].disabled_reason : "Not implemented yet.");
                continue;
            }
            return selected;
        }
    }
}

static int interact_pick_up_world_item(Player* p, WorldItem* world_item)
{
    int world_index;

    if(!p || !world_item || !world_item->active)
        return 0;

    if(!inventory_add(&p->character, &world_item->item))
    {
        log_add("No space in inventory for %s.", world_item->item.name);
        return 0;
    }

    world_index = world_item_index_of(world_item);
    if(world_index >= 0)
        (void)world_item_remove(world_index);

    log_add("Picked up %s.", world_item->item.name);
    return 1;
}

static int interact_equipped_container_slot(Character* c,
                                            const int* before_empty,
                                            ItemType expected_type,
                                            const char* expected_name)
{
    if(!c || !before_empty)
        return -1;

    for(int i = 0; i < MAX_ATTACHED_CONTAINERS; i++)
    {
        if(!before_empty[i])
            continue;
        if(c->containers[i].item.type == ITEM_TYPE_NONE)
            continue;
        if(expected_type != ITEM_TYPE_NONE && c->containers[i].item.type != expected_type)
            continue;
        if(expected_name && expected_name[0] && strcmp(c->containers[i].item.name, expected_name) != 0)
            continue;
        return i;
    }

    return -1;
}

static int interact_transfer_world_container_to_equipped(Character* c, WorldContainer* world_container, int equipped_ci)
{
    int world_container_index;
    int moved = 0;

    if(!c || !world_container || !world_container->active)
        return 0;
    if(equipped_ci < 0 || equipped_ci >= MAX_ATTACHED_CONTAINERS)
        return 0;

    world_container_index = world_container_index_of(world_container);
    if(world_container_index < 0)
        return 0;

    while(world_container->item_count > 0)
    {
        Item moved_item;

        if(c->containers[equipped_ci].count >= c->containers[equipped_ci].capacity)
            break;

        if(!world_container_remove_item(world_container_index, 0, &moved_item))
            break;

        c->containers[equipped_ci].contents[c->containers[equipped_ci].count++] = moved_item;
        moved++;
    }

    if(world_container->item_count <= 0)
        (void)world_container_remove(world_container_index);

    return moved;
}

static int interact_container_item_from_label(const char* label, Item* out_item, int x, int y)
{
    const ItemTemplate* tmpl;

    if(!label || !out_item)
        return 0;

    tmpl = item_template_by_name(label);
    if(tmpl && interact_item_type_is_container(tmpl->type))
    {
        item_init_from_template(out_item, tmpl, x, y);
        return 1;
    }

    if(strstr(label, "Pouch") || strstr(label, "pouch"))
    {
        item_init(out_item, label, 'p', x, y, ITEM_TYPE_CONTAINER_BELTPOUCH, 0, 1);
        return 1;
    }

    item_init(out_item, label, 'B', x, y, ITEM_TYPE_CONTAINER_BACKPACK, 0, 1);
    return 1;
}

static int interact_equip_container_from_ground(Player* p, WorldItem* world_item, WorldContainer* world_container)
{
    Item equip_item;
    int before_empty[MAX_ATTACHED_CONTAINERS];
    int inventory_slot;
    int equipped_ci;
    int world_item_index = -1;

    if(!p)
        return 0;

    for(int i = 0; i < MAX_ATTACHED_CONTAINERS; i++)
        before_empty[i] = (p->character.containers[i].item.type == ITEM_TYPE_NONE);

    if(world_item && world_item->active)
    {
        equip_item = world_item->item;
        world_item_index = world_item_index_of(world_item);
    }
    else if(world_container && world_container->active)
    {
        if(!interact_container_item_from_label(world_container->label,
                                               &equip_item,
                                               p->character.actor.entity.x,
                                               p->character.actor.entity.y))
        {
            log_add("Cannot equip this container.");
            return 0;
        }
    }
    else
    {
        return 0;
    }

    if(!interact_item_type_is_container(equip_item.type))
    {
        log_add("Only container items can be equipped from ground.");
        return 0;
    }

    if(!inventory_add(&p->character, &equip_item))
    {
        log_add("Inventory full: cannot equip %s.", equip_item.name);
        return 0;
    }

    inventory_slot = p->character.inventory_count - 1;
    if(!inventory_equip(&p->character, inventory_slot))
    {
        (void)inventory_remove(&p->character, p->character.inventory_count - 1);
        log_add("Cannot equip %s right now.", equip_item.name);
        return 0;
    }

    if(world_item_index >= 0)
        (void)world_item_remove(world_item_index);

    equipped_ci = interact_equipped_container_slot(&p->character, before_empty, equip_item.type, equip_item.name);
    if(world_container && world_container->active && equipped_ci >= 0)
    {
        int moved = interact_transfer_world_container_to_equipped(&p->character, world_container, equipped_ci);
        if(moved > 0)
            log_add("Moved %d item(s) from ground container into %s.", moved, equip_item.name);
    }

    return 1;
}

static int interaction_run_action(Player* p, const InteractionAction* action)
{
    if(!p || !action)
        return 0;

    switch(action->type)
    {
        case INTERACTION_ACTION_OPEN_CONTAINER:
            if(!action->world_container)
            {
                log_add("This container is empty.");
                return 0;
            }
            if(interact_open_container(p, action->world_container))
                creatures_take_turns(p);
            return 1;

        case INTERACTION_ACTION_PICK_UP_ITEM:
            if(interact_pick_up_world_item(p, action->world_item))
            {
                creatures_take_turns(p);
                return 1;
            }
            return 0;

        case INTERACTION_ACTION_EQUIP_FROM_GROUND:
            if(interact_equip_container_from_ground(p, action->world_item, action->world_container))
            {
                creatures_take_turns(p);
                return 1;
            }
            return 0;

        case INTERACTION_ACTION_EXAMINE_ITEM:
            if(action->world_item && action->world_item->active)
                log_add("You examine %s.", action->world_item->item.name);
            else if(action->world_container && action->world_container->active)
                log_add("You examine %s.", action->world_container->label);
            else
                log_add("You examine the target.");
            creatures_take_turns(p);
            return 1;

        case INTERACTION_ACTION_PET:
            return interact_creature(p, action->creature);

        case INTERACTION_ACTION_FEED:
        case INTERACTION_ACTION_TREAT_INJURY:
        case INTERACTION_ACTION_TALK:
        case INTERACTION_ACTION_GIVE_ITEM:
            log_add("Not implemented yet.");
            return 0;

        case INTERACTION_ACTION_TILE_USE:
            return interact_tile(p, action->tx, action->ty);

        default:
            return 0;
    }
}

static void interaction_collect_actions(Player* p,
                                        int tx,
                                        int ty,
                                        Creature* creature,
                                        Tile* tile,
                                        WorldItem* world_item,
                                        WorldContainer* world_container,
                                        InteractionAction* actions,
                                        int* action_count)
{
    int is_animal = 0;
    int is_character = 0;
    int is_injured_neutral = 0;

    if(!p || !actions || !action_count)
        return;

    if(creature && creature->alive && creature->template)
    {
        int hostile = creature_is_hostile(creature);
        is_animal = creature->template->tamable ? 1 : 0;
        is_character = (!hostile && !is_animal) ? 1 : 0;
        is_injured_neutral = (!hostile && creature->actor.health < creature->actor.max_health) ? 1 : 0;

        if(is_animal)
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_PET,
                                   !hostile,
                                   "Pet",
                                   hostile ? "Too dangerous while hostile" : "",
                                   creature, NULL, NULL, tx, ty);
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_FEED,
                                   0,
                                   "Feed",
                                   "Not implemented yet",
                                   creature, NULL, NULL, tx, ty);
        }

        if(is_injured_neutral)
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_TREAT_INJURY,
                                   0,
                                   "Treat injury",
                                   "Not implemented yet",
                                   creature, NULL, NULL, tx, ty);
        }

        if(is_character)
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_TALK,
                                   0,
                                   "Talk",
                                   "Not implemented yet",
                                   creature, NULL, NULL, tx, ty);
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_GIVE_ITEM,
                                   0,
                                   "Give item",
                                   "Not implemented yet",
                                   creature, NULL, NULL, tx, ty);
        }
    }

    if(world_item && world_item->active)
    {
        if(interact_item_type_is_container(world_item->item.type))
        {
            int pickup_enabled = 1;

            if(world_container && world_container->active && world_container->item_count > 0)
                pickup_enabled = 0;

            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_OPEN_CONTAINER,
                                   1,
                                   "Open",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_PICK_UP_ITEM,
                                   pickup_enabled,
                                   "Pick up",
                                   pickup_enabled ? "" : "Container has items; open or equip it first",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_EQUIP_FROM_GROUND,
                                   1,
                                   "Equip",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_EXAMINE_ITEM,
                                   1,
                                   "Examine",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
        }
        else
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_PICK_UP_ITEM,
                                   1,
                                   "Pick up",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_EXAMINE_ITEM,
                                   1,
                                   "Examine",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
        }
    }
    else if(world_container && world_container->active)
    {
        interaction_action_add(actions, action_count,
                               INTERACTION_ACTION_OPEN_CONTAINER,
                               1,
                               "Open",
                               "",
                               creature,
                               NULL,
                               world_container,
                               tx,
                               ty);
        interaction_action_add(actions, action_count,
                               INTERACTION_ACTION_EQUIP_FROM_GROUND,
                               1,
                               "Equip",
                               "",
                               creature,
                               NULL,
                               world_container,
                               tx,
                               ty);
        interaction_action_add(actions, action_count,
                               INTERACTION_ACTION_EXAMINE_ITEM,
                               1,
                               "Examine",
                               "",
                               creature,
                               NULL,
                               world_container,
                               tx,
                               ty);
    }

    if(tile)
    {
        if(tile_is_door(tile))
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_TILE_USE,
                                   1,
                                   tile->blocks_movement ? "Open door" : "Close door",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
        }
        else if(tile_is_stairs_up(tile))
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_TILE_USE,
                                   1,
                                   "Climb stairs up",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
        }
        else if(tile_is_stairs_down(tile))
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_TILE_USE,
                                   1,
                                   "Climb stairs down",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
        }
        else if(strcmp(tile->name, "Signpost") == 0)
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_TILE_USE,
                                   1,
                                   "Read signpost",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
        }
        else if(strstr(tile->name, "Switch"))
        {
            interaction_action_add(actions, action_count,
                                   INTERACTION_ACTION_TILE_USE,
                                   1,
                                   "Inspect switch",
                                   "",
                                   creature,
                                   world_item,
                                   world_container,
                                   tx,
                                   ty);
        }
    }
}

// Try interacting with tile-level interactables (doors now, extensible for switches/containers).
static int interact_tile(Player* p, int tx, int ty)
{
    Tile* tile;
    WorldContainer* any_container;

    if(!p || !current_area)
        return 0;
    if(tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return 0;

    tile = map_tile_at_layer(current_area, tx, ty, TILE_LAYER_STRUCTURE);

    any_container = world_container_at(tx, ty);
    if(any_container)
    {
        if(interact_open_container(p, any_container))
            creatures_take_turns(p);
        return 1;
    }

    if(!tile)
        return 0;

    if(!tile->interactable)
        return 0;

    if(tile_is_door(tile))
    {
        TileMutationState next_state = tile->blocks_movement ? TILE_MUTATION_STATE_DOOR_OPEN : TILE_MUTATION_STATE_DOOR_CLOSED;

        if(!atlas_set_tile_mutation(current_area, tx, ty, next_state))
        {
            log_add("Failed to toggle door at %d,%d.", tx, ty);
            return 0;
        }

        if(next_state == TILE_MUTATION_STATE_DOOR_OPEN)
            log_add("You open the door.");
        else
            log_add("You close the door.");

        creatures_take_turns(p);
        return 1;
    }

    if(tile_is_stairs_up(tile))
    {
        int tower_floor;

        if(p->character.actor.entity.z >= HERMIT_TOWER_TOP_Z)
        {
            log_add("You are already at the top floor of the Hermit Tower.");
            return 0;
        }

        p->character.actor.entity.z++;
        tower_floor = (p->character.actor.entity.z - HERMIT_TOWER_BASE_Z) + 1;
        log_add("You climb to Hermit Tower floor %d (z=%d).", tower_floor, p->character.actor.entity.z);
        creatures_take_turns(p);
        return 1;
    }

    if(tile_is_stairs_down(tile))
    {
        int tower_floor;

        if(p->character.actor.entity.z <= HERMIT_TOWER_BASE_Z)
        {
            log_add("You are already at the Hermit Tower ground floor (z=%d).", HERMIT_TOWER_BASE_Z);
            return 0;
        }

        p->character.actor.entity.z--;
        tower_floor = (p->character.actor.entity.z - HERMIT_TOWER_BASE_Z) + 1;
        log_add("You descend to Hermit Tower floor %d (z=%d).", tower_floor, p->character.actor.entity.z);
        creatures_take_turns(p);
        return 1;
    }

    if(strstr(tile->name, "Switch"))
    {
        log_add("You inspect the switch, but it is not wired yet.");
        return 0;
    }

    if(strstr(tile->name, "Chest") || strstr(tile->name, "Container"))
    {
        WorldContainer* container = world_container_at(tx, ty);
        if(!container)
        {
            log_add("This container is empty.");
            return 1;
        }

        if(interact_open_container(p, container))
            creatures_take_turns(p);
        return 1;
    }

    if(strcmp(tile->name, "Signpost") == 0)
    {
        int area_index = interact_current_area_index();
        int pz = p->character.actor.entity.z;
        SignpostInstance* signpost;
        int learned_new_location = 0;
        int read_count = 0;

        if(area_index < 0)
        {
            log_add("This signpost cannot be read right now.");
            return 0;
        }

        signpost = world_map_signpost_at_mut(area_index, tx, ty, pz);
        if(!signpost || signpost->sign_count <= 0)
        {
            log_add("The signpost is weathered and unreadable.");
            creatures_take_turns(p);
            return 1;
        }

        for(int i = 0; i < signpost->sign_count; i++)
        {
            int destination = signpost->signs[i].destination_index;
            const char* destination_name;

            if(destination < 0 || destination >= atlas_location_count)
                continue;

            if(atlas_get_knowledge(destination) < LOCATION_KNOWLEDGE_AWARE)
                learned_new_location = 1;

            atlas_upgrade_knowledge(destination, LOCATION_KNOWLEDGE_AWARE);
            atlas_add_location_hint(destination, signpost->signs[i].hint_text);

            destination_name = atlas[destination].name;
            log_add("%s - %s",
                    signpost->signs[i].direction[0] ? signpost->signs[i].direction : "Route",
                    (destination_name && destination_name[0]) ? destination_name : "Unknown");
            read_count++;
        }

        signpost->visited = 1;

        if(read_count <= 0)
        {
            log_add("The signpost has no useful destination markings.");
            creatures_take_turns(p);
            return 1;
        }
        if(learned_new_location)
            log_add("You mark the signposted locations on your atlas.");
        creatures_take_turns(p);
        return 1;
    }

    log_add("Nothing obvious happens when you interact with %s.", tile->name);
    return 0;
}

int inspect_interact_at(Player* p, int tx, int ty)
{
    int px;
    int py;
    int max_range;
    const char* target_name;
    Creature* creature;
    Tile* tile;
    WorldItem* world_item;
    WorldContainer* world_container;
    InteractionAction actions[INTERACTION_ACTIONS_MAX];
    int action_count = 0;
    int selected_action;

    if(!p || !current_area)
        return 0;

    px = p->character.actor.entity.x;
    py = p->character.actor.entity.y;

    if(!map_has_line_of_sight(px, py, tx, ty))
    {
        log_add("Cannot interact at %d,%d: out of sight.", tx, ty);
        return 0;
    }

    max_range = interact_max_range();
    target_name = interact_target_name_at(tx, ty);
    if(target_name && !interact_in_range(px, py, tx, ty, max_range))
    {
        log_add("You are too far away to interact with %s", target_name);
        return 0;
    }

    creature = bestiary_creature_at_3d(tx, ty, p->character.actor.entity.z);
    tile = map_tile_at_layer(current_area, tx, ty, TILE_LAYER_STRUCTURE);
    world_item = world_item_at_3d(tx, ty, p->character.actor.entity.z);
    world_container = world_container_at(tx, ty);

    interaction_collect_actions(p, tx, ty, creature, tile, world_item, world_container, actions, &action_count);

    if(action_count <= 0)
    {
        log_add("Nothing to interact with at %d,%d.", tx, ty);
        return 0;
    }

    selected_action = interaction_show_menu(p, target_name, actions, action_count);
    if(selected_action < 0)
    {
        log_add("Interaction canceled.");
        return 0;
    }

    if(interaction_run_action(p, &actions[selected_action]))
        return 1;

    log_add("Nothing to interact with at %d,%d.", tx, ty);
    return 0;
}

// Prompt player for direction and attempt interaction in that direction, or same tile if space/enter.
// If player has a valid target lock, auto-interact with that target instead of prompting.
void quick_interact(Player* p)
{
    TargetLockResolved resolved;
    int dx = 0;
    int dy = 0;
    int target_x;
    int target_y;
    int key;

    if(!p)
        return;

    // Check if player has a valid target lock
    if(p->target_lock.active && target_lock_resolve_live(p, &resolved, 0))
    {
        // Valid lock: auto-interact with the locked target
        (void)inspect_interact_at(p, resolved.x, resolved.y);
        return;
    }

    // Target lock is invalid: determine reason and handle accordingly
    if(p->target_lock.active)
    {
        int is_permanent = 0;
        
        // Check if lock is in a different area (temporary condition - preserve lock)
        if(!current_area || strcmp(p->target_lock.area_name, current_area->name) != 0)
        {
            log_add("Your locked target has left the area.");
        }
        // Check if lock points to a dead creature (permanent condition - clear lock)
        else if(p->target_lock.kind == TARGET_LOCK_CREATURE)
        {
            int index = p->target_lock.slot_index;
            if(index >= 0 && index < MAX_CREATURES && (!creatures[index].alive || !creatures[index].template))
            {
                log_add("Your target is no longer alive.");
                is_permanent = 1;
            }
        }
        // Check if lock points to a despawned item (permanent condition - clear lock)
        else if(p->target_lock.kind == TARGET_LOCK_WORLD_ITEM)
        {
            int index = p->target_lock.slot_index;
            if(index >= 0 && index < MAX_WORLD_ITEMS && (!world_items[index].active || world_items[index].item.type == ITEM_TYPE_NONE))
            {
                log_add("Your target item no longer exists.");
                is_permanent = 1;
            }
        }
        
        // Clear lock only for permanent conditions
        if(is_permanent)
            target_lock_clear(p);
        
        return;
    }

    // No target lock: show direction prompt as before
    log_add("Interact: w/up=up, s/down=down, a/left=left, d/right=right, space/enter=here, q/esc=cancel");
    key = read_input_key();

    switch(key)
    {
        case 'w': case 'W': case INPUT_KEY_UP:
            dy = -1;
            break;
        case 's': case 'S': case INPUT_KEY_DOWN:
            dy = +1;
            break;
        case 'a': case 'A': case INPUT_KEY_LEFT:
            dx = -1;
            break;
        case 'd': case 'D': case INPUT_KEY_RIGHT:
            dx = +1;
            break;
        case ' ':
        case 13:  // space or enter - interact with same tile
            dx = 0;
            dy = 0;
            break;
        case 'q': case 'Q': case 27:  // escape
            log_add("Interaction canceled.");
            return;
        default:
            log_add("Invalid direction.");
            return;
    }

    target_x = p->character.actor.entity.x + dx;
    target_y = p->character.actor.entity.y + dy;
    (void)inspect_interact_at(p, target_x, target_y);
}
