#ifndef MENU_H
#define MENU_H

#include "bestiary.h"

// Display the start screen
void start_screen();

// Show the main menu and return user choice
int main_menu(); 

// Prompt for character creation, outputs the created player
void character_creator(Creature** out_player);

#endif
