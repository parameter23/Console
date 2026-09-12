/**
 * @file track.h
 * @brief A short, original, somber background loop for Die Nebelkrone -
 *        hand-written (not MIDI-derived, unlike dig-demo's track.h), in
 *        A minor to match the misty/ruin mood of the story.
 */
#ifndef NEBELKRONE_TRACK_H
#define NEBELKRONE_TRACK_H

#include "music.h"

/** @brief The melody, 0/0-terminated. Pass to music_init(). */
extern const MusicNote nebel_track[];

/**
 * @brief A simple root-note pedal bass line under the melody, played on
 *        the engine's optional third voice (see music_init_bass()).
 *        Its four notes sum to the same 8800 ms as nebel_track[], so the
 *        two channels stay in phase across loops.
 */
extern const MusicNote nebel_bass[];

#endif
