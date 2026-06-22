#!/usr/bin/env python3
"""
export_model.py
Train the chord progression model and export prediction data for the
chord_gen Csound opcode.

Run this once (or after retraining) to regenerate the data file:
    python export_model.py

Output: Csound/opcode/chord_gen_data.tsv
"""

import numpy as np
import pandas as pd
from pathlib import Path
from sklearn.ensemble import RandomForestClassifier
from sklearn.preprocessing import LabelEncoder

ROOT = Path(__file__).parent

# ── Load data ─────────────────────────────────────────────────────────────
jazz = pd.read_csv(ROOT / "jazz_harmony_ml_dataset.csv")
pop  = pd.read_csv(ROOT / "pop_harmony_dataset.csv")
data = pd.concat([
    jazz[["Key", "Chord_Progression", "ChordName", "Emotion", "Scale"]],
    pop[["Key",  "Chord_Progression", "ChordName", "Emotion", "Scale"]],
], ignore_index=True)

# ── Fit encoders ──────────────────────────────────────────────────────────
le_emotion = LabelEncoder().fit(data["Emotion"])
le_scale   = LabelEncoder().fit(data["Scale"])
le_prog    = LabelEncoder().fit(data["Chord_Progression"])

# ── Train on full dataset (best params from Optuna) ───────────────────────
X = np.column_stack([
    le_emotion.transform(data["Emotion"]),
    le_scale.transform(data["Scale"]),
])
y = le_prog.transform(data["Chord_Progression"])

clf = RandomForestClassifier(
    n_estimators=100, max_depth=3,
    min_samples_split=16, min_samples_leaf=3,
    max_features=1.0, random_state=42,
).fit(X, y)

# ── Build lookup tables ───────────────────────────────────────────────────
dedup = data.drop_duplicates(["Emotion", "Scale", "Key", "Chord_Progression"])
chordname = (
    data.drop_duplicates(["Key", "Chord_Progression"])
    .set_index(["Key", "Chord_Progression"])["ChordName"]
    .to_dict()
)
valid_pairs = {
    (em, sc): list(zip(grp["Key"], grp["Chord_Progression"]))
    for (em, sc), grp in dedup.groupby(["Emotion", "Scale"])
}

# ── Compute model-weighted scores for every valid pair ────────────────────
col_of = {c: i for i, c in enumerate(clf.classes_)}
rows = []

for (emotion, scale), pairs in valid_pairs.items():
    x     = np.array([[le_emotion.transform([emotion])[0],
                        le_scale.transform([scale])[0]]])
    probs = clf.predict_proba(x)[0]

    scores = []
    for key, prog in pairs:
        pe = le_prog.transform([prog])[0]
        w  = float(probs[col_of[pe]]) if pe in col_of else 1e-10
        scores.append((key, chordname.get((key, prog), ""), w))

    total = sum(w for *_, w in scores)
    for key, chord_name, w in scores:
        rows.append({
            "emotion":    emotion,
            "scale":      scale,
            "key":        key,
            "chord_name": chord_name,
            "weight":     w / total,
        })

# ── Write TSV ─────────────────────────────────────────────────────────────
out = ROOT / "Csound" / "opcode" / "chord_gen_data.tsv"
pd.DataFrame(rows).to_csv(out, sep="\t", index=False, float_format="%.8f")
print(f"Exported {len(rows)} entries → {out}")
