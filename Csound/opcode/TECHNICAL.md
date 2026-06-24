# Emotion-Driven Chord Progression Generation: From Dataset to Csound Opcode

## 1. Overview

The system bridges a machine learning model and the Csound audio programming environment to enable real-time, emotion-driven chord progression generation. The architecture is split into two phases: an **offline training and export phase** (Python / scikit-learn) and an **online inference and scheduling phase** (C opcode inside Csound). The two phases are connected by a standard ONNX model file and a vocabulary lookup table. At audio runtime, the C plugin loads the ONNX model via the ONNX Runtime C API and runs live inference — no Python, no scikit-learn dependency at runtime.

---

## 2. Dataset

### 2.1 Sources

Two curated datasets are combined:

| Dataset | File | Rows | Progressions | Scales |
|---|---|---|---|---|
| Jazz harmony | `jazz_harmony_ml_dataset.csv` | 816 | 17 | 4 |
| Pop harmony | `pop_harmony_dataset.csv` | 446 | 63 | 7 |
| **Combined** | — | **1,262** | **80** | **7** |

### 2.2 Schema

**Jazz dataset** columns: `Key`, `Chord_Progression`, `ChordName`, `Voicing`, `Emotion`, `Scale`, `File_Path`

**Pop dataset** columns: `Key`, `Chord_Progression`, `ChordName`, `Emotion`, `Scale`

The `Voicing` column (jazz-only) encodes four close-position voicing transforms: *Four-Way Close*, *Drop 2*, *Drop 3*, *Drop 2+4*. This column is discarded during chord progression training — it is a rendering detail, not a harmonic classification feature.

### 2.3 Annotation Dimensions

Each row represents one concrete instantiation of a chord progression, annotated along three orthogonal dimensions:

- **Emotion** (13 classes): Delicate, Depressive, Despair, Epic, Fantasy, Gloomy, Joyful, Lonely, Love, Uneasiness, Victorious, Vital, soft
- **Scale / Mode** (7 classes): Ionian, Aeolian, Dorian, Phrygian, Lydian, Mixolydian, Harmonic minor
- **Key** (24 values): all 12 major and 12 minor keys

The jazz dataset covers 4 of the 7 modes (Ionian, Aeolian, Mixolydian, Harmonic minor); the pop dataset introduces Dorian, Lydian, and Phrygian. Combining the two datasets extends harmonic coverage across all common Western scales.

### 2.4 Class Distribution

The emotion distribution is unbalanced. Joyful is the most represented class (235 instances); Gloomy and Uneasiness the least (26–27 instances each). This imbalance reflects the source material rather than annotation error. The Random Forest handles it naturally through its majority-vote aggregation, and the subsequent temperature-weighted sampling at runtime further mitigates over-concentration on dominant classes.

| Emotion | Count |
|---|---|
| Joyful | 235 |
| Fantasy | 157 |
| Depressive | 132 |
| Despair | 107 |
| Delicate | 106 |
| soft | 106 |
| Love | 78 |
| Vital | 78 |
| Victorious | 78 |
| Epic | 78 |
| Lonely | 54 |
| Uneasiness | 27 |
| Gloomy | 26 |

---

## 3. Feature Engineering and Encoding

### 3.1 Feature Selection

The classification task is:

> **Given an emotion and a scale, predict the most probable chord progression.**

The feature vector is therefore two-dimensional:

```
X = [emotion_id,  scale_id]
```

The `Key` column is intentionally excluded from features. Key is a transposition of a progression, not a harmonic character in itself — `I-V-vi-IV` in C major and in G major share identical emotional and structural properties. Including Key would fragment the training data into 24 near-identical shards, adding noise without useful signal.

The `Chord_Progression` column (Roman numeral notation, key-invariant) is the prediction target `y`.

### 3.2 Label Encoding

All three variables are ordinally encoded with scikit-learn `LabelEncoder`, which maps each unique string to a contiguous integer index:

| Variable | Encoder | Vocabulary size |
|---|---|---|
| `Emotion` | `le_emotion` | 13 |
| `Scale` | `le_scale` | 7 |
| `Chord_Progression` | `le_prog` | 80 |

Encoding is fit on the full combined dataset to ensure consistent integer indices. The final feature matrix is shape `(1262, 2)` with integer-valued entries; the target vector is shape `(1262,)`.

---

## 4. Model: Random Forest with Optuna Tuning

### 4.1 Architecture

A **Random Forest Classifier** (scikit-learn `RandomForestClassifier`) is used. Random Forest is chosen for this task because:

- **Calibrated probability output**: `predict_proba()` returns a well-calibrated distribution over all 80 progression classes, directly usable as a sampling distribution at runtime.
- **Handles small, structured datasets**: with 1,262 rows and a 2-dimensional feature space, shallow ensembles generalise better than deep models.
- **ONNX-exportable**: scikit-learn models can be converted to the standard ONNX format via `skl2onnx`, enabling runtime inference in C without any Python dependency.

Two classifiers are trained:

| Classifier | Target | Training data |
|---|---|---|
| `clf_prog` | Chord progression (80 classes) | Combined jazz + pop (1,262 rows) |
| `clf_voicing` | Voicing style (4 classes) | Jazz only (816 rows) |

Only `clf_prog` is exported to ONNX for the Csound opcode. `clf_voicing` is available for future extension.

### 4.2 Hyperparameter Optimisation

Hyperparameters are tuned via **Optuna** (tree-structured Parzen estimator) with 50 trials and 5-fold stratified cross-validation. The objective function averages accuracy across both classifiers to find a single configuration that works well for both tasks:

```python
def objective(trial):
    s_p = cross_val_score(RandomForestClassifier(**params), X_tr_p, ytr_p, ...).mean()
    s_v = cross_val_score(RandomForestClassifier(**params), X_tr_v, ytr_v, ...).mean()
    return (s_p + s_v) / 2
```

Search space:

| Hyperparameter | Range |
|---|---|
| `n_estimators` | 50–500 (step 50) |
| `max_depth` | 2–32 |
| `min_samples_split` | 2–20 |
| `min_samples_leaf` | 1–10 |
| `max_features` | `"sqrt"`, `"log2"`, `1.0` |

### 4.3 Training Procedure

Optuna tuning is performed on an 80/20 train/test split (`random_state=42`, stratified by emotion) to obtain the best hyperparameters. The final classifier for ONNX export is then **retrained on the complete dataset** (all 1,262 rows). This two-stage procedure is critical: training on the full dataset ensures all 80 chord progression classes appear in the model output. Training only on the 80% split leaves 7 rare classes unseen, reducing the output dimension to 73 and breaking the index mapping at runtime.

### 4.4 Export to ONNX via skl2onnx

The trained `clf_prog` is converted to ONNX format using `skl2onnx`:

```python
from skl2onnx import convert_sklearn
from skl2onnx.common.data_types import FloatTensorType

onnx_model = convert_sklearn(
    clf_prog,
    initial_types=[("input", FloatTensorType([None, 2]))],
    options={id(clf_prog): {"zipmap": False}},
)
```

The `zipmap=False` option is required to produce a plain float32 probability tensor rather than a list of dictionaries. The resulting `gen_model.onnx` encodes the full Random Forest decision structure and all learned parameters in a standard, runtime-agnostic format.

The ONNX model has:
- **Input** `"input"`: float32 tensor of shape `[N, 2]` — `[emotion_id, scale_id]`
- **Output** `"label"`: int64 tensor of shape `[N]` — argmax class prediction (unused at runtime)
- **Output** `"probabilities"`: float32 tensor of shape `[N, 80]` — calibrated class probabilities

### 4.5 Vocabulary Lookup Table

In addition to the ONNX file, `train_model.py` exports `gen_data.tsv` — a tab-separated table with 650 rows:

```
emotion  scale  emotion_id  scale_id  prog_id  key  chord_name
Joyful   Ionian  6          3         47       C    C-G-Am-F
...
```

This table serves two purposes in the C plugin:
1. **String → integer mapping**: converts the user's emotion string to `emotion_id` for the ONNX model input
2. **Post-inference lookup**: maps the sampled `prog_id` back to a concrete `chord_name` string for a specific key

The table stores no model probabilities — probabilities are computed live by the ONNX model on every call.

---

## 5. Runtime Inference in Csound

### 5.1 Plugin Architecture

The inference logic is implemented as a Csound plugin — a universal binary (x86_64 + arm64) `.dylib` shared library. Two opcodes are registered:

| Opcode | Phase | Purpose |
|---|---|---|
| `emoChord_init` | i-rate, once | Load the ONNX model and lookup table |
| `emoChord` | i-rate, per call | Run ONNX inference and schedule note events |

The plugin links against `libonnxruntime.dylib` (ONNX Runtime v1.20.1, universal binary) bundled in the same directory. The dylib's `@rpath` is set at build time to the opcode directory, so no environment variables are required at load time.

### 5.2 Score Usage

```csound
; <CsScore>
i1  0  4  "joyful"     ; start=0, dur=4, emotion string as p4

; <CsInstruments>
instr 1
  Sem strget p4
  emoChord Sem, 2, p2, p3, 0.7   ; emotion, synth_instr, start, dur, amp
endin
```

The `<CsScore>` section is a static event scheduler — it defines timing and passes the emotion string as a p-field. Opcode execution happens inside the instrument. From the composer's perspective, writing `i1 0 4 "joyful"` is functionally equivalent to `emoChord("joyful")` at time 0.

### 5.3 Initialisation (`emoChord_init`)

`emoChord_init` takes two string arguments — the path to `gen_model.onnx` and the path to `gen_data.tsv`. It:

1. Calls `OrtGetApiBase()->GetApi(ORT_API_VERSION)` to obtain the ONNX Runtime API table
2. Creates an `OrtEnv` and `OrtSessionOptions`
3. Calls `OrtApi->CreateSession()` to load `gen_model.onnx`
4. Reads the output class count from the `"probabilities"` tensor shape (index 1)
5. Loads `gen_data.tsv` into a static C array of `DataEntry` structs

### 5.4 Inference Procedure (`emoChord`)

**Step 1 — Resolve emotion_id and sample scale**

Scan `DataEntry[]` for rows matching the emotion string (case-insensitive). Extract `emotion_id`. Collect all unique `scale_id` values for that emotion, weighted by their frequency of occurrence in the dataset. Apply temperature scaling and sample one `scale_id`.

**Step 2 — ONNX Runtime inference**

Construct a float32 input tensor `[1, 2]` and call `OrtApi->Run()`:

```c
float input_data[2] = { (float)emotion_id, (float)scale_id };
// ... CreateTensorWithDataAsOrtValue, Run ...
const char *out_names[] = { "probabilities" };
ort->Run(session, NULL, in_names, &in_tensor, 1, out_names, 1, &out_tensor);
float *probs;
ort->GetTensorMutableData(out_tensor, (void**)&probs);
```

`probs[0..79]` contains the Random Forest's calibrated probability distribution over all 80 chord progression classes. Because the RF's `predict_proba()` output is already normalised (sums to 1.0), no softmax is applied.

**Step 3 — Temperature scaling**

Temperature is applied using the power law directly on the RF probabilities. For a probability vector `p` and temperature `T`:

```
p'_i = p_i^(1/T)
p'_i = p'_i / sum_j(p'_j)
```

- `T → 0`: approaches argmax (always picks the highest-probability progression)
- `T = 1`: preserves the RF's trained distribution exactly
- `T > 1`: flattens toward uniform, increasing harmonic variety

**Step 4 — Filter and sample progression**

The probability vector is filtered to only the `prog_id` values present in `gen_data.tsv` for the sampled `(emotion_id, scale_id)` pair. Temperature-weighted inverse-CDF sampling selects one `prog_id`.

**Step 5 — Chord name lookup and display**

All `DataEntry` rows matching `(emotion_id, scale_id, prog_id)` are collected — these are different key transpositions of the same progression. One is selected at random. The `chord_name` string (e.g. `Cm7-F7-Bbmaj7-Ebmaj7`) is printed to the Csound console so the user can see what was generated.

**Step 6 — MIDI note scheduling**

The `chord_name` string is tokenised on `-` delimiters. Each token is parsed into a root pitch class and chord quality suffix, converted to MIDI note numbers, and submitted to Csound's event scheduler via `csound->insert_score_event()` as `EVTBLK` structs. The synthesis instrument specified by `iInstr` receives `p4 = MIDI note` and `p5 = amplitude` for each note in each chord.

---

## 6. System Summary

```
┌─────────────────────────────────────────────────────────────┐
│  OFFLINE (Python / scikit-learn, runs once)                 │
│                                                             │
│  jazz_harmony_ml_dataset.csv ──┐                           │
│  pop_harmony_dataset.csv ──────┴─► combine ─► encode       │
│                                         │                   │
│                               LabelEncoder × 3             │
│                               (emotion, scale, progression) │
│                                         │                   │
│                          Optuna (50 trials, 5-fold CV)     │
│                          → best hyperparameters            │
│                                         │                   │
│                          RandomForestClassifier             │
│                          retrained on full dataset          │
│                          (1,262 rows → 80 classes)         │
│                                         │                   │
│                          skl2onnx.convert_sklearn()        │
│                                         │                   │
│                    gen_model.onnx   gen_data.tsv            │
│                    (RF graph +      (vocab + chord          │
│                     weights)         name lookup)           │
└─────────────────────────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  RUNTIME (C / ONNX Runtime, inside Csound)                  │
│                                                             │
│  emoChord_init ──► OrtCreateSession(gen_model.onnx)            │
│               load gen_data.tsv → DataEntry[]              │
│                                                             │
│  i1 0 4 "joyful"  ──► instr 1 ──► emoChord "joyful", 2, ...│
│                                         │                   │
│                   resolve emotion_id from DataEntry[]      │
│                   sample scale_id (data frequency)         │
│                                         │                   │
│                   OrtRun(input=[emotion_id, scale_id])     │
│                        │                                    │
│                   probs[80]  (RF calibrated, sums to 1)    │
│                        │                                    │
│                   temperature scaling (power law)          │
│                   filter to observed prog_ids              │
│                   weighted sample → prog_id                │
│                        │                                    │
│                   lookup chord_name → print to user        │
│                   parse → MIDI notes                       │
│                   insert_score_event × n notes             │
│                                                             │
│  Synthesis instr ──► p4=MIDI, p5=amp ──► audio out         │
└─────────────────────────────────────────────────────────────┘
```

The ONNX file is the single source of truth for the model. The C plugin contains no weight values, no probability tables, and no model-specific arithmetic beyond temperature scaling — it is a generic execution harness around ONNX Runtime that would work unchanged with any replacement model exported to the same ONNX input/output signature (`"input"` [N,2] → `"probabilities"` [N,80]).

---

## 7. Build Tutorial

This section documents how to reproduce the full system from scratch on macOS.

### 7.1 Prerequisites

| Tool | Install |
|---|---|
| Python 3.10+ | https://python.org or `brew install python` |
| Csound 6.x | https://csound.com/download.html |
| CsoundQt | https://csoundqt.github.io |
| Xcode Command Line Tools | `xcode-select --install` |

Python packages:

```bash
pip install scikit-learn optuna skl2onnx onnxruntime numpy pandas
```

### 7.2 Repository layout

```
AHM-Dataset/
├── jazz_harmony_ml_dataset.csv   # 816-row jazz dataset
├── pop_harmony_dataset.csv       # 446-row pop dataset
├── train_model.py                # training + export script
├── Model/
│   └── Encoder.ipynb             # original notebook (reference)
└── Csound/
    └── opcode/
        ├── gen.c                 # C plugin source
        ├── Makefile
        ├── onnxruntime_c_api.h   # ONNX Runtime C header (v1.20.1)
        ├── libonnxruntime.dylib  # ONNX Runtime universal binary (v1.20.1)
        ├── gen_model.onnx        # generated by train_model.py
        ├── gen_data.tsv          # generated by train_model.py
        ├── libgen.dylib          # compiled by make
        └── gen_demo.csd          # demo score
```

### 7.3 Step 1 — Train the model and export ONNX

```bash
cd /path/to/AHM-Dataset
python train_model.py
```

What `train_model.py` does:

1. **Loads** `jazz_harmony_ml_dataset.csv` and `pop_harmony_dataset.csv`, concatenates them into 1,262 rows.
2. **Encodes** `Emotion` (13 classes), `Scale` (7 classes), and `Chord_Progression` (80 classes) as integer IDs using `sklearn.LabelEncoder`.
3. **Tunes hyperparameters** with Optuna (50 trials, 5-fold stratified CV) jointly optimising accuracy on both progression and voicing classifiers.
4. **Evaluates** the best configuration on an 80/20 held-out split and prints test accuracy.
5. **Retrains** `clf_prog` on the full 1,262-row dataset so all 80 progression classes appear in the ONNX model output.
6. **Exports** via `skl2onnx.convert_sklearn(clf_prog, ..., target_opset=18, options={...: {"zipmap": False}})` to `Csound/opcode/gen_model.onnx`.
7. **Writes** a deduplicated vocabulary table to `Csound/opcode/gen_data.tsv`.

The `target_opset=18` parameter is required: the bundled `libonnxruntime.dylib` (v1.20.1) officially supports opsets up to 21; skl2onnx's default is opset 22, which causes a load error at runtime.

Expected output:

```
Jazz: 816 | Pop: 446 | Combined: 1262
Running Optuna (50 trials)...
Best CV accuracy: 0.xxxx
Test accuracy  progression=0.xxx  voicing=0.xxx
Final model classes: progression=80  voicing=4
ONNX     → .../Csound/opcode/gen_model.onnx
Data     → .../Csound/opcode/gen_data.tsv  (650 rows)
```

### 7.4 Step 2 — Build the Csound plugin

```bash
make -C /path/to/AHM-Dataset/Csound/opcode/
```

The Makefile compiles `gen.c` as a universal binary (x86_64 + arm64) so it works under both native arm64 and Rosetta (CsoundQt is x86_64). The `-Wl,-rpath,$(ORT_DIR)` flag embeds the absolute path to `libonnxruntime.dylib` so the plugin finds it at load time without requiring any environment variables.

Key compiler flags:

```makefile
-shared -fPIC -O2
-arch x86_64 -arch arm64
-I$(CSOUND)/Headers -framework CsoundLib64
-I$(ORT_DIR) -L$(ORT_DIR) -lonnxruntime
-Wl,-rpath,$(ORT_DIR) -lm
```

Output: `libgen.dylib`

### 7.5 Step 3 — Install the plugin

Copy `libgen.dylib` to the Csound global opcodes directory:

```bash
sudo cp /path/to/AHM-Dataset/Csound/opcode/libgen.dylib \
  /Library/Frameworks/CsoundLib64.framework/Versions/6.0/Resources/Opcodes64/
```

Csound scans this directory at startup and automatically registers all opcodes it finds, including `emoChord_init` and `emoChord`.

Do **not** point `OPCODE6DIR64` in CsoundQt Preferences to the `opcode/` directory — that replaces the default opcode search path and disables Csound's built-in audio modules.

### 7.6 Step 4 — Write a Csound score

The plugin exposes two opcodes. A minimal `.csd` looks like this:

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

; Load model and vocabulary (runs once at compile time)
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

; Emotion-driven instrument — p4=emotion string
instr 1
  Sem strget p4         ; read emotion string from score p-field
  emoChord Sem, 2, p2, p3, 0.7   ; infer + schedule chords into instr 2
endin

</CsInstruments>
<CsScore>
; instrument  start  duration  emotion
i1   0   4   "joyful"
i1   6   4   "depressive"
i1  12   4   "fantasy"
e
</CsScore>
</CsoundSynthesizer>
```

**Why the emotion string goes through the score, not the opcode directly:**

`<CsScore>` is a static event scheduler — it is parsed before Csound starts running instruments. Only timing data and p-fields can be written there; opcode calls cannot execute in score context. The pattern `i1 0 4 "joyful"` passes the string as `p4` to instrument 1, which then calls `emoChord` at i-time. This is the standard Csound idiom for parameterised instruments.

### 7.7 What happens at runtime

When `i1 0 4 "joyful"` fires:

1. **`strget p4`** retrieves `"joyful"` from the p-field.
2. **`emoChord`** scans `gen_data.tsv` for rows where `emotion == "joyful"`, obtains `emotion_id`, and collects the associated `scale_id` distribution.
3. A **scale is sampled** (weighted by training data frequency, temperature-scaled).
4. **ONNX Runtime** runs the Random Forest on input `[emotion_id, scale_id]`, returning `probs[80]` — the model's probability distribution over all 80 chord progressions.
5. **Temperature scaling** (power law `p'_i = p_i^(1/T)`) is applied to the RF probabilities. Because RF's `predict_proba()` output is already normalised, no softmax is applied.
6. **A `prog_id` is sampled** from the temperature-adjusted distribution, filtered to progressions observed for the `(emotion, scale)` pair.
7. A **chord name** (e.g. `Cm7-F7-Bbmaj7-Ebmaj7`) is selected and printed to the Csound console.
8. The chord name is **tokenised on `-`**, each token parsed into a root pitch class and quality suffix, converted to MIDI note numbers, and submitted via `csound->insert_score_event()`.
9. **Instrument 2** receives each note as a separate event with `p4=MIDI` and `p5=amplitude`.
