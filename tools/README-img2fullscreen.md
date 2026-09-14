# Bild -> Vollbild-Anzeige (320x240) Konverter

Wandelt ein oder mehrere Bilder (Fotos, gemalte Illustrationen, egal
welches Seitenverhaltnis) in ein volles 320x240-Bild fur diese Engine
um - ein `sprite16_t`-Array ohne Kachel-Wiederverwendung, anders als
`png2tileset.py` (das ist fur *kachelbare* Pixel-Art mit echten
Wiederholungen gedacht; ein Gemalde hat davon praktisch keine).

## Installation

```bash
python3 -m pip install pillow
```

## Grundnutzung

```bash
# Ohne Argumente: konvertiert jedes Bild im aktuellen Ordner
./img2fullscreen.py

# Bestimmte Dateien, eigenes Ausgabeverzeichnis
./img2fullscreen.py Moor.jpg Markt.jpg -o out/
```

Erzeugt pro Bild ein `.c`/`.h`-Paar (`moor_image.c`/`.h` usw.) mit einem
flachen `sprite16_t`-Array (300 Kacheln bei 320x240) plus den passenden
`extern`-Deklarationen.

## WICHTIG: Flash-Budget

Ein 320x240-Bild sind 320*240 = 76800 Byte (1 Byte/Pixel, keine
Kompression) - rund 75 KB. Der STM32F411 hat insgesamt 512 KB Flash fur
*alles* (Engine, Spielcode, alle Kachel-/Sprite-/Bilddaten zusammen).
Das Skript gibt nach jedem Bild die laufende Summe gegen dieses Budget
aus:

```
Moor.jpg: 320x240 -> moor_image.c (75.0 KB) - running total 75.0 KB / 512 KB flash (15%)
```

**Sieben Bilder zu je 75 KB sind zusammen bereits 525 KB - mehr als das
gesamte Flash, noch bevor ueberhaupt Spielcode dazukommt.** Realistisch
passen so neben echtem Spielcode nur zwei bis drei Vollbilder
gleichzeitig in eine Firmware. Optionen, wenn mehr gebraucht wird:

- Nur die tatsaechlich benoetigte Auswahl an Bildern einbinden.
- Kleinere Zielgroesse (`--width`/`--height`, z. B. 320x80 fuer eine
  Illustration wie in nebelkrone statt des ganzen Bildschirms).
- Statt Vollbild eine kachelbare Illustration mit `png2tileset.py`
  bauen, falls das Motiv genug Wiederholung hat, um von der
  Deduplizierung zu profitieren.
- **`--format raw`** (siehe unten) - Bilder auf den externen
  W25Q128-Flash (16 MB) statt ins interne 512-KB-Flash packen. Das
  Budget-Problem verschwindet praktisch komplett (~200 Vollbilder
  passen dort hinein).

## `--format raw`: Bilder fuer den externen W25Q128-Flash

```bash
./img2fullscreen.py Moor.jpg Markt.jpg --format raw --width 320 --height 80 -o out/
```

Erzeugt statt eines `.c`/`.h`-Paars pro Bild eine flache `.bin`-Datei:
genau `width*height` Byte, ein Byte pro Pixel (Palettenindex 0-15),
zeilenweise - exakt das Speicherlayout von `framebuffer8[]`
(`include/framebuffer8.h`), also direkt per `w25q_read()` in den
Framebuffer ladbar, ohne Entpacken. Kein C-Code, keine Kachelaufteilung.

Das Skript gibt pro Bild auch gleich eine Flash-Adresse aus - die
Bilder werden der Eingabereihenfolge nach in aufeinanderfolgende Slots
gelegt, jeder Slot auf die naechste 4-KB-Sektorgrenze aufgerundet
(passend zu `w25q_erase_sector()`, das immer einen ganzen 4-KB-Sektor
loescht). Diese Adressen sind die, die der Semihosting-Uploader
(`examples/flash-uploader/`) fuer den jeweiligen Slot braucht.

Wie diese `.bin`-Dateien tatsaechlich auf den Flash-Chip kommen -
siehe `examples/flash-uploader/README.md`.

## Zuschnitt

Quellbilder haben selten schon 320x240 (4:3). `--fit` steuert die
Anpassung:

| `--fit` | Verhalten |
|---|---|
| `cover` (Standard) | Auf 4:3 zuschneiden (mittig), dann skalieren - fuellt den Schirm, kein Verzerren, aber Bildrand geht verloren |
| `contain` | Ganzes Bild einpassen, notfalls schwarze Balken - nichts geht verloren, aber nicht bildschirmfuellend |
| `stretch` | Exakt auf Zielgroesse strecken - kein Verlust, aber verzerrte Proportionen |

## Optionen

| Option | Bedeutung |
|---|---|
| `-o, --outdir PATH` | Ausgabeverzeichnis (Default: aktueller Ordner) |
| `--width N` / `--height N` | Zielgroesse in Pixeln, Vielfache von 16 (Default: 320x240) |
| `--fit cover\|contain\|stretch` | Zuschnitt-/Skalierungsverhalten (siehe oben) |
| `--palette-from PATH` | Palette aus einer echten `framebuffer8.c` lesen statt der eingebauten Kopie |
| `--no-dither` | Reine Naechste-Farbe-Quantisierung statt Floyd-Steinberg-Dithering |
| `--name-prefix` / `--name-suffix` | Praefix/Suffix fuer die generierten C-Bezeichner (Default-Suffix: `_image`) |

**Dithering ist hier standardmaessig an** (bei `png2tileset.py` ist es
standardmaessig *aus*) - der umgekehrte Kompromiss, weil gemalte/
fotografische Bilder mit Farbverlaeufen durch Dithering deutlich besser
aussehen, waehrend flaechige Pixel-Art meist ohne sauberer wirkt. Bei
kontrastreichen/grafischen Motiven kann `--no-dither` trotzdem das
bessere Ergebnis liefern - beides an einem Testbild vergleichen.

## Farbpalette

Seit `fb8_init_palette()` (=src/framebuffer8.c=) alle 256 statt nur 16
Paletteneintraege befuellt, quantisiert dieses Skript standardmaessig
gegen alle 256: die 16 benannten C64-Farben plus einen 6x6x6-RGB-Wuerfel
(216 Farben) und eine 24-stufige Graurampe (24 Farben) - siehe
`DEFAULT_PALETTE_RGB565` im Skript, die exakt mit `fb8_palette[]`
uebereinstimmt (per Hand synchron gehalten; `--palette-from` liest
stattdessen `base_palette[]` *und* `ext_palette[]` direkt aus einer
echten `framebuffer8.c`). Damit sehen die erzeugten Bilder auf echter
Hardware jetzt deutlich weniger bandingartig/grobkoernig aus als mit der
alten 16-Farb-Quantisierung.

## Ergebnis zeichnen

```c
#include "moor_image.h"

static void draw_fullscreen(const sprite16_t *img, int cols)
{
    for (int row = 0; row < MOOR_IMAGE_ROWS; row++)
        for (int col = 0; col < cols; col++)
            draw_sprite16(col * 16, row * 16, &img[row * cols + col]);
}

/* ... */
draw_fullscreen(moor_image, MOOR_IMAGE_COLS);
```

Bei 320x240 fuellt das den kompletten Bildschirm; `draw()` bekommt vom
Rest der Engine ohnehin ein frisch geleertes Framebuffer, ein Aufruf pro
Frame reicht.
