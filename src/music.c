/**
 * @file music.c
 * @brief Implementation of the MusicNote track sequencer (see music.h).
 */
#include "music.h"
#include "sfx.h"

static const MusicNote *music_track = 0;
static uint32_t music_pos = 0;
static uint32_t music_time_left = 0;
static uint8_t music_playing = 0;

/** @brief See music_init() in the header for the full contract. */
void music_init(const MusicNote *track)
{
    music_track = track;
    music_pos = 0;
    music_time_left = 0;
    music_playing = 1;
}

/** @brief See music_stop() in the header for the full contract. */
void music_stop(void)
{
    music_playing = 0;
    sfx_play_freq(0);   // Voice 0 stumm
}

/** @brief See music_update_1ms() in the header for the full contract. */
void music_update_1ms(void)
{
    if (!music_playing || !music_track)
        return;

    if (music_time_left > 0) {
        music_time_left--;
        return;
    }

    /* Nächste Note */
    uint16_t freq = music_track[music_pos].freq;
    uint16_t dur  = music_track[music_pos].duration;

    if (dur == 0) {
        /* Track-Ende */
        music_pos = 0;
        return;
    }

    /* Note spielen */
    sfx_play_freq(freq);

    music_time_left = dur;
    music_pos++;
}
