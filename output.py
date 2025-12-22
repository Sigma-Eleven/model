# Generated Python code from Wolf DSL
import time

ROLES = ["werewolf", "villager", "seer", "witch"]

# Global Variables
day_count = 1 
game_over = false 
night_count = 0 

# Actions
def action_kill(target):
    print( Werewolf selected target:  + target )

def action_vote(target):
    print( Villager voted for:  + target )

def action_check(target):
    print( Seer checked:  + target )

class GameFlow:
    def __init__(self):
        self.current_phase = None

    def phase_night_phase(self):
        print(f"--- Phase: night_phase ---")
        # Step: werewolf_action
        if ! game_over :
            print(f"Executing step: werewolf_action")
            action_kill()
            pass
        # Step: seer_action
        if ! game_over :
            print(f"Executing step: seer_action")
            action_check()
            pass

    def phase_day_phase(self):
        print(f"--- Phase: day_phase ---")
        # Step: discussion
        if ! game_over :
            print(f"Executing step: discussion")
            pass
        # Step: voting
        if ! game_over :
            print(f"Executing step: voting")
            action_vote()
            pass

def run_game():
    print("=== Starting WerewolfGame ===")
    print( Werewolf game started! ) println ( Roles assigned )
    flow = GameFlow()
    flow.phase_night_phase()
    flow.phase_day_phase()

if __name__ == "__main__":
    run_game()
