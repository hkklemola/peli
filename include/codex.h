#ifndef CODEX_H
#define CODEX_H

#include "player.h"

/**
 * @file codex.h
 * @brief Codex overlay declaration for Atlas, Bestiary, and Crafting Compendium.
 */

typedef enum CodexSection {
    CODEX_SECTION_ATLAS = 0,
    CODEX_SECTION_BESTIARY,
    CODEX_SECTION_CRAFTING,
    CODEX_SECTION_COUNT,
} CodexSection;

void codex_show_overlay(Player* player);

#endif
