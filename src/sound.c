#include "sound.h"
#include "sfx.h"
#include <stddef.h>

/*
 * This used to be declared in sound.h but never implemented anywhere in
 * the project. bd_sound_play() is the game-facing sound API: it maps
 * abstract game events (SoundId) onto the low-level synth effects in
 * sfx.h/sfx.c.
 */

static SoundId pending_sound = SND_NONE;

void bd_sound_play(SoundId id)
{
    /* Latch the most recent request; bd_sound_update() dispatches it.
     * This keeps callers free of any knowledge of sfx.h. */
    pending_sound = id;
}

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
