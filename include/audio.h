#ifndef AUDIO_H
#define AUDIO_H

/*
 * Purpose:
 *   Provide a minimal game audio interface for menu music playback.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the audio subsystem.
 * @return 1 if audio was initialized successfully, 0 otherwise.
 */
int audio_init(void);

/**
 * @brief Shutdown the audio subsystem and free resources.
 */
void audio_shutdown(void);

/**
 * @brief Start playing a music file.
 * @param path Path to the music file.
 * @param loop Non-zero to loop playback forever.
 * @return 1 if playback was started successfully, 0 otherwise.
 */
int audio_play_music(const char* path, int loop);

/**
 * @brief Stop currently playing music.
 */
void audio_stop_music(void);

/**
 * @brief Set current music volume for playback.
 * @param volume Volume level 0..10.
 * @return 1 if the volume was applied successfully, 0 otherwise.
 */
int audio_set_volume(int volume);

/**
 * @brief Check whether music is currently playing.
 * @return 1 if audio is playing, 0 otherwise.
 */
int audio_is_playing(void);

/**
 * @brief Poll the audio subsystem and restart looping music if needed.
 */
void audio_tick(void);

#ifdef __cplusplus
}
#endif

#endif
