/**
 * @file story.h
 * @brief Story-neutral gamebook template - copy this whole folder to
 *        start a new illustrated interactive-fiction adventure, then
 *        replace this file and story.c with your own content (art.c/
 *        tileset16.c and track.c are generic enough to reuse as-is, or
 *        replace them too - see each file's header comment).
 *
 * The whole adventure is one array of Scene nodes (see scenes[] in
 * story.c) that main.c's state machine walks: NODE_TEXT nodes show
 * illustration + text + a UP/DOWN+BTN choice menu; NODE_CHECK nodes
 * roll a die against one hero attribute and branch; NODE_COMBAT nodes
 * run a small turn-based fight; NODE_END_WIN/NODE_END_LOSE end the run.
 * This mirrors the "gamebook" format Das Schwarze Auge itself published
 * as solo adventures, simplified to fit a 5-button joystick: no keyboard
 * parser, just numbered choices and single-die attribute checks instead
 * of the full pen-and-paper ruleset. See examples/nebelkrone/ for a
 * complete, non-placeholder story built on this same template.
 *
 * What's fixed (the engine's mechanics, shared by every story built on
 * this template) vs. what's yours to replace:
 *   - FIXED: three attributes (MUT/KLUGHEIT/GEWANDTHEIT), d20 checks,
 *     combat resolved against GEWANDTHEIT, the five NodeKinds.
 *   - YOURS: the GAME_TITLE/TAGLINE/PREMISE_* text below, hero starting
 *     stats, StoryFlag names beyond the required FLAG_HEAL_ITEM, the
 *     Enemy table, and of course the whole scene graph in story.c.
 */
#ifndef GAMEBOOK_TEMPLATE_STORY_H
#define GAMEBOOK_TEMPLATE_STORY_H

#include <stdint.h>
#include "art.h"
#include "sfx.h"

/* --- Title screen text (see main.c's draw_titel()); max 40 chars/line
 * to fit the 320px-wide screen at 8px/char. --- */
#define GAME_TITLE      "PLATZHALTER-TITEL"
#define GAME_TAGLINE    "EIN ILLUSTRIERTES HELDENABENTEUER"
#define GAME_PREMISE_1  "ERSETZE DIESEN TEXT UND DIE SZENEN"
#define GAME_PREMISE_2  "IN STORY.H/STORY.C MIT DEINER"
#define GAME_PREMISE_3  "EIGENEN GESCHICHTE."

/** @brief The three hero attributes checked by NODE_CHECK scenes. */
typedef enum {
    ATTR_MUT = 0,        /* courage - forcing doors, standing your ground */
    ATTR_KLUGHEIT,       /* wits - searching, puzzling out a safe path */
    ATTR_GEWANDTHEIT,     /* agility - balance, dodging, fleeing; also
                           * the attribute combat is resolved against */
    ATTR_COUNT
} Attr;

extern const char *const attr_name[ATTR_COUNT];

#define HERO_MUT_START         12
#define HERO_KLUGHEIT_START    11
#define HERO_GEWANDTHEIT_START 13
#define HERO_LP_START          15
#define HERO_LP_MAX            15

/**
 * @brief Inventory/story flags a scene can grant (Scene.grant_flag) or
 *        require (Scene.penalty_req_flag) on entry.
 *
 * FLAG_HEAL_ITEM is a required name: main.c's combat menu looks for it
 * by this exact name to decide whether to offer a "use item to heal"
 * action. Grant it from some scene (a chest, a shop, whatever fits your
 * story) if you want that mechanic; if no scene ever grants it, the
 * option simply never appears - you don't have to use it. Add as many
 * more flags of your own as your story needs (torches, keys, whatever
 * gates a choice or an entry penalty).
 */
typedef enum {
    FLAG_HEAL_ITEM = 0,
    FLAG_COUNT
} StoryFlag;

#define HEAL_ITEM_AMOUNT      6
#define HEAL_ITEM_LABEL       "TRANK (HEILT 6 LP)"
#define HEAL_ITEM_USE_MESSAGE "Du trinkst den Heiltrank. (+6 LP)"

/** @brief One combat opponent. */
typedef struct {
    const char *name;
    int lp;
    int dmg_min, dmg_max;
    int hit_chance; /* enemy hits if a d20 roll is <= this */
} Enemy;

typedef enum {
    ENEMY_PLATZHALTER = 0,
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

/**
 * @brief Scene IDs, in story order - see scenes[] in story.c for
 *        content. SCENE_INTRO (must be 0) and SCENE_END_LOSE are
 *        required names: main.c starts every run at SCENE_INTRO and
 *        apply_damage() jumps straight to SCENE_END_LOSE on lethal
 *        damage from any source, so every story needs both. Everything
 *        in between is a minimal placeholder chain exercising each
 *        NodeKind once - replace freely.
 */
enum {
    SCENE_INTRO = 0,
    SCENE_CHECK_EXAMPLE,
    SCENE_CHOICE_EXAMPLE,
    SCENE_ITEM_EXAMPLE,
    SCENE_COMBAT_EXAMPLE,
    SCENE_VICTORY_EXAMPLE,
    SCENE_END_WIN,
    SCENE_END_LOSE,
    SCENE_COUNT
};

extern const Scene scenes[SCENE_COUNT];

#endif
