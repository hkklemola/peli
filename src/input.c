#include "input.h"

#ifdef _WIN32
#include <conio.h>

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
        if(c == 83) return INPUT_KEY_DEL;
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
        if(c == 83) return INPUT_KEY_DEL;
    }

    return c;
}

#else

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios g_original_termios;
static int g_raw_mode_enabled = 0;

static void input_restore_terminal_mode(void)
{
    if(g_raw_mode_enabled)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
        g_raw_mode_enabled = 0;
    }
}

static int input_ensure_raw_mode(void)
{
    struct termios raw;

    if(g_raw_mode_enabled)
        return 1;

    if(!isatty(STDIN_FILENO))
        return 0;

    if(tcgetattr(STDIN_FILENO, &g_original_termios) != 0)
        return 0;

    raw = g_original_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if(tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
        return 0;

    g_raw_mode_enabled = 1;
    atexit(input_restore_terminal_mode);
    return 1;
}

static int input_read_byte_blocking(void)
{
    unsigned char ch;
    ssize_t read_result;

    do
    {
        read_result = read(STDIN_FILENO, &ch, 1);
    }
    while(read_result < 0 && errno == EINTR);

    if(read_result <= 0)
        return -1;

    return (int)ch;
}

static int input_read_byte_nonblocking(void)
{
    fd_set read_set;
    struct timeval timeout;
    int ready;

    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    ready = select(STDIN_FILENO + 1, &read_set, NULL, NULL, &timeout);
    if(ready <= 0)
        return -1;

    return input_read_byte_blocking();
}

static int input_decode_escape_sequence(int nonblocking)
{
    int first = nonblocking ? input_read_byte_nonblocking() : input_read_byte_blocking();
    int second;

    if(first < 0)
        return 27;

    if(first == '[')
    {
        second = nonblocking ? input_read_byte_nonblocking() : input_read_byte_blocking();
        if(second < 0)
            return 27;

        if(second == 'A') return INPUT_KEY_UP;
        if(second == 'B') return INPUT_KEY_DOWN;
        if(second == 'C') return INPUT_KEY_RIGHT;
        if(second == 'D') return INPUT_KEY_LEFT;
        if(second == 'H') return INPUT_KEY_HOME;
        if(second == 'F') return INPUT_KEY_END;

        if(second >= '0' && second <= '9')
        {
            int third = nonblocking ? input_read_byte_nonblocking() : input_read_byte_blocking();
            if(third == '~')
            {
                if(second == '5') return INPUT_KEY_PGUP;
                if(second == '6') return INPUT_KEY_PGDN;
                if(second == '1' || second == '7') return INPUT_KEY_HOME;
                if(second == '4' || second == '8') return INPUT_KEY_END;
                if(second == '3') return INPUT_KEY_DEL;
            }
        }
    }
    else if(first == 'O')
    {
        second = nonblocking ? input_read_byte_nonblocking() : input_read_byte_blocking();
        if(second == 'H') return INPUT_KEY_HOME;
        if(second == 'F') return INPUT_KEY_END;
    }

    return 27;
}

// Read one key and normalize ANSI escape sequences.
int read_input_key(void)
{
    int c;

    if(!input_ensure_raw_mode())
    {
        c = getchar();
        if(c == '\n' || c == '\r')
            return 13;
        return c;
    }

    c = input_read_byte_blocking();
    if(c < 0)
        return -1;

    if(c == 27)
        return input_decode_escape_sequence(0);

    if(c == '\n' || c == '\r')
        return 13;

    return c;
}

// Poll for one key and return -1 when no input is available.
int read_input_key_nonblocking(void)
{
    int c;

    if(!input_ensure_raw_mode())
        return -1;

    c = input_read_byte_nonblocking();
    if(c < 0)
        return -1;

    if(c == 27)
        return input_decode_escape_sequence(1);

    if(c == '\n' || c == '\r')
        return 13;

    return c;
}

#endif

