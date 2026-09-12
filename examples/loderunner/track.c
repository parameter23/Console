/**
 * @file track.c
 * @brief Note data for runner_track/runner_bass (see track.h).
 */
#include "track.h"

const MusicNote runner_track[] = {
    { 440, 150 }, { 392, 150 }, { 440, 150 }, { 523, 150 },
    { 440, 150 }, { 392, 150 }, { 349, 150 }, { 392, 300 },
    { 440, 150 }, { 392, 150 }, { 440, 150 }, { 523, 150 },
    { 587, 150 }, { 523, 150 }, { 440, 150 }, { 392, 300 },
    { 0, 0 },
};

/* Alternating A2/E2 root-fifth pulse under the melody. */
const MusicNote runner_bass[] = {
    { 110, 700 }, { 82, 650 }, { 110, 700 }, { 82, 650 },
    { 0, 0 },
};
