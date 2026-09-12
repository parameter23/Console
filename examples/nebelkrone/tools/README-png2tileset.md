# PNG → sprite16_t-Kachelraster Konverter

## Installation

```bash
python3 -m pip install pillow
```

## Grundnutzung

```bash
./png2tileset.py szene.png -o szene.c
```

Erzeugt `szene.c` (ein `sprite16_t`-Kachel-Array + ein Index-Raster
darauf) und `szene.h` (die passenden `extern`-Deklarationen).

## Anforderungen an das Quellbild

- **Größe**: Breite und Höhe müssen ein Vielfaches von 16 Pixel sein -
  `draw_sprite16()` kennt nur genau 16x16-Kacheln. Mit `--pad` wird
  stattdessen automatisch mit transparenten Pixeln bis zum nächsten
  Vielfachen aufgefüllt, statt einen Fehler zu melden.
- **Farben**: werden auf die 16 Engine-Farben reduziert (jedes Pixel auf
  die nächstliegende davon) - siehe `--list-palette` für die aktuelle
  Liste. Das ist die gleiche 16-Farb-Palette, die `fb8_init_palette()`
  in `src/framebuffer8.c` lädt; mit `--palette-from` liest das Skript
  sie direkt aus einer gegebenen `framebuffer8.c`, statt der im Skript
  eingebauten Kopie zu vertrauen (praktisch, falls sich die Engine-Palette
  mal wieder ändert).
- **Transparenz**: Pixel mit Alphakanal < 128 werden zu Index 0
  (transparent, wie bei jedem handgezeichneten Sprite in diesem
  Projekt). Hat die Quelle keinen Alphakanal, markiert `--chroma-key
  RRGGBB` stattdessen eine bestimmte Farbe als transparent (klassische
  Greenscreen/Magenta-Technik).

## Optionen

| Option | Bedeutung |
|---|---|
| `-o, --output PATH` | Ziel-`.c`-Datei (Pflicht, außer bei `--list-palette`) |
| `--header PATH` | Eigener Pfad für die `.h`-Datei (Default: `.c` → `.h`) |
| `--no-header` | Keine `.h`-Datei schreiben |
| `--tileset-name NAME` | C-Bezeichner für das Kachel-Array (Default: aus dem Ausgabedateinamen) |
| `--grid-name NAME` | C-Bezeichner für das Index-Raster (Default: `<tileset-name ohne _tiles>_grid`) |
| `--palette-from PATH` | Palette aus einer `framebuffer8.c` statt der eingebauten Kopie lesen |
| `--chroma-key RRGGBB` | Diese Farbe als transparent behandeln (ohne Alphakanal) |
| `--alpha-threshold N` | Alpha-Schwelle für Transparenz, 0..255 (Default: 128) |
| `--pad` | Auf das nächste 16px-Vielfache auffüllen statt Fehler zu melden |
| `--dither` | Floyd-Steinberg-Dithering vor der Quantisierung |
| `--list-palette` | Nur die 16 Palettenfarben ausgeben und beenden |

## Beispiele

```bash
# Standard: Kacheln + Raster erzeugen
./png2tileset.py szene.png -o szene.c

# Mit der tatsächlich aktuellen Engine-Palette statt der eingebauten Kopie
./png2tileset.py szene.png -o szene.c --palette-from ../../../src/framebuffer8.c

# Sprite mit Magenta als Transparenzfarbe (kein Alphakanal im Quellbild)
./png2tileset.py held.png -o held.c --chroma-key FF00FF

# Foto/Verlauf: dithern und automatisch auf 16px auffüllen
./png2tileset.py foto.png -o foto.c --dither --pad

# Nur mal schauen, welche 16 Farben überhaupt zur Verfügung stehen
./png2tileset.py --list-palette
```

## Ergebnis einbinden

Das Skript kennt Nebelkrones `art.c`/`story.c`-Struktur nicht - es
erzeugt nur ein eigenständiges Kachel-Array + Index-Raster, unabhängig
vom bestehenden `tileset16[]`. Einfachster Weg, es tatsächlich zu
zeichnen: eine kleine eigene Funktion, analog zu `draw_scene_bg()` in
`art.c`:

```c
#include "szene.h"

void draw_szene(void)
{
    for (int row = 0; row < SZENE_GRID_ROWS; row++)
        for (int col = 0; col < SZENE_GRID_COLS; col++)
            draw_sprite16(col * 16, row * 16,
                          &szene_tiles[szene_grid[row][col]]);
}
```

und die an der gewünschten Stelle aufrufen (z. B. anstelle von oder vor
`draw_scene_bg()` in `main.c`s `nebel_draw()`). Ein 320x80-Bild (wie
Nebelkrones Illustrationsbereich) zeichnet sich so an Position (0,0);
für einen anderen Bereich/eine andere Größe einfach `col*16`/`row*16`
um einen Offset verschieben oder das Quellbild entsprechend zuschneiden.

Das Ergebnis stattdessen als neuen `bg_id_t`-Eintrag direkt in
`art.c`/`art.h` einzuhängen geht auch, ist aber mehr Handarbeit: das
erzeugte Kachel-Array müsste mit dem bestehenden `tileset16[]`
zusammengeführt (oder `draw_scene_bg()` so erweitert werden, dass es
zwischen mehreren Tilesets unterscheiden kann) und die Indizes im
Raster entsprechend verschoben werden.

## Zur Bildqualität

- Für handgezeichnete Pixel-Art (flächige Farben, keine Verläufe)
  liefert die einfache Nächste-Farbe-Quantisierung (Standard, ohne
  `--dither`) die saubersten, vorhersehbarsten Ergebnisse.
- Für Fotos oder Farbverläufe kann `--dither` durch simulierte
  Zwischenfarben helfen - macht aus glatten Flächen aber ein Rauschen
  einzelner Pixel. Am besten an einem Testbild beide Varianten
  vergleichen.
- 16 Farben sind wenig - ein Foto wird immer grob/posterisiert wirken,
  unabhängig von Dithering. Für mehr Farbtreue müsste die Engine
  zusätzliche Paletteneinträge (`fb8_palette[16..255]`, aktuell
  ungenutzt) laden können; das kann dieses Skript (noch) nicht.
