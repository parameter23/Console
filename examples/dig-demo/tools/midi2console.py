#!/usr/bin/env python3
"""
midi2console.py
Convert a MIDI file into a C/C++ MusicNote array like:

const MusicNote boulder_track[] = {
    { 220, 400 }, { 0, 100 },
    ...
    { 0, 0 }
};

Requires:
    python3 -m pip install mido

Usage:
    ./midi2console.py song.mid -o song.h --name boulder_track
    ./midi2console.py song.mid --name my_track --min-duration 20
"""

import argparse
import math
import sys
from pathlib import Path

try:
    import mido
except ImportError:
    print("Fehler: Das Paket 'mido' fehlt.", file=sys.stderr)
    print("Installieren mit: python3 -m pip install mido", file=sys.stderr)
    sys.exit(1)


def midi_note_to_hz(note: int, tuning: float = 440.0) -> int:
    """MIDI note -> nearest integer frequency in Hz."""
    return round(tuning * (2.0 ** ((note - 69) / 12.0)))


def ticks_to_ms(ticks: int, ticks_per_beat: int, tempo_us_per_beat: int) -> float:
    return ticks * tempo_us_per_beat / ticks_per_beat / 1000.0


def global_tempo_segments(mid: "mido.MidiFile"):
    """Tempo map built from ALL tracks merged, not just the one being
    converted. Standard Format-1 MIDI files conventionally store tempo
    (set_tempo) meta events only on track 0 (the "conductor" track);
    the individual instrument tracks usually carry none of their own.
    Building the tempo map from a single non-zero track would then
    silently fall back to the MIDI default of 120 BPM for its entire
    duration, throwing off every absolute ms timestamp whenever the
    song's real tempo differs (as it did for death_waltz.mid's Bass
    Guitar track: extracted at the wrong 120 BPM default rather than
    the song's actual 100 BPM, compressing its total duration by 20%
    relative to a track that happened to carry the tempo event itself)."""
    segments = [(0, 500000)]  # MIDI default: 120 BPM
    tick = 0
    for msg in mido.merge_tracks(mid.tracks):
        tick += msg.time
        if msg.type == "set_tempo":
            segments.append((tick, msg.tempo))
    return segments


def merge_adjacent(notes):
    """Merge adjacent identical entries."""
    out = []
    for freq, dur in notes:
        if dur <= 0:
            continue
        if out and out[-1][0] == freq:
            out[-1] = (freq, out[-1][1] + dur)
        else:
            out.append((freq, dur))
    return out


def quantize_ms(value: float, quantum: int) -> int:
    if quantum <= 1:
        return max(0, round(value))
    return max(0, round(value / quantum) * quantum)


def midi_to_notes(
    filename: str,
    track_index: int,
    tuning: float,
    min_duration: int,
    quantize: int,
):
    mid = mido.MidiFile(filename)

    if not mid.tracks:
        raise ValueError("Die MIDI-Datei enthält keine Tracks.")

    if track_index < 0 or track_index >= len(mid.tracks):
        raise ValueError(
            f"Track {track_index} existiert nicht. "
            f"Vorhandene Tracks: 0..{len(mid.tracks)-1}"
        )

    track = mid.tracks[track_index]
    tempo = 500000  # MIDI default: 120 BPM
    active = {}
    events = []
    absolute_tick = 0

    # We intentionally handle one active note per MIDI pitch.
    # For a simple console this is usually preferable to chords.
    for msg in track:
        absolute_tick += msg.time

        if msg.type == "set_tempo":
            tempo = msg.tempo
            continue

        if msg.type == "note_on" and msg.velocity > 0:
            active[msg.note] = absolute_tick

        elif msg.type in ("note_off", "note_on") and (
            msg.type == "note_off" or msg.velocity == 0
        ):
            start = active.pop(msg.note, None)
            if start is not None:
                events.append((start, absolute_tick, msg.note))

    if not events:
        raise ValueError("Im gewählten Track wurden keine MIDI-Noten gefunden.")

    # MIDI tempo can change. For accurate timing, build tempo segments
    # from the whole file, not just this track - see
    # global_tempo_segments()'s docstring for why.
    tempo_segments = global_tempo_segments(mid)

    def tick_to_ms(t):
        total = 0.0
        for i, (start, current_tempo) in enumerate(tempo_segments):
            end = tempo_segments[i + 1][0] if i + 1 < len(tempo_segments) else t
            if t <= start:
                break
            segment_end = min(t, end)
            if segment_end > start:
                total += ticks_to_ms(
                    segment_end - start, mid.ticks_per_beat, current_tempo
                )
            if t <= end:
                break
        return total

    # Sort by start time and make a monophonic melody.
    # If notes overlap, the earliest-starting note wins; later overlapping
    # notes are shortened/skipped because MusicNote has no polyphony.
    events.sort(key=lambda x: (x[0], x[1], x[2]))
    result = []
    cursor_ms = 0

    for start_tick, end_tick, note in events:
        start_ms = tick_to_ms(start_tick)
        end_ms = tick_to_ms(end_tick)

        if end_ms <= start_ms:
            continue

        # Silence before the note.
        if start_ms > cursor_ms:
            rest = quantize_ms(start_ms - cursor_ms, quantize)
            if rest >= min_duration:
                result.append((0, rest))
            cursor_ms = start_ms

        # Skip overlap with the previous note.
        actual_start = max(start_ms, cursor_ms)
        duration = end_ms - actual_start

        duration_ms = quantize_ms(duration, quantize)
        if duration_ms >= min_duration:
            result.append((midi_note_to_hz(note, tuning), duration_ms))
            cursor_ms = max(cursor_ms, end_ms)

    result = merge_adjacent(result)
    return result


def format_c_array(notes, name: str, guard_name: str):
    lines = []
    lines.append(f"// Generated by midi2console.py")
    lines.append(f"#ifndef {guard_name}")
    lines.append(f"#define {guard_name}")
    lines.append("")
    lines.append(f"const MusicNote {name}[] = {{")

    # 4 entries per line, matching the compact style of the example.
    for i in range(0, len(notes), 4):
        chunk = notes[i:i + 4]
        text = ", ".join(f"{{ {freq}, {dur} }}" for freq, dur in chunk)
        lines.append(f"    {text},")

    lines.append("    { 0, 0 }")
    lines.append("};")
    lines.append("")
    lines.append(f"const unsigned int {name}_length = sizeof({name}) / sizeof({name}[0]);")
    lines.append("")
    lines.append(f"#endif // {guard_name}")
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="MIDI -> MusicNote[] C/C++ Konverter"
    )
    parser.add_argument("midi", help="Eingabe-MIDI-Datei (.mid/.midi)")
    parser.add_argument(
        "-o", "--output",
        help="Ausgabedatei, Standard: stdout"
    )
    parser.add_argument(
        "--name", default="midi_track",
        help="Name des C-Arrays (Default: midi_track)"
    )
    parser.add_argument(
        "--track", type=int, default=0,
        help="MIDI-Track (0-basiert, Default: 0)"
    )
    parser.add_argument(
        "--tuning", type=float, default=440.0,
        help="Kammerton A4 in Hz (Default: 440)"
    )
    parser.add_argument(
        "--min-duration", type=int, default=1,
        help="Kleinste Ausgabe-Dauer in ms (Default: 1)"
    )
    parser.add_argument(
        "--quantize", type=int, default=1,
        help="Dauern auf dieses ms-Raster runden (Default: 1)"
    )
    parser.add_argument(
        "--list-tracks", action="store_true",
        help="Tracks anzeigen und beenden"
    )

    args = parser.parse_args()

    mid = mido.MidiFile(args.midi)

    if args.list_tracks:
        for i, track in enumerate(mid.tracks):
            print(f"{i}: {track.name!r} ({len(track)} MIDI-Events)")
        return

    notes = midi_to_notes(
        args.midi,
        args.track,
        args.tuning,
        args.min_duration,
        args.quantize,
    )

    guard = "".join(c if c.isalnum() else "_" for c in args.name.upper()) + "_H"
    output = format_c_array(notes, args.name, guard)

    if args.output:
        Path(args.output).write_text(output, encoding="utf-8")
    else:
        sys.stdout.write(output)


if __name__ == "__main__":
    main()
