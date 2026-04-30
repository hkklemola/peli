#ifndef NPC_H
#define NPC_H

#include "character.h"

typedef struct Player Player;

typedef enum NpcDialogueProfile {
    NPC_DIALOGUE_NONE = 0,
    NPC_DIALOGUE_OLD_HERMIT,
} NpcDialogueProfile;

#define MAX_NPCS 16

extern NPC npcs[MAX_NPCS];

// Clear all runtime NPC slots.
void npc_init(void);

// Return whether any NPC slots are active.
int npc_any_active(void);

// Return the active NPC at a tile, or NULL.
NPC* npc_at(int x, int y);
NPC* npc_at_3d(int x, int y, int z);

// Return slot index for a runtime NPC pointer, or -1 when invalid.
int npc_index_of(const NPC* npc);

// Return display name for an NPC.
const char* npc_display_name(const NPC* npc);

// Set one NPC's lightweight dialogue profile.
void npc_set_dialogue_profile(NPC* npc, NpcDialogueProfile profile);

// Return one greeting/introduction line for this NPC.
const char* npc_greeting_line(NPC* npc);

// Return one random gossip/tip line for this NPC.
const char* npc_gossip_line(NPC* npc);

// Spawn a neutral wandering NPC constrained to a rectangular home area.
NPC* npc_spawn_wanderer(const char* name,
                        unsigned char symbol,
                        int color,
                        int x,
                        int y,
                        int z,
                        int home_x0,
                        int home_y0,
                        int home_x1,
                        int home_y1);

// Run one AI turn for active NPCs in the current area.
void npcs_take_turns(Player* p);

#endif
