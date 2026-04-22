#include "crafting_compendium.h"
#include "player.h"
#include "overlay_nav.h"
#include "ui_overlay.h"
#include "input.h"
#include "keybind_helpers.h"
#include "combat.h"

#include <stdio.h>
#include <string.h>

static const char* crafting_tier_label(CraftingDiscoveryTier tier)
{
    switch(tier)
    {
        case CRAFTING_DISCOVERY_RUMORED:
            return "rumored";
        case CRAFTING_DISCOVERY_RECORDED:
            return "recorded";
        case CRAFTING_DISCOVERY_ATTEMPTED:
            return "attempted";
        case CRAFTING_DISCOVERY_MASTERED:
            return "mastered";
        case CRAFTING_DISCOVERY_UNKNOWN:
        default:
            return "unknown";
    }
}

void crafting_compendium_show_overlay(Player* player)
{
    (void)player;
    int selected_line = 0;
    int top_index = 0;
    int view_index = -1;

    while(1)
    {
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        int visible_lines = (content_lines > 5) ? (content_lines - 5) : 1;
        int active_count = 0;
        int active_indices[CRAFTING_COMPENDIUM_MAX_ENTRIES];

        for(int i = 0; i < crafting_compendium_count; i++)
        {
            if(crafting_compendium[i].active)
                active_indices[active_count++] = i;
        }

        if(selected_line < 0)
            selected_line = 0;
        if(selected_line >= visible_lines)
            selected_line = visible_lines - 1;
        if(top_index < 0)
            top_index = 0;
        if(top_index > active_count - visible_lines)
            top_index = active_count - visible_lines;
        if(top_index < 0)
            top_index = 0;

        ui_overlay_draw_frame("Crafting Compendium");

        if(view_index >= 0)
        {
            const CraftingCompendiumEntry* entry = (view_index >= 0 && view_index < crafting_compendium_count) ? &crafting_compendium[view_index] : NULL;
            int line = 2;
            char scratch[192];

            ui_overlay_draw_line(0, "Crafting detail: o/back | q cancel | i/c/j switch overlays");
            ui_overlay_draw_line(1, "");

            if(!entry)
            {
                ui_overlay_draw_line(line++, "Entry not found.");
            }
            else
            {
                snprintf(scratch, sizeof(scratch), "Recipe: %s", entry->recipe_id);
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "Station: %s", entry->station[0] ? entry->station : "unknown");
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "Skill: %s", non_weapon_skill_name(entry->skill));
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "Difficulty: %d", entry->difficulty);
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "State: %s", crafting_tier_label(entry->tier));
                ui_overlay_draw_line(line++, scratch);
                snprintf(scratch, sizeof(scratch), "Attempts: %d  Successes: %d", entry->attempts, entry->successes);
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

        ui_overlay_draw_line(0, "Crafting Compendium: W/X scroll | S/Enter view | o/back | i/c/j switch overlays");
        ui_overlay_draw_line(1, "");

        if(active_count == 0)
        {
            ui_overlay_draw_line(2, "No known recipes discovered yet.");
            for(int i = 3; i < status_line; i++)
                ui_overlay_draw_line(i, "");
        }
        else
        {
            for(int line = 0; line < visible_lines; line++)
            {
                int row = 2 + line;
                int entry_index = top_index + line;
                if(entry_index >= active_count)
                {
                    ui_overlay_draw_line(row, "");
                    continue;
                }

                const CraftingCompendiumEntry* entry = &crafting_compendium[active_indices[entry_index]];
                char scratch[128];
                snprintf(scratch, sizeof(scratch), "%c %2d) %s [%s]",
                         (entry_index == top_index + selected_line) ? '>' : ' ',
                         entry_index + 1,
                         entry->recipe_id,
                         crafting_tier_label(entry->tier));
                ui_overlay_draw_line(row, scratch);
            }
        }

        for(int line = 2 + visible_lines; line < status_line; line++)
            ui_overlay_draw_line(line, "");

        ui_overlay_draw_line(status_line, "Press S/Enter to view recipe.");
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
            if(selected_line < visible_lines - 1 && top_index + selected_line < active_count - 1)
                selected_line++;
            else if(top_index + visible_lines < active_count)
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
            if(active_count > 0)
            {
                int index = top_index + selected_line;
                if(index >= 0 && index < active_count)
                    view_index = active_indices[index];
            }
            continue;
        }
    }
}
