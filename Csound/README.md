# chord_play — Csound Opcode

Schedules note events for a named chord or dash-separated chord progression
directly from a Csound orchestra. No score `i` statements needed per note.

```csound
chord_play "C-G-Am-F", 2, p2, 1, 0.7
chord_play "F7-Em7b5-Ebm7-Dm7", 2, 0, 1, 0.6, 3
```

---

## Requirements

| Software | Version | Download |
|---|---|---|
| Csound | 6.x | https://csound.com/download.html |
| CsoundQt | any | https://csoundqt.github.io |
| Xcode Command Line Tools | any | `xcode-select --install` |

Install Csound first — it provides the framework headers needed to compile the opcode.

---

## Build

```bash
# Clone or download the AHM-Dataset repository, then:
cd AHM-Dataset/Csound/opcode
make
```

This produces `libchord_play.dylib` (a universal binary: x86_64 + arm64,
so it works in both native Csound and CsoundQt-via-Rosetta).

---

## Load the opcode

### Option A — per-file (no admin rights needed)

Add this line to `<CsOptions>` in your `.csd` file:

```
--opcode-lib=/absolute/path/to/AHM-Dataset/Csound/opcode/libchord_play.dylib
```

The path must be **absolute** (not `./`). CsoundQt's working directory is not
always the folder the file lives in, so relative paths fail.

### Option B — global install (available in all `.csd` files)

```bash
sudo cp libchord_play.dylib \
  /Library/Frameworks/CsoundLib64.framework/Versions/6.0/Resources/Opcodes64/
```

After copying, no changes to `<CsOptions>` are needed — Csound finds it automatically.

> **Do not** point OPCODE6DIR64 in CsoundQt Preferences to the `opcode/` folder.
> That replaces the default opcode path and breaks Csound's built-in audio modules.
> Use `--opcode-lib=` (Option A) or the global install (Option B) instead.

---

## Syntax

```
chord_play  SChord, iInstr, iStart, iDur, iAmp [, iOctave]
```

| Parameter | Type | Description |
|---|---|---|
| `SChord` | string | Chord name or dash-separated progression (see below) |
| `iInstr` | i-rate | Instrument number to schedule for synthesis |
| `iStart` | i-rate | Score start time in seconds for the first chord |
| `iDur` | i-rate | Duration in seconds per chord |
| `iAmp` | i-rate | Amplitude 0–1 |
| `iOctave` | i-rate (optional) | Root octave for close voicing — default **4** (C4 = MIDI 60) |

`chord_play` is i-rate only. It produces no audio output itself — it schedules
events on `iInstr`, which must be defined separately and handle `p4` (MIDI note)
and `p5` (amplitude).

---

## Supported chord types

| Suffix | Type | Intervals |
|---|---|---|
| *(none)* | Major triad | 0 4 7 |
| `m` | Minor triad | 0 3 7 |
| `7` | Dominant 7th | 0 4 7 10 |
| `maj7` | Major 7th | 0 4 7 11 |
| `m7` | Minor 7th | 0 3 7 10 |
| `m6` | Minor 6th | 0 3 7 9 |
| `m7b5` | Half-diminished (ø7) | 0 3 6 10 |
| `dim` | Diminished triad | 0 3 6 |
| `dim7` | Diminished 7th | 0 3 6 9 |
| `sus` | Suspended 4th | 0 5 7 |

Root notes: `C C# Db D D# Eb E F F# Gb G G# Ab A A# Bb B Cb Fb`

---

## Minimal working example

```csound
<CsoundSynthesizer>
<CsOptions>
--opcode-lib=/absolute/path/to/libchord_play.dylib
-o dac
</CsOptions>
<CsInstruments>

sr     = 44100
ksmps  = 32
nchnls = 2
0dbfs  = 1

gitab  ftgen 0, 0, 4096, 10, 1

; Synthesis instrument — p4 = MIDI note, p5 = amplitude
instr 2
  ifreq  cpsmidinn p4
  iamp   = p5 * 0dbfs
  aenv   expseg 1, p3*0.01, 0.5, p3*0.89, 0.001, p3*0.1, 0.001
  asig   foscili aenv * iamp, ifreq, 1, 2, 2.5, gitab
         outs asig, asig
endin

; Trigger instrument — schedule chords via chord_play
instr 1
  ; chord_play  SChord,        iInstr, iStart, iDur, iAmp, iOctave
  chord_play    "C-G-Am-F",    2,      p2,     1,    0.7,  4
endin

</CsInstruments>
<CsScore>
i1  0  4    ; I-V-vi-IV in C, one chord per second
e
</CsScore>
</CsoundSynthesizer>
```

---

## Usage patterns

**Single chord:**
```csound
chord_play "Fmaj7", 2, 0, 2, 0.7
```

**Full progression (chords separated by `-`):**
```csound
chord_play "C-G-Am-F", 2, p2, 1, 0.7
```

**Jazz progression with 7th chords:**
```csound
chord_play "F7-Em7b5-Ebm7-Dm7", 2, p2, 1, 0.6
```

**Lower octave:**
```csound
chord_play "Dm7-G7-Cmaj7", 2, p2, 1.5, 0.7, 3
```

**Multiple progressions in sequence via the score:**
```csound
; orchestra
instr 1
  chord_play "C-G-Am-F", 2, p2, 1, 0.7
endin

; score
i1  0   4   ; progression at t=0
i1  4   4   ; same progression at t=4
i1  8   4   ; again at t=8
```

---

## Troubleshooting

| Error | Cause | Fix |
|---|---|---|
| `Unexpected untyped word chord_play` | Opcode not loaded | Check the `--opcode-lib=` path is absolute and the file exists |
| `could not open library … (-1)` | Architecture mismatch or wrong path | Rebuild with `make`; use absolute path in `--opcode-lib=` |
| `No real-time audio modules were found` | OPCODE6DIR64 set to wrong folder | Clear OPCODE6DIR64 in CsoundQt Preferences → Environment |
| `WARNING: could not open library librtjack.dylib` | JACK audio not installed | Harmless — PortAudio is used instead; add `-+rtaudio=portaudio` to silence it |
| `chord_play: WARNING: unrecognised chord 'X'` | Unknown chord name in string | Check spelling; see supported chord types table above |
