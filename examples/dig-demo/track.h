/**
 * @file track.h
 * @brief Background music track for the dig-demo, generated from
 *        boulderdash.mid via tools/midi2console.py - see
 *        tools/README-midi2console.md.
 */
#ifndef TRACK_H
#define TRACK_H

#include "music.h"

/** @brief The note sequence itself, 0/0-terminated. Pass to music_init(). */
extern const MusicNote boulder_track[];
/** @brief Number of entries in boulder_track[], including the terminator. */
extern const unsigned int boulder_track_length;

#endif
