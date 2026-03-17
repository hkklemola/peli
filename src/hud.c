#include <stdio.h>
#include "d:/projekti/peli/include/hud.h"
#include "d:/projekti/peli/include/bestiary.h"
#include "d:/projekti/peli/include/log.h"
#include "d:/projekti/peli/include/character.h"
#include "d:/projekti/peli/include/atlas.h"

void hud_init() {
    // Placeholder if you need to initialize HUD elements
}

void draw_hud(Creature* player) {
    if(!player || !current_area) return;

    Tile* t = &current_area->map[player->entity.y][player->entity.x];

    printf("\n====================\n");
    printf("Player: %s (Level %d)\n", player->name, player->actor.level);
    printf("HP: %d/%d  Attack: %d  Defense: %d  Magic: %d  Speed: %d\n",
           player->actor.hp, player->actor.max_hp,
           player->actor.attack, player->actor.defense,
           player->actor.magic, player->actor.speed);
    printf("XP: %d\n", player->actor.experience);
    printf("Location: %s\n", current_area->name);
    printf("Standing on: %s (%c) Walkable: %s\n",
           t->name, t->symbol, t->walkable ? "Yes" : "No");
    printf("====================\n\n");
}

void draw_creature_sheet(Creature* c) {
    if(!c) return;

    printf("\n-- %s --\n", c->name);
    printf("HP: %d/%d  Attack: %d  Defense: %d  Magic: %d  Speed: %d  Level: %d\n",
           c->actor.hp, c->actor.max_hp,
           c->actor.attack, c->actor.defense,
           c->actor.magic, c->actor.speed,
           c->actor.level);
}