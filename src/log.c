#include "log.h"
#include "input.h"
#include "overlay_nav.h"
#include "ui_overlay.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*
 * Purpose:
 *   Implements ring-buffer logging, compact log drawing, and full log overlay UI.
 *
 * Functions:
 *   - log_init: resets log storage.
 *   - log_add: appends formatted messages into ring buffer.
 *   - log_draw: renders latest messages panel.
 *   - log_get_latest: extracts newest entries for UI panels.
 *   - log_show_overlay: interactive full-log viewer.
 */

static char messages[LOG_MAX][LOG_ENTRY_LENGTH];
static int log_start = 0;
static int log_count = 0;

// Reset log ring-buffer state and clear all message strings.
void log_init(void) {
    log_start = 0;
    log_count = 0;
    for (int i = 0; i < LOG_MAX; i++)
        messages[i][0] = '\0';
}

// Append a formatted message, scrolling older messages up when full.
void log_add(const char* format, ...) {
    va_list args;
    int index;

    if(!format)
        return;

    va_start(args, format);
    if (log_count < LOG_MAX) {
        index = (log_start + log_count) % LOG_MAX;
        log_count++;
    } else {
        // Ring-buffer overwrite: advance start and write at previous oldest slot.
        index = log_start;
        log_start = (log_start + 1) % LOG_MAX;
    }
    vsnprintf(messages[index], LOG_ENTRY_LENGTH, format, args);
    va_end(args);
}

// Draw latest message panel in framed format.
void log_draw(void) {
    char lines[LOG_MAX][LOG_ENTRY_LENGTH];
    int count = log_get_latest(lines, 10);
    const int dash_width = ui_overlay_dash_width();
    const int text_width = ui_overlay_text_width();

    putchar('+'); for(int i=0;i<dash_width;i++) putchar('-'); putchar('+'); putchar('\n');
    printf("| %-*.*s |\n", text_width, text_width, "Message Log (latest)");
    putchar('+'); for(int i=0;i<dash_width;i++) putchar('-'); putchar('+'); putchar('\n');
    for(int i=0;i<count;i++) {
        printf("| %-*.*s |\n", text_width, text_width, lines[i]);
    }
    for(int i=count;i<10;i++) {
        printf("| %-*.*s |\n", text_width, text_width, "");
    }
    putchar('+'); for(int i=0;i<dash_width;i++) putchar('-'); putchar('+'); putchar('\n');
}

// Copy newest log entries into output buffer and return entry count.
int log_get_latest(char out_lines[][LOG_ENTRY_LENGTH], int max_lines)
{
    if(max_lines <= 0) return 0;
    int display_count = log_count < max_lines ? log_count : max_lines;
    int start_index = (log_count <= max_lines) ? log_start : (log_start + log_count - max_lines) % LOG_MAX;

    for(int i=0;i<display_count;i++) {
        int index = (start_index + i) % LOG_MAX;
        snprintf(out_lines[i], LOG_ENTRY_LENGTH, "%s", messages[index]);
    }
    return display_count;
}

// Show interactive scrolling overlay for complete message history.
void log_show_overlay(void)
{
    const int overlay_content_lines = ui_overlay_content_lines();
    const int visible_lines = (overlay_content_lines > 4) ? (overlay_content_lines - 4) : 1;
    const int status_line = (overlay_content_lines > 1) ? (overlay_content_lines - 2) : 0;
    int top_index = (log_count > visible_lines) ? (log_count - visible_lines) : 0;

    while(1)
    {
        char line[LOG_ENTRY_LENGTH];
        int max_start = (log_count > visible_lines) ? (log_count - visible_lines) : 0;

        if(top_index < 0) top_index = 0;
        if(top_index > max_start) top_index = max_start;

        ui_overlay_draw_frame("Message Log (full)");
        ui_overlay_draw_line(0, "Esc/Q close | i inventory | c character | l log | j journal");
        ui_overlay_draw_line(1, "");

        for(int i = 0; i < visible_lines; i++)
        {
            int msg_i = top_index + i;
            if(msg_i < log_count)
            {
                int ring_i = (log_start + msg_i) % LOG_MAX;
                ui_overlay_draw_line(2 + i, messages[ring_i]);
            }
            else
            {
                ui_overlay_draw_line(2 + i, "");
            }
        }

        if(log_count <= 0)
            snprintf(line, sizeof(line), "No messages yet.");
        else
        {
            int first = top_index + 1;
            int last = top_index + visible_lines;
            if(last > log_count) last = log_count;
            snprintf(line, sizeof(line), "Showing %d-%d of %d", first, last, log_count);
        }
        ui_overlay_draw_line(status_line, line);
        ui_overlay_draw_global_hotkeys();

        int key = read_input_key();
        if(key == 'q' || key == 'Q' || key == 27)
            break;

        {
            OverlayType next_overlay;
            if(overlay_type_from_key(key, &next_overlay) && next_overlay != OVERLAY_TYPE_LOG)
            {
                overlay_request(next_overlay);
                break;
            }
        }

        if(key == 'w' || key == 'W' || key == INPUT_KEY_UP)
            top_index--;
        else if(key == 's' || key == 'S' || key == INPUT_KEY_DOWN)
            top_index++;
        else if(key == INPUT_KEY_PGUP)
            top_index -= visible_lines;
        else if(key == INPUT_KEY_PGDN)
            top_index += visible_lines;
        else if(key == INPUT_KEY_HOME)
            top_index = 0;
        else if(key == INPUT_KEY_END)
            top_index = max_start;
    }
}

