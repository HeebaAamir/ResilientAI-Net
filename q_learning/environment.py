
from pathlib import Path
import pandas as pd

# Project folders
BASE_DIR = Path(__file__).resolve().parent.parent

METRICS_FILE = BASE_DIR / "data" / "final_per_link_metrics.csv"
RISK_FILE = BASE_DIR / "outputs" / "risk_scores_per_link.csv"

# Three possible routes from the ns-3 topology
PATHS = {
    0: [0, 1],
    1: [2, 3, 4],
    2: [5, 6]
}

# Original routing costs from the topology
PATH_COSTS = {
    0: 2,
    1: 12,
    2: 14
}


def get_risk_category(risk):
    if risk <= 0.30:
        return "L"
    elif risk <= 0.70:
        return "M"
    else:
        return "H"


def load_episodes():
    # Read both datasets
    metrics = pd.read_csv(METRICS_FILE)
    risks = pd.read_csv(RISK_FILE)

    keys = ["run_id", "time", "link_id"]

    # Check that both datasets match
    merged = pd.merge(
        metrics,
        risks,
        on=keys,
        how="outer",
        validate="one_to_one",
        indicator=True
    )

    if not (merged["_merge"] == "both").all():
        raise ValueError("The two CSV files do not have matching rows.")

    merged = merged.drop(columns=["_merge"])

    episodes = []

    # Each run is one episode
    for run_id, run_data in merged.groupby("run_id", sort=True):

        steps = []

        # Each timestamp is one state
        for time, group in run_data.groupby("time", sort=True):

            group = group.set_index("link_id")

            if set(group.index) != set(range(7)):
                raise ValueError(
                    f"Run {run_id}, time {time}: expected all 7 links."
                )

            path_risks = {}
            available_actions = []
            state_parts = []

            # Check all three possible routes
            for action, link_ids in PATHS.items():

                path_links = group.loc[link_ids]

                # A route is available only if all its links are up
                available = (
                    path_links["link_up"].astype(int) == 1
                ).all()

                # Maximum risk among the route's links
                risk = float(path_links["risk"].max())

                path_risks[action] = risk

                if available:
                    available_actions.append(action)
                    state_parts.append(get_risk_category(risk))
                else:
                    # D means the route is currently down
                    state_parts.append("D")

            if not available_actions:
                raise ValueError(
                    f"No available route in run {run_id}, time {time}."
                )

            # Example state: L|D|H
            state = "|".join(state_parts)

            steps.append({
                "run_id": int(run_id),
                "time": float(time),
                "state": state,
                "path_risks": path_risks,
                "valid_actions": available_actions
            })

        episodes.append(steps)

    return episodes


def calculate_reward(step, action):
    # We should never select a currently unavailable route
    if action not in step["valid_actions"]:
        return -20

    risk = step["path_risks"][action]
    cost = PATH_COSTS[action]

    # Higher risk and higher route cost reduce the reward
    reward = 10 - (8 * risk) - (2 * cost / 14)

    return reward
