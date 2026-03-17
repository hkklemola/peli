#include <stdio.h>
#include "d:/projekti/peli/include/character.h"

void create_character(Creature* player) {
    if(!player) return;

    printf("\n Enter your character's name (max 31 chars):");
    printf("\n"); // space before input
    fgets(player->name, sizeof(player->name), stdin);

    // Remove newline if present
    for(int i = 0; i < sizeof(player->name); i++) {
        if(player->name[i] == '\n') {
            player->name[i] = '\0';
            break;
        }
    }

    printf("Welcome, %s! Your adventure begins...\n", player->name);
}