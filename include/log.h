#ifndef LOG_H
#define LOG_H

// Maximum number of messages
#define LOG_MAX 50
#define LOG_ENTRY_LENGTH 128

// Add a message to the log
void log_add(const char* fmt, ...);

// Draw all messages
void log_draw(void);

// Initialize the log system
void log_init(void);

#endif