#ifndef PLAYER_H
#define PLAYER_H

#include "d:/projekti/peli/include/entity.h"   // REQUIRED
#include "d:/projekti/peli/include/actor.h"    // REQUIRED
// ===== Player structure =====
typedef struct {
    Entity entity;      // position and symbol
    Actor actor;        // stats and combat info
    char name[32];      // character name

    // Core stats
    int hp;
    int max_hp;
    int attack;
    int defense;

    // Progression
    int level;
    int xp;

    // Inventory/wealth
    int gold;
} Player;

// Global player instance
extern Player player;

// Create a new player with name and starting position
void player_create(const char* name, int x, int y);

// Handle movement input
void player_handle_input();

#endif