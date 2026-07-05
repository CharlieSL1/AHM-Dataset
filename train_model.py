#!/usr/bin/env python3
"""
train_model.py
Replicate the training pipeline from Model/Encoder.ipynb:
  - Optuna-tuned Random Forest for chord progression (clf_prog)
  - Optuna-tuned Random Forest for voicing          (clf_voicing, jazz-only)
Export clf_prog to ONNX for the gen Csound opcode.

Requirements:
    pip install optuna scikit-learn skl2onnx onnxruntime numpy pandas

Usage:
    python train_model.py

Outputs:
    Csound/opcode/gen_model.onnx  — ONNX model (input: [1,2] float32 → probabilities: [1,108] float32)
    Csound/opcode/gen_data.tsv    — emotion / scale / progression lookup table
"""

import numpy as np
import pandas as pd
import optuna
from pathlib import Path
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import cross_val_score, StratifiedKFold, train_test_split
from sklearn.preprocessing import LabelEncoder
from sklearn.metrics import accuracy_score
from skl2onnx import convert_sklearn
from skl2onnx.common.data_types import FloatTensorType

optuna.logging.set_verbosity(optuna.logging.WARNING)

ROOT = Path(__file__).parent

# ── Load and combine datasets ─────────────────────────────────────────────────
jazz = pd.read_csv(ROOT / "jazz_harmony_ml_dataset.csv")
pop  = pd.read_csv(ROOT / "pop_harmony_dataset.csv")

jazz_df = jazz[["Key", "Chord_Progression", "ChordName", "Emotion", "Scale"]].copy()
pop_df  = pop[["Key",  "Chord_Progression", "ChordName", "Emotion", "Scale"]].copy()
data = pd.concat([jazz_df, pop_df], ignore_index=True)

print(f"Jazz: {len(jazz)} | Pop: {len(pop)} | Combined: {len(data)}")

# ── Label encoders ────────────────────────────────────────────────────────────
le_emotion = LabelEncoder().fit(data["Emotion"])
le_scale   = LabelEncoder().fit(data["Scale"])
le_prog    = LabelEncoder().fit(data["Chord_Progression"])
le_voicing = LabelEncoder().fit(jazz["Voicing"])

# ── Training arrays ───────────────────────────────────────────────────────────
X_all = np.column_stack([
    le_emotion.transform(data["Emotion"]),
    le_scale.transform(data["Scale"]),
]).astype(np.float32)
y_prog = le_prog.transform(data["Chord_Progression"])

X_jazz = np.column_stack([
    le_emotion.transform(jazz["Emotion"]),
    le_scale.transform(jazz["Scale"]),
]).astype(np.float32)
y_voicing = le_voicing.transform(jazz["Voicing"])

# For Optuna CV: filter progressions with <5 total samples to avoid StratifiedKFold
# failures on rare progressions. Final model still trains on all data.
MIN_CV = 5
prog_counts = pd.Series(y_prog).value_counts()
cv_mask = pd.Series(y_prog).isin(prog_counts[prog_counts >= MIN_CV].index).values
data_cv   = data[cv_mask].reset_index(drop=True)
X_cv_all  = X_all[cv_mask]
y_cv_prog = le_prog.transform(data_cv["Chord_Progression"])

X_tr_p, X_te_p, ytr_p, yte_p = train_test_split(
    X_cv_all, y_cv_prog, test_size=0.2, random_state=42, stratify=data_cv["Emotion"])
X_tr_v, X_te_v, ytr_v, yte_v = train_test_split(
    X_jazz, y_voicing,   test_size=0.2, random_state=42, stratify=jazz["Emotion"])

n_filtered = (~cv_mask).sum()
print(f"CV data: {cv_mask.sum()} rows ({data_cv['Chord_Progression'].nunique()} progressions, "
      f"{n_filtered} rows from rare progressions excluded from tuning only)")

N_TRIALS = 50
CV = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)

# ── Optuna tuning ─────────────────────────────────────────────────────────────
def objective(trial):
    params = {
        "n_estimators":      trial.suggest_int("n_estimators", 50, 500, step=50),
        "max_depth":         trial.suggest_int("max_depth", 2, 32),
        "min_samples_split": trial.suggest_int("min_samples_split", 2, 20),
        "min_samples_leaf":  trial.suggest_int("min_samples_leaf", 1, 10),
        "max_features":      trial.suggest_categorical("max_features", ["sqrt", "log2", 1.0]),
        "random_state": 42,
    }
    s_p = cross_val_score(RandomForestClassifier(**params), X_tr_p, ytr_p,
                          cv=CV, scoring="accuracy", n_jobs=-1).mean()
    s_v = cross_val_score(RandomForestClassifier(**params), X_tr_v, ytr_v,
                          cv=CV, scoring="accuracy", n_jobs=-1).mean()
    return (s_p + s_v) / 2

print(f"Running Optuna ({N_TRIALS} trials)...")
study = optuna.create_study(direction="maximize", study_name="harmony_unified_rf")
study.optimize(objective, n_trials=N_TRIALS, show_progress_bar=True)

best_params = {**study.best_params, "random_state": 42}
print(f"Best CV accuracy: {study.best_value:.4f}")
print(f"Best params: {best_params}")

# ── Train final classifiers ───────────────────────────────────────────────────
clf_prog    = RandomForestClassifier(**best_params)
clf_voicing = RandomForestClassifier(**best_params)
clf_prog.fit(X_tr_p, ytr_p)
clf_voicing.fit(X_tr_v, ytr_v)
print(f"\nTest accuracy  progression={accuracy_score(yte_p, clf_prog.predict(X_te_p)):.3f}"
      f"  voicing={accuracy_score(yte_v, clf_voicing.predict(X_te_v)):.3f}")

# Retrain on full dataset for ONNX export — ensures all classes are present
clf_prog    = RandomForestClassifier(**best_params)
clf_voicing = RandomForestClassifier(**best_params)
clf_prog.fit(X_all, y_prog)
clf_voicing.fit(X_jazz, y_voicing)
print(f"Final model classes: progression={len(clf_prog.classes_)}  voicing={len(clf_voicing.classes_)}")

# ── Export clf_prog to ONNX ───────────────────────────────────────────────────
# Input:  float32 tensor "input" of shape [N, 2]  (emotion_id, scale_id)
# Output: float32 tensor "probabilities" of shape [N, 108]

out_onnx = ROOT / "Csound" / "opcode" / "gen_model.onnx"

onnx_model = convert_sklearn(
    clf_prog,
    initial_types=[("input", FloatTensorType([None, 2]))],
    options={id(clf_prog): {"zipmap": False}},
    target_opset=18,
)
with open(out_onnx, "wb") as f:
    f.write(onnx_model.SerializeToString())
print(f"ONNX     → {out_onnx}")

# ── Export lookup table ───────────────────────────────────────────────────────
# TSV columns: emotion, scale, emotion_id, scale_id, prog_id, key, chord_name
dedup = data.drop_duplicates(["Emotion", "Scale", "Key", "Chord_Progression"]).copy()
dedup["emotion_id"] = le_emotion.transform(dedup["Emotion"]).astype(int)
dedup["scale_id"]   = le_scale.transform(dedup["Scale"]).astype(int)
dedup["prog_id"]    = le_prog.transform(dedup["Chord_Progression"]).astype(int)

out_data = ROOT / "Csound" / "opcode" / "gen_data.tsv"
(dedup[["Emotion", "Scale", "emotion_id", "scale_id", "prog_id", "Key", "ChordName"]]
 .rename(columns={"Emotion": "emotion", "Scale": "scale",
                  "Key": "key",        "ChordName": "chord_name"})
 .to_csv(out_data, sep="\t", index=False))
print(f"Data     → {out_data}  ({len(dedup)} rows)")
