/**
 * @file track.h
 * @brief A short, original, mood-neutral background loop - a reasonable
 *        default for any fantasy gamebook built on this template.
 *        Hand-written (not MIDI-derived, unlike dig-demo's track.h), in
 *        A minor. Replace it with your own track if you want different
 *        music; nothing else needs to change to do that (see
 *        music_init()/music_init_bass() in main.c's init callback).
 */
#ifndef GAMEBOOK_TEMPLATE_TRACK_H
#define GAMEBOOK_TEMPLATE_TRACK_H

#include "music.h"

/** @brief The melody, 0/0-terminated. Pass to music_init(). */
extern const MusicNote template_track[];

/**
 * @brief A simple root-note pedal bass line under the melody, played on
 *        the engine's optional third voice (see music_init_bass()).
 *        Its four notes sum to the same 8800 ms as template_track[], so
 *        the two channels stay in phase across loops.
 */
extern const MusicNote template_bass[];

#endif
