/**
 * @file story.h
 * @brief Content for "Der Kobold" - built from gamebook-template, but the
 *        story itself doesn't fit that template's linear Scene-graph
 *        engine (NODE_TEXT/CHECK/COMBAT/END, fixed choice_target links):
 *        this is a randomized 3x3 room-graph explored freely with the
 *        joystick, closer in spirit to the room-based examples this
 *        engine also supports (see dokumentation.org's Pinbelegung/
 *        room-map sections) than to nebelkrone's branching narrative.
 *        So unlike a normal gamebook-template story, main.c owns the
 *        actual game logic (map generation, movement, turn counter,
 *        inventory) - this file only holds text content and the small
 *        enum describing what can occupy a forest tile, keeping with
 *        the template's spirit (content separate from mechanics) even
 *        though the Scene/NodeKind machinery itself isn't used.
 */
#ifndef KOBOLD_STORY_H
#define KOBOLD_STORY_H

/* --- Title screen text; max 40 chars/line to fit the 320px-wide
 * screen at 8px/char. --- */
#define GAME_TITLE      "DER KOBOLD"
#define GAME_TAGLINE    "EIN VERWUNSCHENER WALD"
#define GAME_PREMISE_1  "DU HAST DICH IM WALD VERIRRT."
#define GAME_PREMISE_2  "FOLGE DEM PFAD, LOESE DAS RAETSEL"
#define GAME_PREMISE_3  "UND FINDE DEN WEG HERAUS."

/**
 * @brief What occupies a forest grid cell. STATION_WALD is the default
 *        (plain forest, no special content) - the other four are
 *        placed into four of the eight non-entrance cells at random
 *        each game (see shuffle_stations() in main.c).
 */
typedef enum {
    STATION_WALD = 0,
    STATION_HAUS,
    STATION_KOBOLD,
    STATION_MOOR,
    STATION_BACH,
    STATION_COUNT
} StationType;

#define FOREST_W 3
#define FOREST_H 3
#define MAX_TURNS 30

/* Turn/step limit reached: falling asleep from exhaustion. */
extern const char *const txt_lose_sleep;
/* Put the ring on: turned into a kobold yourself. */
extern const char *const txt_lose_ring;
/* Attacked the kobold: she strikes back, the forest closes for good. */
extern const char *const txt_lose_angriff;
/* Gave the ring to the kobold: she turns back, shows the way out. */
extern const char *const txt_win;

/* Shown once, right after stepping past the path into the forest. */
extern const char *const txt_enter_forest;

/* Per-station flavor/interaction text, indexed by StationType.
 * STATION_BACH has two variants (ring still there / already taken),
 * see txt_bach_found/txt_bach_empty instead of station_text[STATION_BACH]. */
extern const char *const station_text[STATION_COUNT];

extern const char *const txt_bach_found;
extern const char *const txt_bach_empty;
extern const char *const txt_kobold_story;
/* Chosen "Aufmuntern" - required at least once before she'll accept
 * the ring (see cheered_up in main.c). */
extern const char *const txt_kobold_aufmuntern;

/* One-line status hints shown under the illustration during free
 * movement (path and forest respectively). */
extern const char *const txt_hint_path;
extern const char *const txt_hint_forest;

#endif
