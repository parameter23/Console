/**
 * @file story.c
 * @brief Content for Die Nebelkrone (see story.h for the data format).
 *
 * The on-screen font (see font8x8.c) covers full Code Page 850, and
 * draw_text() decodes UTF-8, so German umlauts/ß can be written directly.
 */
#include "story.h"

const char *const attr_name[ATTR_COUNT] = { "MUT", "KLUGHEIT", "GEWANDTHEIT" };

const Enemy enemies[ENEMY_COUNT] = {
    [ENEMY_MOORSCHRAT] = { "MOORSCHRAT", 12, 1, 4, 10 },
};

const Scene scenes[SCENE_COUNT] = {

    [SCENE_INTRO] = {
        .kind = NODE_TEXT,
        .bg = BG_VILLAGE,
        .text = "Nebel wälzt sich durch die Gassen von Aschwald. Die "
                "Ernte ist verfault, seit man die Nebelkrone aus der "
                "Dorfmitte stahl. Die Leute wirken ängstlich und "
                "hüten sich, hinauszugehen.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .num_choices = 2,
        .choice_text = { "Zum Dorfältesten gehen", "Sofort in den Sumpf aufbrechen" },
        .choice_target = { SCENE_ELDER, SCENE_CHECK_MARSH },
    },

    [SCENE_ELDER] = {
        .kind = NODE_TEXT,
        .bg = BG_VILLAGE,
        .text = "Der Älteste drückt dir eine Fackel in die Hand. "
                "\"Ein Moorschrat hat die Krone gestohlen und sich in "
                "den alten Wachturm im Sumpf zurückgezogen. Nimm die "
                "Fackel - im Turm ist es finster.\"",
        .enter_sfx = SFX_PICKUP,
        .grant_flag = FLAG_TORCH,
        .penalty_req_flag = -1,
        .num_choices = 1,
        .choice_text = { "Aufbrechen" },
        .choice_target = { SCENE_CHECK_MARSH },
    },

    [SCENE_CHECK_MARSH] = {
        .kind = NODE_CHECK,
        .bg = BG_SWAMP,
        .text = "Der Pfad verliert sich im Morast. Jeder Schritt muss "
                "sitzen, sonst versinkst du im kalten Schlamm.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .check_attr = ATTR_GEWANDTHEIT,
        .check_pass_target = SCENE_TOWER_BASE,
        .check_fail_target = SCENE_TOWER_BASE,
        .check_fail_damage = 2,
        .check_fail_reason = "Der Sumpf verschlingt dich mit einem letzten, kalten Sog.",
    },

    [SCENE_TOWER_BASE] = {
        .kind = NODE_TEXT,
        .bg = BG_RUIN,
        .text = "Vor dir ragt der eingestürzte Wachturm aus dem Nebel. "
                "Die Haupttür ist verkeilt, doch seitlich im Gemäuer "
                "erahnst du einen schmalen Spalt.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .num_choices = 2,
        .choice_text = { "Die Tür aufbrechen", "Den Spalt untersuchen" },
        .choice_target = { SCENE_CHECK_DOOR, SCENE_CHECK_SEARCH },
    },

    [SCENE_CHECK_DOOR] = {
        .kind = NODE_CHECK,
        .bg = BG_RUIN,
        .text = "Du stemmst dich mit aller Kraft gegen die morsche Tür.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .check_attr = ATTR_MUT,
        .check_pass_target = SCENE_TOWER_INSIDE,
        .check_fail_target = SCENE_TOWER_INSIDE,
        .check_fail_damage = 3,
        .check_fail_reason = "Splitter der brechenden Tür reissen dir eine tiefe Wunde.",
    },

    [SCENE_CHECK_SEARCH] = {
        .kind = NODE_CHECK,
        .bg = BG_RUIN,
        .text = "Du folgst dem Spalt mit den Fingerspitzen und suchst "
                "nach einem verborgenen Zugang.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .check_attr = ATTR_KLUGHEIT,
        .check_pass_target = SCENE_TOWER_UPPER,
        .check_fail_target = SCENE_TOWER_INSIDE,
        .check_fail_damage = 0,
        .check_fail_reason = 0,
    },

    [SCENE_TOWER_INSIDE] = {
        .kind = NODE_TEXT,
        .bg = BG_RUIN_INSIDE,
        .text = "Drinnen riecht es nach nassem Stein und Moder. Ein "
                "Gang führt tiefer hinein, der Boden aus morschen "
                "Brettern knarrt bei jedem Schritt.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = FLAG_TORCH,
        .penalty_damage = 2,
        .penalty_reason = "Im stockfinsteren Gang stolperst du schmerzhaft gegen die Wand.",
        .num_choices = 1,
        .choice_text = { "Vorsichtig weitergehen" },
        .choice_target = { SCENE_CHECK_FLOORBOARDS },
    },

    [SCENE_CHECK_FLOORBOARDS] = {
        .kind = NODE_CHECK,
        .bg = BG_RUIN_INSIDE,
        .text = "Du testest jedes Brett, bevor du dein Gewicht darauf verlagerst.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .check_attr = ATTR_GEWANDTHEIT,
        .check_pass_target = SCENE_TOWER_UPPER,
        .check_fail_target = SCENE_TOWER_UPPER,
        .check_fail_damage = 4,
        .check_fail_reason = "Die morschen Bretter brechen, du stürzt hart auf den Steinboden darunter.",
    },

    [SCENE_TOWER_UPPER] = {
        .kind = NODE_TEXT,
        .bg = BG_RUIN_CHAMBER,
        .text = "Eine runde Kammer mit einer alten Truhe an der Wand. "
                "Eine steile Treppe führt weiter hinauf, von dort hörst "
                "du ein tiefes Knurren.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .num_choices = 2,
        .choice_text = { "Die Truhe öffnen", "Sofort nach oben gehen" },
        .choice_target = { SCENE_CHEST, SCENE_TOWER_TOP },
    },

    [SCENE_CHEST] = {
        .kind = NODE_TEXT,
        .bg = BG_RUIN_CHAMBER,
        .text = "In der Truhe findest du ein kleines Fläschchen mit "
                "schimmernder Flüssigkeit - ein Heiltrank. Du steckst "
                "ihn ein.",
        .enter_sfx = SFX_PICKUP,
        .grant_flag = FLAG_POTION,
        .penalty_req_flag = -1,
        .num_choices = 1,
        .choice_text = { "Nach oben gehen" },
        .choice_target = { SCENE_TOWER_TOP },
    },

    [SCENE_TOWER_TOP] = {
        .kind = NODE_COMBAT,
        .bg = BG_RUIN_TOP_FIGHT,
        .text = "Oben kauert der Moorschrat über der gestohlenen "
                "Nebelkrone. Er faucht und stürzt sich auf dich!",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .enemy_id = ENEMY_MOORSCHRAT,
        .combat_win_target = SCENE_VICTORY_ROOM,
        .combat_flee_target = SCENE_TOWER_BASE,
    },

    [SCENE_VICTORY_ROOM] = {
        .kind = NODE_TEXT,
        .bg = BG_RUIN_TOP_WON,
        .text = "Der Moorschrat flieht winselnd in die Schatten. Du "
                "greifst nach der Nebelkrone - da beginnt der Turm "
                "bedrohlich zu erzittern!",
        .enter_sfx = SFX_LASER,
        .grant_flag = FLAG_CROWN,
        .penalty_req_flag = -1,
        .num_choices = 1,
        .choice_text = { "Schnell fliehen" },
        .choice_target = { SCENE_CHECK_ESCAPE },
    },

    [SCENE_CHECK_ESCAPE] = {
        .kind = NODE_CHECK,
        .bg = BG_RUIN_TOP_WON,
        .text = "Steine lösen sich aus der Decke. Du hetzt die "
                "einstürzende Treppe hinunter.",
        .enter_sfx = SFX_NONE,
        .grant_flag = -1,
        .penalty_req_flag = -1,
        .check_attr = ATTR_GEWANDTHEIT,
        .check_pass_target = SCENE_END_WIN,
        .check_fail_target = SCENE_END_WIN,
        .check_fail_damage = 5,
        .check_fail_reason = "Ein stürzender Deckenbalken erwischt dich beim Sprung aus dem Turm.",
    },

    [SCENE_END_WIN] = {
        .kind = NODE_END_WIN,
        .bg = BG_VILLAGE,
        .text = "Du erreichst Aschwald mit der Nebelkrone in der Hand. "
                "Der Nebel weicht, die Sonne bricht durch die Wolken. "
                "Du bist der Held, den das Dorf brauchte.",
        .enter_sfx = SFX_LASER,
        .grant_flag = -1,
        .penalty_req_flag = -1,
    },

    [SCENE_END_LOSE] = {
        .kind = NODE_END_LOSE,
        .bg = BG_DEATH,
        .text = "Deine Geschichte endet hier, im Dunkel des Nebels.",
        .enter_sfx = SFX_EXPLOSION,
        .grant_flag = -1,
        .penalty_req_flag = -1,
    },
};
