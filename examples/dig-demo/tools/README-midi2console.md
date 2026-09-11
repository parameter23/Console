# MIDI → MusicNote[] Konverter

## Installation

```bash
python3 -m pip install mido
```

## Tracks anzeigen

```bash
./midi2console.py song.mid --list-tracks
```

## Konvertieren

```bash
./midi2console.py song.mid -o boulder_track.h --name boulder_track
```

Optional:

```bash
# Track 2 verwenden
./midi2console.py song.mid --track 2 -o boulder_track.h --name boulder_track

# Dauern auf 10 ms runden
./midi2console.py song.mid --quantize 10 -o boulder_track.h --name boulder_track

# Sehr kurze Noten/Reste ignorieren
./midi2console.py song.mid --min-duration 20 -o boulder_track.h --name boulder_track

# Anderen Kammerton verwenden
./midi2console.py song.mid --tuning 432 -o boulder_track.h --name boulder_track
```

Das Tool erzeugt Frequenzen in Hz und Dauern in Millisekunden. MIDI-Noten werden
mit A4=440 Hz umgerechnet. Da `MusicNote` nur eine Frequenz und eine Dauer
enthält, wird ein Track als monophone Melodie ausgegeben; gleichzeitig klingende
Noten können daher nicht als Akkord dargestellt werden.
