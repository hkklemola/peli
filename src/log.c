#include "d:/projekti/peli/include/log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static char messages[LOG_MAX][LOG_ENTRY_LENGTH];
static int msg_count = 0;
char log_entries[LOG_MAX][LOG_ENTRY_LENGTH];
int log_count = 0;

void log_init(void) {
    log_count = 0;
    for(int i = 0; i < LOG_MAX; i++)
        log_entries[i][0] = '\0';
}
// Add a formatted message to the log
void log_add(const char* format, ...) {
    // Shift older messages up
    for(int i = 0; i < LOG_MAX - 1; i++)
        strcpy(messages[i], messages[i + 1]);

    // Format new message
    va_list args;
    va_start(args, format);
    vsnprintf(messages[LOG_MAX - 1], LOG_ENTRY_LENGTH, format, args);
    va_end(args);

    if(msg_count < LOG_MAX) msg_count++;
}

// Draw the message log
void log_draw() {
    for(int i = 0; i < msg_count; i++)
        printf("%s\n", messages[i]);
}