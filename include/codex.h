#ifndef CODEX_H
#define CODEX_H

#include "player.h"

/**
 * @file codex.h
 * @brief Codex overlay declaration for world map, atlas, journal, log, and reference screens.
 */

typedef enum CodexSection {
    CODEX_SECTION_WORLD_MAP = 0,
    CODEX_SECTION_ATLAS,
    CODEX_SECTION_JOURNAL,
    CODEX_SECTION_LOG,
    CODEX_SECTION_BESTIARY,
    CODEX_SECTION_CRAFTING,
    CODEX_SECTION_COUNT,
} CodexSection;

void codex_show_overlay(Player* player);

#endif
