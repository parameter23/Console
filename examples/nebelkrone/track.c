/**
 * @file track.c
 * @brief Note data for nebel_track (see track.h).
 */
#include "track.h"

const MusicNote nebel_track[] = {
    { 220, 600 }, { 0, 100 }, { 262, 400 }, { 294, 400 }, { 330, 600 }, { 0, 150 },
    { 294, 400 }, { 262, 400 }, { 220, 800 }, { 0, 200 },
    { 165, 600 }, { 196, 400 }, { 220, 400 }, { 247, 600 }, { 0, 150 },
    { 220, 800 }, { 196, 600 }, { 165, 900 }, { 0, 300 },
    { 0, 0 },
};

/* Root-note pedal tones under each melody phrase: A2 under the opening
 * A-C-D-E phrase, E2 under its D-C-A answer, G2 under the E3/G3/A3/B3
 * phrase, back to A2 for the close - all diatonic to A minor, so they
 * hold under the melody without needing beat-for-beat interlocking. */
const MusicNote nebel_bass[] = {
    { 110, 2250 }, /* A2, under notes 1-6 (0..2250ms)   */
    { 82,  1800 }, /* E2, under notes 7-10 (2250..4050ms) */
    { 98,  2150 }, /* G2, under notes 11-15 (4050..6200ms) */
    { 110, 2600 }, /* A2, under notes 16-19 (6200..8800ms) */
    { 0, 0 },
};
