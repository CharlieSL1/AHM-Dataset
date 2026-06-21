#!/usr/bin/env python3
"""
chord_to_csound.py
Convert AHM-Dataset chord progressions to Csound unified (.csd) files.

Outputs:
  Csound/
    jazz/        — one .csd per (Key, Chord_Progression, Voicing) — 816 files
    pop/         — one .csd per (Key, Chord_Progression) — 445 files
    harmony.orc  — standalone orchestra reference

Usage:
    python chord_to_csound.py [--bpm 120] [--beats-per-chord 1.0] [--octave 4]
    # Open any .csd in CsoundQt and press Run, or:
    csound Csound/jazz/Bb_V7-sharpIVm7b5-IVm7-IIIm7_FourWayClose.csd
"""

import re
import argparse
import pandas as pd
from pathlib import Path

# ── MIDI note mapping ─────────────────────────────────────────────────────────
NOTE_MIDI = {
    'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3,
    'E': 4, 'Fb': 4, 'F': 5, 'F#': 6, 'Gb': 6, 'G': 7, 'G#': 8,
    'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11, 'Cb': 11,
}
MIDI_NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']

# ── Chord quality → semitone intervals ───────────────────────────────────────
CHORD_INTERVALS = {
    '':     [0, 4, 7],        # major triad
    'm':    [0, 3, 7],        # minor triad
    '7':    [0, 4, 7, 10],   # dominant 7th
    'maj7': [0, 4, 7, 11],   # major 7th
    'm7':   [0, 3, 7, 10],   # minor 7th
    'm6':   [0, 3, 7, 9],    # minor 6th
    'm7b5': [0, 3, 6, 10],   # half-diminished (ø7)
    'dim':  [0, 3, 6],        # diminished triad
    'sus':  [0, 5, 7],        # suspended 4th (sus4)
}

ROOT_RE = re.compile(r'^([A-G][#b]?)(.*)')


def chord_to_midi(name: str, octave: int = 4) -> list[int]:
    """Parse a chord name (e.g. 'Fm7b5') into MIDI note numbers (close position)."""
    m = ROOT_RE.match(name.strip())
    if not m:
        raise ValueError(f"Cannot parse chord: {name!r}")
    root_str, quality = m.group(1), m.group(2)
    if root_str not in NOTE_MIDI:
        raise ValueError(f"Unknown root note: {root_str!r}")
    intervals = CHORD_INTERVALS.get(quality)
    if intervals is None:
        raise ValueError(f"Unknown chord quality {quality!r} in {name!r}")
    root_midi = 12 * (octave + 1) + NOTE_MIDI[root_str]
    return [root_midi + i for i in intervals]


def apply_voicing(notes: list[int], voicing: str | None) -> list[int]:
    """Apply a jazz voicing transformation to a close-position chord.

    Four-Way Close — no change (already default close position)
    Drop 2         — second-highest note drops an octave
    Drop 2+4       — second and fourth-highest notes each drop an octave
    Drop 3         — third-highest note drops an octave
    """
    notes = sorted(notes)
    if voicing == 'Drop 2' and len(notes) >= 3:
        notes[-2] -= 12
    elif voicing == 'Drop 2+4' and len(notes) >= 4:
        notes[-2] -= 12
        notes[-4] -= 12
    elif voicing == 'Drop 3' and len(notes) >= 3:
        notes[-3] -= 12
    # 'Four-Way Close' = close position (default, no change)
    return sorted(notes)


def midi_note_name(n: int) -> str:
    return f"{MIDI_NAMES[n % 12]}{n // 12 - 1}"


def progression_to_sco(
    chords: list[str],
    voicings: list[str | None] | None = None,
    beats_per_chord: float = 1.0,
    bpm: int = 120,
    instr: int = 1,
    velocity: float = 0.7,
    octave: int = 4,
    metadata: dict | None = None,
) -> str:
    """Return a Csound score (.sco) string for a chord progression."""
    voicings = voicings or [None] * len(chords)
    lines = []

    if metadata:
        for k, v in metadata.items():
            lines.append(f"; {k}: {v}")
        lines.append("")

    lines.append(f"t 0 {bpm}")
    lines.append("")

    beat = 0.0
    for chord_name, voi in zip(chords, voicings):
        notes = chord_to_midi(chord_name, octave)
        notes = apply_voicing(notes, voi)
        label = chord_name + (f" [{voi}]" if voi else "")
        lines.append(f"; beat {beat:.2f} : {label}")
        for n in sorted(notes):
            lines.append(
                f"i{instr}  {beat:.3f}  {beats_per_chord:.3f}  {n}  {velocity:.2f}"
                f"  ; {midi_note_name(n)}"
            )
        beat += beats_per_chord
        lines.append("")

    lines.append("e")
    return "\n".join(lines)


def safe_stem(key: str, progression: str, voicing: str | None = None) -> str:
    """Convert (key, progression[, voicing]) to a filesystem-safe filename stem."""
    key  = key.replace('#', 'sharp').replace(' ', '_')
    prog = progression.replace('#', 'sharp').replace('/', '_').replace(' ', '_')
    stem = f"{key}_{prog}"
    if voicing:
        v = voicing.replace(' ', '').replace('+', 'and')
        stem = f"{stem}_{v}"
    return stem


# Instrument body shared by both .orc and .csd outputs
INSTRUMENTS = """\
; p4 = MIDI note number (60 = C4)   p5 = amplitude 0-1

sr     = 44100
ksmps  = 32
nchnls = 2
0dbfs  = 1

gitab  ftgen 0, 0, 4096, 10, 1   ; sine wave table (orchestra-level, Csound 6+/7)

; Instrument 1: FM piano-like synthesis
instr 1
  ifreq  cpsmidinn p4
  iamp   = p5 * 0dbfs

  ; Amplitude envelope: sharp attack, exponential decay
  aenv   expseg 1, p3 * 0.01, 0.5, p3 * 0.89, 0.001, p3 * 0.1, 0.001

  ; FM synthesis — carrier:modulator ratio 1:2, mod index 2.5
  asig   foscili aenv * iamp, ifreq, 1, 2, 2.5, gitab

         outs asig, asig
endin
"""

ORCHESTRA = (
    "; harmony.orc — AHM-Dataset companion orchestra\n"
    "; Requires Csound 6+\n"
    "; Usage: csound -o output.wav -W harmony.orc <score>.sco\n\n"
    + INSTRUMENTS
)


def make_csd(sco_text: str) -> str:
    """Wrap orchestra + score into a self-contained CsoundSynthesizer (.csd) file."""
    return (
        "<CsoundSynthesizer>\n"
        "<CsOptions>\n"
        "-o output.wav -W\n"
        "</CsOptions>\n"
        "<CsInstruments>\n"
        + INSTRUMENTS +
        "</CsInstruments>\n"
        "<CsScore>\n"
        + sco_text + "\n"
        "</CsScore>\n"
        "</CsoundSynthesizer>\n"
    )


def write_orchestra(output_dir: Path) -> None:
    (output_dir / "harmony.orc").write_text(ORCHESTRA)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert AHM-Dataset chord progressions to Csound scores"
    )
    parser.add_argument("--bpm",             type=int,   default=120,  help="Tempo in BPM")
    parser.add_argument("--beats-per-chord", type=float, default=1.0,  help="Duration per chord (beats)")
    parser.add_argument("--octave",          type=int,   default=4,    help="Root octave for close voicing")
    parser.add_argument("--velocity",        type=float, default=0.7,  help="Note amplitude 0–1")
    args = parser.parse_args()

    ROOT   = Path(__file__).parent
    jazz   = pd.read_csv(ROOT / "jazz_harmony_ml_dataset.csv")
    pop    = pd.read_csv(ROOT / "pop_harmony_dataset.csv")
    pop["Voicing"] = None

    out_dir  = ROOT / "Csound"
    jazz_dir = out_dir / "jazz"
    pop_dir  = out_dir / "pop"
    jazz_dir.mkdir(parents=True, exist_ok=True)
    pop_dir.mkdir(parents=True, exist_ok=True)

    errors: list[str] = []

    # Jazz: one file per (Key, Chord_Progression, Voicing) — captures all 4 voicing variants
    seen_jazz: set[tuple[str, str, str]] = set()
    for _, row in jazz.iterrows():
        key     = str(row["Key"])
        prog    = str(row["Chord_Progression"])
        voicing = str(row["Voicing"]) if pd.notna(row["Voicing"]) else ""
        triple  = (key, prog, voicing)
        if triple in seen_jazz:
            continue
        seen_jazz.add(triple)

        chords   = str(row["ChordName"]).split("-")
        voicings = [voicing] * len(chords) if voicing else None
        meta = {
            "Genre":             "jazz",
            "Key":               key,
            "Chord_Progression": prog,
            "ChordName":         row["ChordName"],
            "Voicing":           voicing or "none",
        }
        try:
            sco = progression_to_sco(
                chords, voicings,
                beats_per_chord=args.beats_per_chord,
                bpm=args.bpm, octave=args.octave,
                velocity=args.velocity, metadata=meta,
            )
        except ValueError as e:
            errors.append(f"jazz | {key} | {prog} | {voicing}: {e}")
            continue
        (jazz_dir / (safe_stem(key, prog, voicing) + ".csd")).write_text(make_csd(sco))

    # Pop: one file per (Key, Chord_Progression) — no voicing column
    seen_pop: set[tuple[str, str]] = set()
    for _, row in pop.iterrows():
        key  = str(row["Key"])
        prog = str(row["Chord_Progression"])
        pair = (key, prog)
        if pair in seen_pop:
            continue
        seen_pop.add(pair)

        chords = str(row["ChordName"]).split("-")
        meta = {
            "Genre":             "pop",
            "Key":               key,
            "Chord_Progression": prog,
            "ChordName":         row["ChordName"],
        }
        try:
            sco = progression_to_sco(
                chords, None,
                beats_per_chord=args.beats_per_chord,
                bpm=args.bpm, octave=args.octave,
                velocity=args.velocity, metadata=meta,
            )
        except ValueError as e:
            errors.append(f"pop | {key} | {prog}: {e}")
            continue
        (pop_dir / (safe_stem(key, prog) + ".csd")).write_text(make_csd(sco))

    write_orchestra(out_dir)

    jazz_count = len(list(jazz_dir.glob("*.csd")))
    pop_count  = len(list(pop_dir.glob("*.csd")))
    print(f"Generated {jazz_count} jazz .csd + {pop_count} pop .csd files in Csound/")
    print(f"Orchestra : Csound/harmony.orc  (standalone reference)")
    print(f"CsoundQt  : open any .csd file and press Run")
    print(f"CLI       : csound Csound/jazz/<file>.csd")

    if errors:
        print(f"\n{len(errors)} parse error(s):")
        for e in errors[:10]:
            print(f"  {e}")


if __name__ == "__main__":
    main()
