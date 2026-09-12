/**
 * @file music.c
 * @brief Implementation of the MusicNote track sequencer (see music.h).
 *
 * Two independent channels: the melody (voice 0, started by music_init())
 * and an optional bass line (voice 2, started by music_init_bass()). Both
 * run the same simple sequencer logic, just against their own track/
 * position/timer state and their own sfx.c voice.
 */
#include "music.h"
#include "sfx.h"

static const MusicNote *music_track = 0;
static uint32_t music_pos = 0;
static uint32_t music_time_left = 0;
static uint8_t music_playing = 0;

static const MusicNote *bass_track = 0;
static uint32_t bass_pos = 0;
static uint32_t bass_time_left = 0;
static uint8_t bass_playing = 0;

/** @brief See music_init() in the header for the full contract. */
void music_init(const MusicNote *track)
{
    music_track = track;
    music_pos = 0;
    music_time_left = 0;
    music_playing = 1;
}

/** @brief See music_init_bass() in the header for the full contract. */
void music_init_bass(const MusicNote *track)
{
    bass_track = track;
    bass_pos = 0;
    bass_time_left = 0;
    bass_playing = 1;
}

/** @brief See music_stop() in the header for the full contract. */
void music_stop(void)
{
    music_playing = 0;
    bass_playing = 0;
    sfx_play_freq(0);       // Voice 0 stumm
    sfx_play_bass_freq(0);  // Voice 2 stumm
}

/** @brief See music_update_1ms() in the header for the full contract. */
void music_update_1ms(void)
{
    if (music_playing && music_track) {
        if (music_time_left > 0) {
            music_time_left--;
        } else {
            uint16_t freq = music_track[music_pos].freq;
            uint16_t dur  = music_track[music_pos].duration;

            if (dur == 0) {
                /* Track-Ende */
                music_pos = 0;
            } else {
                sfx_play_freq(freq);
                music_time_left = dur;
                music_pos++;
            }
        }
    }

    if (bass_playing && bass_track) {
        if (bass_time_left > 0) {
            bass_time_left--;
        } else {
            uint16_t freq = bass_track[bass_pos].freq;
            uint16_t dur  = bass_track[bass_pos].duration;

            if (dur == 0) {
                bass_pos = 0;
            } else {
                sfx_play_bass_freq(freq);
                bass_time_left = dur;
                bass_pos++;
            }
        }
    }
}
