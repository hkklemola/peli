#include "bestiary.h"
#include "player.h"
#include "overlay_nav.h"
#include "ui_overlay.h"
#include "input.h"
#include "keybind_helpers.h"

#include <stdio.h>
#include <string.h>

static const char* bestiary_knowledge_label(BestiaryKnowledge knowledge)
{
    switch(knowledge)
    {
        case BESTIARY_KNOWLEDGE_SIGHTED:
            return "sighted";
        case BESTIARY_KNOWLEDGE_KILLED:
            return "killed";
        case BESTIARY_KNOWLEDGE_STUDIED:
            return "researched";
        case BESTIARY_KNOWLEDGE_UNKNOWN:
        default:
            return "unknown";
    }
}

static const char* bestiary_entry_type_label(BestiaryEntryType type)
{
    return (type == BESTIARY_ENTRY_TYPE_RACE) ? "Race" : "Creature";
}

void bestiary_show_overlay(Player* player)
{
    (void)player;
    int selected_line = 0;
    int top_index = 0;
    int view_index = -1;

    while(1)
    {
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        int visible_lines = (content_lines > 8) ? (content_lines - 8) : 1;
        int race_indices[MAX_BESTIARY_ENTRIES];
        int creature_indices[MAX_BESTIARY_ENTRIES];
        int race_count = 0, creature_count = 0;

        for(int i = 0; i < bestiary_entry_count; i++) {
            if(bestiary_entries[i].knowledge > BESTIARY_KNOWLEDGE_UNKNOWN) {
                if(bestiary_entries[i].type == BESTIARY_ENTRY_TYPE_RACE)
                    race_indices[race_count++] = i;
                else
                    creature_indices[creature_count++] = i;
            }
        }

        int known_count = race_count + creature_count;
        int current = known_count;
        int known_indices[MAX_BESTIARY_ENTRIES];
        for(int i = 0; i < race_count; i++)
            known_indices[i] = race_indices[i];
        for(int i = 0; i < creature_count; i++)
            known_indices[race_count + i] = creature_indices[i];

        if(selected_line < 0)
            selected_line = 0;
        if(selected_line >= visible_lines)
            selected_line = visible_lines - 1;
        if(top_index < 0)
            top_index = 0;
        if(top_index > known_count - visible_lines)
            top_index = known_count - visible_lines;
        if(top_index < 0)
            top_index = 0;

        ui_overlay_draw_frame("Bestiary");

        if(view_index >= 0)
        {
            const BestiaryEntryInfo* entry = bestiary_entry_by_index(view_index);
            int line = 2;
            char scratch[192];

            ui_overlay_draw_line(0, "Bestiary detail: o/back | q cancel | i/c/j switch overlays");
            ui_overlay_draw_line(1, "");

            if(!entry)
            {
                ui_overlay_draw_line(line++, "Entry not found.");
            }
            else
            {
                snprintf(scratch, sizeof(scratch), "Name: %s [%s]", entry->name, bestiary_entry_type_label(entry->type));
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "State: %s", bestiary_knowledge_label(entry->knowledge));
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "Encounters: %d", entry->encounter_count);
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "Kills: %d", entry->kill_count);
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "First sighted: %s", entry->first_sighted_ts[0] ? entry->first_sighted_ts : "unknown");
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "First killed: %s", entry->first_killed_ts[0] ? entry->first_killed_ts : "unknown");
                ui_overlay_draw_line(line++, scratch);
                ui_overlay_draw_line(line++, "");
                ui_overlay_draw_line(line++, "Hints:");

                if(entry->hint_count == 0)
                    ui_overlay_draw_line(line++, "- No hints recorded.");
                else
                {
                    for(int i = 0; i < entry->hint_count && line < status_line; i++)
                    {
                        snprintf(scratch, sizeof(scratch), "- %s", entry->hints[i]);
                        ui_overlay_draw_line(line++, scratch);
                    }
                }
            }

            for(; line < status_line; line++)
                ui_overlay_draw_line(line, "");
            ui_overlay_draw_line(status_line, "Press o or q to return, i/c/j to switch overlays.");
            ui_overlay_draw_global_hotkeys();

            int key = read_input_key();
            OverlayType next;
            if(KEYBIND_OVERLAY_EXIT(key) || KEYBIND_CANCEL(key))
            {
                view_index = -1;
                continue;
            }
            if(overlay_type_from_key(key, &next) && next != OVERLAY_TYPE_CODEX)
            {
                overlay_request(next);
                return;
            }
            continue;
        }

        ui_overlay_draw_line(0, "Bestiary: W/X scroll | S/Enter view | o/back | i/c/j switch overlays");
        ui_overlay_draw_line(1, "");

        if(race_count == 0 && creature_count == 0)
        {
            ui_overlay_draw_line(2, "No discovered creatures or races yet.");
            for(int i = 3; i < status_line; i++)
                ui_overlay_draw_line(i, "");
        }
        else
        {
            int row = 2;
            int display_start = top_index;
            int display_end = display_start + visible_lines;
            if(display_end > known_count)
                display_end = known_count;

            if(race_count > 0)
            {
                if(display_start < race_count)
                {
                    ui_overlay_draw_line(row++, "-- Humanoid Races --");
                    for(int entry_index = display_start; entry_index < race_count && entry_index < display_end; entry_index++)
                    {
                        const BestiaryEntryInfo* entry = bestiary_entry_by_index(race_indices[entry_index]);
                        char scratch[128];
                        snprintf(scratch, sizeof(scratch), "%c %2d) %s (E:%d K:%d)",
                            (entry_index == top_index + selected_line) ? '>' : ' ',
                            entry_index + 1,
                            entry->name,
                            entry->encounter_count,
                            entry->kill_count);
                        ui_overlay_draw_line(row++, scratch);
                    }
                }

                if(display_end > race_count)
                {
                    ui_overlay_draw_line(row++, "-- Creatures --");
                    for(int entry_index = display_start < race_count ? race_count : display_start;
                        entry_index < display_end;
                        entry_index++)
                    {
                        const BestiaryEntryInfo* entry = bestiary_entry_by_index(creature_indices[entry_index - race_count]);
                        char scratch[128];
                        snprintf(scratch, sizeof(scratch), "%c %2d) %s (E:%d K:%d)",
                            (entry_index == top_index + selected_line) ? '>' : ' ',
                            entry_index + 1,
                            entry->name,
                            entry->encounter_count,
                            entry->kill_count);
                        ui_overlay_draw_line(row++, scratch);
                    }
                }
            }
            else
            {
                ui_overlay_draw_line(row++, "-- Creatures --");
                for(int entry_index = display_start; entry_index < display_end; entry_index++)
                {
                    const BestiaryEntryInfo* entry = bestiary_entry_by_index(creature_indices[entry_index]);
                    char scratch[128];
                    snprintf(scratch, sizeof(scratch), "%c %2d) %s (E:%d K:%d)",
                        (entry_index == top_index + selected_line) ? '>' : ' ',
                        entry_index + 1,
                        entry->name,
                        entry->encounter_count,
                        entry->kill_count);
                    ui_overlay_draw_line(row++, scratch);
                }
            }
        }

        for(int line = 2 + visible_lines; line < status_line; line++)
            ui_overlay_draw_line(line, "");

        ui_overlay_draw_line(status_line, "Press S/Enter to view entry.");
        ui_overlay_draw_global_hotkeys();

        int key = read_input_key();
        if(KEYBIND_OVERLAY_EXIT(key))
            break;

        OverlayType next;
        if(overlay_type_from_key(key, &next) && next != OVERLAY_TYPE_CODEX)
        {
            overlay_request(next);
            break;
        }

        if(key == INPUT_KEY_DOWN || KEYBIND_DOWN(key))
        {
            if(selected_line < visible_lines - 1 && top_index + selected_line < known_count - 1)
                selected_line++;
            else if(top_index + visible_lines < known_count)
                top_index++;
            continue;
        }

        if(key == INPUT_KEY_UP || KEYBIND_UP(key))
        {
            if(selected_line > 0)
                selected_line--;
            else if(top_index > 0)
                top_index--;
            continue;
        }

        if(KEYBIND_SELECT(key) || KEYBIND_MATCH_ALPHA(key, 'e', 'E'))
        {
            if(known_count > 0)
            {
                int index = top_index + selected_line;
                if(index >= 0 && index < current)
                    view_index = known_indices[index];
            }
            continue;
        }
    }
}
