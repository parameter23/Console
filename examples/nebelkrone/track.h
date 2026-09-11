/**
 * @file track.h
 * @brief A short, original, somber background loop for Die Nebelkrone -
 *        hand-written (not MIDI-derived, unlike dig-demo's track.h), in
 *        A minor to match the misty/ruin mood of the story.
 */
#ifndef NEBELKRONE_TRACK_H
#define NEBELKRONE_TRACK_H

#include "music.h"

/** @brief The note sequence itself, 0/0-terminated. Pass to music_init(). */
extern const MusicNote nebel_track[];

#endif
