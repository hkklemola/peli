#include <conio.h>   // for _getch()
#include "input.h"

/*
 * Purpose:
 *   Implements keyboard input wrappers for blocking and non-blocking polling.
 */

// Read one key and normalize extended key pairs.
int read_input_key(void)
{
    int c = _getch();
    if(c == 0 || c == 224)
    {
        c = _getch();
        if(c == 72) return INPUT_KEY_UP;
        if(c == 80) return INPUT_KEY_DOWN;
        if(c == 75) return INPUT_KEY_LEFT;
        if(c == 77) return INPUT_KEY_RIGHT;
        if(c == 73) return INPUT_KEY_PGUP;
        if(c == 81) return INPUT_KEY_PGDN;
        if(c == 71) return INPUT_KEY_HOME;
        if(c == 79) return INPUT_KEY_END;
    }
    return c;
}

// Poll for one key and return -1 when no input is available.
int read_input_key_nonblocking(void)
{
    int c;

    if(!_kbhit())
        return -1;

    c = _getch();
    if(c == 0 || c == 224)
    {
        c = _getch();
        if(c == 72) return INPUT_KEY_UP;
        if(c == 80) return INPUT_KEY_DOWN;
        if(c == 75) return INPUT_KEY_LEFT;
        if(c == 77) return INPUT_KEY_RIGHT;
        if(c == 73) return INPUT_KEY_PGUP;
        if(c == 81) return INPUT_KEY_PGDN;
        if(c == 71) return INPUT_KEY_HOME;
        if(c == 79) return INPUT_KEY_END;
    }

    return c;
}

