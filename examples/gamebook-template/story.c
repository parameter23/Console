/**
 * @file story.c
 * @brief Placeholder content for the gamebook template (see story.h for
 *        the data format). Exercises every NodeKind once - NODE_TEXT
 *        with a branching choice, NODE_CHECK, an item grant, NODE_COMBAT
 *        with a win/flee branch, NODE_END_WIN and NODE_END_LOSE - so you
 *        can see the whole mechanic working end to end before you start
 *        replacing it with your own story. See examples/nebelkrone/
 *        story.c for what a full ~15-scene adventure built the same way
 *        looks like.
 *
 * The on-screen font only covers ASCII 32..127 (see font8x8.c), so all
 * German text here is written without umlauts/ß (UE/OE/AE/SS) - keep
 * doing that in your own text too.
 */
#include "story.h"

const char *const attr_name[ATTR_COUNT] = { "MUT", "KLUGHEIT", "GEWANDTHEIT" };

const Enemy enemies[ENEMY_COUNT] = {
    [ENEMY_PLATZHALTER] = { "GEGNER", 10, 1, 3, 9 },
};

const Scene scenes[SCENE_COUNT] = {

    [SCENE_INTRO] = {
        .kind = NODE_TEXT,
        .bg = BG_VILLAGE,
        .text = "Dies ist ein Platzhalter-Abenteuer. Ersetze diese "
                "Szene in story.c mit deiner eigenen Geschichte.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .num_choices = 1,
        .choice_text = { "Weiterziehen" },
        .choice_target = { SCENE_CHECK_EXAMPLE },
    },

    [SCENE_CHECK_EXAMPLE] = {
        .kind = NODE_CHECK,
        .bg = BG_SWAMP,
        .text = "Eine Beispiel-Probe: hier koennte eine gefaehrliche "
                "Stelle im Weg liegen.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .check_attr = ATTR_GEWANDTHEIT,
        .check_pass_target = SCENE_CHOICE_EXAMPLE,
        .check_fail_target = SCENE_CHOICE_EXAMPLE,
        .check_fail_damage = 2,
        .check_fail_reason = "Ein Platzhalter-Missgeschick fordert seinen Tribut.",
    },

    [SCENE_CHOICE_EXAMPLE] = {
        .kind = NODE_TEXT,
        .bg = BG_RUIN,
        .text = "Zwei Wege trennen sich - eine Beispiel-Entscheidung.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .num_choices = 2,
        .choice_text = { "Zur Truhe", "Direkt weiter" },
        .choice_target = { SCENE_ITEM_EXAMPLE, SCENE_COMBAT_EXAMPLE },
    },

    [SCENE_ITEM_EXAMPLE] = {
        .kind = NODE_TEXT,
        .bg = BG_RUIN_CHAMBER,
        .text = "Du findest einen Heiltrank - ein Beispiel fuer eine "
                "Gegenstands-Vergabe.",
        .enter_sfx = SFX_PICKUP,
        .grant_flag = FLAG_HEAL_ITEM,
        .penalty_req_flag = -1,
        .num_choices = 1,
        .choice_text = { "Weiter" },
        .choice_target = { SCENE_COMBAT_EXAMPLE },
    },

    [SCENE_COMBAT_EXAMPLE] = {
        .kind = NODE_COMBAT,
        .bg = BG_RUIN_TOP_FIGHT,
        .text = "Ein Platzhalter-Gegner stellt sich dir in den Weg!",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .enemy_id = ENEMY_PLATZHALTER,
        .combat_win_target = SCENE_VICTORY_EXAMPLE,
        .combat_flee_target = SCENE_CHOICE_EXAMPLE,
    },

    [SCENE_VICTORY_EXAMPLE] = {
        .kind = NODE_TEXT,
        .bg = BG_RUIN_TOP_WON,
        .text = "Du hast den Gegner besiegt.",
        .enter_sfx = SFX_LASER,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .num_choices = 1,
        .choice_text = { "Abenteuer beenden" },
        .choice_target = { SCENE_END_WIN },
    },

    [SCENE_END_WIN] = {
        .kind = NODE_END_WIN,
        .bg = BG_VILLAGE,
        .text = "Platzhalter-Sieg. Ersetze diesen Text mit deinem eigenen Ende.",
        .enter_sfx = SFX_LASER,
        .grant_flag = -1,
        .penalty_req_flag = -1,
    },

    [SCENE_END_LOSE] = {
        .kind = NODE_END_LOSE,
        .bg = BG_DEATH,
        .text = "Platzhalter-Niederlage. Ersetze diesen Text mit deinem eigenen Ende.",
        .enter_sfx = SFX_EXPLOSION,
        .grant_flag = -1,
        .penalty_req_flag = -1,
    },
};
