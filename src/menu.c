#include "d:/projekti/peli/include/menu.h"
#include <string.h>
#include <stdio.h>
#include "d:/projekti/peli/include/spawn.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/bestiary.h" 
#include "d:/projekti/peli/include/map.h"


extern Creature* player;

void start_screen() {
    printf("=== Welcome to the Gorefistia ===\n");
    printf("Press any key to continue...\n");
    getchar();
}

// Display the main menu and return choice
int main_menu() {
    int choice = 0;
    while(1) {
        printf("\n=== Main Menu ===\n");
        printf("1. New Game\n");
        printf("2. Quit\n");
        printf("Choice: ");
        scanf("%d", &choice);
        getchar(); // consume newline

        if(choice == 1 || choice == 2)
            return choice;
    }
}

// Prompt for player name and spawn the player
void character_creator(Creature** out_player) {
    char name[32];

    printf("\nEnter your character's name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = 0;

    if(strlen(name) == 0)
        strcpy(name, "Hero");

    // Spawn player via spawner
    *out_player = spawn_player(MAP_WIDTH/2, MAP_HEIGHT/2, name);

    log_add("Welcome, %s!", (*out_player)->name);

    printf("\nPress any key to continue...\n");
    getchar();
}