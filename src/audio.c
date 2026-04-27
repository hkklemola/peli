#include "audio.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

static int audio_initialized = 0;
static int audio_music_open = 0;
static int audio_music_looping = 0;
static const char* audio_music_alias = "menu_music";

static int audio_file_exists(const char* path)
{
    FILE* file = fopen(path, "rb");
    if(!file)
        return 0;
    fclose(file);
    return 1;
}

static int audio_resolve_path(const char* requested_path, char* resolved_path, size_t resolved_size)
{
    static const char* roots[] = {
        "",
        "../",
        "build-win/",
        "build-lin/",
        "../build-win/",
        "../build-lin/"
    };

    for(int i = 0; i < (int)(sizeof(roots) / sizeof(roots[0])); i++)
    {
        snprintf(resolved_path, resolved_size, "%s%s", roots[i], requested_path);
        if(!audio_file_exists(resolved_path))
            continue;

        if(GetFullPathNameA(resolved_path, (DWORD)resolved_size, resolved_path, NULL) == 0)
            continue;

        for(char* cursor = resolved_path; *cursor; cursor++)
        {
            if(*cursor == '/')
                *cursor = '\\';
        }

        return 1;
    }

    resolved_path[0] = '\0';
    return 0;
}

static int audio_mci_execute(const char* command)
{
    MCIERROR result = mciSendStringA(command, NULL, 0, NULL);
    if(result != 0)
    {
        char error_text[256] = "Unknown MCI error.";
        if(mciGetErrorStringA(result, error_text, sizeof(error_text)))
            fprintf(stderr, "Audio MCI error (%s): %s\n", command, error_text);
        else
            fprintf(stderr, "Audio MCI error (%s): code %u\n", command, (unsigned int)result);
        return 0;
    }
    return 1;
}

static int audio_mci_query(const char* command, char* output, int output_size)
{
    MCIERROR result = mciSendStringA(command, output, output_size, NULL);
    if(result != 0)
    {
        char error_text[256] = "Unknown MCI error.";
        if(mciGetErrorStringA(result, error_text, sizeof(error_text)))
            fprintf(stderr, "Audio MCI query error (%s): %s\n", command, error_text);
        else
            fprintf(stderr, "Audio MCI query error (%s): code %u\n", command, (unsigned int)result);
        return 0;
    }
    return 1;
}

int audio_init(void)
{
    audio_initialized = 1;
    return 1;
}

void audio_shutdown(void)
{
    audio_stop_music();
    audio_initialized = 0;
}

int audio_play_music(const char* path, int loop)
{
    char command[1024];
    char resolved_path[1024];

    if(!audio_initialized || !path || !path[0])
        return 0;

    if(!audio_resolve_path(path, resolved_path, sizeof(resolved_path)))
    {
        fprintf(stderr, "Audio file not found: %s\n", path);
        return 0;
    }

    audio_stop_music();

    snprintf(command, sizeof(command), "open \"%s\" type sequencer alias %s", resolved_path, audio_music_alias);
    if(!audio_mci_execute(command))
    {
        fprintf(stderr, "Audio open failed: %s\n", command);
        return 0;
    }

    audio_music_open = 1;
    if(loop)
        snprintf(command, sizeof(command), "play %s from 0", audio_music_alias);
    else
        snprintf(command, sizeof(command), "play %s from 0", audio_music_alias);

    if(!audio_mci_execute(command))
    {
        fprintf(stderr, "Audio play failed: %s\n", command);
        audio_stop_music();
        return 0;
    }

    audio_music_looping = loop ? 1 : 0;
    return 1;
}

void audio_tick(void)
{
    char status_text[64];

    if(!audio_music_open || !audio_music_looping)
        return;

    if(!audio_mci_query("status menu_music mode", status_text, sizeof(status_text)))
        return;

    for(int i = (int)strlen(status_text) - 1; i >= 0; i--)
    {
        if(status_text[i] == '\r' || status_text[i] == '\n' || status_text[i] == ' ' || status_text[i] == '\t')
            status_text[i] = '\0';
        else
            break;
    }

    if(strcmp(status_text, "stopped") == 0)
    {
        if(!audio_mci_execute("play menu_music from 0"))
            fprintf(stderr, "Audio loop restart failed.\n");
    }
}

void audio_stop_music(void)
{
    char command[256];

    if(!audio_music_open)
        return;

    snprintf(command, sizeof(command), "stop %s", audio_music_alias);
    mciSendStringA(command, NULL, 0, NULL);
    snprintf(command, sizeof(command), "close %s", audio_music_alias);
    mciSendStringA(command, NULL, 0, NULL);

    audio_music_open = 0;
    audio_music_looping = 0;
}

#else

int audio_init(void)
{
    return 1;
}

void audio_shutdown(void)
{
}

int audio_play_music(const char* path, int loop)
{
    (void)path;
    (void)loop;
    return 0;
}

void audio_tick(void)
{
}

void audio_stop_music(void)
{
}

#endif
