# AHM-Dataset — Affective Harmony Machine

Dataset, training pipeline, and Csound opcode for the paper:

> **"Embedding Live Machine Learning in Csound: Emotion-Driven Harmony via an ONNX-Runtime Opcode"**
> Submitted to ICSC 2026. Active development is on the `jinlan` branch.

---

## Dataset

| Source | Progressions | Rows | Coverage |
|---|---|---|---|
| Jazz | 25 | 1,200 | 12 keys × 4 voicings |
| Pop | 83 | 996 | 12 keys |
| **Combined** | **108** | **2,196** | — |

- 6 emotion classes: Joyful, Vital, Epic, Uneasiness, Depressive, Despair
- 7 scale/mode categories: Ionian, Aeolian, Dorian, Phrygian, Lydian, Mixolydian, Harmonic minor
- Annotations: `jazz_harmony_ml_dataset.csv`, `pop_harmony_dataset.csv`
- Master emotion tags: `emotion_tags.csv`

---

## How to train

```bash
pip install scikit-learn optuna skl2onnx onnxruntime pandas numpy
python train_model.py
```

Outputs to `Csound/opcode/`:
- `gen_model.onnx` — Random Forest classifier (input: `[emotion_id, scale_id]`, output: `probabilities [1, 108]`)
- `gen_data.tsv` — chord name lookup table (1,296 rows)

---

## Csound opcode

See `Csound/opcode/README.md` for build instructions and opcode reference.

```csound
emoChord_init "/path/to/gen_model.onnx", "/path/to/gen_data.tsv"

instr 1
  Sem strget p4
  emoChord Sem, 2, p2, p3, 0.7
endin
```

---

## Paper

LaTeX source: `Paper/ICSC2026_template_latex/ahm_icsc2026.tex`
