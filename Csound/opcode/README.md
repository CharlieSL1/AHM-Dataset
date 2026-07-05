# emoChord — Emotion-Driven Chord Generation for Csound

A Csound plugin opcode that generates chord progressions from natural-language emotion descriptions using a machine-learning model at runtime. No Python required during performance.

| Opcode | Purpose |
|---|---|
| `emoChord_init` | Load the ONNX model and vocabulary table (call once at startup) |
| `emoChord` | Run ML inference and schedule chord notes into Csound |

---

## How it works

```
<CsScore>: i1 0 4 "joyful"
        │
        ▼
  instr 1: emoChord "joyful", 2, p2, p3, 0.7
        │
        ├─► ONNX Runtime (Random Forest model)
        │     input: [emotion_id, scale_id]
        │     output: probabilities[108 chord progressions]
        │
        ├─► temperature sampling → chord name (e.g. "Cm7-F7-Bbmaj7-Ebmaj7")
        │     printed to console
        │
        └─► insert_score_event → MIDI notes → instr 2 → audio out
```

The model is a Random Forest trained on 2,196 annotated jazz and pop rows covering 108 unique chord progressions across 6 emotion classes and 7 scale/mode categories. It is exported to ONNX format and runs entirely in C via the ONNX Runtime C API.

---

## Requirements

| Tool | Version | Notes |
|---|---|---|
| Csound | 6.x | https://csound.com/download.html |
| CsoundQt | any | https://csoundqt.github.io |
| Xcode Command Line Tools | any | `xcode-select --install` |
| Python 3.10+ | for training step only | `pip install scikit-learn optuna skl2onnx pandas numpy` |

---

## Setup

### Step 1 — Train the model and export files

```bash
cd /path/to/AHM-Dataset
python train_model.py
```

This produces two files in `Csound/opcode/`:
- `gen_model.onnx` — the trained Random Forest in ONNX format
- `gen_data.tsv` — vocabulary and chord name lookup table

### Step 2 — Build the plugin

```bash
make -C /path/to/AHM-Dataset/Csound/opcode/
```

Produces `libgen.dylib` (universal binary: x86_64 + arm64).

### Step 3 — Install the plugin

Copy `libgen.dylib` to the Csound global opcodes folder so it is available in all `.csd` files:

```bash
sudo cp /path/to/AHM-Dataset/Csound/opcode/libgen.dylib \
  /Library/Frameworks/CsoundLib64.framework/Versions/6.0/Resources/Opcodes64/
```

Or drag `libgen.dylib` there in Finder.

Alternatively, load it per-file by adding to `<CsOptions>`:

```
--opcode-lib=/absolute/path/to/AHM-Dataset/Csound/opcode/libgen.dylib
```

### Step 4 — Run the demo

Open `Csound/opcode/gen_demo.csd` in CsoundQt and press Run.

---

## Opcode reference

### `emoChord_init`

```
emoChord_init  SModelPath, SDataPath
```

Load the ONNX model and vocabulary table. Must be called once before any `emoChord` call. Place it in the orchestra header (before any `instr` block).

| Parameter | Description |
|---|---|
| `SModelPath` | Absolute path to `gen_model.onnx` |
| `SDataPath` | Absolute path to `gen_data.tsv` |

### `emoChord`

```
emoChord  SEmotion, iInstr, iStart, iDur, iAmp [, iTemp, iOctave]
```

Run inference and schedule note events.

| Parameter | Description |
|---|---|
| `SEmotion` | Emotion string — case-insensitive (see list below) |
| `iInstr` | Synthesis instrument number (receives `p4`=MIDI note, `p5`=amp) |
| `iStart` | Score start time in seconds for the first chord |
| `iDur` | Duration per chord in seconds |
| `iAmp` | Amplitude 0–1 |
| `iTemp` | Sampling temperature, default **1.0** |
| `iOctave` | Root octave, default **4** (C4 = MIDI 60) |

**Temperature guide:**

| `iTemp` | Effect |
|---|---|
| `0` | Near-deterministic — strongly favors highest-probability progression |
| `1` | Natural model distribution |
| `2+` | Flatter — more harmonic variety |

**Supported emotions** (case-insensitive):

```
Joyful   Vital   Epic   Uneasiness   Depressive   Despair
```

**Supported chord qualities:**

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

---

## Minimal working example

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

gitab ftgen 0, 0, 4096, 10, 1

emoChord_init "/absolute/path/to/gen_model.onnx", \
              "/absolute/path/to/gen_data.tsv"

; Synthesis instrument — p4=MIDI note, p5=amplitude
instr 2
  ifreq cpsmidinn p4
  iamp  = p5 * 0dbfs
  aenv  expseg 1, p3*0.01, 0.5, p3*0.89, 0.001, p3*0.1, 0.001
  asig  foscili aenv * iamp, ifreq, 1, 2, 2.5, gitab
        outs asig, asig
endin

; Emotion-driven instrument — p4=emotion string from score
instr 1
  Sem strget p4
  emoChord Sem, 2, p2, p3, 0.7
endin

</CsInstruments>
<CsScore>
i1   0   4   "joyful"
i1   6   4   "depressive"
i1  12   4   "uneasiness"
e
</CsScore>
</CsoundSynthesizer>
```

---

## Troubleshooting

| Error | Cause | Fix |
|---|---|---|
| `emoChord: not initialised` | `emoChord_init` not called | Add `emoChord_init "..."` to the orchestra header |
| `emoChord_init: cannot open data '...'` | Wrong path to TSV | Use absolute path; run `train_model.py` first |
| `emoChord: ORT error: ... Opset 22` | ONNX opset version mismatch | Ensure `train_model.py` exports with `target_opset=18` |
| `emoChord: unknown emotion '...'` | Typo or unsupported emotion | Check the emotion list above |
| `Unexpected untyped word emoChord` | Plugin not loaded | Confirm `libgen.dylib` is in the Csound opcodes folder |
| `could not open library … (-1)` | Architecture mismatch | Rebuild with `make` (universal binary required) |
| `No real-time audio modules found` | `OPCODE6DIR64` overrides default path | Clear `OPCODE6DIR64` in CsoundQt Preferences → Environment |
