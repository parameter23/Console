/**
 * @file track.h
 * @brief Background music for the Boulder Dash port - a single melody
 *        track, no separate bass line (see include/music.h's
 *        music_init()/music_init_bass()).
 */
#ifndef BOULDERDASH_TRACK_H
#define BOULDERDASH_TRACK_H

#include "music.h"

/** @brief 0/0-terminated. Pass to music_init(). */
extern const MusicNote boulder_track[];

/** @brief Number of entries in boulder_track[], including the
 *         terminating {0,0}. */
extern const unsigned int boulder_track_length;

#endif
