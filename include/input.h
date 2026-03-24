// input.h
#ifndef INPUT_H
#define INPUT_H

/*
 * Purpose:
 *   Declares keyboard input helpers for blocking and non-blocking reads.
 *
 * Functions:
 *   - read_input_key: blocking key read.
 *   - read_input_key_nonblocking: non-blocking key read, returns -1 when idle.
 */

#define INPUT_KEY_UP 1000
#define INPUT_KEY_DOWN 1001
#define INPUT_KEY_LEFT 1002
#define INPUT_KEY_RIGHT 1003
#define INPUT_KEY_PGUP 1004
#define INPUT_KEY_PGDN 1005
#define INPUT_KEY_HOME 1006
#define INPUT_KEY_END 1007
#define INPUT_KEY_DEL  1008

// Read one key from input, blocking until a key is available.
int read_input_key(void);

// Read one key if available, otherwise return -1.
int read_input_key_nonblocking(void);

#endif

