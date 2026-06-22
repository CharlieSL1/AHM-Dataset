# Emotion-Driven Chord Progression Generation: From Dataset to Csound Opcode

## 1. Overview

The system bridges a machine learning classifier and the Csound audio programming environment to enable real-time, emotion-driven chord progression generation. The architecture is split into two phases: an **offline training and export phase** (Python) and an **online inference and scheduling phase** (C opcode inside Csound). The two phases communicate through a static probability table written to disk, eliminating any Python or scikit-learn dependency at audio runtime.

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

The `Voicing` column (jazz-only) encodes four close-position voicing transforms: *Four-Way Close*, *Drop 2*, *Drop 3*, *Drop 2+4*. This column is discarded during training — it is a rendering detail, not a harmonic classification feature.

### 2.3 Annotation Dimensions

Each row represents one concrete instantiation of a chord progression, annotated along three orthogonal dimensions:

- **Emotion** (13 classes): Delicate, Depressive, Despair, Epic, Fantasy, Gloomy, Joyful, Lonely, Love, Uneasiness, Victorious, Vital, soft
- **Scale / Mode** (7 classes): Ionian, Aeolian, Dorian, Phrygian, Lydian, Mixolydian, Harmonic minor
- **Key** (24 values): all 12 major and 12 minor keys

The jazz dataset covers 4 of the 7 modes (Ionian, Aeolian, Mixolydian, Harmonic minor); the pop dataset introduces Dorian, Lydian, and Phrygian. Combining the two datasets extends harmonic coverage across all common Western scales.

### 2.4 Class Distribution

The emotion distribution is unbalanced. Joyful is the most represented class (235 instances); Gloomy and Uneasiness the least (26–27 instances each). This imbalance reflects the source material rather than annotation error and is handled implicitly by the Random Forest's majority-vote aggregation.

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

Encoding is fit on the full combined dataset before train/test splitting to ensure consistent integer indices. The final feature matrix is shape `(1262, 2)` with integer-valued entries; the target vector is shape `(1262,)`.

---

## 4. Model Training

### 4.1 Architecture

A **Random Forest Classifier** (scikit-learn `RandomForestClassifier`) is used. Random Forest is chosen for this task for the following reasons:

- **Interpretable probability output**: `predict_proba()` returns a well-calibrated distribution over all 80 progression classes, directly usable as a sampling distribution.
- **Handles small, structured datasets**: with 1,262 rows and a 2-dimensional feature space, deep models would overfit; shallow ensembles generalise better.
- **Deterministic with fixed seed**: reproducible exports across runs.

### 4.2 Hyperparameter Selection

Hyperparameters were tuned via Optuna (tree-structured Parzen estimator) with 5-fold cross-validation. The best configuration found:

| Hyperparameter | Value | Rationale |
|---|---|---|
| `n_estimators` | 100 | Ensemble size; diminishing returns beyond this for this dataset size |
| `max_depth` | 3 | Shallow trees prevent memorising individual (emotion, scale, key) triples |
| `min_samples_split` | 16 | Requires statistical mass before splitting |
| `min_samples_leaf` | 3 | Smooths leaf probability estimates |
| `max_features` | 1.0 | Uses both features at every split (feature space is only 2D) |
| `random_state` | 42 | Reproducibility |

`max_depth=3` is particularly significant: with two binary-valued-like features, a depth-3 tree can make at most 7 distinct leaf predictions. This constrains the model to learn coarse emotion–scale–progression associations rather than memorising key-specific patterns.

### 4.3 Training Procedure

The model is trained on the full combined dataset (no held-out test split in the export step) to maximise coverage of the 80-class output space. Cross-validation accuracy was measured separately during hyperparameter search.

---

## 5. Probability Export

### 5.1 Why Export Rather Than Embed

The Csound plugin API is pure C. Embedding scikit-learn, NumPy, or a Python interpreter inside a `.dylib` would create a runtime dependency chain incompatible with Csound's lightweight plugin model. The solution is to **materialise the model's predictions offline** and ship only the resulting probability table to the C layer.

### 5.2 Export Logic

For every unique `(emotion, scale)` pair present in the dataset:

1. Construct the feature vector `x = [le_emotion(emotion), le_scale(scale)]`
2. Call `clf.predict_proba(x)` to obtain a probability vector over all 80 `Chord_Progression` classes
3. For each concrete `(key, progression)` pair observed in the data for this `(emotion, scale)` group:
   - Look up the encoded progression index `pe = le_prog(progression)`
   - Extract the model's probability for that class: `w = probs[pe]`
4. Normalise the resulting weights so they sum to 1 within each `(emotion, scale)` group
5. Map each `(key, progression)` pair back to its concrete `ChordName` string (e.g. `I-V-vi-IV` → `C-G-Am-F` for key C)

### 5.3 Output Format

Results are written to `Csound/opcode/chord_gen_data.tsv` (tab-separated, 650 data rows + 1 header):

```
emotion    scale    key    chord_name    weight
Joyful     Ionian   C      C-G-Am-F      0.12345678
Joyful     Ionian   C      C-Am-F-G      0.08901234
...
```

`chord_name` contains the fully-spelled chord names ready for the Csound chord parser (dash-separated, e.g. `Cm7-F7-Bbmaj7-Ebmaj7`). The weight is normalised within each `(emotion, scale)` group, so each group's weights sum to 1.0.

---

## 6. Runtime Inference in Csound

### 6.1 Plugin Architecture

The inference logic is implemented as a Csound plugin — a C shared library (`.dylib` on macOS) that registers custom opcodes with Csound's opcode table at load time via `csound->AppendOpcodes()`. Two opcodes are registered:

| Opcode | Phase | Purpose |
|---|---|---|
| `chord_gen_init` | i-rate, once | Load the TSV into a global in-memory array |
| `chord_gen` | i-rate, per call | Sample and schedule a chord progression |

### 6.2 Data Loading (`chord_gen_init`)

`chord_gen_init` reads the TSV line by line into a static C struct:

```c
static struct {
    int      loaded;
    GenEntry entries[2048];
    int      n;
} g_chord_gen;
```

Each `GenEntry` stores `emotion`, `scale`, `key`, `chord_name`, and `weight` as fixed-length C strings and a `double`. Loading is idempotent: if `loaded == 1`, the function returns immediately. This matches the model's exported conditional distribution `P(chord_name | emotion, scale)` in table form.

### 6.3 Inference Procedure (`chord_gen`)

Given an emotion string and a temperature parameter `T`, `chord_gen` performs a two-stage ancestral sampling procedure:

**Stage 1 — Sample the scale**

Iterate over all entries matching the requested emotion. Aggregate weights by scale name to form a marginal distribution `P(scale | emotion)`. Apply temperature scaling and sample one scale.

**Stage 2 — Sample the chord progression**

Filter entries matching `(emotion, sampled_scale)`. This gives the conditional distribution `P(chord_name | emotion, scale)`. Apply temperature scaling and sample one `chord_name`.

**Temperature scaling** is applied at both stages. For a weight vector `w` of length `n`:

```
w'_i = w_i^(1/T)
w'_i = w'_i / sum(w'_j)
```

- `T → 0`: concentrates mass on the argmax (deterministic)
- `T = 1`: identity transform, preserves the model's trained distribution
- `T > 1`: flattens the distribution toward uniform, increasing variety

Sampling uses the inverse CDF method over a uniform `rand()` draw.

### 6.4 Score Event Scheduling

Once a `chord_name` string is selected (e.g. `Cm7-F7-Bbmaj7-Ebmaj7`), the opcode schedules real Csound note events via `csound->insert_score_event()`. The string is tokenised on `-` separators; each chord token is parsed into its constituent MIDI note numbers (root + intervals determined by the chord quality suffix). Each note is submitted as an `EVTBLK` with:

- `opcod = 'i'` (instrument event)
- `p[1]` = synthesis instrument number
- `p[2]` = start time (advancing by `iDur` per chord)
- `p[3]` = duration
- `p[4]` = MIDI note number
- `p[5]` = amplitude

This fires independently of the calling instrument's timeline — the synthesis instrument receives note events exactly as if they had been written in the score by hand.

---

## 7. System Summary

```
┌─────────────────────────────────────────────────────────────┐
│  OFFLINE (Python, runs once)                                │
│                                                             │
│  jazz_harmony_ml_dataset.csv ──┐                           │
│  pop_harmony_dataset.csv ──────┴─► combine ─► encode       │
│                                         │                   │
│                               LabelEncoder × 3             │
│                               (emotion, scale, progression) │
│                                         │                   │
│                               RandomForestClassifier        │
│                               (2-dim input → 80 classes)   │
│                                         │                   │
│                               predict_proba × all groups   │
│                                         │                   │
│                               chord_gen_data.tsv            │
└─────────────────────────────────────────────────────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────┐
│  RUNTIME (C, inside Csound)                                 │
│                                                             │
│  chord_gen_init ──► load TSV into memory                   │
│                                                             │
│  chord_gen "Joyful" ──► Stage 1: sample scale              │
│                              (temperature-weighted)         │
│                         Stage 2: sample chord_name         │
│                              (temperature-weighted)         │
│                         Parse chord tokens → MIDI notes    │
│                         insert_score_event × n notes       │
│                                                             │
│  Synthesis instr ──► receive p4=MIDI, p5=amp ──► audio out │
└─────────────────────────────────────────────────────────────┘
```

The offline/online split means the C layer contains no floating-point learning logic — only table lookup, probability arithmetic, and Csound API calls. The model is fully replaceable: regenerating the TSV with a different classifier or dataset requires no changes to the C plugin.
