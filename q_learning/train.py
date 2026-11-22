
import random
import json
import pandas as pd

from environment import load_episodes, calculate_reward, PATH_COSTS
from agent import QLearningAgent


def train_agent(agent, training_episodes, epochs=500):

    for epoch in range(epochs):

        # Gradually reduce exploration
        epsilon = max(0.05, 1.0 - (epoch / (epochs * 0.8)))

        for episode in training_episodes:

            for i, step in enumerate(episode):

                state = step["state"]
                valid_actions = step["valid_actions"]

                action = agent.choose_action(
                    state, valid_actions, epsilon
                )

                reward = calculate_reward(step, action)

                # Check whether this is the last timestamp
                done = (i == len(episode) - 1)

                if done:
                    target = reward

                else:
                    next_step = episode[i + 1]

                    next_state = next_step["state"]
                    next_actions = next_step["valid_actions"]

                    best_next_value = max(
                        agent.get_q_value(next_state, a)
                        for a in next_actions
                    )

                    target = reward + (
                        agent.discount_factor * best_next_value
                    )

                agent.update(state, action, target)


def evaluate_agent(agent, test_episodes):

    results = []

    for episode in test_episodes:

        for step in episode:

            state = step["state"]
            valid_actions = step["valid_actions"]

            # Use the learned policy without exploration
            action = agent.choose_action(
                state, valid_actions, epsilon=0
            )

            risk = step["path_risks"][action]
            cost = PATH_COSTS[action]
            reward = calculate_reward(step, action)

            # Baseline 1: shortest currently available route
            shortest_action = min(
                valid_actions,
                key=lambda a: PATH_COSTS[a]
            )

            # Baseline 2: currently available route with lowest risk
            safest_action = min(
                valid_actions,
                key=lambda a: (
                    step["path_risks"][a],
                    PATH_COSTS[a]
                )
            )

            results.append({
                "run_id": step["run_id"],
                "time": step["time"],
                "state": state,
                "chosen_action": action,
                "chosen_path": " -> ".join(
                    map(str, get_path(action))
                ),
                "chosen_risk": risk,
                "chosen_cost": cost,
                "reward": reward,
                "shortest_path_action": shortest_action,
                "lowest_risk_action": safest_action,

                 # Risk of each candidate route
                "path0_risk": step["path_risks"][0],
                "path1_risk": step["path_risks"][1],
                "path2_risk": step["path_risks"][2]
            })

    return pd.DataFrame(results)


def get_path(action):

    paths = {
        0: [0, 1, 5],
        1: [0, 2, 3, 5],
        2: [0, 4, 5]
    }

    return paths[action]


def main():

    random.seed(42)

    episodes = load_episodes()

    # Split by complete runs, not individual timestamps
    random.shuffle(episodes)

    split_index = int(len(episodes) * 0.8)

    training_episodes = episodes[:split_index]
    test_episodes = episodes[split_index:]

    print("Total runs:", len(episodes))
    print("Training runs:", len(training_episodes))
    print("Testing runs:", len(test_episodes))

    agent = QLearningAgent()

    print("\nTraining Q-learning agent...")
    train_agent(agent, training_episodes)

    print("Training completed.")

    print("\nEvaluating agent...")
    results = evaluate_agent(agent, test_episodes)

    # Save testing decisions
    results.to_csv(
        "outputs/q_learning_decisions.csv",
        index=False
    )

    # Save learned Q-table
    with open("outputs/q_table.json", "w") as file:
        json.dump(agent.q_table, file, indent=4)

    print("\nEvaluation results:")
    print("Average reward:", round(results["reward"].mean(), 3))
    print("Average selected risk:", round(results["chosen_risk"].mean(), 3))
    print("Average selected cost:", round(results["chosen_cost"].mean(), 3))

    print("\nSelected routes:")
    print(results["chosen_action"].value_counts().sort_index())

    print("\nFiles saved:")
    print("outputs/q_learning_decisions.csv")
    print("outputs/q_table.json")


if __name__ == "__main__":
    main()
