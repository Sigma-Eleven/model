from src.Game import Game, GameAction, GamePhase, GameStep, ActionContext
from src.Player import Player
from enum import Enum
from typing import List, Dict, Any, Optional
from pathlib import Path
import random
import time

class Role(str, Enum):
    Werewolf = "Werewolf"
    Villager = "Villager"

# Helper Functions
def get_players(game, role=None, status='alive'):
    target_role = role.value if hasattr(role, 'value') else role
    players = []
    for p in game.players.values():
        # If status is not 'all', only include players with a valid role
        if status != 'all' and not p.role:
            continue
        if status == 'alive' and not p.is_alive: continue
        if status == 'dead' and p.is_alive: continue
        if target_role and p.role != target_role: continue
        players.append(p.name)
    return players

def get_role(game, player_name):
    if player_name in game.players:
        return game.players[player_name].role
    return None

def kill(game, player_name):
    if player_name in game.players:
        game.players[player_name].is_alive = False

def stop_game(game, msg=""):
    if msg: game.announce(msg)
    game.stop_game()

def set_data(game, player_name, key, value):
    if player_name in game.players:
        p = game.players[player_name]
        if key == "role":
            p.role = value
        else:
            setattr(p, key, value)

def get_data(game, player_name, key):
    if player_name in game.players:
        p = game.players[player_name]
        if key == "role":
            return p.role
        return getattr(p, key, None)
    return None

def shuffle(game, lst):
    new_list = list(lst)
    random.shuffle(new_list)
    return new_list

def dsl_vote(game, voters, candidates, prompts=None):
    if prompts is None:
        prompts = {
            "start": "Start Voting",
            "prompt": "{0}, please choose your target",
            "action": "{0} voted for {1}",
            "result_out": "Voting result: {0} is out",
            "result_tie": "Voting tie, no one is out"
        }
    return game.process_vote(voters, candidates, prompts)

def dsl_discussion(game, participants, prompts=None):
    if prompts is None:
        prompts = {
            "start": "Start Discussion",
            "prompt": "It is {0}'s turn to speak",
            "speech": "{0}: {1}",
            "ready_msg": "{0} is ready ({1}/{2})",
            "timeout": "Discussion timeout",
            "alive_players": "Current alive: {0}"
        }
    return game.process_discussion(participants, prompts)

class WolfAction(GameAction):
    def description(self):
        return "狼人选择一名目标"

    def execute(self, context):
        game = context.game
        wolves = get_players(game, Role.Werewolf, "alive")
        target = dsl_vote(game, wolves, get_players(game, None, "alive"))
        if target != None:
            game.killed_player = target
            game.announce("狼人已选定目标", wolves)

class DayAction(GameAction):
    def description(self):
        return "公布死讯并进行公投"

    def execute(self, context):
        game = context.game
        if game.killed_player != "":
            game.announce("昨晚死亡的玩家是: " + game.killed_player)
            kill(game, game.killed_player)
            game.killed_player = ""
        else:
            game.announce("昨晚是个平安夜")
        w_count = len(get_players(game, Role.Werewolf, "alive"))
        v_count = len(get_players(game, Role.Villager, "alive"))
        if w_count == 0:
            stop_game(game, "好人胜利！")
        else:
            if w_count >= v_count:
                stop_game(game, "狼人胜利！")
            else:
                alive = get_players(game, None, "alive")
                dsl_discussion(game, alive)
                voted = dsl_vote(game, alive, alive)
                if voted != None:
                    game.announce(voted + " 被投票处决")
                    kill(game, voted)

class SimpleWerewolfGame(Game):
    def __init__(self, players_data, event_emitter=None, input_handler=None):
        super().__init__("SimpleWerewolf", players_data, event_emitter, input_handler)
        self.killed_player = ""

    def _init_phases(self):
        Night = GamePhase("夜晚阶段")
        WolfKill = GameStep("WolfKill", [Role.Werewolf], WolfAction())
        Night.add_step(WolfKill)
        self.phases.append(Night)
        Day = GamePhase("白天阶段")
        PublicVote = GameStep("PublicVote", [], DayAction())
        Day.add_step(PublicVote)
        self.phases.append(Day)

    def check_game_over(self):
        return False # TODO: Implement game over logic

    def setup_game(self):
        game = self
        config, prompts, player_config_map = self.load_basic_config(Path(__file__).parent)
        # Initialize players from players_data if not already in player_config_map
        for p_data in self._players_data:
            name = p_data.get('player_name') or p_data.get('name')
            if name and name not in player_config_map:
                player_config_map[name] = p_data

        for name, p_data in player_config_map.items():
            p_prompts = prompts.get('system', {}).copy()
            if 'prompt' in p_prompts: p_prompts['PROMPT'] = p_prompts['prompt']
            if 'reminder' in p_prompts: p_prompts['REMINDER'] = p_prompts['reminder']
            self.players[name] = Player(name, None, p_data, p_prompts, self.logger, self.input_handler, self.event_emitter)
        players = get_players(game, None, "all")
        shuffled = shuffle(game, players)
        for i in [0, 1, 2, 3, 4, 5]:
            if i < len(shuffled):
                p = shuffled[i]
                if i == 0:
                    set_data(game, p, "role", Role.Werewolf)
                else:
                    set_data(game, p, "role", Role.Villager)
        game.announce("游戏开始，身份已秘密分配")
        # Config: min=3, max=6
        self.announce(f"Game started with {len(self.players)} players.")

Game = SimpleWerewolfGame
