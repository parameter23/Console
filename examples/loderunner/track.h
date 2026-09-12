/**
 * @file track.h
 * @brief A short, original, up-tempo background loop for the runner -
 *        hand-written (not MIDI-derived), in A minor.
 */
#ifndef LODERUNNER_TRACK_H
#define LODERUNNER_TRACK_H

#include "music.h"

/** @brief The melody, 0/0-terminated. Pass to music_init(). */
extern const MusicNote runner_track[];

/**
 * @brief A simple alternating root/fifth bass line under the melody,
 *        played on the engine's optional third voice (see
 *        music_init_bass()). Its four notes sum to the same 2700 ms as
 *        runner_track[], so the two channels stay in phase across loops.
 */
extern const MusicNote runner_bass[];

#endif
