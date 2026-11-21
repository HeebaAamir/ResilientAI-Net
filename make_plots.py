import joblib
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from sklearn.metrics import precision_score, recall_score, f1_score

W = 2
data = pd.read_csv("data/final_per_link_metrics.csv")
risk = pd.read_csv("outputs/risk_scores_per_link.csv")
df = data.merge(risk, on=["run_id", "time", "link_id"]).sort_values(["run_id", "link_id", "time"])

# recompute the same label used in training
g = df.groupby(["run_id", "link_id"])
fut = pd.concat([g["link_up"].shift(-k) for k in range(1, W + 1)], axis=1)
df["label"] = (fut.min(axis=1) == 0).astype(int)
ev = df[(df["link_up"] == 1) & fut.notna().all(axis=1)]

# 1) feature importance
m = joblib.load("models/rf_link_model.joblib")
imp = pd.Series(m["model"].feature_importances_, m["features"]).sort_values()
plt.figure(figsize=(6, 4)); imp.plot.barh(color="#3b6ea5")
plt.title("Random Forest feature importance"); plt.xlabel("Importance"); plt.tight_layout()
plt.savefig("outputs/fig_feature_importance.png", dpi=150); plt.close()

# 2) precision / recall / F1 vs threshold
ths = [i / 20 for i in range(2, 19)]
P = [precision_score(ev.label, ev.risk >= t, zero_division=0) for t in ths]
R = [recall_score(ev.label, ev.risk >= t) for t in ths]
F = [f1_score(ev.label, ev.risk >= t, zero_division=0) for t in ths]
plt.figure(figsize=(6, 4))
plt.plot(ths, P, label="Precision"); plt.plot(ths, R, label="Recall"); plt.plot(ths, F, label="F1")
plt.xlabel("Risk threshold"); plt.ylabel("Score"); plt.title("Metrics vs threshold (unseen runs)")
plt.legend(); plt.grid(alpha=.3); plt.tight_layout()
plt.savefig("outputs/fig_threshold_metrics.png", dpi=150); plt.close()

# 3) one example run: risk of links 0 and 2 over time, with failure moments
run = df["run_id"].min()
fig, ax = plt.subplots(figsize=(7, 4))
for l in (0, 2):
    s = df[(df.run_id == run) & (df.link_id == l)]
    down = s[s.link_up == 0].time
    s_up = s[s.link_up == 1]                      # only seconds before the link is down
    ax.plot(s_up.time, s_up.risk, marker="o", label=f"Link {l} risk")
    if len(down):
        ax.axvline(down.min(), linestyle="--", color="gray")
ax.set_xlabel("Time (s)"); ax.set_ylabel("Risk"); ax.set_title(f"Run {run}: predicted risk (dashed = link goes down)")
ax.legend(); ax.grid(alpha=.3); plt.tight_layout()
plt.savefig("outputs/fig_example_run.png", dpi=150); plt.close()
print("saved 3 figures in outputs/")