#include "npc.h"

#include "actor.h"
#include "atlas.h"
#include "collision.h"
#include "log.h"
#include "map.h"
#include "player.h"
#include "tile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NPC_MOVE_STATE_WANDER 0
#define NPC_MOVE_STATE_REST 1

static const char* const npc_generic_gossip_lines[] = {
    "The weather has moods of its own.",
    "Roads are safer when you know where they lead.",
    "A sharp eye is often worth more than a sharp blade.",
};

static const char* const old_hermit_gossip_lines[] = {
    "Did you know? Damage is damage!",
    "Pointy stick good!",
    "Don't forget your towel!",
    "You die when you are killed.",
    "Signposts point directions to places.",
    "You should be doing something more useful.",
    "Remember to drink water.",
};

NPC npcs[MAX_NPCS];

static int npc_area_matches_current(const NPC* npc)
{
    return npc && npc->active && current_area && strcmp(npc->area_name, current_area->name) == 0;
}

static int npc_home_contains(const NPC* npc, int x, int y, int z)
{
    if(!npc)
        return 0;
    if(z != npc->home_z)
        return 0;
    return x >= npc->home_x0 && x <= npc->home_x1 && y >= npc->home_y0 && y <= npc->home_y1;
}

static void npc_begin_wander(NPC* npc)
{
    static const int dirs[4][2] = {
        { 0, -1 },
        { 0, 1 },
        { -1, 0 },
        { 1, 0 }
    };
    int choice;

    if(!npc)
        return;

    choice = rand() % 4;
    npc->move_state = NPC_MOVE_STATE_WANDER;
    npc->state_turns = 2 + (rand() % 3);
    npc->move_dx = dirs[choice][0];
    npc->move_dy = dirs[choice][1];
}

static int npc_try_move(NPC* npc, int dx, int dy)
{
    int nx;
    int ny;
    int nz;

    if(!npc || !npc->active)
        return 0;

    nz = npc->character.actor.entity.z;
    nx = npc->character.actor.entity.x + dx;
    ny = npc->character.actor.entity.y + dy;

    if(!npc_home_contains(npc, nx, ny, nz))
        return 0;
    if(is_blocked_3d(nx, ny, nz, 0))
        return 0;

    npc->character.actor.entity.x = nx;
    npc->character.actor.entity.y = ny;
    return 1;
}

static void npc_take_turn(NPC* npc)
{
    if(!npc || !npc->active)
        return;

    if(npc->state_turns <= 0)
    {
        if(rand() % 4 == 0)
        {
            npc->move_state = NPC_MOVE_STATE_REST;
            npc->state_turns = 1 + (rand() % 2);
            npc->move_dx = 0;
            npc->move_dy = 0;
            return;
        }

        npc_begin_wander(npc);
    }

    if(npc->move_state == NPC_MOVE_STATE_REST)
    {
        npc->state_turns--;
        if(npc->state_turns <= 0)
            npc_begin_wander(npc);
        return;
    }

    if(!npc_try_move(npc, npc->move_dx, npc->move_dy))
        npc_begin_wander(npc);

    npc->state_turns--;
}

void npc_init(void)
{
    memset(npcs, 0, sizeof(npcs));
}

NPC* npc_at_3d(int x, int y, int z)
{
    for(int i = 0; i < MAX_NPCS; i++)
    {
        if(!npc_area_matches_current(&npcs[i]))
            continue;
        if(npcs[i].character.actor.entity.x == x &&
           npcs[i].character.actor.entity.y == y &&
           npcs[i].character.actor.entity.z == z)
            return &npcs[i];
    }

    return NULL;
}

NPC* npc_at(int x, int y)
{
    return npc_at_3d(x, y, AREA_GROUND_Z);
}

int npc_index_of(const NPC* npc)
{
    if(!npc)
        return -1;

    for(int i = 0; i < MAX_NPCS; i++)
    {
        if(&npcs[i] == npc)
            return i;
    }

    return -1;
}

const char* npc_display_name(const NPC* npc)
{
    if(!npc || npc->character.name[0] == '\0')
        return "Unknown NPC";
    return npc->character.name;
}

void npc_set_dialogue_profile(NPC* npc, NpcDialogueProfile profile)
{
    if(!npc)
        return;

    npc->dialogue_profile = (int)profile;
    npc->greeted_this_session = 0;
    npc->last_gossip_index = -1;
}

static const char* npc_pick_gossip_line(NPC* npc, const char* const* lines, int line_count)
{
    int index;

    if(!lines || line_count <= 0)
        return "...";

    index = rand() % line_count;
    if(npc && line_count > 1 && index == npc->last_gossip_index)
        index = (index + 1 + (rand() % (line_count - 1))) % line_count;

    if(npc)
        npc->last_gossip_index = index;

    return lines[index];
}

const char* npc_greeting_line(NPC* npc)
{
    if(!npc)
        return "The stranger does not respond.";

    switch((NpcDialogueProfile)npc->dialogue_profile)
    {
        case NPC_DIALOGUE_OLD_HERMIT:
            if(!npc->greeted_this_session)
            {
                npc->greeted_this_session = 1;
                return "Eh? Visitor. I'm the Old Hermit. Mind the stairs and mind your head.";
            }
            return "Oh, it's you again. Congratulations on still being alive.";
        case NPC_DIALOGUE_NONE:
        default:
            npc->greeted_this_session = 1;
            return "They offer a short nod and a cautious greeting.";
    }
}

const char* npc_gossip_line(NPC* npc)
{
    if(!npc)
        return "The stranger has nothing to say.";

    switch((NpcDialogueProfile)npc->dialogue_profile)
    {
        case NPC_DIALOGUE_OLD_HERMIT:
            return npc_pick_gossip_line(npc,
                                        old_hermit_gossip_lines,
                                        (int)(sizeof(old_hermit_gossip_lines) / sizeof(old_hermit_gossip_lines[0])));
        case NPC_DIALOGUE_NONE:
        default:
            return npc_pick_gossip_line(npc,
                                        npc_generic_gossip_lines,
                                        (int)(sizeof(npc_generic_gossip_lines) / sizeof(npc_generic_gossip_lines[0])));
    }
}

NPC* npc_spawn_wanderer(const char* name,
                        char symbol,
                        int color,
                        int x,
                        int y,
                        int z,
                        int home_x0,
                        int home_y0,
                        int home_x1,
                        int home_y1)
{
    NPC* npc = NULL;
    int spawn_x = x;
    int spawn_y = y;

    if(!current_area || !name || name[0] == '\0')
        return NULL;

    if(home_x0 > home_x1)
    {
        int tmp = home_x0;
        home_x0 = home_x1;
        home_x1 = tmp;
    }
    if(home_y0 > home_y1)
    {
        int tmp = home_y0;
        home_y0 = home_y1;
        home_y1 = tmp;
    }

    for(int i = 0; i < MAX_NPCS; i++)
    {
        if(npcs[i].active && strcmp(npcs[i].area_name, current_area->name) == 0 && strcmp(npcs[i].character.name, name) == 0)
            return &npcs[i];
        if(!npcs[i].active && !npc)
            npc = &npcs[i];
    }

    if(!npc)
    {
        log_add("No free NPC slot for %s.", name);
        return NULL;
    }

    if(is_blocked_3d(spawn_x, spawn_y, z, 0))
    {
        int found = 0;
        for(int scan_y = home_y0; scan_y <= home_y1 && !found; scan_y++)
        {
            for(int scan_x = home_x0; scan_x <= home_x1; scan_x++)
            {
                if(!is_blocked_3d(scan_x, scan_y, z, 0))
                {
                    spawn_x = scan_x;
                    spawn_y = scan_y;
                    found = 1;
                    break;
                }
            }
        }

        if(!found)
        {
            log_add("Failed to find room for %s.", name);
            return NULL;
        }
    }

    memset(npc, 0, sizeof(*npc));
    npc->active = 1;
    npc->hostile = 0;
    npc->dialogue_profile = NPC_DIALOGUE_NONE;
    npc->greeted_this_session = 0;
    npc->last_gossip_index = -1;
    npc->home_x0 = home_x0;
    npc->home_y0 = home_y0;
    npc->home_x1 = home_x1;
    npc->home_y1 = home_y1;
    npc->home_z = z;
    snprintf(npc->area_name, sizeof(npc->area_name), "%s", current_area->name);
    snprintf(npc->character.name, sizeof(npc->character.name), "%s", name);
    npc->character.versatile_grip_mode = WEAPON_GRIP_ONE_HANDED;

    npc->character.actor.strength = 14;
    npc->character.actor.constitution = 15;
    npc->character.actor.endurance = 16;
    npc->character.actor.agility = 12;
    npc->character.actor.dexterity = 13;
    npc->character.actor.speed = 12;
    npc->character.actor.intellect = 18;
    npc->character.actor.wisdom = 24;
    npc->character.actor.resolve = 22;
    npc->character.actor.composure = 22;
    npc->character.actor.charisma = 10;
    npc->character.actor.beauty = 8;
    npc->character.actor.perception = 18;
    npc->character.actor.wits = 18;
    actor_ensure_base_attributes(&npc->character.actor);
    npc->character.actor.max_health = actor_derived_max_health(&npc->character.actor);
    npc->character.actor.health = npc->character.actor.max_health;
    npc->character.actor.max_stamina = actor_derived_max_stamina(&npc->character.actor);
    npc->character.actor.stamina = npc->character.actor.max_stamina;
    npc->character.actor.max_action_points = actor_derived_max_action_points(&npc->character.actor);
    npc->character.actor.action_points = npc->character.actor.max_action_points;
    npc->character.actor.max_willpower = actor_derived_max_willpower(&npc->character.actor);
    npc->character.actor.willpower = npc->character.actor.max_willpower;
    npc->character.actor.max_mana = actor_derived_max_mana(&npc->character.actor);
    npc->character.actor.mana = npc->character.actor.max_mana;
    npc->character.actor.entity.x = spawn_x;
    npc->character.actor.entity.y = spawn_y;
    npc->character.actor.entity.z = z;
    npc->character.actor.entity.symbol = symbol;
    npc->character.actor.entity.color = color;
    npc->character.actor.entity.blocks = 1;
    npc->character.actor.entity.layer = TILE_LAYER_EFFECT;
    npc->character.actor.entity.hide_below = 0;

    npc_begin_wander(npc);
    return npc;
}

void npcs_take_turns(Player* p)
{
    (void)p;

    if(!current_area)
        return;

    for(int i = 0; i < MAX_NPCS; i++)
    {
        NPC* npc = &npcs[i];

        if(!npc_area_matches_current(npc))
            continue;

        npc_take_turn(npc);
    }
}
