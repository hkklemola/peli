#ifndef LOG_H
#define LOG_H

/*
 * Purpose:
 *   Declares message-log storage, rendering, and overlay browsing APIs.
 *
 * Functions:
 *   - log_add/log_init: lifecycle and append operations.
 *   - log_draw/log_get_latest: rendering and extraction helpers.
 *   - log_show_overlay: full-screen log browsing UI.
 */

// Maximum number of messages
#define LOG_MAX 50
#define LOG_ENTRY_LENGTH 256

// Append one formatted message to the log ring buffer.
void log_add(const char* fmt, ...);

// Draw the recent message panel.
void log_draw(void);

// Copy latest log lines into output buffer and return copied count.
int log_get_latest(char out_lines[][LOG_ENTRY_LENGTH], int max_lines);

// Show the full log in an interactive scrolling overlay.
void log_show_overlay(void);

// Reset log storage to an empty state.
void log_init(void);

#endif
