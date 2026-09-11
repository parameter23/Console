/**
 * @file sound.h
 * @brief Game-facing sound-event API for the dig-demo, mapping abstract
 *        events onto sfx.h's low-level synth effects.
 */

/**
 * @brief Abstract sound events a game action can trigger.
 */
typedef enum {
    SND_NONE = 0,
    SND_STEP,
    SND_DIAMOND,
    SND_ROCK_FALL,
    SND_ROCK_KILL,
    SND_EXIT_ACTIVATE,
    SND_LEVEL_COMPLETE,
    SND_LIFE_LOST,
} SoundId;

/**
 * @brief Requests a sound event. Only the most recent request is kept;
 *        call bd_sound_update() to actually dispatch it.
 * @param id Event to play.
 */
void bd_sound_play(SoundId id);

/**
 * @brief Dispatches the most recently requested sound event (if any) to
 *        sfx_play(). Call once per main-loop iteration.
 */
void bd_sound_update(void);
