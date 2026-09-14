/**
 * @file story.c
 * @brief Text content for "Der Kobold" (see story.h).
 */
#include "story.h"

const char *const txt_enter_forest =
    "Die Hecken schliessen sich hinter dir. Dieser Wald ist verhext. "
    "Loese das Raetsel oder bleibe fuer immer hier!";

const char *const station_text[STATION_COUNT] = {
    [STATION_WALD] =
        "Dichte Baeume und Moos, so weit das Auge reicht. Der Wald "
        "wirkt in jede Richtung gleich.",
    [STATION_HAUS] =
        "Ein altes Haus steht windschief zwischen den Baeumen. Efeu "
        "waechst durch die Fensterhoehlen. Es wirkt, als waere hier "
        "einmal jemand gluecklich gewesen.",
    [STATION_KOBOLD] =
        "Auf einem bemoosten Stein sitzt ein kleiner Kobold und "
        "beobachtet dich mit schiefem Kopf.",
    [STATION_MOOR] =
        "Der Boden wird weich und nass. Nebel haengt ueber dunklem "
        "Moorwasser. Irgendwo blubbert es leise.",
    [STATION_BACH] =
        "Ein schmaler Bach ploetschert ueber glatte Steine.",
};

const char *const txt_bach_found =
    "Im klaren Wasser glitzert etwas. Es ist ein schmaler Ring aus "
    "dunklem Metall.";

const char *const txt_bach_empty =
    "Der Bach ploetschert leise ueber die Steine. Hier gibt es nichts "
    "mehr zu finden.";

const char *const txt_kobold_story =
    "Der Kobold mustert dich. 'Ich bin eine verzauberte Frau', sagt "
    "er, 'ich wohnte einst in einem schoenen Haus am Moor. Brichst du "
    "meinen Zauber, lebe ich wieder als Frau - und du kannst den Wald "
    "verlassen.'";

const char *const txt_kobold_aufmuntern =
    "Du sprichst ihr Mut zu und erzaehlst von der Welt draussen. Der "
    "Kobold wirkt geruehrt. 'Vielleicht bist du doch anders als die "
    "anderen', murmelt er.";

const char *const txt_win =
    "Der Kobold nimmt den Ring - und verwandelt sich vor deinen Augen "
    "zurueck in eine Frau. 'Danke', sagt sie leise und deutet auf eine "
    "Luecke in den Hecken. 'Dort geht es hinaus.' Du trittst hinaus in "
    "die Freiheit.";

const char *const txt_lose_ring =
    "Kaum steckt der Ring an deinem Finger, spuerst du ein Kribbeln. "
    "Deine Haut wird gruen und rau, deine Gestalt schrumpft. Du bist "
    "nun selbst ein Kobold - gefangen im verwunschenen Wald.";

const char *const txt_lose_angriff =
    "Kaum hebst du die Hand, faucht der Kobold und ein gruener Blitz "
    "trifft dich. Du spuerst, wie der Wald sich fester um dich "
    "schliesst. Von hier gibt es kein Entkommen mehr.";

const char *const txt_lose_sleep =
    "Deine Beine werden schwer. Nach ungezaehlten Schritten durch den "
    "immer gleichen Wald sinkst du erschoepft zu Boden und schlaefst "
    "ein. Der Wald haelt dich fuer immer.";

const char *const txt_hint_path = "HOCH: WEITERGEHEN  RUNTER: ZURUECK";
const char *const txt_hint_forest = "PFEILTASTEN: GEHEN   KNOPF: UMSEHEN";
