import os
import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import GroupKFold, cross_val_predict
from sklearn.metrics import precision_score, recall_score, f1_score, confusion_matrix

DATA = "data/final_per_link_metrics.csv"
W = 2   # predict: will this link go down within the next W seconds?

df = pd.read_csv(DATA).sort_values(["run_id", "link_id", "time"]).reset_index(drop=True)

# ---- clean: -1 means "no traffic measured" ----
df["has_traffic"] = (df["delay_ms"] >= 0).astype(int)
df["delay"] = df["delay_ms"].where(df["delay_ms"] >= 0, 0.0)
df["loss"] = df["loss_rate"].where(df["loss_rate"] >= 0, 0.0)

# ---- label: link is up now AND goes down within next W seconds ----
g = df.groupby(["run_id", "link_id"])
fut = pd.concat([g["link_up"].shift(-k) for k in range(1, W + 1)], axis=1)
df["label"] = (fut.min(axis=1) == 0).astype(int)
df["has_window"] = fut.notna().all(axis=1)

# ---- features (current and past only) ----
g = df.groupby(["run_id", "link_id"])
df["delay_mean3"] = g["delay"].transform(lambda s: s.rolling(3, min_periods=1).mean())
df["delay_diff"] = g["delay"].diff().fillna(0)
df["loss_mean3"] = g["loss"].transform(lambda s: s.rolling(3, min_periods=1).mean())
df["loss_diff"] = g["loss"].diff().fillna(0)
df["traffic_mean3"] = g["has_traffic"].transform(lambda s: s.rolling(3, min_periods=1).mean())
df["links_down_now"] = (1 - df["link_up"]).groupby([df["run_id"], df["time"]]).transform("sum")

FEATS = ["delay", "loss", "queue_occ", "has_traffic", "delay_mean3", "delay_diff",
         "loss_mean3", "loss_diff", "traffic_mean3", "links_down_now"]
USE_LINKS_DOWN = False    # run once with True, then once with False
if not USE_LINKS_DOWN:
    FEATS.remove("links_down_now")
FEATS = [f for f in FEATS if df[f].nunique() > 1]     # drop useless constant columns
print("features used:", FEATS)

train = df[(df["link_up"] == 1) & df["has_window"]]
X, y, runs = train[FEATS], train["label"], train["run_id"]
print(f"training rows={len(train)} runs={runs.nunique()} positives={int(y.sum())}")

rf = RandomForestClassifier(n_estimators=200, max_depth=6, min_samples_leaf=3,
                            class_weight="balanced", random_state=42, n_jobs=-1)
oof = cross_val_predict(rf, X, y, groups=runs, cv=GroupKFold(n_splits=5),
                        method="predict_proba")[:, 1]

for th in [0.3, 0.4, 0.5, 0.6, 0.7]:
    pred = (oof >= th).astype(int)
    tn, fp, fn, tp = confusion_matrix(y, pred, labels=[0, 1]).ravel()
    print(f"th={th}  precision={precision_score(y, pred, zero_division=0):.2f}  "
          f"recall={recall_score(y, pred):.2f}  F1={f1_score(y, pred):.2f}  "
          f"false_alarm={fp/(fp+tn):.2f}")

rf.fit(X, y)
print(pd.Series(rf.feature_importances_, FEATS).sort_values(ascending=False).round(3))

# ---- risk file for Urooj: one row per link per second ----
df["risk"] = rf.predict_proba(df[FEATS])[:, 1]
df.loc[train.index, "risk"] = oof          # honest (unseen-run) scores where available
df.loc[df["link_up"] == 0, "risk"] = 1.0   # already down
df["risk_level"] = pd.cut(df["risk"], [-0.01, 0.3, 0.7, 1.01], labels=["low", "med", "high"])

os.makedirs("models", exist_ok=True)
os.makedirs("outputs", exist_ok=True)
joblib.dump({"model": rf, "features": FEATS, "W": W}, "models/rf_link_model.joblib")
df[["run_id", "time", "link_id", "risk", "risk_level"]].round(3).to_csv(
    "outputs/risk_scores_per_link.csv", index=False)
print("saved models/rf_link_model.joblib and outputs/risk_scores_per_link.csv")