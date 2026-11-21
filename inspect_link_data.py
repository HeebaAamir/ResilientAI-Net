import pandas as pd
df = pd.read_csv("data/final_per_link_metrics.csv")
print("shape:", df.shape, "| runs:", df.run_id.nunique(), "| links:", sorted(df.link_id.unique()))
print("delay_ms == -1:", (df.delay_ms == -1).mean().round(2), "(share with no traffic)")
print("loss_rate > 0:", (df.loss_rate > 0).sum(), "rows")
print("queue_occ > 0:", (df.queue_occ > 0).sum(), "rows")
print("link_up == 0:", (df.link_up == 0).sum(), "rows")
print(df[df.link_up == 0].groupby("link_id").run_id.nunique().rename("runs where this link goes down"))