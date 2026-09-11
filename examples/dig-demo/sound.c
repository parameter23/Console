/**
 * @file sound.c
 * @brief Implementation of the sound-event dispatcher (see sound.h).
 */
#include "sound.h"
#include "sfx.h"
#include <stddef.h>

static SoundId pending_sound = SND_NONE;

/** @brief See bd_sound_play() in the header for the full contract. */
void bd_sound_play(SoundId id)
{
    /* Latch the most recent request; bd_sound_update() dispatches it.
     * This keeps callers free of any knowledge of sfx.h. */
    pending_sound = id;
}

/** @brief See bd_sound_update() in the header for the full contract. */
void bd_sound_update(void)
{
    if (pending_sound == SND_NONE)
        return;

    switch (pending_sound) {
        case SND_STEP:            sfx_play(SFX_MOVE);       break;
        case SND_DIAMOND:         sfx_play(SFX_PICKUP);     break;
        case SND_ROCK_FALL:       sfx_play(SFX_NOISE_SHORT);break;
        case SND_ROCK_KILL:       sfx_play(SFX_EXPLOSION);  break;
        case SND_EXIT_ACTIVATE:   sfx_play(SFX_LASER);      break;
        case SND_LEVEL_COMPLETE:  sfx_play(SFX_LASER);      break;
        case SND_LIFE_LOST:       sfx_play(SFX_EXPLOSION);  break;
        default: break;
    }

    pending_sound = SND_NONE;
}
