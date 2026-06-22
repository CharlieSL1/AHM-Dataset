# AHM-Dataset Csound Opcodes

Two opcodes for generating and playing chord progressions inside Csound.

| Opcode | What it does |
|---|---|
| `chord_play` | Schedule note events for an explicit chord/progression string |
| `chord_gen` | Sample a chord progression from the ML model for a given emotion, then play it |

---

## Requirements

| Software | Version | Download |
|---|---|---|
| Csound | 6.x | https://csound.com/download.html |
| CsoundQt | any | https://csoundqt.github.io |
| Xcode Command Line Tools | any | `xcode-select --install` |
| Python 3.10+ with scikit-learn | for export step only | `pip install scikit-learn pandas` |

---

## Setup

### 1. Build the opcodes

```bash
cd AHM-Dataset/Csound/opcode
make
```

Produces `libchord_play.dylib` and `libchord_gen.dylib` (universal binaries: x86_64 + arm64).

### 2. Export the model data (chord_gen only)

```bash
cd AHM-Dataset
python export_model.py
```

Trains the Random Forest on the dataset and writes `Csound/opcode/chord_gen_data.tsv`.
Run this once, or again after updating the dataset.

### 3. Install the opcodes

**Option A — global install** (available in all `.csd` files):

```bash
sudo cp libchord_play.dylib libchord_gen.dylib \
  /Library/Frameworks/CsoundLib64.framework/Versions/6.0/Resources/Opcodes64/
```

**Option B — per-file** (no admin rights needed): add to `<CsOptions>`:

```
--opcode-lib=/absolute/path/to/AHM-Dataset/Csound/opcode/libchord_play.dylib
--opcode-lib=/absolute/path/to/AHM-Dataset/Csound/opcode/libchord_gen.dylib
```

> **Do not** set OPCODE6DIR64 in CsoundQt Preferences to the `opcode/` folder —
> that replaces Csound's default opcode path and breaks built-in audio modules.

---

## chord_play

Schedule note events for an explicit chord name or dash-separated progression.

### Syntax

```
chord_play  SChord, iInstr, iStart, iDur, iAmp [, iOctave]
```

| Parameter | Description |
|---|---|
| `SChord` | Chord name (`"F7"`) or progression (`"C-G-Am-F"`) |
| `iInstr` | Synthesis instrument number |
| `iStart` | Score start time in seconds |
| `iDur` | Duration per chord in seconds |
| `iAmp` | Amplitude 0–1 |
| `iOctave` | Root octave, default **4** |

### Supported chord types

| Suffix | Type |
|---|---|
| *(none)* | Major triad |
| `m` | Minor triad |
| `7` | Dominant 7th |
| `maj7` | Major 7th |
| `m7` | Minor 7th |
| `m6` | Minor 6th |
| `m7b5` | Half-diminished |
| `dim` | Diminished triad |
| `dim7` | Diminished 7th |
| `sus` | Suspended 4th |

### Example

```csound
instr 1
  chord_play "C-G-Am-F", 2, p2, 1, 0.7
endin
```

---

## chord_gen

Sample a chord progression from the ML model given an emotion, then play it.

### Syntax

```
chord_gen_init  SDataPath
chord_gen       SEmotion, iInstr, iStart, iDur, iAmp [, iTemp, iOctave]
```

| Parameter | Description |
|---|---|
| `SDataPath` | Absolute path to `chord_gen_data.tsv` |
| `SEmotion` | Emotion name — case-insensitive (see list below) |
| `iInstr` | Synthesis instrument number |
| `iStart` | Score start time in seconds |
| `iDur` | Duration per chord in seconds |
| `iAmp` | Amplitude 0–1 |
| `iTemp` | Sampling temperature, default **1.0** — 0=deterministic, 1=natural, >1=more varied |
| `iOctave` | Root octave, default **4** |

`chord_gen_init` must be called **once** before any `chord_gen` call.
The idiomatic place is the orchestra header (before any `instr` block).

### Supported emotions

```
Delicate   Depressive   Despair   Epic       Fantasy
Gloomy     Joyful       Lonely    Love       Uneasiness
Victorious Vital        soft
```

Emotion matching is case-insensitive (`"joyful"` = `"Joyful"`).

### Temperature guide

| iTemp | Effect |
|---|---|
| `0` | Always picks the highest-probability progression (deterministic) |
| `1` | Natural model distribution — reflects training data probabilities |
| `2` | Flatter distribution — more variety, less likely choices appear |

### Example

```csound
; Orchestra header — load data once
chord_gen_init "/absolute/path/to/AHM-Dataset/Csound/opcode/chord_gen_data.tsv"

instr 1
  ; Natural distribution
  chord_gen "Joyful", 2, p2, 1, 0.7

  ; Deterministic (always same result)
  ; chord_gen "Joyful", 2, p2, 1, 0.7, 0

  ; High variety
  ; chord_gen "Joyful", 2, p2, 1, 0.7, 2
endin
```

---

## Minimal working example (both opcodes)

```csound
<CsoundSynthesizer>
<CsOptions>
-o dac
</CsOptions>
<CsInstruments>

sr     = 44100
ksmps  = 32
nchnls = 2
0dbfs  = 1

gitab  ftgen 0, 0, 4096, 10, 1
chord_gen_init "/absolute/path/to/chord_gen_data.tsv"

; Synthesis instrument — p4=MIDI note, p5=amplitude
instr 2
  ifreq  cpsmidinn p4
  iamp   = p5 * 0dbfs
  aenv   expseg 1, p3*0.01, 0.5, p3*0.89, 0.001, p3*0.1, 0.001
  asig   foscili aenv * iamp, ifreq, 1, 2, 2.5, gitab
         outs asig, asig
endin

instr 1  ;; explicit chord
  chord_play "C-G-Am-F", 2, p2, 1, 0.7
endin

instr 3  ;; emotion-driven
  chord_gen "Joyful", 2, p2, 1, 0.7
endin

</CsInstruments>
<CsScore>
i1  0  4   ; explicit I-V-vi-IV
i3  5  4   ; model-sampled Joyful progression
e
</CsScore>
</CsoundSynthesizer>
```

---

## Troubleshooting

| Error | Cause | Fix |
|---|---|---|
| `chord_gen: no data loaded` | `chord_gen_init` not called | Add `chord_gen_init "..."` to the orchestra header |
| `chord_gen: cannot open '...'` | Wrong path to TSV | Use absolute path; run `export_model.py` first |
| `chord_gen: unknown emotion '...'` | Typo or unsupported emotion | Check the emotion list above |
| `Unexpected untyped word chord_gen` | Opcode not loaded | Check `--opcode-lib=` path or global install |
| `could not open library … (-1)` | Architecture mismatch | Rebuild with `make`; use absolute path |
| `No real-time audio modules found` | OPCODE6DIR64 set to wrong folder | Clear OPCODE6DIR64 in CsoundQt Preferences → Environment |
