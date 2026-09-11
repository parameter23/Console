#pragma once
#include <stdint.h>

typedef struct {
    uint16_t freq;     // 0 = Pause
    uint16_t duration; // in ms
} MusicNote;

void music_init(const MusicNote *track);
void music_update_1ms(void);
void music_stop(void);


extern const MusicNote boulder_track[];
extern const unsigned int boulder_track_length;
