# Generated Python code from Wolf DSL
import time

ROLES = ["werewolf", "villager", "seer", "witch"]

# Global Variables
day_count = 1
game_over = False
night_count = 0

# Actions
def action_kill(target):
    print(f"Werewolf selected target: {target}")

def action_vote(target):
    print(f"Villager voted for: {target}")

def action_check(target):
    print(f"Seer checked: {target}")

class GameFlow:
    def __init__(self):
        self.current_phase = None

    def phase_night_phase(self):
        print(f"--- Phase: night_phase ---")
        # Step: werewolf_action
        if not game_over:
            print(f"Executing step: werewolf_action")
            action_kill(None)
            pass
        # Step: seer_action
        if not game_over:
            print(f"Executing step: seer_action")
            action_check(None)
            pass

    def phase_day_phase(self):
        print(f"--- Phase: day_phase ---")
        # Step: discussion
        if not game_over:
            print(f"Executing step: discussion")
            pass
        # Step: voting
        if not game_over:
            print(f"Executing step: voting")
            action_vote(None)
            pass

def run_game():
    print("=== Starting WerewolfGame ===")
    print("Werewolf game started!")
    print("Roles assigned")
    flow = GameFlow()
    flow.phase_night_phase()
    flow.phase_day_phase()

if __name__ == "__main__":
    run_game()
