/**
 * @file story.h
 * @brief Data-driven gamebook content for Die Nebelkrone: attributes,
 *        story flags, the enemy table and the scene graph itself.
 *
 * The whole adventure is one array of Scene nodes (see scenes[] in
 * story.c) that main.c's state machine walks: NODE_TEXT nodes show
 * illustration + text + a UP/DOWN+BTN choice menu; NODE_CHECK nodes
 * roll a die against one hero attribute and branch; NODE_COMBAT nodes
 * run a small turn-based fight; NODE_END_WIN/NODE_END_LOSE end the run.
 * This mirrors the "gamebook" format Das Schwarze Auge itself published
 * as solo adventures, simplified to fit a 5-button joystick: no keyboard
 * parser, just numbered choices and single-die attribute checks instead
 * of the full pen-and-paper ruleset.
 */
#ifndef NEBELKRONE_STORY_H
#define NEBELKRONE_STORY_H

#include <stdint.h>
#include "art.h"
#include "sfx.h"

/** @brief The three hero attributes checked by NODE_CHECK scenes. */
typedef enum {
    ATTR_MUT = 0,        /* courage - forcing doors, standing your ground */
    ATTR_KLUGHEIT,       /* wits - searching, puzzling out a safe path */
    ATTR_GEWANDTHEIT,     /* agility - balance, dodging, fleeing */
    ATTR_COUNT
} Attr;

extern const char *const attr_name[ATTR_COUNT];

#define HERO_MUT_START         12
#define HERO_KLUGHEIT_START    11
#define HERO_GEWANDTHEIT_START 13
#define HERO_LP_START          15
#define HERO_LP_MAX            15

/** @brief Inventory/story flags a scene can grant or check for. */
typedef enum {
    FLAG_TORCH = 0,
    FLAG_POTION,
    FLAG_CROWN,
    FLAG_COUNT
} StoryFlag;

/** @brief One combat opponent. */
typedef struct {
    const char *name;
    int lp;
    int dmg_min, dmg_max;
    int hit_chance; /* enemy hits if a d20 roll is <= this */
} Enemy;

typedef enum {
    ENEMY_MOORSCHRAT = 0,
    ENEMY_COUNT
} EnemyId;

extern const Enemy enemies[ENEMY_COUNT];

/** @brief What a scene node does and how the player interacts with it. */
typedef enum {
    NODE_TEXT,      /* illustration + text + a choice menu */
    NODE_CHECK,     /* auto-rolls one attribute check, then branches */
    NODE_COMBAT,    /* a turn-based fight against enemies[enemy_id] */
    NODE_END_WIN,   /* victory screen, offers a new game */
    NODE_END_LOSE,  /* game-over screen, offers a new game */
} NodeKind;

#define MAX_CHOICES 4

/** @brief One node of the story graph. */
typedef struct {
    NodeKind kind;
    bg_id_t bg;
    const char *text;
    SfxType enter_sfx;   /* played once when the scene is entered, or SFX_NONE */
    int8_t grant_flag;   /* StoryFlag granted on entry, or -1 */

    /* Entry penalty for lacking an item (e.g. exploring without a torch). */
    int8_t penalty_req_flag;    /* StoryFlag required to avoid it, or -1 */
    uint8_t penalty_damage;
    const char *penalty_reason; /* shown if this damage proves fatal */

    /* NODE_TEXT */
    uint8_t num_choices;
    const char *choice_text[MAX_CHOICES];
    int8_t choice_target[MAX_CHOICES];

    /* NODE_CHECK */
    uint8_t check_attr; /* Attr */
    int8_t check_pass_target;
    int8_t check_fail_target;
    uint8_t check_fail_damage;
    const char *check_fail_reason; /* shown if that damage proves fatal */

    /* NODE_COMBAT */
    uint8_t enemy_id;
    int8_t combat_win_target;
    int8_t combat_flee_target;
} Scene;

/** @brief Scene IDs, in story order - see scenes[] in story.c for content. */
enum {
    SCENE_INTRO = 0,
    SCENE_ELDER,
    SCENE_CHECK_MARSH,
    SCENE_TOWER_BASE,
    SCENE_CHECK_DOOR,
    SCENE_CHECK_SEARCH,
    SCENE_TOWER_INSIDE,
    SCENE_CHECK_FLOORBOARDS,
    SCENE_TOWER_UPPER,
    SCENE_CHEST,
    SCENE_TOWER_TOP,
    SCENE_VICTORY_ROOM,
    SCENE_CHECK_ESCAPE,
    SCENE_END_WIN,
    SCENE_END_LOSE,
    SCENE_COUNT
};

extern const Scene scenes[SCENE_COUNT];

#endif
