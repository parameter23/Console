/**
 * @file story.c
 * @brief Text content for "Der Kobold" (see story.h).
 */
#include "story.h"

const char *const txt_enter_forest =
    "Die Hecken schliessen sich hinter dir. Dieser Wald ist verhext. "
    "Löse das Rätsel oder bleibe für immer hier!";

const char *const station_text[STATION_COUNT] = {
    [STATION_WALD] =
        "Dichte Bäume und Moos, so weit das Auge reicht. Der Wald "
        "wirkt in jede Richtung gleich.",
    [STATION_HAUS] =
        "Ein altes Haus steht windschief zwischen den Bäumen. Efeu "
        "wächst durch die Fensterhöhlen. Es wirkt, als wäre hier "
        "einmal jemand glücklich gewesen.",
    [STATION_KOBOLD] =
        "Auf einem bemoosten Stein sitzt ein kleiner Kobold und "
        "beobachtet dich mit schiefem Kopf.",
    [STATION_MOOR] =
        "Der Boden wird weich und nass. Nebel hängt über dunklem "
        "Moorwasser. Irgendwo blubbert es leise.",
    [STATION_BACH] =
        "Ein schmaler Bach plätschert über glatte Steine.",
};

const char *const txt_bach_found =
    "Im klaren Wasser glitzert etwas. Es ist ein schmaler Ring aus "
    "dunklem Metall.";

const char *const txt_bach_empty =
    "Der Bach plätschert leise über die Steine. Hier gibt es nichts "
    "mehr zu finden.";

const char *const txt_kobold_story =
    "Der Kobold mustert dich. 'Ich bin eine verzauberte Frau', sagt "
    "er, 'ich wohnte einst in einem schönen Haus am Moor. Brichst du "
    "meinen Zauber, lebe ich wieder als Frau - und du kannst den Wald "
    "verlassen.'";

const char *const txt_kobold_aufmuntern =
    "Du sprichst ihr Mut zu und erzählst von der Welt draussen. Der "
    "Kobold wirkt gerührt. 'Vielleicht bist du doch anders als die "
    "anderen', murmelt er.";

const char *const txt_win =
    "Der Kobold nimmt den Ring - und verwandelt sich vor deinen Augen "
    "zurück in eine Frau. 'Danke', sagt sie leise und deutet auf eine "
    "Lücke in den Hecken. 'Dort geht es hinaus.' Du trittst hinaus in "
    "die Freiheit.";

const char *const txt_lose_ring =
    "Kaum steckt der Ring an deinem Finger, spürst du ein Kribbeln. "
    "Deine Haut wird grün und rau, deine Gestalt schrumpft. Du bist "
    "nun selbst ein Kobold - gefangen im verwunschenen Wald.";

const char *const txt_lose_angriff =
    "Kaum hebst du die Hand, faucht der Kobold und ein grüner Blitz "
    "trifft dich. Du spürst, wie der Wald sich fester um dich "
    "schliesst. Von hier gibt es kein Entkommen mehr.";

const char *const txt_lose_sleep =
    "Deine Beine werden schwer. Nach ungezählten Schritten durch den "
    "immer gleichen Wald sinkst du erschöpft zu Boden und schläfst "
    "ein. Der Wald hält dich für immer.";

const char *const txt_hint_path = "HOCH: WEITERGEHEN  RUNTER: ZURÜCK";
const char *const txt_hint_forest = "PFEILTASTEN: GEHEN   KNOPF: UMSEHEN";
