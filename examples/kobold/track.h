/**
 * @file track.h
 * @brief Background music for "Der Kobold" - the melody and bass line
 *        of death_waltz.mid (tools/death_waltz.mid), extracted with
 *        examples/dig-demo/tools/midi2console.py --track 0 (melody,
 *        "Strings") and --track 2 (bass, "Bass Guitar"). Those were
 *        the highest- and lowest-pitched of the file's six tracks
 *        respectively - see tools/README-midi2console.md.
 */
#ifndef KOBOLD_TRACK_H
#define KOBOLD_TRACK_H

#include "music.h"

/**
 * @brief The melody, 0/0-terminated. Pass to music_init(). Padded with
 *        a trailing rest so its total duration matches
 *        death_waltz_bass[]'s (115200 ms), keeping the two
 *        independently-looping voices in phase across loop boundaries.
 */
extern const MusicNote death_waltz_melody[];

/**
 * @brief The bass line, played on the engine's optional third voice
 *        (see music_init_bass()).
 */
extern const MusicNote death_waltz_bass[];

#endif
