import os
import joblib
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import GroupKFold, cross_val_predict
from sklearn.metrics import precision_score, recall_score, f1_score, confusion_matrix

DATA = "data/final_multi_failure_dataset.csv"

df = pd.read_csv(DATA).sort_values(["runId", "sampleTime"]).reset_index(drop=True)

# trends over time inside each run (past information only)
g = df.groupby("runId")
df["delay_diff"] = g["avgDelayMs"].diff().fillna(0)
df["pdr_diff"] = g["pdr"].diff().fillna(0)
df["thr_diff"] = g["throughputMbps"].diff().fillna(0)
df["delay_mean3"] = g["avgDelayMs"].transform(lambda s: s.rolling(3, min_periods=1).mean())

FEATS = ["pdr", "avgDelayMs", "throughputMbps", "txPackets",
         "delay_diff", "pdr_diff", "thr_diff", "delay_mean3"]
X, y, runs = df[FEATS], df["futureFailure"], df["runId"]

rf = RandomForestClassifier(n_estimators=200, max_depth=6, min_samples_leaf=3,
                            class_weight="balanced", random_state=42, n_jobs=-1)

# test on runs the model has never seen (split by run, as the proposal says)
proba = cross_val_predict(rf, X, y, groups=runs, cv=GroupKFold(n_splits=5),
                          method="predict_proba")[:, 1]

print(f"rows={len(df)} runs={runs.nunique()} positives={int(y.sum())}")
for th in [0.3, 0.4, 0.5, 0.6, 0.7]:
    pred = (proba >= th).astype(int)
    tn, fp, fn, tp = confusion_matrix(y, pred, labels=[0, 1]).ravel()
    print(f"th={th}  precision={precision_score(y, pred, zero_division=0):.2f}  "
          f"recall={recall_score(y, pred):.2f}  F1={f1_score(y, pred):.2f}  "
          f"false_alarm={fp/(fp+tn):.2f}")

rf.fit(X, y)
print(pd.Series(rf.feature_importances_, FEATS).sort_values(ascending=False).round(3))

os.makedirs("models", exist_ok=True)
os.makedirs("outputs", exist_ok=True)
joblib.dump(rf, "models/rf_model.joblib")
out = df[["runId", "sampleTime", "flowId"]].copy()
out["risk"] = proba.round(3)
out["risk_level"] = pd.cut(out["risk"], [-0.01, 0.3, 0.7, 1.01], labels=["low", "med", "high"])
out.to_csv("outputs/risk_scores.csv", index=False)
print("saved model + outputs/risk_scores.csv")