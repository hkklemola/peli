#include "inventory.h"

// Forward declaration for slot mapping utility
EquipmentSlotType equipment_slot_for_item_type(ItemType type);


// --- STUBS FOR MISSING FUNCTIONS (implementations) ---
#include "tile.h"


#include "bestiary.h"
#include "npc.h"
#include "atlas.h"
#include "combat.h"
#include "collision.h"
#include "furniture.h"
#include "map.h"
#include "input.h"
#include "keybind_helpers.h"
#include "item_data.h"
#include "log.h"
#include "movement.h"
#include "draw.h"


// Forward declarations for types used before their definition
struct Creature;
struct Tile;
struct InteractionAction;
#include "world_items.h"
#include "interact.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>



#include "atlas.h"
#include "inventory.h"

#include "world_items.h"
#include "bestiary.h"
#include "map.h"
#include "interact.h"
#include "target_lock.h"
#include "ui_overlay.h"
#include "world_items.h"
#include "crafting_compendium.h"


// --- STUB IMPLEMENTATIONS FOR MISSING FUNCTIONS ---
// TODO: Replace with real implementations if needed.


static const char* interact_target_name_at(int tx, int ty)
{
    int pz = player.character.actor.entity.z;
    Creature* creature = bestiary_creature_at_3d(tx, ty, pz);
    NPC* npc = npc_at_3d(tx, ty, pz);
    const Tile* tile;
    WorldItem* world_item;
    WorldContainer* world_container;
    WorldCorpse* world_corpse;

    if(creature && creature->alive && creature->template)
        return creature->template->name;
    if(npc && npc->active)
        return npc_display_name(npc);

    if(!current_area || tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return NULL;

    world_corpse = world_corpse_at_3d(tx, ty, pz);
    if(world_corpse && world_corpse->active && world_corpse->label[0])
        return world_corpse->label;

    {
        Furniture* furn = furniture_at(current_area, tx, ty);
        if(furn && furn->type != FURNITURE_NONE)
        {
            if(furniture_uses_container_type(furn->type))
            {
                WorldContainer* wc = world_container_at_3d(tx, ty, pz);
                if(wc && wc->active && wc->label[0])
                    return wc->label;
            }
            return furniture_display_name(furn);
        }
    }

    tile = map_top_visible_tile(current_area, tx, ty, NULL);
    if(tile->interactable && tile->name[0])
        return tile->name;
    if(tile_is_fishable(tile) && tile->name[0])
        return tile->name;

    world_item = world_item_at_3d(tx, ty, pz);
    if(world_item && world_item->active)
        return item_display_name(&world_item->item);

    world_container = world_container_at_3d(tx, ty, pz);
    if(world_container && world_container->active)
        return world_container->label;

    return NULL;
}

static int interact_has_adjacent_hostile(const Player* p)
{
    int px;
    int py;
    int pz;

    if(!p)
        return 0;

    px = p->character.actor.entity.x;
    py = p->character.actor.entity.y;
    pz = p->character.actor.entity.z;

    for(int i = 0; i < MAX_CREATURES; i++)
    {
        const Creature* creature = &creatures[i];
        int dx;
        int dy;

        if(!creature->alive || !creature->template)
            continue;
        if(!creature_is_hostile(creature))
            continue;
        if(creature->actor.entity.z != pz)
            continue;

        dx = creature->actor.entity.x - px;
        dy = creature->actor.entity.y - py;
        if(dx < -1 || dx > 1 || dy < -1 || dy > 1)
            continue;

        return 1;
    }

    return 0;
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

static int tile_is_staircase(const Tile* tile)
{
    if(!tile)
        return 0;

    if(strcmp(tile->name, "Staircase") == 0 ||
       strcmp(tile->name, "Stairs Up") == 0 ||
       strcmp(tile->name, "Stairs Down") == 0)
        return 1;

    return tile->symbol == '<' || tile->symbol == '>';
}

static int stairs_find_connected_step(const Area* area,
                                      int stair_x,
                                      int stair_y,
                                      int current_z,
                                      int dz,
                                      int* out_x,
                                      int* out_y)
{
    int next_z;
    int best_score = 9999;
    int best_x = stair_x;
    int best_y = stair_y;

    if(!area || dz == 0)
        return 0;

    next_z = current_z + ((dz > 0) ? 1 : -1);
    if(next_z < AREA_GROUND_Z || next_z > map_max_view_floor(area))
        return 0;

    for(int search_radius = 0; search_radius <= 2; ++search_radius)
    {
        for(int dy = -search_radius; dy <= search_radius; ++dy)
        {
            for(int dx = -search_radius; dx <= search_radius; ++dx)
            {
                int nx = stair_x + dx;
                int ny = stair_y + dy;
                const Tile* candidate;
                int score;

                if(nx < 0 || ny < 0 || nx >= area->width || ny >= area->height)
                    continue;

                candidate = map_tile_at_layer_z((Area*)area, nx, ny, next_z, TILE_LAYER_WALL);
                if(!tile_is_staircase(candidate))
                    continue;

                score = abs(dx) + abs(dy);
                if(score < best_score)
                {
                    best_score = score;
                    best_x = nx;
                    best_y = ny;
                }
            }
        }
    }

    if(best_score == 9999)
        return 0;

    if(out_x)
        *out_x = best_x;
    if(out_y)
        *out_y = best_y;
    return 1;
}

static int stairs_can_move(const Player* p, int stair_x, int stair_y, int dz)
{
    if(!p || !current_area || dz == 0)
        return 0;

    return stairs_find_connected_step(current_area,
                                      stair_x,
                                      stair_y,
                                      p->character.actor.entity.z,
                                      dz,
                                      NULL,
                                      NULL);
}

static int stairs_floor_number_for_z(int z)
{
    if(z <= HERMIT_TOWER_BASE_Z)
        return 1;

    return ((z - HERMIT_TOWER_BASE_Z) / HERMIT_TOWER_FLOOR_Z_STEP) + 1;
}

static int stairs_is_floor_landing_z(int z)
{
    if(z < HERMIT_TOWER_BASE_Z)
        return 0;

    return ((z - HERMIT_TOWER_BASE_Z) % HERMIT_TOWER_FLOOR_Z_STEP) == 0;
}

static int interact_use_stairs(Player* p, int stair_x, int stair_y, int dz)
{
    int next_z;
    int next_x = stair_x;
    int next_y = stair_y;

    if(!p || !current_area || dz == 0)
        return 0;

    dz = (dz > 0) ? 1 : -1;
    if(!stairs_find_connected_step(current_area,
                                   stair_x,
                                   stair_y,
                                   p->character.actor.entity.z,
                                   dz,
                                   &next_x,
                                   &next_y))
    {
        if(dz > 0)
            log_add("This staircase does not continue upward from here.");
        else
            log_add("This staircase does not continue downward from here.");
        return 0;
    }

    next_z = p->character.actor.entity.z + dz;
    p->dragged_world_item_index = -1;
    p->character.actor.entity.x = next_x;
    p->character.actor.entity.y = next_y;
    p->character.actor.entity.z = next_z;

    if(stairs_is_floor_landing_z(next_z))
    {
        log_add("You %s the staircase to Hermit Tower floor %d (z=%d).",
                (dz > 0) ? "climb" : "descend",
                stairs_floor_number_for_z(next_z),
                next_z);
    }
    else
    {
        log_add("You %s the staircase to stair level z=%d.",
                (dz > 0) ? "climb" : "descend",
                next_z);
    }

    creatures_take_turns(p);

    {
        const Tile* landed_tile = map_top_visible_tile(current_area, next_x, next_y, NULL);
        if(tile_is_staircase(landed_tile))
            (void)interact_at(p, next_x, next_y);
    }

    return 1;
}

static int interact_has_tool_for_skill_anywhere(const Player* p, NonWeaponSkillType skill_type);

#define FISHING_MAX_TURNS 30
#define FISHING_TURN_INTERVAL_MS 1000
#define FISHING_BASE_CATCH_CHANCE 1
#define FISHING_SKILL_PER_CATCH_BONUS 10
#define FISHING_XP_PER_CATCH 5

typedef enum FishingChannelEndReason {
    FISHING_CHANNEL_NONE = 0,
    FISHING_CHANNEL_CAUGHT,
    FISHING_CHANNEL_DANGER,
    FISHING_CHANNEL_INVALID_TARGET,
} FishingChannelEndReason;

typedef struct FishingChannelState {
    int tx;
    int ty;
    FishingChannelEndReason end_reason;
} FishingChannelState;

static int interact_fishing_catch_chance_percent(const Player* p)
{
    int fishing_skill;

    if(!p)
        return FISHING_BASE_CATCH_CHANCE;

    fishing_skill = actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_FISHING);
    if(fishing_skill < 0)
        fishing_skill = 0;

    return FISHING_BASE_CATCH_CHANCE + (fishing_skill / FISHING_SKILL_PER_CATCH_BONUS);
}

static void interact_finish_fishing_catch(Player* p)
{
    const ItemTemplate* tmpl = item_template_by_name("Raw Fish");
    int levels_gained;

    if(!p)
        return;

    levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor,
                                                   NON_WEAPON_SKILL_FISHING,
                                                   FISHING_XP_PER_CATCH);

    if(tmpl)
    {
        Item fish;

        item_init_from_template(&fish, tmpl, -1, -1);
        if(inventory_add(&p->character, &fish))
            log_add("You caught a raw fish!");
        else
            log_add("You caught a fish, but your inventory is full.");
    }
    else
    {
        log_add("You caught something, but it slipped away.");
    }

    if(levels_gained > 0)
    {
        log_add("Your %s skill improved to %d!",
                non_weapon_skill_name(NON_WEAPON_SKILL_FISHING),
                actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_FISHING));
    }
}

static int interact_fishing_channel_tick(Player* p, void* user_data)
{
    FishingChannelState* state = (FishingChannelState*)user_data;
    const Tile* tile;
    int catch_chance;

    if(!p || !state)
        return 0;

    tile = map_top_visible_tile(current_area, state->tx, state->ty, NULL);
    if(!tile || !tile_is_fishable(tile))
    {
        state->end_reason = FISHING_CHANNEL_INVALID_TARGET;
        log_add("You lose the fishing spot.");
        return 0;
    }

    if(interact_has_adjacent_hostile(p))
    {
        state->end_reason = FISHING_CHANNEL_DANGER;
        log_add("You stop fishing as danger closes in.");
        return 0;
    }

    catch_chance = interact_fishing_catch_chance_percent(p);
    if(rand() % 100 < catch_chance)
    {
        state->end_reason = FISHING_CHANNEL_CAUGHT;
        interact_finish_fishing_catch(p);
        return 0;
    }

    return 1;
}

static int interact_use_fishing_rod(Player* p, int tx, int ty)
{
    const Tile* tile;
    FishingChannelState state;
    PlayerTimedActionResult result;
    int turns_completed = 0;

    if(!p)
        return 0;

    // Check for fishing tool
    if(!interact_has_tool_for_skill_anywhere(p, NON_WEAPON_SKILL_FISHING))
    {
        log_add("You need a fishing tool to fish.");
        return 0;
    }

    tile = map_top_visible_tile(current_area, tx, ty, NULL);
    if(!tile || !tile_is_fishable(tile))
    {
        log_add("You can't fish there.");
        return 0;
    }

    // Check for danger
    if(interact_has_adjacent_hostile(p))
    {
        log_add("Too dangerous to fish right now.");
        return 0;
    }

    state.tx = tx;
    state.ty = ty;
    state.end_reason = FISHING_CHANNEL_NONE;

    log_add("You cast your line.");

    result = player_run_timed_action(p,
                                     FISHING_MAX_TURNS,
                                     FISHING_TURN_INTERVAL_MS,
                                     "Fishing...",
                                     1,
                                     interact_fishing_channel_tick,
                                     &state,
                                     &turns_completed);

    if(state.end_reason == FISHING_CHANNEL_CAUGHT)
        return 1;

    if(result == PLAYER_TIMED_ACTION_CANCELED)
    {
        log_add("You stop fishing.");
        return 1;
    }

    if(state.end_reason == FISHING_CHANNEL_DANGER || state.end_reason == FISHING_CHANNEL_INVALID_TARGET)
        return 1;

    if(p->character.actor.health <= 0)
        return 1;

    log_add("Nothing bites after %d turns.", turns_completed > 0 ? turns_completed : FISHING_MAX_TURNS);

    return 1;
}

int interact_fish_at(Player* p, int tx, int ty)
{
    return interact_use_fishing_rod(p, tx, ty);
}

int interact_prompt_fishing(Player* p)
{
    int key;
    int dx = 0;
    int dy = 0;

    if(!p)
        return 0;

    log_add("Fish: W/up=up, X/down=down, A/left=left, D/right=right, space/enter=here, Q/esc=cancel");

    while(1)
    {
        key = read_input_key();
        switch(key)
        {
            case INPUT_KEY_UP: case 'w': case 'W': dy = -1; dx = 0; break;
            case INPUT_KEY_DOWN: case 'x': case 'X': dy = 1; dx = 0; break;
            case INPUT_KEY_LEFT: case 'a': case 'A': dx = -1; dy = 0; break;
            case INPUT_KEY_RIGHT: case 'd': case 'D': dx = 1; dy = 0; break;
            case ' ': case 13: dy = 0; dx = 0; break;
            case 'q': case 'Q': case 27:
                log_add("Fishing canceled.");
                return 0;
            default:
                continue;
        }

        return interact_use_fishing_rod(p, p->character.actor.entity.x + dx, p->character.actor.entity.y + dy);
    }
}

static int interact_current_area_index(void)
{
    if(!current_area || !current_area->name || current_area->name[0] == '\0')
        return -1;

    return atlas_find_location(current_area->name);
}

static int interact_max_range(void)
{
    return INTERACT_RANGE_DEFAULT;
}

static int interact_in_range(int px, int py, int tx, int ty, int max_range)
{
    int dx = abs(px - tx);
    int dy = abs(py - ty);
    return (dx > dy ? dx : dy) <= max_range;
}

static int interact_furniture_owns_world_container(const Furniture* furn, const WorldContainer* world_container)
{
    int world_container_index;

    if(!furn || !world_container || !world_container->active)
        return 0;

    if(furniture_interaction_type(furn) != FURNITURE_INTERACTION_OPEN_CONTAINER)
        return 0;

    world_container_index = world_container_index_of(world_container);
    if(furn->world_container_index >= 0 && world_container_index == furn->world_container_index)
        return 1;
    if(furn->input_world_container_index >= 0 && world_container_index == furn->input_world_container_index)
        return 1;
    if(furn->output_world_container_index >= 0 && world_container_index == furn->output_world_container_index)
        return 1;

    return current_area &&
           strcmp(world_container->area_name, current_area->name) == 0 &&
           world_container->x == furn->base.base.x &&
           world_container->y == furn->base.base.y &&
           world_container->z == furn->base.base.z;
}

#define BOOKSHELF_SHELF_COUNT 6
#define BOOKSHELF_SHELF_CAPACITY 20

static int interact_bookshelf_shelf_from_label(const char* label)
{
    const char* prefix = "Bookshelf Shelf ";
    size_t prefix_len = strlen(prefix);
    int shelf_number;

    if(!label)
        return -1;

    if(strncmp(label, prefix, prefix_len) != 0)
        return -1;

    shelf_number = atoi(label + (int)prefix_len);
    if(shelf_number < 1 || shelf_number > BOOKSHELF_SHELF_COUNT)
        return -1;

    return shelf_number - 1;
}

static int interact_bookshelf_collect_shelves(const Furniture* furn, WorldContainer* shelves[BOOKSHELF_SHELF_COUNT])
{
    int count = 0;

    if(!furn || !current_area || furn->type != FURNITURE_BOOKSHELF)
        return 0;

    for(int i = 0; i < BOOKSHELF_SHELF_COUNT; i++)
        shelves[i] = NULL;

    for(int i = 0; i < MAX_WORLD_CONTAINERS; i++)
    {
        WorldContainer* candidate = &world_containers[i];
        int shelf_index;

        if(!candidate->active)
            continue;
        if(strcmp(candidate->area_name, current_area->name) != 0)
            continue;
        if(candidate->x != furn->base.base.x || candidate->y != furn->base.base.y || candidate->z != furn->base.base.z)
            continue;

        shelf_index = interact_bookshelf_shelf_from_label(candidate->label);
        if(shelf_index < 0 || shelf_index >= BOOKSHELF_SHELF_COUNT)
            continue;
        if(!shelves[shelf_index])
            count++;
        shelves[shelf_index] = candidate;
    }

    return count;
}

static WorldContainer* interact_bookshelf_pick_shelf(Player* p, const Furniture* furn)
{
    WorldContainer* shelves[BOOKSHELF_SHELF_COUNT];
    int shelf_count;
    int selected = 0;
    int need_world_redraw = 1;

    shelf_count = interact_bookshelf_collect_shelves(furn, shelves);
    if(shelf_count <= 0)
    {
        log_add("No bookshelf shelves are available.");
        return NULL;
    }

    while(selected < BOOKSHELF_SHELF_COUNT && !shelves[selected])
        selected++;
    if(selected >= BOOKSHELF_SHELF_COUNT)
        selected = 0;

    while(1)
    {
        int line_i = 0;
        int content_lines;
        int status_line;
        int key;

        if(need_world_redraw)
        {
            draw_world(p);
            ui_overlay_draw_frame("Bookshelf");
            ui_overlay_invalidate_cache();
            need_world_redraw = 0;
        }

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;

        for(int i = 0; i < BOOKSHELF_SHELF_COUNT && line_i < status_line; i++)
        {
            char line[128];

            if(!shelves[i])
            {
                snprintf(line, sizeof(line), "%c %d. Shelf %d (missing)", (i == selected) ? '>' : ' ', i + 1, i + 1);
            }
            else
            {
                snprintf(line,
                         sizeof(line),
                         "%c %d. Shelf %d (%d/%d books)",
                         (i == selected) ? '>' : ' ',
                         i + 1,
                         i + 1,
                         shelves[i]->item_count,
                         BOOKSHELF_SHELF_CAPACITY);
            }

            ui_overlay_draw_line(line_i++, line);
        }

        while(line_i < status_line)
            ui_overlay_draw_line(line_i++, "");

        ui_overlay_draw_line(status_line, "Esc/Q back | Enter open shelf | 1-6 quick select | W/S move");
        ui_overlay_draw_global_hotkeys();

        key = read_input_key();

        if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
            return NULL;

        if(key >= '1' && key <= '6')
        {
            int target = key - '1';
            if(target >= 0 && target < BOOKSHELF_SHELF_COUNT && shelves[target])
                return shelves[target];
            continue;
        }

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            int cursor = selected;
            do
            {
                cursor = (cursor - 1 + BOOKSHELF_SHELF_COUNT) % BOOKSHELF_SHELF_COUNT;
            } while(cursor != selected && !shelves[cursor]);
            if(shelves[cursor])
                selected = cursor;
            continue;
        }

        if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
        {
            int cursor = selected;
            do
            {
                cursor = (cursor + 1) % BOOKSHELF_SHELF_COUNT;
            } while(cursor != selected && !shelves[cursor]);
            if(shelves[cursor])
                selected = cursor;
            continue;
        }

        if(key == 13 && selected >= 0 && selected < BOOKSHELF_SHELF_COUNT && shelves[selected])
            return shelves[selected];
    }
}

typedef enum InteractionActionType {
    INTERACTION_ACTION_OPEN_CONTAINER = 0,
    INTERACTION_ACTION_PICK_UP_ITEM,
    INTERACTION_ACTION_EQUIP_FROM_GROUND,
    INTERACTION_ACTION_DRAG_WORLD_ITEM,
    INTERACTION_ACTION_EXAMINE_ITEM,
    INTERACTION_ACTION_PET,
    INTERACTION_ACTION_FEED,
    INTERACTION_ACTION_TREAT_INJURY,
    INTERACTION_ACTION_TALK,
    INTERACTION_ACTION_NPC_GREET,
    INTERACTION_ACTION_NPC_GOSSIP,
    INTERACTION_ACTION_GIVE_ITEM,
    INTERACTION_ACTION_TILE_USE,
    INTERACTION_ACTION_ADD_FUEL_TO_FORGE,
    INTERACTION_ACTION_EXTINGUISH_FORGE,
    INTERACTION_ACTION_SKIN_CORPSE,
    INTERACTION_ACTION_BUTCHER_CORPSE,
} InteractionActionType;

typedef struct InteractionAction {
    InteractionActionType type;
    int enabled;
    char label[80];
    char disabled_reason[80];
    Creature* creature;
    NPC* npc;
    WorldItem* world_item;
    WorldContainer* world_container;
    WorldCorpse* world_corpse;
    Furniture* furniture; // NEW: direct pointer to furniture entity
    int tx;
    int ty;
    int stair_delta_z;
    // New fields for slot-based system
    int inventory_slot; // Index in inventory, if relevant
    int equipment_slot; // Index in equipment, if relevant
    int container_slot; // Index in container, if relevant
    int source_type;    // 0=none, 1=inventory, 2=equipment, 3=ground, 4=container
    int dest_type;      // 0=none, 1=inventory, 2=equipment, 3=ground, 4=container
} InteractionAction;

#define INTERACTION_ACTIONS_MAX 24

static int interact_item_type_is_container(ItemType type)
{
    return type == ITEM_TYPE_CONTAINER_BACKPACK || type == ITEM_TYPE_CONTAINER_POUCH || type == ITEM_TYPE_CONTAINER_QUIVER;
}

static int interact_item_available_quantity(const Item* item)
{
    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;

    if(item->stackable)
        return (item->quantity > 0) ? item->quantity : 0;

    return 1;
}

static int interact_item_is_draggable_lumber(const Item* item);
static WorldItem* interact_dragged_world_item(Player* p);
static int interaction_action_keeps_menu_open(const InteractionAction* action);
static int interaction_show_menu(Player* p, const char* target_name, InteractionAction* actions, int action_count);
static int interaction_run_action(Player* p, const InteractionAction* action);
typedef struct SmeltingRecipe SmeltingRecipe;
static int interact_recipe_is_coal_related(const SmeltingRecipe* recipe);

static int interact_count_carried_item_quantity(const Player* p, const char* item_name)
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
        if(strcmp(item->name, item_name) != 0)
            continue;

        available = interact_item_available_quantity(item);
        if(available <= 0)
            continue;

        total += available;
    }

    return total;
}

static int interact_has_item_anywhere(const Player* p, const char* item_name)
{
    if(!p || !item_name || !item_name[0])
        return 0;

    if(interact_count_carried_item_quantity(p, item_name) > 0)
        return 1;

    for(int i = 0; i < p->character.equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &p->character.equipment_slots[i];
        const Item* item = &slot->item;

        if(item->type == ITEM_TYPE_NONE)
            continue;
        if(strcmp(item->name, item_name) != 0)
            continue;
        if(interact_item_available_quantity(item) <= 0)
            continue;

        return 1;
    }

    return 0;
}

static int interact_has_tool_for_skill_anywhere(const Player* p, NonWeaponSkillType skill_type)
{
    if(!p || skill_type < 0 || skill_type >= NON_WEAPON_SKILL_COUNT)
        return 0;

    for(int i = 0; i < p->character.equipment_slot_count; ++i)
    {
        const Item* item = &p->character.equipment_slots[i].item;

        if(item->type == ITEM_TYPE_NONE)
            continue;
        if(item_tool_non_weapon_skill(item) == skill_type)
            return 1;
    }

    return 0;
}

static const Item* interact_find_carried_item(const Player* p, const char* item_name)
{
    int i;

    if(!p || !item_name || !item_name[0])
        return NULL;

    for(i = 0; i < p->character.equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &p->character.equipment_slots[i];
        const Item* item = &slot->item;

        if(slot->slot_type != EQUIP_SLOT_NONE || item->type == ITEM_TYPE_NONE)
            continue;
        if(strcmp(item->name, item_name) == 0)
            return item;
    }

    return NULL;
}

static int interact_consume_carried_item_quantity(Player* p, const char* item_name, int amount)
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
        if(strcmp(item->name, item_name) != 0)
            continue;

        available = interact_item_available_quantity(item);
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

static int interact_consume_inventory_slot_quantity(Player* p, int slot_index, int amount)
{
    EquipmentSlot* slot;
    Item* item;
    int available;

    if(!p || slot_index < 0 || slot_index >= p->character.equipment_slot_count || amount <= 0)
        return 0;

    slot = &p->character.equipment_slots[slot_index];
    item = &slot->item;

    if(slot->slot_type != EQUIP_SLOT_NONE || item->type == ITEM_TYPE_NONE)
        return 0;

    available = interact_item_available_quantity(item);
    if(available < amount)
        return 0;

    available -= amount;
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

    return 1;
}

static int interact_add_template_item_to_inventory(Player* p, const char* template_name, int quantity)
{
    const ItemTemplate* tmpl;

    if(!p || !template_name || !template_name[0] || quantity <= 0)
        return 0;

    tmpl = item_template_by_name(template_name);
    if(!tmpl)
        return 0;

    while(quantity > 0)
    {
        Item produced;
        int chunk = 1;

        item_init_from_template(&produced, tmpl, -1, -1);
        if(produced.type == ITEM_TYPE_NONE)
            return 0;

        if(produced.stackable)
        {
            int stack_max = produced.stack_max > 0 ? produced.stack_max : 99;
            chunk = (quantity < stack_max) ? quantity : stack_max;
            produced.quantity = chunk;
        }
        else
        {
            produced.quantity = 1;
        }

        if(!inventory_add(&p->character, &produced))
            return 0;

        quantity -= chunk;
    }

    return 1;
}

static int interact_find_adjacent_drop_tile(int center_x, int center_y, int z, int* out_x, int* out_y)
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

        if(tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
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

static int interact_drop_template_item_near_furniture(const Furniture* furn, const char* template_name, int quantity)
{
    const ItemTemplate* tmpl;
    int drop_x;
    int drop_y;

    if(!furn || !current_area || !template_name || !template_name[0] || quantity <= 0)
        return 0;

    if(!interact_find_adjacent_drop_tile(furn->base.base.x, furn->base.base.y, furn->base.base.z, &drop_x, &drop_y))
        return 0;

    tmpl = item_template_by_name(template_name);
    if(!tmpl)
        return 0;

    while(quantity > 0)
    {
        Item produced;
        int chunk = 1;

        item_init_from_template(&produced, tmpl, drop_x, drop_y);
        if(produced.type == ITEM_TYPE_NONE)
            return 0;

        if(produced.stackable)
        {
            int stack_max = produced.stack_max > 0 ? produced.stack_max : 99;
            chunk = (quantity < stack_max) ? quantity : stack_max;
            produced.quantity = chunk;
        }
        else
        {
            produced.quantity = 1;
        }

        if(!world_item_drop_3d(&produced, current_area->name, drop_x, drop_y, furn->base.base.z))
            return 0;

        quantity -= chunk;
    }

    return 1;
}

static WorldContainer* interact_find_station_container_by_label(const Furniture* furn, const char* label)
{
    if(!furn || !current_area || !label || !label[0])
        return NULL;

    for(int i = 0; i < MAX_WORLD_CONTAINERS; ++i)
    {
        WorldContainer* candidate = &world_containers[i];

        if(!candidate->active)
            continue;
        if(strcmp(candidate->area_name, current_area->name) != 0)
            continue;
        if(candidate->x != furn->base.base.x || candidate->y != furn->base.base.y || candidate->z != furn->base.base.z)
            continue;
        if(strcmp(candidate->label, label) != 0)
            continue;

        return candidate;
    }

    return NULL;
}

static WorldContainer* interact_station_container(Furniture* furn,
                                                  int* container_index_field,
                                                  const char* label,
                                                  int create_if_missing)
{
    WorldContainer* container;

    if(!furn || !current_area || !container_index_field || !label || !label[0])
        return NULL;

    if(*container_index_field >= 0 && *container_index_field < MAX_WORLD_CONTAINERS)
    {
        container = &world_containers[*container_index_field];
        if(container->active && strcmp(container->area_name, current_area->name) == 0 &&
           container->x == furn->base.base.x && container->y == furn->base.base.y &&
           container->z == furn->base.base.z && strcmp(container->label, label) == 0)
            return container;
    }

    container = interact_find_station_container_by_label(furn, label);
    if(container)
    {
        *container_index_field = world_container_index_of(container);
        return container;
    }

    if(!create_if_missing)
        return NULL;

    *container_index_field = world_container_spawn_3d(current_area->name,
                                                      furn->base.base.x,
                                                      furn->base.base.y,
                                                      furn->base.base.z,
                                                      label);
    if(*container_index_field < 0)
        return NULL;

    return &world_containers[*container_index_field];
}

static WorldContainer* interact_station_input_container(Furniture* furn, int create_if_missing)
{
    if(!furn || !furniture_has_input_container_type(furn->type))
        return NULL;

    return interact_station_container(furn,
                                      &furn->input_world_container_index,
                                      furniture_input_container_label_for_type(furn->type),
                                      create_if_missing);
}

static WorldContainer* interact_station_output_container(Furniture* furn, int create_if_missing)
{
    if(!furn || !furniture_has_output_container_type(furn->type))
        return NULL;

    return interact_station_container(furn,
                                      &furn->output_world_container_index,
                                      furniture_output_container_label_for_type(furn->type),
                                      create_if_missing);
}

static int interact_store_template_item_in_station_output(Furniture* furn, const char* template_name, int quantity)
{
    const ItemTemplate* tmpl;
    WorldContainer* output;
    int output_index;

    if(!furn || !template_name || !template_name[0] || quantity <= 0)
        return 0;

    output = interact_station_output_container(furn, 1);
    if(!output)
        return 0;

    output_index = world_container_index_of(output);
    if(output_index < 0)
        return 0;

    tmpl = item_template_by_name(template_name);
    if(!tmpl)
        return 0;

    while(quantity > 0)
    {
        Item produced;
        int chunk = 1;

        item_init_from_template(&produced, tmpl, output->x, output->y);
        produced.object.base.z = output->z;
        if(produced.type == ITEM_TYPE_NONE)
            return 0;

        if(produced.stackable)
        {
            int stack_max = produced.stack_max > 0 ? produced.stack_max : 99;
            chunk = (quantity < stack_max) ? quantity : stack_max;
            produced.quantity = chunk;
        }

        if(!world_container_add_item(output_index, &produced))
            return 0;

        quantity -= chunk;
    }

    return 1;
}

static const Item* interact_equipped_axe(const Player* p)
{
    const Item* main_hand;
    const Item* off_hand;

    if(!p)
        return NULL;

    main_hand = &p->character.equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    off_hand = &p->character.equipment_slots[EQUIP_SLOT_OFF_HAND].item;

    if(main_hand->type != ITEM_TYPE_NONE &&
       (main_hand->weapon_skill_type == WEAPON_SKILL_AXE ||
        main_hand->weapon_skill_type == WEAPON_SKILL_AXE_2H))
        return main_hand;

    if(off_hand->type != ITEM_TYPE_NONE &&
       (off_hand->weapon_skill_type == WEAPON_SKILL_AXE ||
        off_hand->weapon_skill_type == WEAPON_SKILL_AXE_2H))
        return off_hand;

    return NULL;
}

static const Item* interact_equipped_smithing_hammer(const Player* p)
{
    const Item* main_hand;
    const Item* off_hand;

    if(!p)
        return NULL;

    main_hand = &p->character.equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    off_hand = &p->character.equipment_slots[EQUIP_SLOT_OFF_HAND].item;

    if(main_hand->type != ITEM_TYPE_NONE &&
       item_is_tool(main_hand) &&
       (item_has_category(main_hand, "blacksmithing") || item_has_category(main_hand, "smithing")))
        return main_hand;

    if(off_hand->type != ITEM_TYPE_NONE &&
       item_is_tool(off_hand) &&
       (item_has_category(off_hand, "blacksmithing") || item_has_category(off_hand, "smithing")))
        return off_hand;

    return NULL;
}

static const Item* interact_equipped_sledge_hammer(const Player* p)
{
    const Item* main_hand;
    const Item* off_hand;

    if(!p)
        return NULL;

    main_hand = &p->character.equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    off_hand  = &p->character.equipment_slots[EQUIP_SLOT_OFF_HAND].item;

    if(main_hand->type != ITEM_TYPE_NONE &&
       item_is_tool(main_hand) &&
       (item_has_category(main_hand, "sledgehammer") || strcmp(main_hand->name, "Sledge Hammer") == 0))
        return main_hand;

    if(off_hand->type != ITEM_TYPE_NONE &&
       item_is_tool(off_hand) &&
       (item_has_category(off_hand, "sledgehammer") || strcmp(off_hand->name, "Sledge Hammer") == 0))
        return off_hand;

    return NULL;
}

static const Item* interact_equipped_tongs(const Player* p)
{
    const Item* main_hand;
    const Item* off_hand;

    if(!p)
        return NULL;

    main_hand = &p->character.equipment_slots[EQUIP_SLOT_MAIN_HAND].item;
    off_hand = &p->character.equipment_slots[EQUIP_SLOT_OFF_HAND].item;

    if(main_hand->type != ITEM_TYPE_NONE &&
       item_is_tool(main_hand) &&
       (item_has_category(main_hand, "tongs") || strcmp(main_hand->name, "Iron Tongs") == 0))
        return main_hand;

    if(off_hand->type != ITEM_TYPE_NONE &&
       item_is_tool(off_hand) &&
       (item_has_category(off_hand, "tongs") || strcmp(off_hand->name, "Iron Tongs") == 0))
        return off_hand;

    return NULL;
}

static int interact_item_requires_tongs(const Item* item)
{
    return item &&
           item->type != ITEM_TYPE_NONE &&
           item->heat_state == ITEM_HEAT_HOT &&
           item->is_material &&
           item->material_type == MATERIAL_TYPE_METAL;
}

static int interact_template_requires_tongs(const char* item_name)
{
    const ItemTemplate* tmpl;

    if(!item_name || !item_name[0])
        return 0;

    tmpl = item_template_by_name(item_name);
    if(!tmpl || !tmpl->is_material || tmpl->material_type != MATERIAL_TYPE_METAL)
        return 0;

    for(int i = 0; i < 4; ++i)
    {
        if(strcmp(tmpl->categories[i], "hot") == 0 || strcmp(tmpl->categories[i], "heated") == 0)
            return 1;
    }

    return 0;
}

static int interact_can_handle_item_with_tongs(const Player* p, const Item* item, const char* action_text)
{
    if(!interact_item_requires_tongs(item))
        return 1;

    if(interact_equipped_tongs(p))
        return 1;

    if(action_text && action_text[0])
        log_add("You need tongs equipped to %s %s.", action_text, item->name);
    else
        log_add("You need tongs equipped to handle %s.", item->name);

    return 0;
}

static const struct {
    const char* item_name;
    int fuel_units;
} interact_forge_fuels[] = {
    { "Log", 3 },
    { "Oak Log", 3 },
    { "Spruce Log", 3 },
    { "Pine Log", 3 },
    { "Birch Log", 3 },
    { "Yew Log", 4 },
    { "Maple Log", 3 },
    { "Wood Log", 3 },
    { "Lumber", 3 },
    { "Oak Lumber", 3 },
    { "Spruce Lumber", 3 },
    { "Pine Lumber", 3 },
    { "Birch Lumber", 3 },
    { "Yew Lumber", 4 },
    { "Maple Lumber", 3 },
    { "Wood Plank", 1 },
    { "Firewood", 1 },
    { "Oak Firewood", 1 },
    { "Spruce Firewood", 1 },
    { "Pine Firewood", 1 },
    { "Birch Firewood", 1 },
    { "Yew Firewood", 1 },
    { "Maple Firewood", 1 },
    { "Charcoal", 2 },
    { "Coke", 5 }
};

static int interact_fuel_capacity_for_furniture(const Furniture* furn)
{
    if(furn && furn->type == FURNITURE_CHARCOAL_KILN)
        return FURNITURE_CHARCOAL_KILN_MAX_FUEL_UNITS;

    return FURNITURE_FORGE_MAX_FUEL_UNITS;
}

static const char* interact_heat_source_name(const Furniture* furn)
{
    if(furn && furn->type == FURNITURE_CHARCOAL_KILN)
        return "kiln";

    return "furnace";
}

static int interact_is_firewood_fuel_name(const char* item_name)
{
    return item_name && strstr(item_name, "Firewood") != NULL;
}

static int interact_fuel_allowed_for_furniture(const Furniture* furn, int fuel_index)
{
    if(!furn || fuel_index < 0 || fuel_index >= (int)(sizeof(interact_forge_fuels) / sizeof(interact_forge_fuels[0])))
        return 0;

    if(furn->type == FURNITURE_CHARCOAL_KILN)
        return interact_is_firewood_fuel_name(interact_forge_fuels[fuel_index].item_name);

    return 1;
}

static int interact_try_add_forge_fuel_selected(Player* p, Furniture* furn, int fuel_index)
{
    int max_fuel_units;
    int remaining_capacity;

    if(!p || !furn || (furn->type != FURNITURE_FURNACE && furn->type != FURNITURE_FORGE && furn->type != FURNITURE_CHARCOAL_KILN))
        return 0;

    if(fuel_index < 0 || fuel_index >= (int)(sizeof(interact_forge_fuels) / sizeof(interact_forge_fuels[0])))
        return 0;

    if((furn->type == FURNITURE_FURNACE || furn->type == FURNITURE_CHARCOAL_KILN) && furn->process_turns_remaining > 0)
    {
        log_add("The %s is already processing a batch.", interact_heat_source_name(furn));
        return 0;
    }

    if(!interact_fuel_allowed_for_furniture(furn, fuel_index))
    {
        if(furn->type == FURNITURE_CHARCOAL_KILN)
            log_add("The charcoal kiln only accepts firewood.");
        return 0;
    }

    max_fuel_units = interact_fuel_capacity_for_furniture(furn);

    if(furn->fuel_units < 0)
        furn->fuel_units = 0;
    if(furn->fuel_units > max_fuel_units)
        furn->fuel_units = max_fuel_units;

    remaining_capacity = max_fuel_units - furn->fuel_units;

    if(remaining_capacity <= 0)
    {
        log_add("The %s is already full (%d/%d fuel).", interact_heat_source_name(furn), furn->fuel_units, max_fuel_units);
        return 0;
    }

    if(interact_forge_fuels[fuel_index].fuel_units > remaining_capacity)
    {
        log_add("The %s only has room for %d more fuel. Use smaller fuel or process first.", interact_heat_source_name(furn), remaining_capacity);
        return 0;
    }

    if(!interact_consume_carried_item_quantity(p, interact_forge_fuels[fuel_index].item_name, 1))
    {
        log_add("You don't have %s.", interact_forge_fuels[fuel_index].item_name);
        return 0;
    }

    furn->fuel_units += interact_forge_fuels[fuel_index].fuel_units;
    if(furn->fuel_units > max_fuel_units)
        furn->fuel_units = max_fuel_units;

    log_add("You add %s to the %s.", interact_forge_fuels[fuel_index].item_name, interact_heat_source_name(furn));
    log_add("The %s now has %d/%d fuel.", interact_heat_source_name(furn), furn->fuel_units, max_fuel_units);
    return 1;
}

static int interact_pick_furnace_fuel(Player* p, Furniture* furn)
{
    int available_fuels[sizeof(interact_forge_fuels) / sizeof(interact_forge_fuels[0])];
    int fuel_count = 0;
    int selected = 0;
    int max_fuel_units;
    int remaining_capacity;
    int need_world_redraw = 1;
    int any_fuel_added = 0;

    if(!p || !furn || (furn->type != FURNITURE_FURNACE && furn->type != FURNITURE_FORGE && furn->type != FURNITURE_CHARCOAL_KILN))
        return 0;

    max_fuel_units = interact_fuel_capacity_for_furniture(furn);

    while(1)
    {
        int line_i = 0;
        int content_lines;
        int status_line;
        int key;
        char status_text[256];

        remaining_capacity = max_fuel_units - furn->fuel_units;

        if(remaining_capacity <= 0)
        {
            if(any_fuel_added)
            {
                log_add("The %s is now full.", interact_heat_source_name(furn));
                creatures_take_turns(p);
                return 1;
            }
            log_add("The %s is already full.", interact_heat_source_name(furn));
            return 0;
        }

        if(need_world_redraw)
        {
            fuel_count = 0;
            selected = 0;

            for(int i = 0; i < (int)(sizeof(interact_forge_fuels) / sizeof(interact_forge_fuels[0])); ++i)
            {
                if(!interact_fuel_allowed_for_furniture(furn, i))
                    continue;
                if(interact_forge_fuels[i].fuel_units > remaining_capacity)
                    continue;
                if(interact_count_carried_item_quantity(p, interact_forge_fuels[i].item_name) > 0)
                {
                    available_fuels[fuel_count] = i;
                    fuel_count++;
                }
            }

            if(fuel_count <= 0)
            {
                if(any_fuel_added)
                {
                    log_add("No more suitable fuel available.");
                    creatures_take_turns(p);
                    return 1;
                }
                log_add("You don't have any suitable fuel.");
                return 0;
            }

            draw_world(p);
            ui_overlay_draw_frame("Add Fuel");
            ui_overlay_invalidate_cache();
            need_world_redraw = 0;
        }

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;

        for(int i = 0; i < fuel_count && line_i < status_line; i++)
        {
            char line[128];
            int fuel_idx = available_fuels[i];
            int quantity = interact_count_carried_item_quantity(p, interact_forge_fuels[fuel_idx].item_name);

            snprintf(line,
                     sizeof(line),
                     "%c %d. %s (%d fuel) x%d",
                     (i == selected) ? '>' : ' ',
                     i + 1,
                     interact_forge_fuels[fuel_idx].item_name,
                     interact_forge_fuels[fuel_idx].fuel_units,
                     quantity);
            ui_overlay_draw_line(line_i++, line);
        }

        while(line_i < status_line)
            ui_overlay_draw_line(line_i++, "");

        snprintf(status_text,
             sizeof(status_text),
             "Esc/Q back | Enter add | 1-9 quick add | W/S move | %s: %d/%d",
             (furn->type == FURNITURE_CHARCOAL_KILN) ? "Kiln" : "Furnace",
             furn->fuel_units,
             max_fuel_units);
        ui_overlay_draw_line(status_line, status_text);
        ui_overlay_draw_global_hotkeys();

        key = read_input_key();

        if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
        {
            if(any_fuel_added)
                creatures_take_turns(p);
            return any_fuel_added;
        }

        if(key >= '1' && key <= '9')
        {
            int target = key - '1';
            if(target >= 0 && target < fuel_count)
            {
                if(interact_try_add_forge_fuel_selected(p, furn, available_fuels[target]))
                {
                    any_fuel_added = 1;
                    need_world_redraw = 1;
                }
            }
            continue;
        }

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            selected = (selected - 1 + fuel_count) % fuel_count;
            continue;
        }

        if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
        {
            selected = (selected + 1) % fuel_count;
            continue;
        }

        if(key == 13 && selected >= 0 && selected < fuel_count)
        {
            if(interact_try_add_forge_fuel_selected(p, furn, available_fuels[selected]))
            {
                any_fuel_added = 1;
                need_world_redraw = 1;
            }
        }
    }
}

static int interact_can_add_forge_fuel(const Player* p, const Furniture* furn)
{
    int max_fuel_units;
    int remaining_capacity;

    if(!p || !furn || (furn->type != FURNITURE_FURNACE && furn->type != FURNITURE_FORGE && furn->type != FURNITURE_CHARCOAL_KILN))
        return 0;

    if((furn->type == FURNITURE_FURNACE || furn->type == FURNITURE_CHARCOAL_KILN) && furn->process_turns_remaining > 0)
        return 0;

    max_fuel_units = interact_fuel_capacity_for_furniture(furn);

    if(furn->fuel_units >= max_fuel_units)
        return 0;

    remaining_capacity = max_fuel_units - ((furn->fuel_units > 0) ? furn->fuel_units : 0);
    for(int i = 0; i < (int)(sizeof(interact_forge_fuels) / sizeof(interact_forge_fuels[0])); ++i)
    {
        if(!interact_fuel_allowed_for_furniture(furn, i))
            continue;
        if(interact_forge_fuels[i].fuel_units > remaining_capacity)
            continue;
        if(interact_count_carried_item_quantity(p, interact_forge_fuels[i].item_name) > 0)
            return 1;
    }

    return 0;
}

static int interact_is_ignited_forge_at(int x, int y, int z)
{
    Furniture* furn;

    if(!current_area)
        return 0;

    furn = furniture_at_3d(current_area, x, y, z);
    if(!furn || furn->type != FURNITURE_FORGE)
        return 0;

    return furn->is_ignited && furn->fuel_units > 0;
}

static int interact_has_adjacent_ignited_forge(int center_x, int center_y, int z)
{
    for(int dy = -1; dy <= 1; ++dy)
    {
        for(int dx = -1; dx <= 1; ++dx)
        {
            if(dx == 0 && dy == 0)
                continue;

            if(interact_is_ignited_forge_at(center_x + dx, center_y + dy, z))
                return 1;
        }
    }

    return 0;
}

static int interact_anvil_has_required_heat(const Player* p, const Furniture* anvil)
{
    int z;

    if(!p || !anvil)
        return 0;

    z = p->character.actor.entity.z;

    if(interact_has_adjacent_ignited_forge(anvil->base.base.x, anvil->base.base.y, z))
        return 1;

    return interact_has_adjacent_ignited_forge(p->character.actor.entity.x,
                                               p->character.actor.entity.y,
                                               z);
}

static int interact_recipe_requires_unlock(const char* recipe_id)
{
    if(!recipe_id || recipe_id[0] == '\0')
        return 0;

    return strcmp(recipe_id, "Hunting Spear") == 0 ||
           strcmp(recipe_id, "Woodsman Axe") == 0 ||
           strcmp(recipe_id, "Bearded Axe") == 0;
}

static int interact_recipe_is_unlocked(const Player* p, const char* recipe_id)
{
    if(!interact_recipe_requires_unlock(recipe_id))
        return 1;
    if(!p)
        return 0;

    return character_has_recipe_unlock(&p->character, recipe_id);
}

static void interact_register_recipe_in_compendium(const char* recipe_id,
                                                   const char* station,
                                                   NonWeaponSkillType skill,
                                                   int difficulty)
{
    if(!recipe_id || recipe_id[0] == '\0')
        return;

    (void)crafting_compendium_register_recipe(recipe_id, station, skill, difficulty);

    if(interact_recipe_requires_unlock(recipe_id))
        (void)crafting_compendium_upgrade_tier(recipe_id, CRAFTING_DISCOVERY_RUMORED);
}

static int interact_try_add_forge_fuel(Player* p, Furniture* furn, int consume_turn, int log_failure)
{
    int max_fuel_units;
    int remaining_capacity;
    int saw_carried_fuel = 0;

    if(!p || !furn || (furn->type != FURNITURE_FURNACE && furn->type != FURNITURE_FORGE && furn->type != FURNITURE_CHARCOAL_KILN))
        return 0;

    if((furn->type == FURNITURE_FURNACE || furn->type == FURNITURE_CHARCOAL_KILN) && furn->process_turns_remaining > 0)
    {
        if(log_failure)
            log_add("The %s is already processing a batch.", interact_heat_source_name(furn));
        return 0;
    }

    max_fuel_units = interact_fuel_capacity_for_furniture(furn);

    if(furn->fuel_units < 0)
        furn->fuel_units = 0;
    if(furn->fuel_units > max_fuel_units)
        furn->fuel_units = max_fuel_units;

    remaining_capacity = max_fuel_units - furn->fuel_units;
    if(remaining_capacity <= 0)
    {
        if(log_failure)
            log_add("The %s is already full (%d/%d fuel).", interact_heat_source_name(furn), furn->fuel_units, max_fuel_units);
        return 0;
    }

    for(int i = 0; i < (int)(sizeof(interact_forge_fuels) / sizeof(interact_forge_fuels[0])); ++i)
    {
        if(!interact_fuel_allowed_for_furniture(furn, i))
            continue;
        if(interact_count_carried_item_quantity(p, interact_forge_fuels[i].item_name) <= 0)
            continue;

        saw_carried_fuel = 1;
        if(interact_forge_fuels[i].fuel_units > remaining_capacity)
            continue;
        if(!interact_consume_carried_item_quantity(p, interact_forge_fuels[i].item_name, 1))
            continue;

        furn->fuel_units += interact_forge_fuels[i].fuel_units;
        if(furn->fuel_units > max_fuel_units)
            furn->fuel_units = max_fuel_units;

        log_add("You add %s to the %s.", interact_forge_fuels[i].item_name, interact_heat_source_name(furn));
        log_add("The %s now has %d/%d fuel.", interact_heat_source_name(furn), furn->fuel_units, max_fuel_units);
        if(consume_turn)
            creatures_take_turns(p);
        return 1;
    }

    if(log_failure)
    {
        if(saw_carried_fuel)
            log_add("The %s only has room for %d more fuel. Use smaller fuel or process first.", interact_heat_source_name(furn), remaining_capacity);
        else if(furn->type == FURNITURE_CHARCOAL_KILN)
            log_add("The charcoal kiln is cold. Add firewood first.");
        else
            log_add("The furnace is cold. Add logs, lumber, or firewood as fuel first.");
    }

    return 0;
}

static int interact_use_sawhorse(Player* p, Furniture* furn)
{
    static const struct {
        const char* input_name;
        const char* output_name;
        int output_quantity;
        int stage;
    } recipes[] = {
        { "Log", "Wood Bolt", 2, 1 },
        { "Wood Log", "Wood Bolt", 2, 1 },
        { "Oak Log", "Oak Bolt", 2, 1 },
        { "Spruce Log", "Spruce Bolt", 2, 1 },
        { "Pine Log", "Pine Bolt", 2, 1 },
        { "Birch Log", "Birch Bolt", 2, 1 },
        { "Yew Log", "Yew Bolt", 2, 1 },
        { "Maple Log", "Maple Bolt", 2, 1 },
        { "Wood Bolt", "Wood Billet", 2, 2 },
        { "Oak Bolt", "Oak Billet", 2, 2 },
        { "Spruce Bolt", "Spruce Billet", 2, 2 },
        { "Pine Bolt", "Pine Billet", 2, 2 },
        { "Birch Bolt", "Birch Billet", 2, 2 },
        { "Yew Bolt", "Yew Billet", 2, 2 },
        { "Maple Bolt", "Maple Billet", 2, 2 },
        { "Wood Billet", "Firewood", 2, 3 },
        { "Oak Billet", "Oak Firewood", 2, 3 },
        { "Spruce Billet", "Spruce Firewood", 2, 3 },
        { "Pine Billet", "Pine Firewood", 2, 3 },
        { "Birch Billet", "Birch Firewood", 2, 3 },
        { "Yew Billet", "Yew Firewood", 2, 3 },
        { "Maple Billet", "Maple Firewood", 2, 3 }
    };
    int processed_any = 0;
    int had_locked_recipe = 0;
    int levels_gained = 0;
    WorldItem* dragged_item;

    if(!p || !furn)
        return 0;

    dragged_item = interact_dragged_world_item(p);
    if(dragged_item && interact_item_is_draggable_lumber(&dragged_item->item))
    {
        char dragged_name[sizeof(dragged_item->item.name)];
        int dragged_amount = interact_item_available_quantity(&dragged_item->item);
        int dragged_index = world_item_index_of(dragged_item);

        snprintf(dragged_name, sizeof(dragged_name), "%s", dragged_item->item.name);
        if(dragged_amount > 0)
        {
            if(!interact_add_template_item_to_inventory(p, dragged_name, dragged_amount))
            {
                log_add("You do not have enough room to lift %s onto the sawhorse.", dragged_name);
                return 1;
            }

            if(dragged_index >= 0)
                (void)world_item_remove(dragged_index);
            p->dragged_world_item_index = -1;
            log_add("You lift %d %s onto the sawhorse.", dragged_amount, dragged_name);
        }
    }

    if(!interact_has_item_anywhere(p, "Saw"))
    {
        log_add("You need a saw in your pack or hands to use the sawhorse.");
        return 1;
    }

    for(int i = 0; i < (int)(sizeof(recipes) / sizeof(recipes[0])); ++i)
    {
        int output_amount;

        interact_register_recipe_in_compendium(recipes[i].output_name,
                                               "Sawhorse",
                                               NON_WEAPON_SKILL_CARPENTRY,
                                               recipes[i].stage);

        if(interact_count_carried_item_quantity(p, recipes[i].input_name) <= 0)
            continue;

        if(!item_template_by_name(recipes[i].output_name))
        {
            log_add("The sawhorse lacks a valid recipe output for %s.", recipes[i].input_name);
            continue;
        }

        if(!interact_recipe_is_unlocked(p, recipes[i].output_name))
        {
            had_locked_recipe = 1;
            continue;
        }

        if(!interact_consume_carried_item_quantity(p, recipes[i].input_name, 1))
            continue;

        output_amount = recipes[i].output_quantity;
        if(!interact_drop_template_item_near_furniture(furn, recipes[i].output_name, output_amount))
        {
            (void)interact_add_template_item_to_inventory(p, recipes[i].input_name, 1);
            log_add("There is no clear space beside the sawhorse for the processed %s.", recipes[i].output_name);
            return 1;
        }

        (void)crafting_compendium_mark_attempt(recipes[i].output_name, 1);

        log_add("You process 1 %s into %d %s at the sawhorse and leave the result beside the station.",
                recipes[i].input_name,
                output_amount,
                recipes[i].output_name);
        processed_any = 1;
        break;
    }

    if(processed_any)
    {
        levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, NON_WEAPON_SKILL_CARPENTRY, 5);
        if(levels_gained > 0)
        {
            log_add("Your %s skill improved to %d!",
                    non_weapon_skill_name(NON_WEAPON_SKILL_CARPENTRY),
                    actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_CARPENTRY));
        }

        creatures_take_turns(p);
        return 1;
    }

    if(had_locked_recipe)
    {
        log_add("You need to read a matching recipe book before crafting advanced sawhorse recipes.");
        return 1;
    }

    log_add("You need logs, wood bolts, or wood billets in your pack, or a dragged log ready for the sawhorse.");
    return 1;
}

static int interact_use_chopping_block(Player* p, Furniture* furn)
{
    static const struct {
        const char* input_name;
        const char* output_name;
        int output_quantity;
    } recipes[] = {
        { "Wood Billet", "Firewood", 2 },
        { "Oak Billet", "Oak Firewood", 2 },
        { "Spruce Billet", "Spruce Firewood", 2 },
        { "Pine Billet", "Pine Firewood", 2 },
        { "Birch Billet", "Birch Firewood", 2 },
        { "Yew Billet", "Yew Firewood", 2 },
        { "Maple Billet", "Maple Firewood", 2 }
    };
    int processed_any = 0;
    int levels_gained = 0;

    if(!p || !furn)
        return 0;

    if(!interact_equipped_axe(p))
    {
        log_add("You need an axe equipped in hand to use the chopping block.");
        return 1;
    }

    for(int i = 0; i < (int)(sizeof(recipes) / sizeof(recipes[0])); ++i)
    {
        int output_amount;

        interact_register_recipe_in_compendium(recipes[i].output_name,
                                               "Chopping Block",
                                               NON_WEAPON_SKILL_LUMBERJACKING,
                                               1);

        if(interact_count_carried_item_quantity(p, recipes[i].input_name) <= 0)
            continue;

        if(!item_template_by_name(recipes[i].output_name))
        {
            log_add("The chopping block lacks a valid recipe output for %s.", recipes[i].input_name);
            continue;
        }

        if(!interact_consume_carried_item_quantity(p, recipes[i].input_name, 1))
            continue;

        output_amount = recipes[i].output_quantity;
        if(!interact_drop_template_item_near_furniture(furn, recipes[i].output_name, output_amount))
        {
            (void)interact_add_template_item_to_inventory(p, recipes[i].input_name, 1);
            log_add("There is no clear space beside the chopping block for the split %s.", recipes[i].output_name);
            return 1;
        }

        (void)crafting_compendium_mark_attempt(recipes[i].output_name, 1);

        log_add("You split 1 %s into %d %s at the chopping block and leave the result beside the station.",
                recipes[i].input_name,
                output_amount,
                recipes[i].output_name);
        processed_any = 1;
        break;
    }

    if(processed_any)
    {
        levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, NON_WEAPON_SKILL_LUMBERJACKING, 5);
        if(levels_gained > 0)
        {
            log_add("Your %s skill improved to %d!",
                    non_weapon_skill_name(NON_WEAPON_SKILL_LUMBERJACKING),
                    actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_LUMBERJACKING));
        }

        creatures_take_turns(p);
        return 1;
    }

    log_add("You need wood billets in your pack to split at the chopping block.");
    return 1;
}

typedef struct SmeltingRecipe {
    int recipe_index;
    const char* input_name;
    int input_amount;
    const char* secondary_input_name;
    int secondary_input_amount;
    const char* output_name;
    int output_amount;
    int fuel_cost;
    int difficulty;
    int xp;
    int is_bloom_reheat;
} SmeltingRecipe;

static const SmeltingRecipe interact_furnace_batch_recipes[] = {
    { 0, "Iron Ore", 1, NULL, 0, "Small Heated Bloom", 1, 1, 2, 5, 0 },
    { 1, "Copper Ore", 1, NULL, 0, "Copper Ingot", 1, 1, 2, 5, 0 },
    { 2, "Tin Ore", 1, NULL, 0, "Tin Ingot", 1, 1, 2, 5, 0 },
    { 3, "Lead Ore", 1, NULL, 0, "Lead Ingot", 1, 1, 2, 5, 0 },
    { 4, "Zinc Ore", 1, NULL, 0, "Zinc Ingot", 1, 1, 2, 5, 0 },
    { 5, "Coal Ore", 1, NULL, 0, "Coke", 1, 1, 3, 7, 0 },
    { 6, "Small Iron Bloom", 1, NULL, 0, "Small Heated Bloom", 1, 1, 1, 1, 1 },
    { 7, "Medium Iron Bloom", 1, NULL, 0, "Medium Sized Heated Bloom", 1, 1, 1, 1, 1 },
    { 8, "Large Iron Bloom", 1, NULL, 0, "Large Heated Bloom", 1, 1, 1, 1, 1 }
};

static const char* interact_heated_bloom_template_for_size(int size)
{
    if(size <= 0)
        return NULL;
    if(size <= 6)
        return "Small Heated Bloom";
    if(size <= 14)
        return "Medium Sized Heated Bloom";
    return "Large Heated Bloom";
}

static const char* interact_cooled_bloom_template_for_heated(const char* heated_name)
{
    if(!heated_name || !heated_name[0])
        return NULL;

    if(strcmp(heated_name, "Small Heated Bloom") == 0)
        return "Small Iron Bloom";
    if(strcmp(heated_name, "Medium Sized Heated Bloom") == 0)
        return "Medium Iron Bloom";
    if(strcmp(heated_name, "Large Heated Bloom") == 0)
        return "Large Iron Bloom";

    return NULL;
}

static int interact_is_hot_bloom_item(const Item* item)
{
    return item && item->type != ITEM_TYPE_NONE &&
           (strcmp(item->name, "Small Heated Bloom") == 0 ||
            strcmp(item->name, "Medium Sized Heated Bloom") == 0 ||
            strcmp(item->name, "Large Heated Bloom") == 0);
}

static int interact_is_cooled_bloom_item_name(const char* name)
{
    return name &&
           (strcmp(name, "Small Iron Bloom") == 0 ||
            strcmp(name, "Medium Iron Bloom") == 0 ||
            strcmp(name, "Large Iron Bloom") == 0 ||
            strcmp(name, "Iron Bloom") == 0);
}

static int interact_store_bloom_size_in_station_output(Furniture* furn, int size)
{
    const char* template_name;
    const ItemTemplate* tmpl;
    WorldContainer* output;
    int output_index;
    Item bloom;

    if(!furn || size <= 0)
        return 0;

    template_name = interact_heated_bloom_template_for_size(size);
    if(!template_name)
        return 0;

    tmpl = item_template_by_name(template_name);
    if(!tmpl)
        return 0;

    output = interact_station_output_container(furn, 1);
    if(!output)
        return 0;

    output_index = world_container_index_of(output);
    if(output_index < 0)
        return 0;

    item_init_from_template(&bloom, tmpl, output->x, output->y);
    bloom.object.base.z = output->z;
    bloom.quantity = size;

    return world_container_add_item(output_index, &bloom);
}

static void interact_transform_item_to_template(Item* item, const char* template_name)
{
    const ItemTemplate* tmpl;
    Item replacement;
    ItemQuality quality;
    int quantity;
    int x;
    int y;
    int z;

    if(!item || !template_name || !template_name[0])
        return;

    tmpl = item_template_by_name(template_name);
    if(!tmpl)
        return;

    quality = item->quality;
    quantity = (item->quantity > 0) ? item->quantity : 1;
    x = item->object.base.x;
    y = item->object.base.y;
    z = item->object.base.z;

    item_init_from_template_with_quality(&replacement, tmpl, x, y, quality);
    replacement.object.base.z = z;
    if(replacement.stackable)
        replacement.quantity = quantity;
    else
        replacement.quantity = 1;

    *item = replacement;
}

static void interact_advance_hot_item(Item* item, int log_if_player_owned)
{
    const char* cooled_name;

    if(!item || item->type == ITEM_TYPE_NONE || item->heat_state != ITEM_HEAT_HOT)
        return;

    if(item->heat_turns_remaining > 0)
        item->heat_turns_remaining--;

    if(item->heat_turns_remaining > 0)
        return;

    if(interact_is_hot_bloom_item(item))
    {
        cooled_name = interact_cooled_bloom_template_for_heated(item->name);
        if(!cooled_name)
            cooled_name = "Small Iron Bloom";
        interact_transform_item_to_template(item, cooled_name);
        if(log_if_player_owned)
            log_add("Your heated bloom cools into %s.", item->name);
        return;
    }

    item->heat_state = ITEM_HEAT_NONE;
    item->heat_turns_remaining = 0;
}

static void interact_advance_hot_items_for_turn(Player* p)
{
    if(!p)
        return;

    for(int i = 0; i < p->character.equipment_slot_count; ++i)
        interact_advance_hot_item(&p->character.equipment_slots[i].item, 1);

    for(int i = 0; i < MAX_WORLD_ITEMS; ++i)
    {
        if(!world_items[i].active)
            continue;
        interact_advance_hot_item(&world_items[i].item, 0);
    }

    for(int i = 0; i < MAX_WORLD_CONTAINERS; ++i)
    {
        if(!world_containers[i].active)
            continue;

        for(int item_i = 0; item_i < world_containers[i].item_count; ++item_i)
            interact_advance_hot_item(&world_containers[i].items[item_i], 0);
    }
}

static int interact_container_is_furnace_input(const WorldContainer* container)
{
    return container && strcmp(container->label, "Furnace Input") == 0;
}

static int interact_furnace_recipe_count(void)
{
    return (int)(sizeof(interact_furnace_batch_recipes) / sizeof(interact_furnace_batch_recipes[0]));
}

static const SmeltingRecipe* interact_furnace_recipe_by_index(int recipe_index)
{
    if(recipe_index < 0 || recipe_index >= interact_furnace_recipe_count())
        return NULL;

    return &interact_furnace_batch_recipes[recipe_index];
}

static int interact_item_matches_furnace_recipe_input(const Item* item, const SmeltingRecipe* recipe)
{
    if(!item || !recipe || item->type == ITEM_TYPE_NONE)
        return 0;

    return strcmp(item->name, recipe->input_name) == 0;
}

static int interact_furnace_input_total_quantity(const WorldContainer* container)
{
    int total = 0;

    if(!container || !container->active)
        return 0;

    for(int i = 0; i < container->item_count; ++i)
    {
        const Item* item = &container->items[i];
        if(item->type == ITEM_TYPE_NONE)
            continue;
        total += interact_item_available_quantity(item);
    }

    return total;
}

static int interact_furnace_matching_input_quantity(const WorldContainer* container, const SmeltingRecipe* recipe)
{
    int total = 0;

    if(!container || !recipe)
        return 0;

    for(int i = 0; i < container->item_count; ++i)
    {
        const Item* item = &container->items[i];
        if(!interact_item_matches_furnace_recipe_input(item, recipe))
            continue;
        total += interact_item_available_quantity(item);
    }

    return total;
}

static int interact_furnace_remove_input_quantity(WorldContainer* container, const SmeltingRecipe* recipe, int amount)
{
    int container_index;

    if(!container || !recipe || amount <= 0)
        return 0;

    container_index = world_container_index_of(container);
    if(container_index < 0)
        return 0;

    for(int i = 0; i < container->item_count && amount > 0; )
    {
        Item* item = &container->items[i];
        int available;
        int consume_amount;

        if(!interact_item_matches_furnace_recipe_input(item, recipe))
        {
            i++;
            continue;
        }

        available = interact_item_available_quantity(item);
        consume_amount = (available < amount) ? available : amount;

        if(item->stackable && available > consume_amount)
        {
            item->quantity = available - consume_amount;
            amount -= consume_amount;
            i++;
            continue;
        }

        {
            Item removed_item;
            if(!world_container_remove_item(container_index, i, &removed_item))
                return 0;
        }
        amount -= consume_amount;
    }

    return amount == 0;
}

static int interact_furnace_reheat_input_size(const WorldContainer* container, const SmeltingRecipe* recipe)
{
    if(!container || !recipe)
        return 0;

    for(int i = 0; i < container->item_count; ++i)
    {
        const Item* item = &container->items[i];

        if(!interact_item_matches_furnace_recipe_input(item, recipe))
            continue;
        if(item->type == ITEM_TYPE_NONE)
            continue;

        return item->quantity > 0 ? item->quantity : 1;
    }

    return 0;
}

static int interact_furnace_first_available_recipe(WorldContainer* container)
{
    for(int recipe_i = 0; recipe_i < interact_furnace_recipe_count(); ++recipe_i)
    {
        if(interact_furnace_matching_input_quantity(container, &interact_furnace_batch_recipes[recipe_i]) > 0)
            return recipe_i;
    }

    return -1;
}

static void interact_reset_station_process(Furniture* furn)
{
    if(!furn)
        return;

    furn->process_turns_total = 0;
    furn->process_turns_remaining = 0;
    furn->process_firewood_burned = 0;
    furn->process_bonus_output = 0;
    furn->process_recipe_index = -1;
    furn->process_output_count = 0;
    furn->process_failed_count = 0;
}

static int interact_furnace_input_accept_quantity(const WorldContainer* container,
                                                  const Item* item,
                                                  int* accepted_quantity,
                                                  char* reason,
                                                  size_t reason_size)
{
    int total_quantity;
    int available_quantity;
    int recipe_index;
    const SmeltingRecipe* recipe;

    if(accepted_quantity)
        *accepted_quantity = 0;

    if(!container || !item || item->type == ITEM_TYPE_NONE)
        return 0;

    recipe_index = -1;
    for(int i = 0; i < interact_furnace_recipe_count(); ++i)
    {
        if(interact_item_matches_furnace_recipe_input(item, &interact_furnace_batch_recipes[i]))
        {
            recipe_index = i;
            break;
        }
    }

    if(recipe_index < 0)
    {
        if(reason && reason_size > 0)
            snprintf(reason, reason_size, "Furnace input only accepts smeltable ore.");
        return 0;
    }

    total_quantity = interact_furnace_input_total_quantity(container);
    if(total_quantity >= FURNITURE_FORGE_MAX_FUEL_UNITS)
    {
        if(reason && reason_size > 0)
            snprintf(reason, reason_size, "Furnace input is full.");
        return 0;
    }

    recipe = &interact_furnace_batch_recipes[recipe_index];
    {
        int existing_recipe_index = interact_furnace_first_available_recipe((WorldContainer*)container);
        if(existing_recipe_index >= 0)
        {
            const SmeltingRecipe* existing_recipe = &interact_furnace_batch_recipes[existing_recipe_index];
            if(strcmp(existing_recipe->input_name, recipe->input_name) != 0)
            {
                if(reason && reason_size > 0)
                    snprintf(reason, reason_size, "Furnace input can only hold one ore type per batch.");
                return 0;
            }
        }
    }

    available_quantity = interact_item_available_quantity(item);
    if(available_quantity <= 0)
        return 0;

    available_quantity = FURNITURE_FORGE_MAX_FUEL_UNITS - total_quantity < available_quantity
        ? FURNITURE_FORGE_MAX_FUEL_UNITS - total_quantity
        : available_quantity;

    if(accepted_quantity)
        *accepted_quantity = available_quantity;

    return available_quantity > 0;
}

static int interact_start_furnace_batch(Player* p, Furniture* furn)
{
    WorldContainer* input;
    const SmeltingRecipe* recipe;
    int recipe_index;
    int batch_size;
    int smelting_skill;
    int coal_burning_skill;
    int success_chance;
    int output_count = 0;
    int failed_count = 0;

    if(!p || !furn || furn->type != FURNITURE_FURNACE)
        return 0;

    if(furn->process_turns_remaining > 0)
    {
        log_add("The furnace is already smelting a batch.");
        return 1;
    }

    input = interact_station_input_container(furn, 1);
    if(!input)
    {
        log_add("The furnace input is missing.");
        return 0;
    }

    recipe_index = interact_furnace_first_available_recipe(input);
    if(recipe_index < 0)
    {
        log_add("The furnace input is empty. Load ore or a cooled bloom into it first.");
        return 1;
    }

    recipe = interact_furnace_recipe_by_index(recipe_index);
    if(!recipe)
        return 0;

    batch_size = interact_furnace_matching_input_quantity(input, recipe);
    if(batch_size > furn->fuel_units)
        batch_size = furn->fuel_units;
    if(batch_size > FURNITURE_FORGE_MAX_FUEL_UNITS)
        batch_size = FURNITURE_FORGE_MAX_FUEL_UNITS;

    if(batch_size <= 0)
    {
        log_add("The furnace needs at least 1 ore and 1 fuel to start smelting.");
        return 1;
    }

    if(recipe->is_bloom_reheat)
    {
        int bloom_size;

        bloom_size = interact_furnace_reheat_input_size(input, recipe);
        if(bloom_size <= 0)
        {
            log_add("The furnace batch stalls because the bloom is missing.");
            return 1;
        }

        if(!interact_furnace_remove_input_quantity(input, recipe, 1))
        {
            log_add("The furnace batch stalls because the bloom is missing.");
            return 1;
        }

        furn->is_ignited = 1;
        furn->process_turns_total = bloom_size;
        furn->process_turns_remaining = bloom_size;
        furn->process_firewood_burned = 0;
        furn->process_bonus_output = 0;
        furn->process_recipe_index = recipe_index;
        furn->process_output_count = bloom_size;
        furn->process_failed_count = 0;

        log_add("You load %s into the furnace to reheat. It will be ready in %d turns.",
                recipe->input_name,
                bloom_size);
        creatures_take_turns(p);
        return 1;
    }

    smelting_skill = actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_SMELTING);
    coal_burning_skill = actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_COAL_BURNING);
    success_chance = 60 + (smelting_skill * 6) - (recipe->difficulty * 8);
    if(interact_recipe_is_coal_related(recipe))
        success_chance += (coal_burning_skill * 4) / 5;
    success_chance += crafting_compendium_mastery_success_bonus(recipe->output_name);
    if(success_chance < 15)
        success_chance = 15;
    if(success_chance > 95)
        success_chance = 95;

    for(int i = 0; i < batch_size; ++i)
    {
        int success = (rand() % 100) < success_chance;
        (void)crafting_compendium_mark_attempt(recipe->output_name, success);
        if(success)
            output_count += recipe->output_amount;
        else
            failed_count += recipe->input_amount;
    }

    furn->is_ignited = 1;
    furn->process_turns_total = batch_size;
    furn->process_turns_remaining = batch_size;
    furn->process_firewood_burned = 0;
    furn->process_bonus_output = 0;
    furn->process_recipe_index = recipe_index;
    furn->process_output_count = output_count;
    furn->process_failed_count = failed_count;

    log_add("You ignite the furnace and start smelting %d %s.", batch_size, recipe->input_name);
    creatures_take_turns(p);
    return 1;
}

static void interact_finish_furnace_batch(Player* p, Furniture* furn)
{
    const SmeltingRecipe* recipe;
    NonWeaponSkillType gain_skill;
    int xp_amount;
    int levels_gained;

    if(!p || !furn)
        return;

    recipe = interact_furnace_recipe_by_index(furn->process_recipe_index);
    if(!recipe)
    {
        interact_reset_station_process(furn);
        furn->is_ignited = (furn->fuel_units > 0) ? 1 : 0;
        return;
    }

    if(furn->process_output_count > 0)
    {
        if(strcmp(recipe->input_name, "Iron Ore") == 0)
        {
            if(interact_store_bloom_size_in_station_output(furn, furn->process_output_count))
            {
                log_add("The furnace finishes smelting and leaves a %s (size %d) in the output.",
                        interact_heated_bloom_template_for_size(furn->process_output_count),
                        furn->process_output_count);
            }
            else
            {
                log_add("The furnace finishes smelting, but the output container is full.");
            }
        }
        else if(recipe->is_bloom_reheat)
        {
            if(interact_store_bloom_size_in_station_output(furn, furn->process_output_count))
            {
                log_add("The furnace finishes reheating and leaves a %s (size %d) in the output.",
                        interact_heated_bloom_template_for_size(furn->process_output_count),
                        furn->process_output_count);
            }
            else
            {
                log_add("The furnace finishes reheating, but the output container is full.");
            }
        }
        else if(interact_store_template_item_in_station_output(furn, recipe->output_name, furn->process_output_count))
        {
            log_add("The furnace finishes smelting and leaves %d %s in the output.",
                    furn->process_output_count,
                    recipe->output_name);
        }
        else
        {
            log_add("The furnace finishes smelting, but the output container is full.");
        }
    }

    if(furn->process_failed_count > 0)
        log_add("%d ore are spoiled in the batch.", furn->process_failed_count);

    gain_skill = interact_recipe_is_coal_related(recipe) ? NON_WEAPON_SKILL_COAL_BURNING : NON_WEAPON_SKILL_SMELTING;
    xp_amount = (furn->process_output_count * recipe->xp) + (furn->process_failed_count * (recipe->xp / 2));
    if(xp_amount <= 0)
        xp_amount = recipe->xp;
    levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, gain_skill, xp_amount);
    if(levels_gained > 0)
    {
        log_add("Your %s skill improved to %d!",
                non_weapon_skill_name(gain_skill),
                actor_get_non_weapon_skill(&p->character.actor, gain_skill));
    }

    interact_reset_station_process(furn);
    furn->is_ignited = (furn->fuel_units > 0) ? 1 : 0;
}

static void interact_finish_kiln_batch(Player* p, Furniture* furn)
{
    int charcoal_output;
    int levels_gained;

    if(!p || !furn)
        return;

    charcoal_output = furn->process_firewood_burned + furn->process_bonus_output;
    if(charcoal_output < furn->process_firewood_burned)
        charcoal_output = furn->process_firewood_burned;

    if(charcoal_output > 0)
    {
        if(interact_store_template_item_in_station_output(furn, "Charcoal", charcoal_output))
            log_add("The charcoal kiln cools down and leaves %d Charcoal in the output.", charcoal_output);
        else
            log_add("The charcoal kiln finishes burning, but the output container is full.");
    }

    levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor,
                                                   NON_WEAPON_SKILL_COAL_BURNING,
                                                   furn->process_turns_total > 0 ? furn->process_turns_total : 5);
    if(levels_gained > 0)
    {
        log_add("Your %s skill improved to %d!",
                non_weapon_skill_name(NON_WEAPON_SKILL_COAL_BURNING),
                actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_COAL_BURNING));
    }

    interact_reset_station_process(furn);
    furn->is_ignited = (furn->fuel_units > 0) ? 1 : 0;
}

void interact_process_station_turn(Player* p)
{
    if(!p || !current_area)
        return;

    interact_advance_hot_items_for_turn(p);

    for(int furn_i = 0; furn_i < current_area->furniture_count; ++furn_i)
    {
        Furniture* furn = &current_area->furniture[furn_i];

        if(!furn || furn->type == FURNITURE_NONE || furn->process_turns_remaining <= 0)
            continue;

        if(furn->type == FURNITURE_CHARCOAL_KILN)
        {
            if(furn->fuel_units <= 0)
            {
                log_add("The charcoal kiln sputters out before the batch completes.");
                furn->is_ignited = 0;
                interact_reset_station_process(furn);
                continue;
            }

            furn->fuel_units--;
            furn->process_turns_remaining--;
            furn->process_firewood_burned++;

            if(furn->process_turns_remaining <= 0)
                interact_finish_kiln_batch(p, furn);

            continue;
        }

        if(furn->type == FURNITURE_FURNACE)
        {
            WorldContainer* input;
            const SmeltingRecipe* recipe;

            if(furn->fuel_units <= 0)
            {
                log_add("The furnace runs out of fuel before the batch completes.");
                furn->is_ignited = 0;
                interact_reset_station_process(furn);
                continue;
            }

            input = interact_station_input_container(furn, 1);
            recipe = interact_furnace_recipe_by_index(furn->process_recipe_index);
            if(!input || !recipe ||
               (!recipe->is_bloom_reheat && !interact_furnace_remove_input_quantity(input, recipe, 1)))
            {
                log_add("The furnace batch stalls because its input is interrupted.");
                furn->is_ignited = 0;
                interact_reset_station_process(furn);
                continue;
            }

            furn->fuel_units--;
            furn->process_turns_remaining--;

            if(furn->process_turns_remaining <= 0)
                interact_finish_furnace_batch(p, furn);
        }
    }
}

static int interact_name_is_coal_related(const char* name)
{
    return name &&
           (strstr(name, "Coal") != NULL ||
            strstr(name, "coal") != NULL ||
            strstr(name, "Coke") != NULL ||
            strstr(name, "coke") != NULL ||
            strstr(name, "Charcoal") != NULL ||
            strstr(name, "charcoal") != NULL);
}

static int interact_recipe_is_coal_related(const SmeltingRecipe* recipe)
{
    if(!recipe)
        return 0;

    return interact_name_is_coal_related(recipe->input_name) ||
           interact_name_is_coal_related(recipe->secondary_input_name) ||
           interact_name_is_coal_related(recipe->output_name);
}

static int interact_execute_smelting_recipe(Player* p, Furniture* furn, const SmeltingRecipe* recipe)
{
    int coal_burning_skill;
    int coal_related;
    NonWeaponSkillType gain_skill;
    int smelting_skill;
    int success_chance;
    int roll;
    int levels_gained;

    if(!p || !furn || !recipe)
        return 0;

    if(furn->fuel_units < recipe->fuel_cost)
    {
        log_add("The furnace is too low on fuel for that process. Add more fuel first.");
        return 0;
    }

    if(!item_template_by_name(recipe->output_name))
    {
        log_add("The furnace lacks a valid recipe output for %s.", recipe->input_name);
        return 0;
    }

    if(!interact_consume_carried_item_quantity(p, recipe->input_name, recipe->input_amount))
        return 0;

    if(recipe->secondary_input_name && recipe->secondary_input_name[0])
    {
        if(!interact_consume_carried_item_quantity(p, recipe->secondary_input_name, recipe->secondary_input_amount))
        {
            (void)interact_add_template_item_to_inventory(p, recipe->input_name, recipe->input_amount);
            return 0;
        }
    }

    coal_related = interact_recipe_is_coal_related(recipe);
    smelting_skill = actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_SMELTING);
    coal_burning_skill = actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_COAL_BURNING);
    gain_skill = coal_related ? NON_WEAPON_SKILL_COAL_BURNING : NON_WEAPON_SKILL_SMELTING;

    success_chance = 60 + (smelting_skill * 6) - (recipe->difficulty * 8);
    if(coal_related)
        success_chance += (coal_burning_skill * 4) / 5;
    success_chance += crafting_compendium_mastery_success_bonus(recipe->output_name);
    if(success_chance < 15)
        success_chance = 15;
    if(success_chance > 95)
        success_chance = 95;
    roll = rand() % 100;

    (void)crafting_compendium_mark_attempt(recipe->output_name, roll < success_chance);

    furn->fuel_units -= recipe->fuel_cost;
    if(furn->fuel_units < 0)
        furn->fuel_units = 0;

    if(roll >= success_chance)
    {
        log_add("You fail to properly smelt %s.", recipe->input_name);
    }
    else if(!interact_add_template_item_to_inventory(p, recipe->output_name, recipe->output_amount))
    {
        if(!interact_drop_template_item_near_furniture(furn, recipe->output_name, recipe->output_amount))
        {
            (void)interact_add_template_item_to_inventory(p, recipe->input_name, recipe->input_amount);
            if(recipe->secondary_input_name && recipe->secondary_input_name[0])
                (void)interact_add_template_item_to_inventory(p, recipe->secondary_input_name, recipe->secondary_input_amount);
            log_add("You do not have enough room to collect the smelted %s.", recipe->output_name);
            return 1;
        }

        log_add("You smelt %d %s into %d %s and set the output beside the furnace.",
                recipe->input_amount,
                recipe->input_name,
                recipe->output_amount,
                recipe->output_name);
    }
    else
    {
        log_add("You smelt %d %s into %d %s.",
                recipe->input_amount,
                recipe->input_name,
                recipe->output_amount,
                recipe->output_name);
    }

    if(furn->fuel_units <= 0)
    {
        furn->fuel_units = 0;
        furn->is_ignited = 0;
        log_add("The furnace burns through its fuel and goes dark.");
    }
    else
    {
        log_add("The %s remains lit with %d/%d fuel left.",
                interact_heat_source_name(furn),
                furn->fuel_units,
                interact_fuel_capacity_for_furniture(furn));
    }

    if(roll < success_chance)
        levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, gain_skill, recipe->xp);
    else
        levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, gain_skill, recipe->xp / 2);

    if(levels_gained > 0)
    {
        log_add("Your %s skill improved to %d!",
                non_weapon_skill_name(gain_skill),
                actor_get_non_weapon_skill(&p->character.actor, gain_skill));
    }

    return 1;
}

static int interact_pick_smelting_recipe(Player* p, Furniture* furn)
{
    static const struct {
        const char* input_name;
        int input_amount;
        const char* secondary_input_name;
        int secondary_input_amount;
        const char* output_name;
        int output_amount;
        int fuel_cost;
        int difficulty;
        int xp;
        FurnitureType required_furniture;
    } all_recipes[] = {
        { "Iron Ore", 1, NULL, 0, "Iron Ingot", 1, 1, 2, 5, FURNITURE_FURNACE },
        { "Copper Ore", 1, NULL, 0, "Copper Ingot", 1, 1, 2, 5, FURNITURE_FURNACE },
        { "Tin Ore", 1, NULL, 0, "Tin Ingot", 1, 1, 2, 5, FURNITURE_FURNACE },
        { "Lead Ore", 1, NULL, 0, "Lead Ingot", 1, 1, 2, 5, FURNITURE_FURNACE },
        { "Zinc Ore", 1, NULL, 0, "Zinc Ingot", 1, 1, 2, 5, FURNITURE_FURNACE },
        { "Coal Ore", 1, NULL, 0, "Coke", 1, 1, 3, 7, FURNITURE_FURNACE },
        { "Iron Ingot", 1, NULL, 0, "Wrought Iron", 1, 1, 3, 8, FURNITURE_FURNACE },
        { "Iron Ingot", 1, NULL, 0, "Cast Iron", 1, 2, 4, 9, FURNITURE_FURNACE },
        { "Wrought Iron", 1, "Charcoal", 1, "Crucible Steel", 1, 2, 6, 12, FURNITURE_FURNACE }
    };
    SmeltingRecipe available_recipes[sizeof(all_recipes) / sizeof(all_recipes[0])];
    int recipe_count = 0;
    int selected = 0;
    int max_fuel_units;
    int need_world_redraw = 1;
    int any_smelting_done = 0;

    if(!p || !furn)
        return 0;

    max_fuel_units = interact_fuel_capacity_for_furniture(furn);

    while(1)
    {
        int line_i = 0;
        int content_lines;
        int status_line;
        int key;
        char status_text[256];

        // Recalculate available recipes each loop
        recipe_count = 0;
        selected = 0;

        for(int i = 0; i < (int)(sizeof(all_recipes) / sizeof(all_recipes[0])); ++i)
        {
            int coal_related = interact_name_is_coal_related(all_recipes[i].input_name) ||
                               interact_name_is_coal_related(all_recipes[i].secondary_input_name) ||
                               interact_name_is_coal_related(all_recipes[i].output_name);

            interact_register_recipe_in_compendium(all_recipes[i].output_name,
                                                   (all_recipes[i].required_furniture == FURNITURE_CHARCOAL_KILN) ? "Charcoal Kiln" : "Furnace",
                                                   coal_related ? NON_WEAPON_SKILL_COAL_BURNING : NON_WEAPON_SKILL_SMELTING,
                                                   all_recipes[i].difficulty);

            if(all_recipes[i].required_furniture != furn->type)
                continue;

            if(interact_count_carried_item_quantity(p, all_recipes[i].input_name) < all_recipes[i].input_amount)
                continue;

            if(all_recipes[i].secondary_input_name && all_recipes[i].secondary_input_name[0])
            {
                if(interact_count_carried_item_quantity(p, all_recipes[i].secondary_input_name) < all_recipes[i].secondary_input_amount)
                    continue;
            }

            if(furn->fuel_units < all_recipes[i].fuel_cost)
                continue;

            available_recipes[recipe_count].recipe_index = i;
            available_recipes[recipe_count].input_name = all_recipes[i].input_name;
            available_recipes[recipe_count].input_amount = all_recipes[i].input_amount;
            available_recipes[recipe_count].secondary_input_name = all_recipes[i].secondary_input_name;
            available_recipes[recipe_count].secondary_input_amount = all_recipes[i].secondary_input_amount;
            available_recipes[recipe_count].output_name = all_recipes[i].output_name;
            available_recipes[recipe_count].output_amount = all_recipes[i].output_amount;
            available_recipes[recipe_count].fuel_cost = all_recipes[i].fuel_cost;
            available_recipes[recipe_count].difficulty = all_recipes[i].difficulty;
            available_recipes[recipe_count].xp = all_recipes[i].xp;
            recipe_count++;
        }

        if(recipe_count <= 0)
        {
            if(any_smelting_done)
            {
                log_add("No more recipes available to smelt.");
                creatures_take_turns(p);
                return 1;
            }
            log_add("You don't have the materials or fuel to smelt anything.");
            return 0;
        }

        if(need_world_redraw)
        {
            draw_world(p);
            ui_overlay_draw_frame("Smelt");
            ui_overlay_invalidate_cache();
            need_world_redraw = 0;
        }

        content_lines = ui_overlay_content_lines();
        status_line = (content_lines > 1) ? (content_lines - 2) : 0;

        for(int i = 0; i < recipe_count && line_i < status_line; i++)
        {
            char line[128];
            int input_qty = interact_count_carried_item_quantity(p, available_recipes[i].input_name);
            int secondary_qty = 0;
            
            if(available_recipes[i].secondary_input_name && available_recipes[i].secondary_input_name[0])
                secondary_qty = interact_count_carried_item_quantity(p, available_recipes[i].secondary_input_name);

            if(available_recipes[i].secondary_input_name && available_recipes[i].secondary_input_name[0])
            {
                snprintf(line,
                         sizeof(line),
                         "%c %d. %s x%d + %s x%d -> %s",
                         (i == selected) ? '>' : ' ',
                         i + 1,
                         available_recipes[i].input_name,
                         input_qty,
                         available_recipes[i].secondary_input_name,
                         secondary_qty,
                         available_recipes[i].output_name);
            }
            else
            {
                snprintf(line,
                         sizeof(line),
                         "%c %d. %s x%d -> %s",
                         (i == selected) ? '>' : ' ',
                         i + 1,
                         available_recipes[i].input_name,
                         input_qty,
                         available_recipes[i].output_name);
            }
            ui_overlay_draw_line(line_i++, line);
        }

        while(line_i < status_line)
            ui_overlay_draw_line(line_i++, "");

        snprintf(status_text, sizeof(status_text), "Esc/Q back | Enter smelt | 1-9 quick smelt | W/S move | Fuel: %d/%d", furn->fuel_units, max_fuel_units);
        ui_overlay_draw_line(status_line, status_text);
        ui_overlay_draw_global_hotkeys();

        key = read_input_key();

        if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
        {
            if(any_smelting_done)
                creatures_take_turns(p);
            return any_smelting_done;
        }

        if(key >= '1' && key <= '9')
        {
            int target = key - '1';
            if(target >= 0 && target < recipe_count)
            {
                if(interact_execute_smelting_recipe(p, furn, &available_recipes[target]))
                {
                    any_smelting_done = 1;
                    need_world_redraw = 1;
                }
            }
            continue;
        }

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            selected = (selected - 1 + recipe_count) % recipe_count;
            continue;
        }

        if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
        {
            selected = (selected + 1) % recipe_count;
            continue;
        }

        if(key == 13 && selected >= 0 && selected < recipe_count)
        {
            if(interact_execute_smelting_recipe(p, furn, &available_recipes[selected]))
            {
                any_smelting_done = 1;
                need_world_redraw = 1;
            }
        }
    }
}

static int interact_use_forge(Player* p, Furniture* furn)
{
    if(!p || !furn)
        return 0;

    if(furn->fuel_units < 0)
        furn->fuel_units = 0;
    if(furn->fuel_units > FURNITURE_FORGE_MAX_FUEL_UNITS)
        furn->fuel_units = FURNITURE_FORGE_MAX_FUEL_UNITS;

    if(furn->fuel_units <= 0)
    {
        furn->fuel_units = 0;
        furn->is_ignited = 0;
        return interact_try_add_forge_fuel(p, furn, 1, 1);
    }

    if(!furn->is_ignited)
    {
        return interact_start_furnace_batch(p, furn);
    }

    if(furn->process_turns_remaining > 0)
    {
        log_add("The furnace is already smelting a batch (%d turns remaining).", furn->process_turns_remaining);
        return 1;
    }

    return interact_start_furnace_batch(p, furn);
}

static int interact_use_charcoal_kiln(Player* p, Furniture* furn)
{
    int coal_burning_skill;
    int firewood_used;
    int bonus_output;

    if(!p || !furn)
        return 0;

    if(furn->fuel_units < 0)
        furn->fuel_units = 0;
    if(furn->fuel_units > FURNITURE_CHARCOAL_KILN_MAX_FUEL_UNITS)
        furn->fuel_units = FURNITURE_CHARCOAL_KILN_MAX_FUEL_UNITS;

    if(furn->fuel_units <= 0)
    {
        furn->fuel_units = 0;
        furn->is_ignited = 0;
        return interact_try_add_forge_fuel(p, furn, 1, 1);
    }

    if(furn->process_turns_remaining > 0)
    {
        log_add("The charcoal kiln is already burning a batch (%d turns remaining).", furn->process_turns_remaining);
        return 1;
    }

    firewood_used = furn->fuel_units;
    coal_burning_skill = actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_COAL_BURNING);
    bonus_output = (firewood_used * coal_burning_skill) / 100;

    furn->is_ignited = 1;
    furn->process_turns_total = firewood_used;
    furn->process_turns_remaining = firewood_used;
    furn->process_firewood_burned = 0;
    furn->process_bonus_output = bonus_output;
    furn->process_recipe_index = -1;
    furn->process_output_count = 0;
    furn->process_failed_count = 0;
    log_add("You ignite the kiln. The batch will burn for %d turns.", firewood_used);

    creatures_take_turns(p);
    return 1;
}

static int interact_use_anvil(Player* p, Furniture* anvil)
{
    static const struct {
        const char* input_name;
        int input_amount;
        const char* secondary_input_name;
        int secondary_input_amount;
        const char* output_name;
        int output_amount;
        const char* partial_output_name;
        int partial_output_amount;
        int difficulty;
        int xp;
    } recipes[] = {
        { "Wrought Iron", 1, NULL, 0, "Iron Nails", 8, "Iron Scrap", 1, 2, 6 },
        { "Wrought Iron", 1, NULL, 0, "Iron Hinge", 2, "Iron Scrap", 1, 3, 7 },
        { "Wrought Iron", 1, NULL, 0, "Iron Hook", 1, "Iron Scrap", 1, 3, 7 },
        { "Wrought Iron", 1, NULL, 0, "Iron Piton", 3, "Iron Scrap", 1, 3, 8 },
        { "Wrought Iron", 1, "Wood Plank", 1, "Hunting Spear", 1, "Iron Scrap", 1, 4, 9 },
        { "Wrought Iron", 1, NULL, 0, "Iron Mace", 1, "Iron Scrap", 1, 5, 10 },
        { "Crucible Steel", 1, NULL, 0, "Throwing Knife", 1, "Iron Scrap", 1, 6, 11 },
        { "Crucible Steel", 1, "Wood Plank", 1, "Woodsman Axe", 1, "Iron Scrap", 1, 7, 12 },
        { "Crucible Steel", 1, "Wood Plank", 1, "Bearded Axe", 1, "Iron Scrap", 1, 8, 13 }
    };
    int had_locked_recipe = 0;
    int missing_tongs_for_heated = 0;

    if(!p || !anvil)
        return 0;

    if(!interact_anvil_has_required_heat(p, anvil))
    {
        log_add("You need an ignited forge adjacent to the station or to yourself before forging.");
        return 1;
    }

    /* --- Heated bloom working (requires sledge hammer instead of smithing hammer) --- */
    {
        static const char* const bloom_templates[] = {
            "Small Heated Bloom", "Medium Sized Heated Bloom", "Large Heated Bloom"
        };
        int b;

        for(b = 0; b < 3; ++b)
        {
            const Item* bloom;
            int bloom_size;
            int ingot_count;
            int levels_gained;

            if(interact_count_carried_item_quantity(p, bloom_templates[b]) <= 0)
                continue;

            bloom = interact_find_carried_item(p, bloom_templates[b]);
            bloom_size = bloom ? bloom->quantity : 1;

            if(!interact_equipped_tongs(p))
            {
                log_add("You need tongs equipped to handle a heated bloom at the anvil.");
                return 1;
            }

            if(!interact_equipped_sledge_hammer(p))
            {
                log_add("You need a sledge hammer to work the bloom into wrought iron.");
                return 1;
            }

            ingot_count = (bloom_size * 2) / 3;
            if(ingot_count < 1)
                ingot_count = 1;

            interact_register_recipe_in_compendium("Wrought Iron", "Anvil",
                                                   NON_WEAPON_SKILL_BLACKSMITHING, 3);

            if(!interact_consume_carried_item_quantity(p, bloom_templates[b], 1))
                continue;

            if(!interact_add_template_item_to_inventory(p, "Wrought Iron", ingot_count))
                (void)interact_drop_template_item_near_furniture(anvil, "Wrought Iron", ingot_count);

            log_add("You work the %s under the sledge. The slag falls away, leaving %d wrought iron bar%s.",
                    bloom_templates[b], ingot_count, ingot_count == 1 ? "" : "s");

            levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor,
                                                           NON_WEAPON_SKILL_BLACKSMITHING, 7);
            if(levels_gained > 0)
            {
                log_add("Your %s skill improved to %d!",
                        non_weapon_skill_name(NON_WEAPON_SKILL_BLACKSMITHING),
                        actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_BLACKSMITHING));
            }

            creatures_take_turns(p);
            return 1;
        }
    }

    if(!interact_equipped_smithing_hammer(p))
    {
        log_add("You need a smithing hammer equipped in hand to smith here.");
        return 1;
    }

    for(int i = 0; i < (int)(sizeof(recipes) / sizeof(recipes[0])); ++i)
    {
        int blacksmithing_skill;
        int success_chance;
        int partial_chance;
        int roll;
        int levels_gained;

        interact_register_recipe_in_compendium(recipes[i].output_name,
                                               "Anvil",
                                               NON_WEAPON_SKILL_BLACKSMITHING,
                                               recipes[i].difficulty);

        if(interact_count_carried_item_quantity(p, recipes[i].input_name) < recipes[i].input_amount)
            continue;

        if(interact_template_requires_tongs(recipes[i].input_name) && !interact_equipped_tongs(p))
        {
            missing_tongs_for_heated = 1;
            continue;
        }

        if(recipes[i].secondary_input_name && recipes[i].secondary_input_name[0])
        {
            if(interact_count_carried_item_quantity(p, recipes[i].secondary_input_name) < recipes[i].secondary_input_amount)
                continue;

            if(interact_template_requires_tongs(recipes[i].secondary_input_name) && !interact_equipped_tongs(p))
            {
                missing_tongs_for_heated = 1;
                continue;
            }
        }

        if(!item_template_by_name(recipes[i].output_name))
        {
            log_add("The anvil recipe output %s is missing.", recipes[i].output_name);
            return 1;
        }

        if(!interact_recipe_is_unlocked(p, recipes[i].output_name))
        {
            had_locked_recipe = 1;
            continue;
        }

        if(!interact_consume_carried_item_quantity(p, recipes[i].input_name, recipes[i].input_amount))
            continue;

        if(recipes[i].secondary_input_name && recipes[i].secondary_input_name[0])
        {
            if(!interact_consume_carried_item_quantity(p, recipes[i].secondary_input_name, recipes[i].secondary_input_amount))
            {
                (void)interact_add_template_item_to_inventory(p, recipes[i].input_name, recipes[i].input_amount);
                continue;
            }
        }

        blacksmithing_skill = actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_BLACKSMITHING);
        success_chance = 50 + (blacksmithing_skill * 7) - (recipes[i].difficulty * 8);
        success_chance += crafting_compendium_mastery_success_bonus(recipes[i].output_name);
        if(success_chance < 10)
            success_chance = 10;
        if(success_chance > 95)
            success_chance = 95;

        partial_chance = success_chance + 25;
        if(partial_chance > 98)
            partial_chance = 98;

        roll = rand() % 100;

        (void)crafting_compendium_mark_attempt(recipes[i].output_name, roll < success_chance);

        if(roll < success_chance)
        {
            if(!interact_add_template_item_to_inventory(p, recipes[i].output_name, recipes[i].output_amount))
            {
                if(!interact_drop_template_item_near_furniture(anvil, recipes[i].output_name, recipes[i].output_amount))
                {
                    (void)interact_add_template_item_to_inventory(p, recipes[i].input_name, recipes[i].input_amount);
                    if(recipes[i].secondary_input_name && recipes[i].secondary_input_name[0])
                        (void)interact_add_template_item_to_inventory(p, recipes[i].secondary_input_name, recipes[i].secondary_input_amount);
                    log_add("You cannot place the forged %s anywhere.", recipes[i].output_name);
                    return 1;
                }

                log_add("You forge %d %s and place it beside the anvil.", recipes[i].output_amount, recipes[i].output_name);
            }
            else
            {
                log_add("You forge %d %s.", recipes[i].output_amount, recipes[i].output_name);
            }

            levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, NON_WEAPON_SKILL_BLACKSMITHING, recipes[i].xp);
        }
        else if(roll < partial_chance)
        {
            if(recipes[i].partial_output_name && recipes[i].partial_output_name[0] && item_template_by_name(recipes[i].partial_output_name))
            {
                if(!interact_add_template_item_to_inventory(p, recipes[i].partial_output_name, recipes[i].partial_output_amount))
                    (void)interact_drop_template_item_near_furniture(anvil, recipes[i].partial_output_name, recipes[i].partial_output_amount);
            }

            log_add("Your hammer work is uneven. You salvage only scrap.");
            levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, NON_WEAPON_SKILL_BLACKSMITHING, recipes[i].xp / 2);
        }
        else
        {
            log_add("The piece warps under the hammer and is ruined.");
            levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, NON_WEAPON_SKILL_BLACKSMITHING, recipes[i].xp / 3);
        }

        if(levels_gained > 0)
        {
            log_add("Your %s skill improved to %d!",
                    non_weapon_skill_name(NON_WEAPON_SKILL_BLACKSMITHING),
                    actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_BLACKSMITHING));
        }

        creatures_take_turns(p);
        return 1;
    }

    for(int slot_i = 0; slot_i < p->character.equipment_slot_count; ++slot_i)
    {
        const EquipmentSlot* slot = &p->character.equipment_slots[slot_i];
        if(slot->slot_type != EQUIP_SLOT_NONE || slot->item.type == ITEM_TYPE_NONE)
            continue;
        if(interact_is_cooled_bloom_item_name(slot->item.name))
        {
            log_add("Cooled blooms cannot be refined; you need a freshly heated bloom from the furnace.");
            return 1;
        }
    }

    if(had_locked_recipe)
    {
        log_add("You recognize the technique, but need a recipe book to forge that item.");
        return 1;
    }

    if(missing_tongs_for_heated)
    {
        log_add("You need iron tongs equipped to handle heated metal at the anvil.");
        return 1;
    }

    log_add("You need a heated bloom, wrought iron, or crucible steel components in your pack to forge here.");
    return 1;
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
    action->npc = NULL;
    action->world_item = world_item;
    action->world_container = world_container;
    action->world_corpse = NULL;
    action->furniture = NULL; // default, set by caller if needed
    action->tx = tx;
    action->ty = ty;
    // Slot-based fields are set by caller if needed

    snprintf(action->label, sizeof(action->label), "%s", label ? label : "Action");
    if(disabled_reason && disabled_reason[0] != '\0')
        snprintf(action->disabled_reason, sizeof(action->disabled_reason), "%s", disabled_reason);
    else
        action->disabled_reason[0] = '\0';

    (*count)++;
}

// Try interacting with a creature first, per inspect interaction priority.
static int interact_inventory_visible_count(const Character* c)
{
    int count = 0;

    if(!c)
        return 0;

    for(int i = EQUIP_SLOT_COUNT; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->slot_type != EQUIP_SLOT_NONE || slot->item.type == ITEM_TYPE_NONE)
            continue;
        count++;
    }

    return count;
}

static int interact_inventory_slot_from_visible_index(const Character* c, int visible_index)
{
    int count = 0;

    if(!c || visible_index < 0)
        return -1;

    for(int i = EQUIP_SLOT_COUNT; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        if(slot->slot_type != EQUIP_SLOT_NONE || slot->item.type == ITEM_TYPE_NONE)
            continue;
        if(count == visible_index)
            return i;
        count++;
    }

    return -1;
}

static const char* interact_food_reaction_label(CreatureFoodReaction reaction)
{
    switch(reaction)
    {
        case CREATURE_FOOD_REACTION_LIKED: return "liked";
        case CREATURE_FOOD_REACTION_TOLERATED: return "tolerated";
        case CREATURE_FOOD_REACTION_HATED: return "hated";
        default: return "unknown";
    }
}

static int interact_feedable_inventory_count(const Character* c, const Creature* creature)
{
    int count = 0;

    if(!c || !creature)
        return 0;

    for(int i = EQUIP_SLOT_COUNT; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        const Item* item = &slot->item;

        if(slot->slot_type != EQUIP_SLOT_NONE || item->type == ITEM_TYPE_NONE)
            continue;
        if(interact_item_available_quantity(item) <= 0)
            continue;
        if(!creature_can_eat_item(creature, item))
            continue;

        count++;
    }

    return count;
}

static int interact_feedable_slot_from_visible_index(const Character* c,
                                                     const Creature* creature,
                                                     int visible_index)
{
    int count = 0;

    if(!c || !creature || visible_index < 0)
        return -1;

    for(int i = EQUIP_SLOT_COUNT; i < c->equipment_slot_count; ++i)
    {
        const EquipmentSlot* slot = &c->equipment_slots[i];
        const Item* item = &slot->item;

        if(slot->slot_type != EQUIP_SLOT_NONE || item->type == ITEM_TYPE_NONE)
            continue;
        if(interact_item_available_quantity(item) <= 0)
            continue;
        if(!creature_can_eat_item(creature, item))
            continue;

        if(count == visible_index)
            return i;
        count++;
    }

    return -1;
}

static int interact_select_food_for_creature(Player* p, Creature* creature, int* out_slot_index)
{
    int selected = 0;
    int scroll_offset = 0;
    char title[96];

    if(!p || !creature || !creature->template || !out_slot_index)
        return 0;

    snprintf(title, sizeof(title), "Feed - %s", creature->template->name);

    while(1)
    {
        int item_count = interact_feedable_inventory_count(&p->character, creature);
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
        max_scroll = item_count - visible_rows;
        if(max_scroll < 0)
            max_scroll = 0;

        if(item_count <= 0)
        {
            selected = 0;
            scroll_offset = 0;
            if(line_i < status_line)
                ui_overlay_draw_line(line_i++, "You have no suitable food for this animal.");
            while(line_i < status_line)
                ui_overlay_draw_line(line_i++, "");
            ui_overlay_draw_line(status_line, "Esc/Q back");
            ui_overlay_draw_global_hotkeys();
        }
        else
        {
            if(selected < 0)
                selected = 0;
            if(selected >= item_count)
                selected = item_count - 1;
            if(selected < scroll_offset)
                scroll_offset = selected;
            if(visible_rows > 0 && selected >= scroll_offset + visible_rows)
                scroll_offset = selected - visible_rows + 1;
            if(scroll_offset < 0)
                scroll_offset = 0;
            if(scroll_offset > max_scroll)
                scroll_offset = max_scroll;

            for(int visible_i = scroll_offset; visible_i < item_count && line_i < status_line; ++visible_i)
            {
                int slot_index = interact_feedable_slot_from_visible_index(&p->character, creature, visible_i);
                char line[128];
                char display_name[96];
                const Item* item;
                int shown_quantity;
                CreatureFoodReaction reaction;

                if(slot_index < 0)
                    continue;

                item = &p->character.equipment_slots[slot_index].item;
                shown_quantity = (item->quantity > 0) ? item->quantity : 1;
                reaction = creature_template_food_reaction(creature->template, item);
                item_format_display_name(item, display_name, sizeof(display_name));
                snprintf(line,
                         sizeof(line),
                         "%c %2d. %-22s x%d [%s]",
                         (visible_i == selected) ? '>' : ' ',
                         visible_i + 1,
                         display_name,
                         shown_quantity,
                         interact_food_reaction_label(reaction));
                ui_overlay_draw_line(line_i++, line);
            }

            while(line_i < status_line)
                ui_overlay_draw_line(line_i++, "");

            ui_overlay_draw_line(status_line, "Enter feed | W/S move | PgUp/PgDn jump | Home/End | Esc/Q back");
            ui_overlay_draw_global_hotkeys();
        }

        key = read_input_key();

        if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
            return 0;

        if(item_count <= 0)
            continue;

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            if(selected > 0)
                selected--;
            continue;
        }

        if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
        {
            if(selected < item_count - 1)
                selected++;
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
            if(selected >= item_count)
                selected = item_count - 1;
            continue;
        }

        if(key == INPUT_KEY_HOME)
        {
            selected = 0;
            continue;
        }

        if(key == INPUT_KEY_END)
        {
            selected = item_count - 1;
            continue;
        }

        if(key == 13)
        {
            int slot_index = interact_feedable_slot_from_visible_index(&p->character, creature, selected);

            if(slot_index < 0)
                continue;

            *out_slot_index = slot_index;
            return 1;
        }
    }
}

static int interact_deposit_to_container(Player* p, WorldContainer* container)
{
    int selected = 0;
    int scroll_offset = 0;
    int deposited_any = 0;
    char title[96];

    if(!p || !container || !container->active)
        return 0;

    if(strstr(container->label, "Output") != NULL)
    {
        log_add("%s only stores completed output.", container->label);
        return 0;
    }

    snprintf(title, sizeof(title), "Deposit - %s", container->label);

    while(1)
    {
        int item_count = interact_inventory_visible_count(&p->character);
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
        max_scroll = item_count - visible_rows;
        if(max_scroll < 0)
            max_scroll = 0;

        if(item_count <= 0)
        {
            selected = 0;
            scroll_offset = 0;
            if(line_i < status_line) ui_overlay_draw_line(line_i++, "You have no carried inventory items to deposit.");
            while(line_i < status_line) ui_overlay_draw_line(line_i++, "");
            ui_overlay_draw_line(status_line, "Esc/Q back | Enter none");
            ui_overlay_draw_global_hotkeys();
        }
        else
        {
            if(selected < 0) selected = 0;
            if(selected >= item_count) selected = item_count - 1;
            if(selected < scroll_offset)
                scroll_offset = selected;
            if(visible_rows > 0 && selected >= scroll_offset + visible_rows)
                scroll_offset = selected - visible_rows + 1;
            if(scroll_offset < 0)
                scroll_offset = 0;
            if(scroll_offset > max_scroll)
                scroll_offset = max_scroll;

            for(int visible_i = scroll_offset; visible_i < item_count && line_i < status_line; ++visible_i)
            {
                int slot_index = interact_inventory_slot_from_visible_index(&p->character, visible_i);
                char line[128];
                char display_name[96];
                const Item* item;
                int shown_quantity;

                if(slot_index < 0)
                    continue;

                item = &p->character.equipment_slots[slot_index].item;
                shown_quantity = (item->quantity > 0) ? item->quantity : 1;
                item_format_display_name(item, display_name, sizeof(display_name));
                snprintf(line,
                         sizeof(line),
                         "%c %2d. %-28s x%d",
                         (visible_i == selected) ? '>' : ' ',
                         visible_i + 1,
                         display_name,
                         shown_quantity);
                ui_overlay_draw_line(line_i++, line);
            }

            while(line_i < status_line)
                ui_overlay_draw_line(line_i++, "");

            ui_overlay_draw_line(status_line, "Enter deposit | W/S move | PgUp/PgDn jump | Home/End | Esc/Q back");
            ui_overlay_draw_global_hotkeys();
        }

        key = read_input_key();

        if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
            return deposited_any;

        if(item_count <= 0)
            continue;

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            if(selected > 0) selected--;
            continue;
        }

        if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
        {
            if(selected < item_count - 1) selected++;
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
            if(selected >= item_count)
                selected = item_count - 1;
            continue;
        }

        if(key == INPUT_KEY_HOME)
        {
            selected = 0;
            continue;
        }

        if(key == INPUT_KEY_END)
        {
            selected = item_count - 1;
            continue;
        }

        if(key == 13)
        {
            int container_index = world_container_index_of(container);
            int slot_index = interact_inventory_slot_from_visible_index(&p->character, selected);
            Item moved_item;
            char moved_name[96];
            int accepted_quantity = 0;
            char reject_reason[96] = "";

            if(container_index < 0 || slot_index < 0)
                continue;

            moved_item = p->character.equipment_slots[slot_index].item;
            if(moved_item.type == ITEM_TYPE_NONE)
                continue;

            item_format_display_name(&moved_item, moved_name, sizeof(moved_name));
            accepted_quantity = interact_item_available_quantity(&moved_item);

            if(interact_container_is_furnace_input(container))
            {
                if(!interact_furnace_input_accept_quantity(container, &moved_item, &accepted_quantity, reject_reason, sizeof(reject_reason)))
                {
                    log_add("%s", reject_reason[0] ? reject_reason : "That item does not belong in the furnace input.");
                    continue;
                }

                if(accepted_quantity < interact_item_available_quantity(&moved_item) && moved_item.stackable)
                    moved_item.quantity = accepted_quantity;
            }

            if(!world_container_add_item(container_index, &moved_item))
            {
                log_add("%s cannot hold any more items.", container->label);
                continue;
            }

            if(p->character.equipment_slots[slot_index].item.stackable &&
               p->character.equipment_slots[slot_index].item.quantity > accepted_quantity)
            {
                p->character.equipment_slots[slot_index].item.quantity -= accepted_quantity;
            }
            else if(!inventory_remove(&p->character, slot_index))
            {
                Item rollback_item;
                (void)world_container_remove_item(container_index, container->item_count - 1, &rollback_item);
                log_add("Failed to move %s into %s.", moved_name, container->label);
                continue;
            }

            deposited_any = 1;
            if(interact_container_is_furnace_input(container) && accepted_quantity != interact_item_available_quantity(&moved_item))
                log_add("You place %d %s into %s.", accepted_quantity, moved_name, container->label);
            else
                log_add("You place %s into %s.", moved_name, container->label);
            continue;
        }
    }
}

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
        int animal_handling = actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_ANIMAL_HANDLING);
        creature_apply_pet_event(creature, animal_handling);

        if(creature->template->tamable)
        {
            int levels_gained;

            log_add("You pet the %s. [%s]", creature->template->name, taming_stage_name(creature->taming_stage));
            levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, NON_WEAPON_SKILL_ANIMAL_HANDLING, 5);
            if(levels_gained > 0)
            {
                log_add("Your %s skill improved to %d!",
                        non_weapon_skill_name(NON_WEAPON_SKILL_ANIMAL_HANDLING),
                        actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_ANIMAL_HANDLING));
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

static int interact_feed_creature(Player* p, Creature* creature)
{
    int slot_index;
    int animal_handling;
    int levels_gained = 0;
    Item offered_item;
    CreatureFoodReaction reaction;
    char offered_name[96];

    if(!p || !creature || !creature->alive || !creature->template)
        return 0;

    if(creature_is_hostile(creature))
    {
        log_add("%s is hostile. Feeding it now would be reckless.", creature->template->name);
        return 0;
    }

    if(interact_feedable_inventory_count(&p->character, creature) <= 0)
    {
        log_add("You have no suitable food for the %s.", creature->template->name);
        return 0;
    }

    if(!interact_select_food_for_creature(p, creature, &slot_index))
        return 0;

    if(slot_index < 0 || slot_index >= p->character.equipment_slot_count)
        return 0;

    offered_item = p->character.equipment_slots[slot_index].item;
    if(interact_item_available_quantity(&offered_item) <= 0)
        return 0;

    animal_handling = actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_ANIMAL_HANDLING);
    reaction = creature_template_food_reaction(creature->template, &offered_item);
    if(reaction == CREATURE_FOOD_REACTION_NONE)
    {
        log_add("The %s refuses that offering.", creature->template->name);
        return 0;
    }

    if(!interact_consume_inventory_slot_quantity(p, slot_index, 1))
    {
        log_add("You fail to hand over the food.");
        return 0;
    }

    reaction = creature_apply_feed_event(creature, &offered_item, animal_handling);
    item_format_display_name(&offered_item, offered_name, sizeof(offered_name));

    if(reaction == CREATURE_FOOD_REACTION_LIKED)
    {
        log_add("You feed %s to the %s. It eagerly eats it. [%s]",
                offered_name,
                creature->template->name,
                taming_stage_name(creature->taming_stage));
        levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, NON_WEAPON_SKILL_ANIMAL_HANDLING, 8);
    }
    else if(reaction == CREATURE_FOOD_REACTION_TOLERATED)
    {
        log_add("You feed %s to the %s. It accepts the offering. [%s]",
                offered_name,
                creature->template->name,
                taming_stage_name(creature->taming_stage));
        levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, NON_WEAPON_SKILL_ANIMAL_HANDLING, 3);
    }
    else
    {
        log_add("You offer %s to the %s. It recoils from the food. [%s]",
                offered_name,
                creature->template->name,
                taming_stage_name(creature->taming_stage));
    }

    if(levels_gained > 0)
    {
        log_add("Your %s skill improved to %d!",
                non_weapon_skill_name(NON_WEAPON_SKILL_ANIMAL_HANDLING),
                actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_ANIMAL_HANDLING));
    }

    creatures_take_turns(p);
    return 1;
}

static int interact_npc_response(Player* p, NPC* npc, const char* topic, const char* response, const char* footer)
{
    const char* speaker;

    if(!p || !npc || !npc->active || !response || response[0] == '\0')
        return 0;

    speaker = npc_display_name(npc);
    ui_overlay_show_mini_prompt(speaker, response, footer ? footer : "");
    log_add("%s (%s): %s", speaker, topic ? topic : "talk", response);
    creatures_take_turns(p);
    return 1;
}

static int interact_talk_npc(Player* p, NPC* npc)
{
    InteractionAction talk_actions[2];
    int selected_action;

    if(!p || !npc || !npc->active)
        return 0;

    memset(talk_actions, 0, sizeof(talk_actions));

    talk_actions[0].type = INTERACTION_ACTION_NPC_GREET;
    talk_actions[0].enabled = 1;
    talk_actions[0].npc = npc;
    snprintf(talk_actions[0].label, sizeof(talk_actions[0].label), "Greet");

    talk_actions[1].type = INTERACTION_ACTION_NPC_GOSSIP;
    talk_actions[1].enabled = 1;
    talk_actions[1].npc = npc;
    snprintf(talk_actions[1].label, sizeof(talk_actions[1].label), "Gossip");

    selected_action = interaction_show_menu(p, npc_display_name(npc), talk_actions, 2);
    if(selected_action < 0)
        return 0;

    return interaction_run_action(p, &talk_actions[selected_action]);
}

static int interact_process_corpse(Player* p, WorldCorpse* corpse, int skinning_phase)
{
    int levels_gained;
    int corpse_index;
    int total_harvested;
    int added_to_inventory = 0;
    int dropped_to_ground = 0;

    if(!p || !corpse || !corpse->active)
        return 0;

    if(corpse->type != WORLD_CORPSE_CREATURE)
    {
        log_add("This corpse cannot be processed that way.");
        return 0;
    }

    if(!interact_has_tool_for_skill_anywhere(p, NON_WEAPON_SKILL_SKINNING))
    {
        log_add("You need a skinning tool in your hands or pack to process %s.", corpse->label);
        return 1;
    }

    if(skinning_phase)
    {
        if(corpse->skinned)
        {
            log_add("%s has already been skinned.", corpse->label);
            return 1;
        }

        corpse->skinned = 1;
        total_harvested = world_corpse_drop_loot(corpse,
                                                 &p->character,
                                                 1,
                                                 &added_to_inventory,
                                                 &dropped_to_ground);
        if(total_harvested > 0)
        {
            if(dropped_to_ground > 0)
                log_add("You skin the %s and pack what you can; the rest falls nearby.", corpse->source_name);
            else
                log_add("You skin the %s and stow the harvest in your inventory.", corpse->source_name);
        }
        else
            log_add("You skin the %s, but find little worth keeping.", corpse->source_name);
    }
    else
    {
        if(corpse->butchered)
        {
            log_add("%s has already been butchered.", corpse->label);
            return 1;
        }

        corpse->butchered = 1;
        total_harvested = world_corpse_drop_loot(corpse,
                                                 &p->character,
                                                 0,
                                                 &added_to_inventory,
                                                 &dropped_to_ground);
        if(total_harvested > 0)
        {
            if(dropped_to_ground > 0)
                log_add("You butcher the %s and carry what you can; the rest falls nearby.", corpse->source_name);
            else
                log_add("You butcher the %s and pack the cuts into your inventory.", corpse->source_name);
        }
        else
            log_add("You butcher the %s, but recover nothing useful.", corpse->source_name);
    }

    levels_gained = actor_gain_non_weapon_skill_xp(&p->character.actor, NON_WEAPON_SKILL_SKINNING, skinning_phase ? 5 : 8);
    if(levels_gained > 0)
    {
        log_add("Your %s skill improved to %d!",
                non_weapon_skill_name(NON_WEAPON_SKILL_SKINNING),
                actor_get_non_weapon_skill(&p->character.actor, NON_WEAPON_SKILL_SKINNING));
    }

    world_corpse_refresh_label(corpse);
    if((corpse->skinning_loot_count <= 0 || corpse->skinned) &&
       (corpse->butchering_loot_count <= 0 || corpse->butchered))
    {
        corpse_index = world_corpse_index_of(corpse);
        if(corpse_index >= 0)
        {
            log_add("Nothing usable remains of the %s.", corpse->source_name);
            (void)world_corpse_remove(corpse_index);
        }
    }

    creatures_take_turns(p);
    return 1;
}

// Open one world container and allow item pickup via overlay.
int interact_open_container(Player* p, WorldContainer* container)
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
            ui_overlay_draw_line(status_line, "Esc/Q close | D deposit item | W/S move");
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
                char display_name[96];
                item_format_display_name(&container->items[i], display_name, sizeof(display_name));
                snprintf(line, sizeof(line), "%c %2d. %-28s x%d",
                         (i == selected) ? '>' : ' ',
                         i + 1,
                         display_name,
                         container->items[i].quantity > 0 ? container->items[i].quantity : 1);
                ui_overlay_draw_line(line_i++, line);
            }

            while(line_i < status_line)
                ui_overlay_draw_line(line_i++, "");

            ui_overlay_draw_line(status_line, "Esc/Q close | Enter take | D deposit | W/S move | PgUp/PgDn jump | Home/End");
            ui_overlay_draw_global_hotkeys();
        }

        {
            int key = read_input_key();

            if(key == 'q' || key == 'Q' || key == 27 || key == 'e' || key == 'E')
                break;

            if(key == 'd' || key == 'D')
            {
                if(interact_deposit_to_container(p, container))
                {
                    took_any = 1;
                    need_world_redraw = 1;
                }
                continue;
            }

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
                    int handled_pickup = 0;

                    if(strcmp(picked_item.name, "Gold Coins") == 0)
                    {
                        int gold_amount = picked_item.quantity > 0 ? picked_item.quantity : 1;
                        p->gold += gold_amount;
                        log_add("You take %d gold from %s.", gold_amount, container->label);
                        handled_pickup = 1;
                    }
                    else if(!interact_can_handle_item_with_tongs(p, &picked_item, "handle"))
                    {
                        (void)world_container_add_item(container_index, &picked_item);
                        need_world_redraw = 1;
                    }
                    else if(inventory_add(&p->character, &picked_item))
                    {
                        char picked_name[96];
                        item_format_display_name(&picked_item, picked_name, sizeof(picked_name));
                        log_add("You take %s from %s.", picked_name, container->label);
                        handled_pickup = 1;
                    }
                    else
                    {
                        char picked_name[96];
                        item_format_display_name(&picked_item, picked_name, sizeof(picked_name));
                        log_add("No space in inventory for %s.", picked_name);
                        (void)world_container_add_item(container_index, &picked_item);
                        need_world_redraw = 1;
                    }

                    if(handled_pickup)
                    {
                        took_any = 1;
                        need_world_redraw = 1;

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
                }
            }
        }
    }

    return took_any;
}

static int interact_tile(Player* p, int tx, int ty);

static char g_interaction_last_target[96] = "";
static char g_interaction_last_label[80] = "";

static int interaction_initial_selection(const char* target_name, InteractionAction* actions, int action_count)
{
    if(!actions || action_count <= 0)
        return 0;

    if(g_interaction_last_label[0] != '\0')
    {
        for(int i = 0; i < action_count; ++i)
        {
            if(strcmp(actions[i].label, g_interaction_last_label) != 0)
                continue;

            if((target_name && target_name[0] && strcmp(g_interaction_last_target, target_name) == 0) ||
               strcmp(actions[i].label, "Go up") == 0 ||
               strcmp(actions[i].label, "Go down") == 0)
                return i;
        }
    }

    return 0;
}

static int interaction_show_menu(Player* p, const char* target_name, InteractionAction* actions, int action_count)
{
    int selected;
    int scroll_offset = 0;
    char title[96];

    if(!p || !actions || action_count <= 0)
        return -1;

    selected = interaction_initial_selection(target_name, actions, action_count);

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
            char state_tag[96];

            if(actions[i].enabled)
                state_tag[0] = '\0';
            else if(actions[i].disabled_reason[0] != '\0')
                snprintf(state_tag, sizeof(state_tag), " [%s]", actions[i].disabled_reason);
            else
                snprintf(state_tag, sizeof(state_tag), " [disabled]");

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

            snprintf(g_interaction_last_target,
                     sizeof(g_interaction_last_target),
                     "%s",
                     (target_name && target_name[0]) ? target_name : "Target");
            snprintf(g_interaction_last_label,
                     sizeof(g_interaction_last_label),
                     "%s",
                     actions[selected].label);
            return selected;
        }
    }
}

static int interaction_action_keeps_menu_open(const InteractionAction* action)
{
    if(!action || !action->furniture)
        return 0;

        if(action->furniture->type != FURNITURE_FORGE &&
            action->furniture->type != FURNITURE_FURNACE &&
         action->furniture->type != FURNITURE_ANVIL &&
       action->furniture->type != FURNITURE_SAWHORSE &&
       action->furniture->type != FURNITURE_CHOPPING_BLOCK)
        return 0;

    return action->type == INTERACTION_ACTION_TILE_USE ||
           action->type == INTERACTION_ACTION_ADD_FUEL_TO_FORGE ||
           action->type == INTERACTION_ACTION_EXTINGUISH_FORGE;
}

static int interact_item_is_draggable_lumber(const Item* item)
{
    if(!item || item->type == ITEM_TYPE_NONE)
        return 0;

    return item_is_material(item) &&
           item->material_type == MATERIAL_TYPE_WOOD &&
           item->material_state == MATERIAL_STATE_UNREFINED;
}

static int interact_toggle_drag_world_item(Player* p, WorldItem* world_item)
{
    int world_index;

    if(!p || !world_item || !world_item->active)
        return 0;

    world_index = world_item_index_of(world_item);
    if(world_index < 0)
    {
        log_add("You cannot get a grip on %s right now.", world_item->item.name);
        return 0;
    }

    if(p->dragged_world_item_index == world_index)
    {
        p->dragged_world_item_index = -1;
        log_add("You let go of %s.", world_item->item.name);
    }
    else
    {
        p->dragged_world_item_index = world_index;
        log_add("You start dragging %s behind you.", world_item->item.name);
    }

    creatures_take_turns(p);
    return 1;
}

static WorldItem* interact_dragged_world_item(Player* p)
{
    WorldItem* dragged_item;

    if(!p || !current_area)
        return NULL;
    if(p->dragged_world_item_index < 0 || p->dragged_world_item_index >= MAX_WORLD_ITEMS)
        return NULL;

    dragged_item = &world_items[p->dragged_world_item_index];
    if(!dragged_item->active)
        return NULL;
    if(strcmp(dragged_item->area_name, current_area->name) != 0)
        return NULL;

    return dragged_item;
}

int interact_pick_up_world_item(Player* p, WorldItem* world_item)
{
    int world_index;
    WorldCorpse* corpse;

    if(!p || !world_item || !world_item->active)
        return 0;

    corpse = world_corpse_at_3d(world_item->item.object.base.x,
                                world_item->item.object.base.y,
                                world_item->item.object.base.z);
    if(corpse && corpse->active)
    {
        log_add("You should process or loot %s instead.", corpse->label);
        return 0;
    }

    if(!interact_can_handle_item_with_tongs(p, &world_item->item, "pick up"))
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


}

int interact_transfer_world_container_to_equipped(Character* c, WorldContainer* world_container, int equipped_ci)
{
    int world_container_index;
    int moved = 0;

    if(!c || !world_container || !world_container->active)
        return 0;
    // Removed MAX_ATTACHED_CONTAINERS check; slot-based system handles bounds







    return moved;
}

static int interact_container_label_is_item_container(const char* label)
{
    const ItemTemplate* tmpl;

    if(!label || label[0] == '\0')
        return 0;

    tmpl = item_template_by_name(label);
    if(tmpl)
        return interact_item_type_is_container(tmpl->type);

    if(strstr(label, "Pouch") || strstr(label, "pouch"))
        return 1;
    if(strstr(label, "Backpack") || strstr(label, "backpack"))
        return 1;
    if(strstr(label, "Quiver") || strstr(label, "quiver"))
        return 1;

    return 0;
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
        item_init(out_item, label, 'p', x, y, ITEM_TYPE_CONTAINER_POUCH, 0, 1);
        return 1;
    }

    if(strstr(label, "Quiver") || strstr(label, "quiver"))
    {
        item_init(out_item, label, '}', x, y, ITEM_TYPE_CONTAINER_QUIVER, 0, 1);
        return 1;
    }

    if(strstr(label, "Backpack") || strstr(label, "backpack"))
    {
        item_init(out_item, label, 'B', x, y, ITEM_TYPE_CONTAINER_BACKPACK, 0, 1);
        return 1;
    }

    return 0;
}

int interact_equip_container_from_ground(Player* p, WorldItem* world_item, WorldContainer* world_container)
{
    Item equip_item;
    int inventory_slot;
    int equipped_ci;
    int world_item_index = -1;
    int equip_slot = -1;

    if(!p)
        return 0;


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

    inventory_slot = -1;
    // Find the slot where the item was added
    for(int i = 0; i < p->character.equipment_slot_count; ++i) {
        if(p->character.equipment_slots[i].slot_type == EQUIP_SLOT_NONE &&
           p->character.equipment_slots[i].item.type == equip_item.type &&
           strcmp(p->character.equipment_slots[i].item.name, equip_item.name) == 0) {
            inventory_slot = i;
            break;
        }
    }
    // Find a suitable container equipment slot (dynamic or backpack).
    for(int i = 0; i < p->character.equipment_slot_count; ++i) {
        if(p->character.equipment_slots[i].item.type == ITEM_TYPE_NONE &&
           (p->character.equipment_slots[i].is_container_slot || p->character.equipment_slots[i].slot_type == EQUIP_SLOT_CONTAINER_BACKPACK)) {
            equip_slot = i;
            break;
        }
    }
    if(equip_slot < 0 || !inventory_equip(&p->character, inventory_slot, equip_slot))
    {
        if(inventory_slot >= 0) (void)inventory_remove(&p->character, inventory_slot);
        log_add("Cannot equip %s right now.", equip_item.name);
        return 0;
    }

    if(world_item_index >= 0)
    {
        (void)world_item_remove(world_item_index);
    }
    else if(world_container && world_container->active)
    {
        world_container->active = 0;
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
            if(action->world_item && interact_pick_up_world_item(p, action->world_item))
            {
                creatures_take_turns(p);
                return 1;
            }
            return 0;

        case INTERACTION_ACTION_DRAG_WORLD_ITEM:
            if(action->world_item)
                return interact_toggle_drag_world_item(p, action->world_item);
            return 0;

        case INTERACTION_ACTION_EQUIP_FROM_GROUND:
            // Equip item from ground directly to equipment slot
            if(action->world_item) {
                int inv_slot = -1;
                char item_name[32];
                snprintf(item_name, sizeof(item_name), "%s", action->world_item->item.name);
                if(!interact_can_handle_item_with_tongs(p, &action->world_item->item, "handle"))
                    return 0;
                // Add to inventory first
                if(inventory_add(&p->character, &action->world_item->item)) {
                    // Find the slot where it was added
                    for(int i = 0; i < p->character.equipment_slot_count; ++i) {
                        if(p->character.equipment_slots[i].slot_type == EQUIP_SLOT_NONE &&
                           p->character.equipment_slots[i].item.type == action->world_item->item.type &&
                           strcmp(p->character.equipment_slots[i].item.name, item_name) == 0) {
                            inv_slot = i;
                            break;
                        }
                    }
                    if(inv_slot >= 0 && inventory_auto_equip(&p->character, inv_slot)) {
                        world_item_remove(world_item_index_of(action->world_item));
                        log_add("Equipped %s from ground.", item_name);
                        creatures_take_turns(p);
                        return 1;
                    } else {
                        log_add("Cannot equip %s right now.", item_name);
                    }
                } else {
                    log_add("No space in inventory for %s.", item_name);
                }
            }
            return 0;

        case INTERACTION_ACTION_EXAMINE_ITEM:
            if (action->world_corpse) {
                log_add("You examine %s.", action->world_corpse->label);
            } else if (action->world_item) {
                log_add("You examine %s.", action->world_item->item.name);
            } else if (action->world_container) {
                log_add("You examine %s.", action->world_container->label);
            } else if (action->inventory_slot >= 0) {
                log_add("You examine %s.", p->character.equipment_slots[action->inventory_slot].item.name);
            } else if (action->equipment_slot >= 0) {
                log_add("You examine equipped %s.", p->character.equipment_slots[action->equipment_slot].item.name);
            } else {
                log_add("You examine the target.");
            }
            creatures_take_turns(p);
            return 1;

        // Inventory/equipment slot-based actions (use, equip, unequip)
        // Use item (consumable)
        // NOTE: Could add INTERACTION_ACTION_USE_ITEM for clarity
        case INTERACTION_ACTION_PET:
            return interact_creature(p, action->creature);

        case INTERACTION_ACTION_FEED:
            return interact_feed_creature(p, action->creature);

        case INTERACTION_ACTION_TREAT_INJURY:
        case INTERACTION_ACTION_GIVE_ITEM:
            log_add("Not implemented yet.");
            return 0;

        case INTERACTION_ACTION_TALK:
            if(action->npc)
                return interact_talk_npc(p, action->npc);
            log_add("Not implemented yet.");
            return 0;

        case INTERACTION_ACTION_NPC_GREET:
            if(action->npc)
                return interact_npc_response(p,
                                             action->npc,
                                             "greet",
                                             npc_greeting_line(action->npc),
                                             "The conversation is brief.");
            return 0;

        case INTERACTION_ACTION_NPC_GOSSIP:
            if(action->npc)
                return interact_npc_response(p,
                                             action->npc,
                                             "gossip",
                                             npc_gossip_line(action->npc),
                                             "The npc drifts back into their own thoughts.");
            return 0;

        case INTERACTION_ACTION_ADD_FUEL_TO_FORGE:
            if(action->furniture && (action->furniture->type == FURNITURE_FURNACE || action->furniture->type == FURNITURE_FORGE || action->furniture->type == FURNITURE_CHARCOAL_KILN))
                return interact_pick_furnace_fuel(p, action->furniture);
            return 0;

        case INTERACTION_ACTION_SKIN_CORPSE:
            return interact_process_corpse(p, action->world_corpse, 1);

        case INTERACTION_ACTION_BUTCHER_CORPSE:
            return interact_process_corpse(p, action->world_corpse, 0);

        case INTERACTION_ACTION_EXTINGUISH_FORGE:
            if(action->furniture && (action->furniture->type == FURNITURE_FURNACE || action->furniture->type == FURNITURE_FORGE || action->furniture->type == FURNITURE_CHARCOAL_KILN) && action->furniture->is_ignited)
            {
                action->furniture->is_ignited = 0;
                log_add("You extinguish the %s. %d fuel remains.", interact_heat_source_name(action->furniture), action->furniture->fuel_units);
                creatures_take_turns(p);
                return 1;
            }
            return 0;

        case INTERACTION_ACTION_TILE_USE:
            if(action->stair_delta_z != 0)
                return interact_use_stairs(p, action->tx, action->ty, action->stair_delta_z);

            if(action->furniture) {
                Furniture* furn = action->furniture;
                switch(furniture_interaction_type(furn)) {
                    case FURNITURE_INTERACTION_TOGGLE_DOOR:
                        if(furniture_toggle_door(current_area, furn->base.base.x, furn->base.base.y)) {
                            log_add(furn->is_open ? "You open the door." : "You close the door.");
                            creatures_take_turns(p);
                            return 1;
                        }
                        return 0;
                    case FURNITURE_INTERACTION_OPEN_CONTAINER:
                        {
                            WorldContainer* container = NULL;

                            if(furn->type == FURNITURE_BOOKSHELF)
                            {
                                container = interact_bookshelf_pick_shelf(p, furn);
                            }
                            else
                            {
                                if(furn->world_container_index >= 0 && furn->world_container_index < MAX_WORLD_CONTAINERS) {
                                    WorldContainer* cand = &world_containers[furn->world_container_index];
                                    if(cand->active && strcmp(cand->area_name, current_area->name) == 0 &&
                                       cand->x == furn->base.base.x && cand->y == furn->base.base.y &&
                                       cand->z == furn->base.base.z) {
                                        container = cand;
                                    }
                                }
                                if(!container) {
                                    container = world_container_at_3d(furn->base.base.x, furn->base.base.y, furn->base.base.z);
                                }
                            }

                            if(container && container->active) {
                                if(interact_open_container(p, container)) {
                                    creatures_take_turns(p);
                                    return 1;
                                }
                            }
                            return 0;
                        }
                    case FURNITURE_INTERACTION_INSPECT:
                        if(furn->type == FURNITURE_FURNACE)
                            return interact_use_forge(p, furn);

                        if(furn->type == FURNITURE_CHARCOAL_KILN)
                            return interact_use_charcoal_kiln(p, furn);

                        if(furn->type == FURNITURE_FORGE)
                            return interact_use_anvil(p, furn);

                        if(furn->type == FURNITURE_ANVIL)
                            return interact_use_anvil(p, furn);

                        if(furn->type == FURNITURE_SAWHORSE)
                            return interact_use_sawhorse(p, furn);

                        if(furn->type == FURNITURE_CHOPPING_BLOCK)
                            return interact_use_chopping_block(p, furn);

                        if(furniture_is_destructible(furn))
                        {
                            log_add("You inspect %s. Hardness %d, structure %d/%d.",
                                    furniture_display_name(furn),
                                    furniture_hardness(furn),
                                    furniture_current_structure_points(furn),
                                    furniture_max_structure_points(furn));
                        }
                        else
                        {
                            log_add("You inspect %s.", furniture_display_name(furn));
                        }
                        creatures_take_turns(p);
                        return 1;
                    case FURNITURE_INTERACTION_SIT:
                        log_add("You sit on the chair.");
                        creatures_take_turns(p);
                        return 1;
                    case FURNITURE_INTERACTION_READ_SIGN: {
                        int area_index = interact_current_area_index();
                        int pz = p->character.actor.entity.z;
                        SignpostInstance* signpost;
                        int learned_new_location = 0;
                        int read_count = 0;
                        if(area_index < 0) {
                            log_add("This signpost cannot be read right now.");
                            return 0;
                        }
                        signpost = world_map_signpost_at_mut(area_index, furn->base.base.x, furn->base.base.y, pz);
                        if(!signpost || signpost->sign_count <= 0) {
                            log_add("The signpost is weathered and unreadable.");
                            creatures_take_turns(p);
                            return 1;
                        }
                        for(int i = 0; i < signpost->sign_count; i++) {
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
                        if(read_count <= 0) {
                            log_add("The signpost has no useful destination markings.");
                            creatures_take_turns(p);
                            return 1;
                        }
                        if(learned_new_location)
                            log_add("You mark the signposted locations on your atlas.");
                        creatures_take_turns(p);
                        return 1;
                    }
                    case FURNITURE_INTERACTION_REST:
                        if(player_start_sleep(p, interact_has_adjacent_hostile(p)))
                            return 1;
                        return 0;
                    case FURNITURE_INTERACTION_NONE:
                    default:
                        break;
                }
            }
            return interact_tile(p, action->tx, action->ty);

        default:
            // Handle unequip/equip/use for slot-based actions
            if(action->inventory_slot >= 0) {
                // Use if consumable (but not ammo)
                Item* item = &p->character.equipment_slots[action->inventory_slot].item;
                if((item->type == ITEM_TYPE_CONSUMABLE || item->type == ITEM_TYPE_BOOK || item->type == ITEM_TYPE_SCROLL) && !item->is_ammo) {
                    if(inventory_use(&p->character, action->inventory_slot)) {
                        if(item->type == ITEM_TYPE_BOOK || item->type == ITEM_TYPE_SCROLL)
                            log_add("You finish reading %s.", item->name);
                        else
                            log_add("Used %s.", item->name);
                        creatures_take_turns(p);
                        return 1;
                    } else {
                        if(item->type == ITEM_TYPE_BOOK || item->type == ITEM_TYPE_SCROLL)
                            log_add("Failed to read %s.", item->name);
                        else
                            log_add("Failed to use %s.", item->name);
                    }
                } else if(item->is_ammo) {
                    log_add("%s is ammo for ranged weapons.", item->name);
                } else {
                    char item_name[32];
                    snprintf(item_name, sizeof(item_name), "%s", item->name);
                    if(inventory_auto_equip(&p->character, action->inventory_slot)) {
                        log_add("Equipped %s.", item_name);
                        creatures_take_turns(p);
                        return 1;
                    } else {
                        log_add("Failed to equip %s.", item_name);
                    }
                }
            } else if(action->equipment_slot >= 0) {
                // Unequip
                EquipmentSlot* eq = &p->character.equipment_slots[action->equipment_slot];
                if(eq->item.type != ITEM_TYPE_NONE) {
                    if(inventory_unequip_slot(&p->character, eq->slot_type)) {
                        log_add("Unequipped %s.", eq->item.name);
                        creatures_take_turns(p);
                        return 1;
                    } else {
                        log_add("Failed to unequip %s.", eq->item.name);
                    }
                }
            }
            return 0;
    }
}

static void interaction_collect_actions(Player* p,
                                        int tx,
                                        int ty,
                                        Creature* creature,
                                        const Tile* tile,
                                        WorldItem* world_item,
                                        WorldContainer* world_container,
                                        InteractionAction* actions,
                                        int* action_count)
{
    if(!p || !actions || !action_count)
        return;

    // --- Creature interactions (unchanged for now) ---
    if(creature && creature->alive && creature->template)
    {
        int hostile = creature_is_hostile(creature);
        int is_animal = creature->template->tamable ? 1 : 0;
        int is_character = (!hostile && !is_animal) ? 1 : 0;
        int is_injured_neutral = (!hostile && creature->actor.health < creature->actor.max_health) ? 1 : 0;
        int has_food = interact_feedable_inventory_count(&p->character, creature) > 0;

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
                                   !hostile && has_food,
                                   "Feed",
                                   hostile ? "Too dangerous while hostile" : "Need suitable food",
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

    Furniture* furn = furniture_at(current_area, tx, ty);
    NPC* npc = npc_at_3d(tx, ty, p->character.actor.entity.z);
    WorldCorpse* world_corpse = world_corpse_at_3d(tx, ty, p->character.actor.entity.z);

    if(npc && npc->active && *action_count < INTERACTION_ACTIONS_MAX)
    {
        InteractionAction a = {0};
        a.type = INTERACTION_ACTION_TALK;
        a.enabled = 1;
        a.npc = npc;
        snprintf(a.label, sizeof(a.label), "Talk with %s", npc_display_name(npc));
        actions[(*action_count)++] = a;
    }

    if(world_corpse && world_corpse->active)
    {
        if(world_corpse->type == WORLD_CORPSE_CHARACTER)
        {
            InteractionAction a = {0};

            a.type = INTERACTION_ACTION_OPEN_CONTAINER;
            a.enabled = 1;
            a.world_corpse = world_corpse;
            if(world_corpse->world_container_index >= 0 && world_corpse->world_container_index < MAX_WORLD_CONTAINERS)
                a.world_container = &world_containers[world_corpse->world_container_index];
            snprintf(a.label, sizeof(a.label), "Loot %s", world_corpse->label);
            actions[(*action_count)++] = a;
        }
        else
        {
            int can_process = interact_has_tool_for_skill_anywhere(p, NON_WEAPON_SKILL_SKINNING);
            if(!world_corpse->skinned && world_corpse->skinning_loot_count > 0 && *action_count < INTERACTION_ACTIONS_MAX)
            {
                InteractionAction a = {0};
                a.type = INTERACTION_ACTION_SKIN_CORPSE;
                a.enabled = can_process;
                a.world_corpse = world_corpse;
                snprintf(a.label, sizeof(a.label), "Skin %s", world_corpse->source_name);
                if(!can_process)
                    snprintf(a.disabled_reason, sizeof(a.disabled_reason), "Tool Required: Knife");
                actions[(*action_count)++] = a;
            }
            if(!world_corpse->butchered && world_corpse->butchering_loot_count > 0 && *action_count < INTERACTION_ACTIONS_MAX)
            {
                InteractionAction a = {0};
                a.type = INTERACTION_ACTION_BUTCHER_CORPSE;
                a.enabled = can_process;
                a.world_corpse = world_corpse;
                snprintf(a.label, sizeof(a.label), "Butcher %s", world_corpse->source_name);
                if(!can_process)
                    snprintf(a.disabled_reason, sizeof(a.disabled_reason), "Tool Required: Knife");
                actions[(*action_count)++] = a;
            }
        }
    }

    // --- Ground/container/furniture interactions only ---
    // Inventory and equipment actions belong in the inventory UI, not the world interaction menu.

    // Ground/world item
    if(world_item && world_item->active && !(world_corpse && world_corpse->active)) {
        InteractionAction a = {0};
        char item_label[96];
        int is_lumber_drag = interact_item_is_draggable_lumber(&world_item->item);
        int world_index = world_item_index_of(world_item);

        item_format_display_name(&world_item->item, item_label, sizeof(item_label));
        a.world_item = world_item;
        a.source_type = 3; // ground

        if(is_lumber_drag)
        {
            a.type = INTERACTION_ACTION_DRAG_WORLD_ITEM;
            a.enabled = 1;
            if(p && p->dragged_world_item_index == world_index)
                snprintf(a.label, sizeof(a.label), "Stop dragging %s", item_label);
            else
                snprintf(a.label, sizeof(a.label), "Drag %s", item_label);
            actions[(*action_count)++] = a;
        }
        else
        {
            a.type = INTERACTION_ACTION_PICK_UP_ITEM;
            a.enabled = 1;
            snprintf(a.label, sizeof(a.label), "Pick up %s", item_label);
            actions[(*action_count)++] = a;

            // Equip directly from ground if possible
            a.type = INTERACTION_ACTION_EQUIP_FROM_GROUND;
            a.enabled = 1;
            snprintf(a.label, sizeof(a.label), "Equip %s from ground", item_label);
            actions[(*action_count)++] = a;
        }

        // Examine
        a.type = INTERACTION_ACTION_EXAMINE_ITEM;
        a.enabled = 1;
        snprintf(a.label, sizeof(a.label), "Examine %s", item_label);
        actions[(*action_count)++] = a;
    }

    // Standalone world containers (skip duplicates when a furniture entity already owns this container)
    if(world_container && world_container->active && !(world_corpse && world_corpse->active) && !interact_furniture_owns_world_container(furn, world_container)) {
        InteractionAction a = {0};
        a.type = INTERACTION_ACTION_OPEN_CONTAINER;
        a.enabled = 1;
        snprintf(a.label, sizeof(a.label), "Open %s", world_container->label);
        a.world_container = world_container;
        a.source_type = 4; // container
        actions[(*action_count)++] = a;

        if(interact_container_label_is_item_container(world_container->label))
        {
            a.type = INTERACTION_ACTION_EQUIP_FROM_GROUND;
            a.enabled = 1;
            snprintf(a.label, sizeof(a.label), "Equip %s", world_container->label);
            actions[(*action_count)++] = a;
        }

        // Examine
        a.type = INTERACTION_ACTION_EXAMINE_ITEM;
        a.enabled = 1;
        snprintf(a.label, sizeof(a.label), "Examine %s", world_container->label);
        actions[(*action_count)++] = a;
    }

    // Furniture interactions (new entity layer)
    if(furn && furn->type != FURNITURE_NONE)
    {
        InteractionAction a = {0};
        a.type = INTERACTION_ACTION_TILE_USE;
        a.enabled = (furn->interactable ? 1 : 0);
        a.furniture = furn;
        a.tx = tx;
        a.ty = ty;

        switch(furniture_interaction_type(furn))
        {
            case FURNITURE_INTERACTION_TOGGLE_DOOR:
            case FURNITURE_INTERACTION_READ_SIGN:
            case FURNITURE_INTERACTION_REST:
            case FURNITURE_INTERACTION_INSPECT:
            case FURNITURE_INTERACTION_SIT:
                if(furn->type == FURNITURE_SAWHORSE)
                {
                    WorldItem* dragged_item = interact_dragged_world_item(p);

                    if(dragged_item && interact_item_is_draggable_lumber(&dragged_item->item))
                        snprintf(a.label, sizeof(a.label), "Lift %s onto sawhorse", dragged_item->item.name);
                    else
                        furniture_get_interaction_label(furn, a.label, sizeof(a.label));
                }
                else
                {
                    furniture_get_interaction_label(furn, a.label, sizeof(a.label));
                }
                actions[(*action_count)++] = a;
                if(furn->type == FURNITURE_FURNACE || furn->type == FURNITURE_FORGE || furn->type == FURNITURE_CHARCOAL_KILN)
                {
                    int max_fuel_units = interact_fuel_capacity_for_furniture(furn);
                    const char* source_name = (furn->type == FURNITURE_CHARCOAL_KILN) ? "kiln" : "furnace";

                    if(interact_can_add_forge_fuel(p, furn) && *action_count < INTERACTION_ACTIONS_MAX)
                    {
                        InteractionAction fuel_action = {0};
                        fuel_action.type = INTERACTION_ACTION_ADD_FUEL_TO_FORGE;
                        fuel_action.enabled = 1;
                        fuel_action.furniture = furn;
                        fuel_action.tx = tx;
                        fuel_action.ty = ty;
                        snprintf(fuel_action.label, sizeof(fuel_action.label), "Add fuel to %s (%d/%d)", source_name, furn->fuel_units, max_fuel_units);
                        actions[(*action_count)++] = fuel_action;
                    }

                    if(furn->is_ignited && *action_count < INTERACTION_ACTIONS_MAX)
                    {
                        InteractionAction forge_action = {0};
                        forge_action.type = INTERACTION_ACTION_EXTINGUISH_FORGE;
                        forge_action.enabled = 1;
                        forge_action.furniture = furn;
                        forge_action.tx = tx;
                        forge_action.ty = ty;
                        snprintf(forge_action.label, sizeof(forge_action.label), "Extinguish %s", source_name);
                        actions[(*action_count)++] = forge_action;
                    }

                    if(furn->type == FURNITURE_FURNACE && *action_count < INTERACTION_ACTIONS_MAX)
                    {
                        InteractionAction input_action = {0};
                        WorldContainer* input = interact_station_input_container(furn, 1);
                        input_action.type = INTERACTION_ACTION_OPEN_CONTAINER;
                        input_action.enabled = (input != NULL && furn->process_turns_remaining <= 0) ? 1 : 0;
                        input_action.world_container = input;
                        input_action.furniture = furn;
                        input_action.tx = tx;
                        input_action.ty = ty;
                        if(!input_action.enabled)
                            snprintf(input_action.disabled_reason, sizeof(input_action.disabled_reason),
                                     (furn->process_turns_remaining > 0) ? "Batch is already burning" : "No furnace input present");
                        snprintf(input_action.label, sizeof(input_action.label), "Open furnace input");
                        actions[(*action_count)++] = input_action;
                    }

                    if(furniture_has_output_container_type(furn->type) && *action_count < INTERACTION_ACTIONS_MAX)
                    {
                        InteractionAction output_action = {0};
                        WorldContainer* output = interact_station_output_container(furn, 1);
                        output_action.type = INTERACTION_ACTION_OPEN_CONTAINER;
                        output_action.enabled = output ? 1 : 0;
                        output_action.world_container = output;
                        output_action.furniture = furn;
                        output_action.tx = tx;
                        output_action.ty = ty;
                        if(!output_action.enabled)
                            snprintf(output_action.disabled_reason, sizeof(output_action.disabled_reason), "No output container present");
                        snprintf(output_action.label,
                                 sizeof(output_action.label),
                                 (furn->type == FURNITURE_CHARCOAL_KILN) ? "Open kiln output" : "Open output");
                        actions[(*action_count)++] = output_action;
                    }
                }
                break;
            case FURNITURE_INTERACTION_OPEN_CONTAINER:
            {
                const char* container_name = furniture_container_label_for_type(furn->type);
                WorldContainer* wc = NULL;

                if(furn->type == FURNITURE_BOOKSHELF)
                {
                    WorldContainer* shelves[BOOKSHELF_SHELF_COUNT];
                    a.enabled = (interact_bookshelf_collect_shelves(furn, shelves) > 0) ? 1 : 0;
                }
                else
                {
                    if(furn->world_container_index >= 0 && furn->world_container_index < MAX_WORLD_CONTAINERS) {
                        WorldContainer* cand = &world_containers[furn->world_container_index];
                        if(cand->active && strcmp(cand->area_name, current_area->name) == 0 &&
                           cand->x == furn->base.base.x && cand->y == furn->base.base.y &&
                           cand->z == furn->base.base.z)
                        {
                            wc = cand;
                        }
                    }
                    if(!wc) {
                        wc = world_container_at_3d(furn->base.base.x, furn->base.base.y, furn->base.base.z);
                    }

                    if(wc && wc->active)
                    {
                        a.enabled = 1;
                        if(wc->label[0])
                            container_name = wc->label;
                    }
                    else
                    {
                        a.enabled = 0;
                    }
                }

                if(!a.enabled)
                    snprintf(a.disabled_reason, sizeof(a.disabled_reason), "No container present");
                snprintf(a.label, sizeof(a.label), "Open %s", container_name);
                actions[(*action_count)++] = a;
                break;
            }
            case FURNITURE_INTERACTION_NONE:
            default:
                break;
        }
    }

    // Tile-based interactions (doors, stairs, etc.)
    if(tile) {
        if(tile_is_door(tile)) {
            InteractionAction a = {0};
            a.type = INTERACTION_ACTION_TILE_USE;
            a.enabled = 1;
            a.tx = tx;
            a.ty = ty;
            snprintf(a.label, sizeof(a.label), tile->blocks_movement ? "Open door" : "Close door");
            actions[(*action_count)++] = a;
        } else if(tile_is_staircase(tile)) {
            if(*action_count < INTERACTION_ACTIONS_MAX)
            {
                InteractionAction a = {0};
                a.type = INTERACTION_ACTION_TILE_USE;
                a.enabled = stairs_can_move(p, tx, ty, 1);
                a.tx = tx;
                a.ty = ty;
                a.stair_delta_z = 1;
                if(!a.enabled)
                    snprintf(a.disabled_reason, sizeof(a.disabled_reason), "Already at top floor");
                snprintf(a.label, sizeof(a.label), "Go up");
                actions[(*action_count)++] = a;
            }

            if(*action_count < INTERACTION_ACTIONS_MAX)
            {
                InteractionAction a = {0};
                a.type = INTERACTION_ACTION_TILE_USE;
                a.enabled = stairs_can_move(p, tx, ty, -1);
                a.tx = tx;
                a.ty = ty;
                a.stair_delta_z = -1;
                if(!a.enabled)
                    snprintf(a.disabled_reason, sizeof(a.disabled_reason), "Already at ground floor");
                snprintf(a.label, sizeof(a.label), "Go down");
                actions[(*action_count)++] = a;
            }
        } else if(strcmp(tile->name, "Signpost") == 0) {
            InteractionAction a = {0};
            a.type = INTERACTION_ACTION_TILE_USE;
            a.enabled = 1;
            a.tx = tx;
            a.ty = ty;
            snprintf(a.label, sizeof(a.label), "Read signpost");
            actions[(*action_count)++] = a;
        } else if(strstr(tile->name, "Switch")) {
            InteractionAction a = {0};
            a.type = INTERACTION_ACTION_TILE_USE;
            a.enabled = 1;
            a.tx = tx;
            a.ty = ty;
            snprintf(a.label, sizeof(a.label), "Inspect switch");
            actions[(*action_count)++] = a;
        } else if(tile_is_fishable(tile) && *action_count < INTERACTION_ACTIONS_MAX) {
            InteractionAction a = {0};
            a.type = INTERACTION_ACTION_TILE_USE;
            a.enabled = interact_has_tool_for_skill_anywhere(p, NON_WEAPON_SKILL_FISHING);
            a.tx = tx;
            a.ty = ty;
            snprintf(a.label, sizeof(a.label), "Fish");
            if(!a.enabled)
                snprintf(a.disabled_reason, sizeof(a.disabled_reason), "Need a fishing tool");
            actions[(*action_count)++] = a;
        }
    }
}

// Try interacting with tile-level interactables (doors now, extensible for switches/containers).
static int interact_tile(Player* p, int tx, int ty)
{
    const Tile* tile;
    WorldContainer* any_container;

    if(!p || !current_area)
        return 0;
    if(tx < 0 || tx >= current_area->width || ty < 0 || ty >= current_area->height)
        return 0;

    tile = map_top_visible_tile(current_area, tx, ty, NULL);

    any_container = world_container_at_3d(tx, ty, p->character.actor.entity.z);
    if(any_container)
    {
        if(interact_open_container(p, any_container))
        {
            creatures_take_turns(p);
            return 1;
        }
        return 0;
    }

    {
        Furniture* furn = furniture_at(current_area, tx, ty);
        if(furn)
        {
            switch(furniture_interaction_type(furn))
            {
                case FURNITURE_INTERACTION_TOGGLE_DOOR:
                    if(furniture_toggle_door(current_area, tx, ty))
                    {
                        log_add(furn->is_open ? "You open the door." : "You close the door.");
                        creatures_take_turns(p);
                        return 1;
                    }
                    break;
                case FURNITURE_INTERACTION_READ_SIGN:
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
                case FURNITURE_INTERACTION_INSPECT:
                    if(furn->type == FURNITURE_FURNACE)
                        return interact_use_forge(p, furn);

                    if(furn->type == FURNITURE_CHARCOAL_KILN)
                        return interact_use_charcoal_kiln(p, furn);

                    if(furn->type == FURNITURE_FORGE)
                        return interact_use_anvil(p, furn);

                    if(furn->type == FURNITURE_ANVIL)
                        return interact_use_anvil(p, furn);

                    if(furn->type == FURNITURE_SAWHORSE)
                        return interact_use_sawhorse(p, furn);

                    if(furn->type == FURNITURE_CHOPPING_BLOCK)
                        return interact_use_chopping_block(p, furn);

                    if(furniture_is_destructible(furn))
                    {
                        log_add("You inspect %s. Hardness %d, structure %d/%d.",
                                furniture_display_name(furn),
                                furniture_hardness(furn),
                                furniture_current_structure_points(furn),
                                furniture_max_structure_points(furn));
                    }
                    else
                    {
                        log_add("You inspect %s.", furniture_display_name(furn));
                    }
                    creatures_take_turns(p);
                    return 1;
                case FURNITURE_INTERACTION_SIT:
                    log_add("You sit on the chair.");
                    creatures_take_turns(p);
                    return 1;
                case FURNITURE_INTERACTION_REST:
                    if(player_start_sleep(p, interact_has_adjacent_hostile(p)))
                        return 1;
                    return 0;
                case FURNITURE_INTERACTION_OPEN_CONTAINER:
                case FURNITURE_INTERACTION_NONE:
                default:
                    break;
            }
        }
    }

    if(!tile)
        return 0;

    if(!tile->interactable)
    {
        if(tile_is_fishable(tile))
            return interact_use_fishing_rod(p, tx, ty);
        return 0;
    }

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

    if(tile_is_staircase(tile))
    {
        int preferred_dz = (tile->symbol == '>') ? -1 : 1;

        if(stairs_can_move(p, tx, ty, preferred_dz))
            return interact_use_stairs(p, tx, ty, preferred_dz);
        if(stairs_can_move(p, tx, ty, -preferred_dz))
            return interact_use_stairs(p, tx, ty, -preferred_dz);

        log_add("The staircase does not lead anywhere from here.");
        return 0;
    }

    if(strstr(tile->name, "Switch"))
    {
        log_add("You inspect the switch, but it is not wired yet.");
        return 0;
    }

    if(strstr(tile->name, "Chest") || strstr(tile->name, "Container"))
    {
        int pz = p->character.actor.entity.z;
        WorldContainer* container = world_container_at_3d(tx, ty, pz);
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

int interact_at(Player* p, int tx, int ty)
{
    // int px, py; // Removed, use p->character.actor.entity.x/y directly
    int max_range;
    const char* target_name;
    Creature* creature;
    const Tile* tile;
    WorldItem* world_item;
    WorldContainer* world_container;
    InteractionAction actions[INTERACTION_ACTIONS_MAX];
    int selected_action;
    int performed_any = 0;

    if(!p || !current_area)
        return 0;


    if(!map_has_line_of_sight(p->character.actor.entity.x, p->character.actor.entity.y, tx, ty))
    {
        log_add("Cannot interact at %d,%d: out of sight.", tx, ty);
        return 0;
    }

    max_range = interact_max_range();
    target_name = interact_target_name_at(tx, ty);
    if(target_name && !interact_in_range(p->character.actor.entity.x, p->character.actor.entity.y, tx, ty, max_range))
    {
        log_add("You are too far away to interact with %s", target_name);
        return 0;
    }

    creature = bestiary_creature_at_3d(tx, ty, p->character.actor.entity.z);
    tile = map_top_visible_tile(current_area, tx, ty, NULL);
    world_item = world_item_at_3d(tx, ty, p->character.actor.entity.z);
    world_container = world_container_at_3d(tx, ty, p->character.actor.entity.z);

    while(1)
    {
        int action_count = 0;

        interaction_collect_actions(p, tx, ty, creature, tile, world_item, world_container, actions, &action_count);

        if(action_count <= 0)
        {
            if(!performed_any)
                log_add("Nothing to interact with at %d,%d.", tx, ty);
            return performed_any;
        }

        selected_action = interaction_show_menu(p, target_name, actions, action_count);
        if(selected_action < 0)
        {
            if(!performed_any)
            {
                log_add("Interaction canceled.");
                return 0;
            }
            return 1;
        }

        if(interaction_run_action(p, &actions[selected_action]))
        {
            performed_any = 1;
            if(interaction_action_keeps_menu_open(&actions[selected_action]))
                continue;
            return 1;
        }

        if(!interaction_action_keeps_menu_open(&actions[selected_action]))
        {
            log_add("Nothing to interact with at %d,%d.", tx, ty);
            return performed_any;
        }
    }
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
        (void)interact_at(p, resolved.x, resolved.y);
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
    log_add("Interact: w/up=up, x/down=down, a/left=left, d/right=right, space/enter=here, q/esc=cancel");
    key = read_input_key();

    switch(key)
    {
        case 'w': case 'W': case INPUT_KEY_UP:
            dy = -1;
            break;
        case 'x': case 'X': case INPUT_KEY_DOWN:
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

    if(current_area)
    {
        const Tile* tile = map_top_visible_tile(current_area, target_x, target_y, NULL);
        if(tile_is_fishable(tile) && interact_has_tool_for_skill_anywhere(p, NON_WEAPON_SKILL_FISHING))
        {
            (void)interact_fish_at(p, target_x, target_y);
            return;
        }
    }

    (void)interact_at(p, target_x, target_y);
}
