#include "journal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "input.h"
#include "log.h"
#include "overlay_nav.h"
#include "ui_overlay.h"

/**
 * @file journal.c
 * @brief Implementation of the quest/event journal and journal overlay UI.
 *
 * Manages the player's journal entries for quests, discoveries, and significant events.
 * Provides journal overlay screen with scrolling, editing, and removal of entries.
 */

/**
 * @brief Generate a human-readable timestamp in YYYY-MM-DD HH:MM format.
 * @param out Output buffer for the timestamp string (must be at least JOURNAL_TIMESTAMP_LENGTH).
 * @note If time cannot be retrieved, stores "unknown-time" in the output.
 */
static void journal_timestamp_now(char out[JOURNAL_TIMESTAMP_LENGTH])
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    if(!tm_info)
    {
        snprintf(out, JOURNAL_TIMESTAMP_LENGTH, "unknown-time");
        return;
    }

    strftime(out, JOURNAL_TIMESTAMP_LENGTH, "%Y-%m-%d %H:%M", tm_info);
}

/**
 * @brief Remove a journal entry by index, shifting remaining entries down.
 * @param p The player whose journal entry should be removed.
 * @param index The journal entry index to remove (0-based).
 * @note If index is out of bounds, does nothing.
 */
static void journal_remove_at(Player* p, int index)
{
    if(!p || index < 0 || index >= p->journal_count)
        return;

    for(int i = index + 1; i < p->journal_count; i++)
    {
        snprintf(p->journal_entries[i - 1], JOURNAL_ENTRY_LENGTH, "%s", p->journal_entries[i]);
        snprintf(p->journal_timestamps[i - 1], JOURNAL_TIMESTAMP_LENGTH, "%s", p->journal_timestamps[i]);
    }

    if(p->journal_count > 0)
    {
        p->journal_count--;
        p->journal_entries[p->journal_count][0] = '\0';
        p->journal_timestamps[p->journal_count][0] = '\0';
    }
}

static int journal_selected_index(const Player* p, int top_index, int selected_line)
{
    int idx;

    if(!p || p->journal_count <= 0)
        return -1;

    idx = p->journal_count - 1 - (top_index + selected_line);
    if(idx < 0 || idx >= p->journal_count)
        return -1;
    return idx;
}

void journal_init(Player* p)
{
    if(!p)
        return;

    p->journal_count = 0;
    for(int i = 0; i < JOURNAL_MAX_ENTRIES; i++)
    {
        p->journal_entries[i][0] = '\0';
        p->journal_timestamps[i][0] = '\0';
    }
}

int journal_add_entry(Player* p, const char* text)
{
    if(!p || !text || !text[0])
        return 0;

    if(p->journal_count >= JOURNAL_MAX_ENTRIES)
    {
        journal_remove_at(p, 0);
        log_add("Journal full. Oldest note was removed.");
    }

    snprintf(p->journal_entries[p->journal_count], JOURNAL_ENTRY_LENGTH, "%s", text);
    p->journal_entries[p->journal_count][JOURNAL_ENTRY_LENGTH - 1] = '\0';
    journal_timestamp_now(p->journal_timestamps[p->journal_count]);
    p->journal_count++;
    return 1;
}

void journal_show_overlay(Player* p)
{
    int key;
    int top_index = 0;
    int selected_line = 0;
    int writing = 0;
    int edit_mode = 0;
    int edit_index = -1;
    int draft_len = 0;
    char line[256];
    char status[256] = "j/Enter new | e edit | x delete | W/S/Arrows browse | Esc/Q close";
    char draft[JOURNAL_ENTRY_LENGTH] = "";

    if(!p)
        return;

    while(1)
    {
        int content_lines = ui_overlay_content_lines();
        int status_line = (content_lines > 1) ? (content_lines - 2) : 0;
        int visible_lines = (content_lines > 5) ? (content_lines - 5) : 1;
        int max_start = (p->journal_count > visible_lines) ? (p->journal_count - visible_lines) : 0;

        if(top_index < 0) top_index = 0;
        if(top_index > max_start) top_index = max_start;
        if(selected_line < 0) selected_line = 0;
        if(selected_line >= visible_lines) selected_line = visible_lines - 1;
        while(journal_selected_index(p, top_index, selected_line) < 0 && selected_line > 0)
            selected_line--;

        ui_overlay_draw_frame("Journal");

        if(writing)
            ui_overlay_draw_line(0, edit_mode ? "Editing note: Enter save | Esc cancel | Backspace delete" : "Writing note: Enter save | Esc cancel | Backspace delete");
        else
            ui_overlay_draw_line(0, "j/Enter new | e edit selected | x delete selected | W/S/Arrows browse");

        ui_overlay_draw_line(1, "");

        if(writing)
        {
            snprintf(line, sizeof(line), "Note: %s", draft);
            ui_overlay_draw_line(2, line);

            for(int i = 3; i < status_line; i++)
                ui_overlay_draw_line(i, "");
        }
        else if(p->journal_count <= 0)
        {
            ui_overlay_draw_line(2, "No notes yet. Press j to write one.");
            for(int i = 3; i < status_line; i++)
                ui_overlay_draw_line(i, "");
        }
        else
        {
            for(int i = 0; i < visible_lines; i++)
            {
                int line_row = 2 + i;
                int idx = p->journal_count - 1 - (top_index + i);

                if(line_row >= status_line)
                    break;

                if(idx >= 0)
                {
                    snprintf(line, sizeof(line), "%c%2d) [%s] %s", (i == selected_line ? '>' : ' '), idx + 1, p->journal_timestamps[idx][0] ? p->journal_timestamps[idx] : "no-time", p->journal_entries[idx]);
                    ui_overlay_draw_line(line_row, line);
                }
                else
                {
                    ui_overlay_draw_line(line_row, "");
                }
            }

            for(int i = 2 + visible_lines; i < status_line; i++)
                ui_overlay_draw_line(i, "");
        }

        if(writing)
            snprintf(line, sizeof(line), "Note length: %d/%d", draft_len, JOURNAL_ENTRY_LENGTH - 1);
        else if(p->journal_count <= 0)
            snprintf(line, sizeof(line), "%s", status);
        else
            snprintf(line, sizeof(line), "Selected %d | showing newest-%d through newest-%d of %d", journal_selected_index(p, top_index, selected_line) + 1, top_index + 1, top_index + visible_lines > p->journal_count ? p->journal_count : top_index + visible_lines, p->journal_count);

        ui_overlay_draw_line(status_line, line);
        ui_overlay_draw_global_hotkeys();

        key = read_input_key();

        if(writing)
        {
            if(key == 27)
            {
                writing = 0;
                draft_len = 0;
                draft[0] = '\0';
                snprintf(status, sizeof(status), "New note canceled.");
                continue;
            }

            if(key == 13)
            {
                if(draft_len > 0)
                {
                    if(edit_mode && edit_index >= 0 && edit_index < p->journal_count)
                    {
                        snprintf(p->journal_entries[edit_index], JOURNAL_ENTRY_LENGTH, "%s", draft);
                        p->journal_entries[edit_index][JOURNAL_ENTRY_LENGTH - 1] = '\0';
                        journal_timestamp_now(p->journal_timestamps[edit_index]);
                        snprintf(status, sizeof(status), "Updated journal note.");
                    }
                    else if(journal_add_entry(p, draft))
                    {
                        snprintf(status, sizeof(status), "Added journal note.");
                    }
                }
                else
                    snprintf(status, sizeof(status), "Empty note was not saved.");

                writing = 0;
                edit_mode = 0;
                edit_index = -1;
                draft_len = 0;
                draft[0] = '\0';
                top_index = 0;
                selected_line = 0;
                continue;
            }

            if((key == 8 || key == 127) && draft_len > 0)
            {
                draft_len--;
                draft[draft_len] = '\0';
                continue;
            }

            if(key >= 32 && key <= 126 && draft_len < JOURNAL_ENTRY_LENGTH - 1)
            {
                draft[draft_len++] = (char)key;
                draft[draft_len] = '\0';
            }

            continue;
        }

        if(key == 'q' || key == 'Q' || key == 27)
            break;

        {
            OverlayType next_overlay;
            if(overlay_type_from_key(key, &next_overlay) && next_overlay != OVERLAY_TYPE_JOURNAL)
            {
                overlay_request(next_overlay);
                break;
            }
        }

        if(key == 'j' || key == 'J' || key == 13)
        {
            writing = 1;
            edit_mode = 0;
            edit_index = -1;
            draft_len = 0;
            draft[0] = '\0';
            continue;
        }

        if((key == 'e' || key == 'E') && p->journal_count > 0)
        {
            int idx = journal_selected_index(p, top_index, selected_line);
            if(idx >= 0)
            {
                writing = 1;
                edit_mode = 1;
                edit_index = idx;
                snprintf(draft, sizeof(draft), "%s", p->journal_entries[idx]);
                draft_len = (int)strlen(draft);
                snprintf(status, sizeof(status), "Editing note %d.", idx + 1);
            }
            continue;
        }

        if((key == 'x' || key == 'X') && p->journal_count > 0)
        {
            int idx = journal_selected_index(p, top_index, selected_line);
            if(idx >= 0)
            {
                journal_remove_at(p, idx);
                if(top_index > 0 && top_index > ((p->journal_count > visible_lines) ? (p->journal_count - visible_lines) : 0))
                    top_index--;
                snprintf(status, sizeof(status), "Deleted note %d.", idx + 1);
            }
            continue;
        }

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
        {
            if(journal_selected_index(p, top_index, selected_line + 1) >= 0)
                selected_line++;
            else
                top_index++;
        }
        else if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
        {
            if(selected_line > 0)
                selected_line--;
            else
                top_index--;
        }
        else if(key == INPUT_KEY_PGUP)
            top_index += visible_lines;
        else if(key == INPUT_KEY_PGDN)
            top_index -= visible_lines;
        else if(key == INPUT_KEY_HOME)
            top_index = max_start;
        else if(key == INPUT_KEY_END)
            top_index = 0;
    }
}
