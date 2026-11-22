
import random


class QLearningAgent:

    def __init__(self, learning_rate=0.1, discount_factor=0.9):
        self.learning_rate = learning_rate
        self.discount_factor = discount_factor

        # Each state has three possible action values
        self.q_table = {}

    def get_q_value(self, state, action):
        if state not in self.q_table:
            self.q_table[state] = [0.0, 0.0, 0.0]

        return self.q_table[state][action]

    def choose_action(self, state, valid_actions, epsilon):

        # Exploration: try a random available route
        if random.random() < epsilon:
            return random.choice(valid_actions)

        # Exploitation: choose the route with the highest Q-value
        return max(
            valid_actions,
            key=lambda action: self.get_q_value(state, action)
        )

    def update(self, state, action, target):

        current_value = self.get_q_value(state, action)

        new_value = current_value + self.learning_rate * (
            target - current_value
        )

        self.q_table[state][action] = new_value
