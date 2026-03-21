#ifndef JOURNAL_H
#define JOURNAL_H

#include "player.h"

// Reset a player's journal entries.
void journal_init(Player* p);

// Add one journal entry, returns 1 on success.
int journal_add_entry(Player* p, const char* text);

// Open interactive journal overlay.
void journal_show_overlay(Player* p);

#endif
