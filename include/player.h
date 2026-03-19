#ifndef PLAYER_H
#define PLAYER_H

#include "d:/projekti/peli/include/character.h" // REQUIRED
// ===== Player structure =====
typedef struct Player {
    Character character;    // inherit from Character
    // Player-specific fields
    int experience;
    int level;
    // inventory, skills, etc.
    int gold;
    int inventory[10]; // simple inventory with 10 slots
    int equipped_weapon; // index of equipped weapon in inventory
    int equipped_armor;  // index of equipped armor in inventory
    int equipped_accessory; // index of equipped accessory in inventory
} Player;

// Global player instance
extern Player player;

// Functions
void player_create(Player* p, const char* name);
void player_place(Player* p, int x, int y);
int player_place_random(Player* p); // returns 1 if success, 0 if failed

#endif