from src.Game import Game, GameAction, GamePhase, GameStep, ActionContext
from src.Player import Player
from enum import Enum
from typing import List, Dict, Any, Optional
from pathlib import Path
import random
import time

class Role(str, Enum):
    Spy = "Spy"
    Civilian = "Civilian"

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
    if "start" in prompts: game.announce(prompts["start"], None, "#@")
    votes = {name: 0 for name in candidates}
    for voter_name in voters:
        voter = game.players.get(voter_name)
        if not voter or not voter.is_alive: continue
        prompt = prompts["prompt"].format(voter_name)
        # Filter candidates: remove self, dead players, and non-role entities
        voter_candidates = []
        for c in candidates:
            p_target = game.players.get(c)
            if p_target:
                if not p_target.is_alive or not p_target.role: continue
                if c == voter_name: continue
            voter_candidates.append(c)
        if not voter_candidates: continue
        target = voter.choose(prompt, voter_candidates)
        if target in votes: votes[target] += 1
        if "action" in prompts: game.announce(prompts["action"].format(voter_name, target), None, "#:")
    max_votes = max(votes.values()) if votes else 0
    targets = [name for name, count in votes.items() if count == max_votes and count > 0]
    if len(targets) == 1:
        winner = targets[0]
        if "result_out" in prompts: game.announce(prompts["result_out"].format(winner), None, "#!")
        return winner
    if "result_tie" in prompts: game.announce(prompts["result_tie"], None, "#@")
    return None

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

class DescribeAction(GameAction):
    def description(self):
        return "玩家轮流描述自己的词语"

    def execute(self, context):
        game = context.game
        alive_players = get_players(game, None, "alive")
        for p in alive_players:
            word = get_data(game, p, "word")
            game.announce("请 " + p + " 开始描述", [p])
            dsl_discussion(game, [p])

class VoteAction(GameAction):
    def description(self):
        return "投票选出卧底"

    def execute(self, context):
        game = context.game
        alive_players = get_players(game, None, "alive")
        target = dsl_vote(game, alive_players, alive_players)
        if target != None:
            game.announce("投票结果：" + target + " 被处决")
            kill(game, target)
            target_role = get_role(game, target)
            if target_role == Role.Spy:
                game.announce("他竟然真的是卧底！")
            else:
                game.announce("他只是个无辜的平民...")

class SpyGameGame(Game):
    def __init__(self, players_data, event_emitter=None, input_handler=None):
        super().__init__("SpyGame", players_data, event_emitter, input_handler)
        self.civilian_word = "蒸汽"
        self.spy_word = "桑拿"
        self.winner = ""

    def _init_phases(self):
        GameLoop = GamePhase("主循环")
        描述 = GameStep("描述", [], DescribeAction())
        GameLoop.add_step(描述)
        投票 = GameStep("投票", [], VoteAction())
        GameLoop.add_step(投票)
        self.phases.append(GameLoop)

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
        spy_player = shuffled[0]
        set_data(game, spy_player, "role", Role.Spy)
        set_data(game, spy_player, "word", game.spy_word)
        for i in [1, 2, 3]:
            x = 1
        for p in shuffled:
            if p == spy_player:
                set_data(game, p, "role", Role.Spy)
                set_data(game, p, "word", game.spy_word)
            else:
                set_data(game, p, "role", Role.Civilian)
                set_data(game, p, "word", game.civilian_word)
        game.announce("游戏开始，身份和词语已分发")
        # Config: min=4, max=10
        self.announce(f"Game started with {len(self.players)} players.")

Game = SpyGameGame
