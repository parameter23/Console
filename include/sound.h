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

void bd_sound_play(SoundId id);
void bd_sound_update(void);   // im Mainloop aufrufen
