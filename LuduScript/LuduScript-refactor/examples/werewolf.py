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
    Seer = "Seer"
    Witch = "Witch"

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

class WerewolfAction(GameAction):
    def description(self):
        return "狼人夜间讨论并杀人"

    def execute(self, context):
        game = context.game
        wolves = get_players(game, Role.Werewolf, "alive")
        if len(wolves) > 0:
            game.announce("狼人请睁眼", wolves)
            target = dsl_vote(game, wolves, get_players(game, None, "alive"))
            if target != None:
                game.killed_player = target
                game.announce("狼人确认了袭击目标", wolves)
            game.announce("狼人请闭眼", wolves)

class SeerAction(GameAction):
    def description(self):
        return "预言家查验身份"

    def execute(self, context):
        game = context.game
        seer = get_players(game, Role.Seer, "alive")
        if len(seer) > 0:
            game.announce("预言家请睁眼", seer)
            target = dsl_vote(game, seer, get_players(game, None, "alive"))
            if target != None:
                target_role = get_role(game, target)
                if target_role == Role.Werewolf:
                    game.announce("他是狼人", seer)
                else:
                    game.announce("他是好人", seer)
            game.announce("预言家请闭眼", seer)

class WitchAction(GameAction):
    def description(self):
        return "女巫使用药水"

    def execute(self, context):
        game = context.game
        witch = get_players(game, Role.Witch, "alive")
        if len(witch) > 0:
            game.announce("女巫请睁眼", witch)
            if game.killed_player != "":
                if game.witch_save_used == False:
                    game.announce("今晚死的是 " + game.killed_player + ", 是否使用解药?", witch)
                    choice = dsl_vote(game, witch, ["Yes", "No"])
                    if choice == "Yes":
                        game.killed_player = ""
                        game.witch_save_used = True
                        game.announce("女巫使用了解药", witch)
            if game.witch_poison_used == False:
                game.announce("是否使用毒药?", witch)
                choice = dsl_vote(game, witch, ["Yes", "No"])
                if choice == "Yes":
                    target = dsl_vote(game, witch, get_players(game, None, "alive"))
                    if target != None:
                        kill(game, target)
                        game.witch_poison_used = True
                        game.announce("女巫使用了毒药", witch)
            game.announce("女巫请闭眼", witch)

class DayAction(GameAction):
    def description(self):
        return "公布死讯，讨论，投票"

    def execute(self, context):
        game = context.game
        if game.killed_player != "":
            game.announce("昨晚 " + game.killed_player + " 死亡")
            kill(game, game.killed_player)
            game.killed_player = ""
        else:
            game.announce("昨晚是个平安夜")
        wolves = len(get_players(game, Role.Werewolf, "alive"))
        villagers = len(get_players(game, Role.Villager, "alive"))
        seer = len(get_players(game, Role.Seer, "alive"))
        witch = len(get_players(game, Role.Witch, "alive"))
        good_count = villagers + seer + witch
        if wolves == 0:
            stop_game(game, "游戏结束: 好人胜利!")
        else:
            if wolves >= good_count:
                stop_game(game, "游戏结束: 狼人胜利!")
            else:
                alive = get_players(game, None, "alive")
                dsl_discussion(game, alive)
                voted_out = dsl_vote(game, alive, alive)
                if voted_out != None:
                    game.announce(voted_out + " 被投票出局")
                    kill(game, voted_out)
                    w = len(get_players(game, Role.Werewolf, "alive"))
                    g = len(get_players(game, Role.Villager, "alive")) + len(get_players(game, Role.Seer, "alive")) + len(get_players(game, Role.Witch, "alive"))
                    if w == 0:
                        stop_game(game, "游戏结束: 好人胜利!")
                    else:
                        if w >= g:
                            stop_game(game, "游戏结束: 狼人胜利!")
                else:
                    game.announce("平票，无人出局")

class WerewolfGame(Game):
    def __init__(self, players_data, event_emitter=None, input_handler=None):
        super().__init__("Werewolf", players_data, event_emitter, input_handler)
        self.killed_player = ""
        self.witch_save_used = False
        self.witch_poison_used = False

    def _init_phases(self):
        Night = GamePhase("夜晚")
        WerewolfStep = GameStep("WerewolfStep", [Role.Werewolf], WerewolfAction())
        Night.add_step(WerewolfStep)
        SeerStep = GameStep("SeerStep", [Role.Seer], SeerAction())
        Night.add_step(SeerStep)
        WitchStep = GameStep("WitchStep", [Role.Witch], WitchAction())
        Night.add_step(WitchStep)
        self.phases.append(Night)
        Day = GamePhase("白天")
        DayStep = GameStep("DayStep", [], DayAction())
        Day.add_step(DayStep)
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
        num_players = len(shuffled)
        for i in [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]:
            if i < num_players:
                p = shuffled[i]
                if i == 0:
                    set_data(game, p, "role", Role.Werewolf)
                else:
                    if i == 1:
                        set_data(game, p, "role", Role.Werewolf)
                    else:
                        if i == 2:
                            set_data(game, p, "role", Role.Seer)
                        else:
                            if i == 3:
                                set_data(game, p, "role", Role.Witch)
                            else:
                                set_data(game, p, "role", Role.Villager)
        game.announce("游戏开始，身份已分配")
        # Config: min=6, max=12
        self.announce(f"Game started with {len(self.players)} players.")

Game = WerewolfGame
