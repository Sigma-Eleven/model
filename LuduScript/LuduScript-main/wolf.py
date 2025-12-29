from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
import json
import os
from pathlib import Path
import random
import sys
import time
from typing import Any, Callable, Dict, List, Optional, Union

# Import base Game classes
try:
    from src.Game import Game, ActionContext, GameAction, GameStep, GamePhase, Player
except ImportError:
    # Fallback if Game.py is not found (for standalone testing)
    sys.path.append(str(Path(__file__).resolve().parent))
    from src.Game import Game, ActionContext, GameAction, GameStep, GamePhase, Player

# models.py
class Role(Enum):
    WEREWOLF = "werewolf"
    VILLAGER = "villager"
    SEER = "seer"
    WITCH = "witch"
    HUNTER = "hunter"
    GUARD = "guard"


class DeathReason(Enum):
    KILLED_BY_WEREWOLF = "在夜晚被杀害"
    POISONED_BY_WITCH = "被女巫毒杀"
    VOTED_OUT = "被投票出局"
    SHOT_BY_HUNTER = "被猎人带走"

# -----------------------------------------------------------------------------
# Generated Actions from DSL
# -----------------------------------------------------------------------------

class NightStartAction(GameAction):
    def description(self) -> str:
        return "night_start"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.day_number = game.day_number + 1
        game.announce(f"#@ 第 {game.day_number} 天夜晚降临")
        game.killed_player = ""
        game.announce("#@ 入夜初始化完成，重置死亡标记与守护状态")


class GuardAction(GameAction):
    def description(self) -> str:
        return "guard_action"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.announce("#@ 守卫请睁眼")
        game.prompt = "守卫, 请选择你要守护的玩家(不能连续两晚守护同一个人): "
        game.announce(game.prompt)
        game.alive_players = game.get_alive_players(None)
        game.valid_targets =[]
        for p in game.alive_players:
            if p != game.last_guarded:
                game.valid_targets.append(p)
        game.target = ""
        game.announce(f"#@ 守卫选择守护: {game.target}")
        game.last_guarded = game.target
        game.announce("#@ 守卫请闭眼")
        game.announce("#@ 守卫行动完成，已标记守护目标")


class WerewolfNightAction(GameAction):
    def description(self) -> str:
        return "werewolf_night_action"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.werewolves = game.get_alive_players(["werewolf"])
        if len(game.werewolves) == 0:
            return
        game.announce("#@ 狼人请睁眼")
        game.announce(f"#@ 现在的狼人有: {', '.join(game.werewolves)}")
        if len(game.werewolves) == 1:
            game.announce("#@ 独狼无需讨论，直接进入投票阶段")
        else:
            game.announce("#@ 狼人请开始讨论")
            game.announce("#@ 请轮流发言, 输入 '0' 表示准备好投票")
            game.discussion_rounds = 0
            game.max_discussion_rounds = 5
            game.ready_to_vote = False
            while  not  game.ready_to_vote  and  game.discussion_rounds < game.max_discussion_rounds:
                game.discussion_rounds = game.discussion_rounds + 1
                game.announce(f"#@ 讨论轮次: {game.discussion_rounds}")
                game.ready_to_vote = True
        game.announce("#@ 狼人请投票")
        game.alive_players = game.get_alive_players(None)
        game.kill_target = ""
        game.killed_player = game.kill_target
        game.announce(f"#@ 狼人达成一致, 选择了击杀 {game.killed_player}")
        game.announce("#@ 狼人请闭眼")


class SeerAction(GameAction):
    def description(self) -> str:
        return "seer_action"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.announce("#@ 预言家请睁眼")
        game.prompt = "预言家, 请选择要查验的玩家: "
        game.announce(game.prompt)
        game.alive_players = game.get_alive_players(None)
        game.target = ""
        game.role = ""
        game.identity = "狼人" if(game.role == "werewolf") else "好人"
        game.announce(f"#@ 查验结果: {game.target} 的身份是 {game.identity}")
        game.announce("#@ 预言家请闭眼")


class WitchAction(GameAction):
    def description(self) -> str:
        return "witch_action"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.announce("#@ 女巫请睁眼")
        game.is_guarded = False
        if game.killed_player != "":
            if game.is_guarded:
                game.announce(f"#@ 今晚是个平安夜, {game.killed_player} 被守护了")
            else:
                game.announce(f"#@ 今晚 {game.killed_player} 被杀害了")
                if  not  game.witch_save_used:
                    game.use_save = ""
                    if game.use_save == "y":
                        game.witch_save_used = True
                        game.announce(f"#@ 你使用解药救了 {game.killed_player}")
                        game.killed_player = ""
        if  not  game.witch_poison_used:
            game.use_poison = ""
            if game.use_poison == "y":
                game.alive_players = game.get_alive_players(None)
                game.target = ""
                game.witch_poison_used = True
                game.announce(f"#@ 你使用毒药毒了 {game.target}")
                if game.killed_player == "":
                    game.killed_player = game.target
                else:
                    game.announce(f"#! {game.target} 死了, 原因是被女巫毒杀")
        game.announce("#@ 女巫请闭眼")


class DayStartAction(GameAction):
    def description(self) -> str:
        return "day_start"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.announce("#: 天亮了.")
        game.announce(f"#@ 现在是第 {game.day_number} 天白天")
        if game.killed_player != "":
            game.announce(f"#! {game.killed_player} 死了, 原因是在夜晚被杀害")
            game.role = ""
            if game.role == "hunter":
                game.handle_hunter_shot(game.killed_player)
        else:
            game.announce("#@ 今晚是平安夜")


class DayDiscussionAction(GameAction):
    def description(self) -> str:
        return "day_discussion"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.alive_players = game.get_alive_players(None)
        game.announce(f"#@ 场上存活的玩家: {', '.join(game.alive_players)}")
        for player in game.alive_players:
            game.speech = ""
            game.announce(f"#: {player} 发言: {game.speech}")


class DayVoteAction(GameAction):
    def description(self) -> str:
        return "day_vote"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.announce("#@ 请开始投票")
        game.alive_players = game.get_alive_players(None)
        game.votes = {
        }
        for player in game.alive_players:
            game.votes[player] = 0
        for voter in game.alive_players:
            game.target = ""
            game.votes[game.target] = game.votes[game.target] + 1
            game.announce(f"#: {voter} 投票给 {game.target}")
        game.max_votes = 0
        game.voted_out_players =[]
        for player in game.alive_players:
            if game.votes[player] > game.max_votes:
                game.max_votes = game.votes[player]
                game.voted_out_players =[player]
            elif game.votes[player] == game.max_votes:
                game.voted_out_players.append(player)
        if len(game.voted_out_players) == 1:
            game.voted_out = game.voted_out_players[0]
            game.announce(f"#! 投票结果: {game.voted_out} 被投票出局")
            game.role = ""
            if game.role == "hunter":
                game.handle_hunter_shot(game.voted_out)
        else:
            game.announce("#@ 投票平票, 无人出局")
        game.check_game_over()


class WerewolfGame(Game):
    def __init__(self, players_data, event_emitter=None, input_handler=None):
        super().__init__("Werewolf", players_data, event_emitter, input_handler)
        self.roles = {}
        self.killed_player = None
        self.witch_save_used = False
        self.witch_poison_used = False
        self.day_number = 0
        self.last_guarded = None
        self.game_over = False
        self.winner = None
        # self.players and self.logger are initialized in super().__init__
        self._init_players(players_data)
        # self.phases is initialized in super().run_game -> _init_phases

    def announce(self, message: str, visible_to: list = None, prefix: str = "#@") -> None:
        super().announce(message, visible_to, prefix)

    def _init_players(self, players_data):
        roles_list = [
            Role.WEREWOLF.value, Role.WEREWOLF.value,
            Role.VILLAGER.value, Role.VILLAGER.value,
            Role.SEER.value, Role.WITCH.value, Role.HUNTER.value, Role.GUARD.value
        ]
        # Adjust roles if player count mismatch (simple logic)
        if len(players_data) != len(roles_list):
            # Fallback or error. For now, trim or pad with Villager.
            if len(players_data) > len(roles_list):
                roles_list.extend([Role.VILLAGER.value] * (len(players_data) - len(roles_list)))
            else:
                roles_list = roles_list[:len(players_data)]
        random.shuffle(roles_list)

        for i, p_data in enumerate(players_data):
            name = p_data['player_name']
            role = roles_list[i]
            self.roles[role] = self.roles.get(role, 0) + 1
            # Create Player instance (using inner class)
            player = self._create_player(name, role)
            self.players[name] = player

    def _create_player(self, name, role):
        # Simple inner class for Player implementation
        game_instance = self
        class GamePlayer(Player):
            def __init__(self, name, role):
                # Initialize base Player if needed, though Game.py's Player might differ.
                # Assuming Game.py's Player is simple or we just overwrite.
                self.name = name
                self.role = role
                self.is_alive = True
                self.is_guarded = False
            def speak(self, prompt: str) -> str:
                if game_instance.input_handler:
                    return game_instance.input_handler(game_instance.game_name, self.name, prompt, [], False)
                return input(prompt)
            def choose(self, prompt: str, candidates: List[str], allow_skip: bool = False) -> Optional[str]:
                if game_instance.input_handler:
                    return game_instance.input_handler(game_instance.game_name, self.name, prompt, candidates, allow_skip)
                retries = 0
                max_retries = 3
                while retries < max_retries:
                    game_instance.announce(f"\n{prompt}", [self.name])
                    game_instance.announce(f"候选项: {candidates}", [self.name])
                    choice = input("请输入选择: ").strip()
                    if allow_skip and not choice:
                        return None
                    if choice in candidates:
                        return choice
                    game_instance.announce("无效的选择，请重试。", [self.name])
                    retries += 1
                game_instance.announce("重试次数已达上限。正在随机选择。", [self.name])
                if candidates:
                    selection = random.choice(candidates)
                    game_instance.announce(f"随机选择了: {selection}", [self.name])
                    return selection
                return None

    def _init_phases(self):
        night = GamePhase("Night")
        night.add_step(GameStep(
            name="入夜初始化",
            roles_involved=["all"],
            action=NightStartAction()))
        night.add_step(GameStep(
            name="守卫守护",
            roles_involved=[Role("guard")],
            action=GuardAction()))
        night.add_step(GameStep(
            name="狼人行动",
            roles_involved=[Role("werewolf")],
            action=WerewolfNightAction()))
        night.add_step(GameStep(
            name="预言家查验",
            roles_involved=[Role("seer")],
            action=SeerAction()))
        night.add_step(GameStep(
            name="女巫用药",
            roles_involved=[Role("witch")],
            action=WitchAction()))
        self.phases.append(night)

        day = GamePhase("Day")
        day.add_step(GameStep(
            name="天亮结算",
            roles_involved=["all"],
            action=DayStartAction()))
        day.add_step(GameStep(
            name="白天讨论",
            roles_involved=["all"],
            action=DayDiscussionAction()))
        day.add_step(GameStep(
            name="白天投票",
            roles_involved=["all"],
            action=DayVoteAction()))
        self.phases.append(day)

    def _get_player_by_role(self, role: Role):
        for p in self.players.values():
            if p.role == role.value and p.is_alive:
                return p
        return None

    def _get_alive_players(self, roles: List[Role] = None):
        if roles:
            role_values = [r.value for r in roles]
            return [n for n, p in self.players.items() if p.is_alive and p.role in role_values]
        return [n for n, p in self.players.items() if p.is_alive]

    def get_alive_players(self, roles: List[str] = None):
        role_enums = []
        if roles:
            for r in roles:
                try:
                    role_enums.append(Role(r))
                except ValueError:
                    pass
        return self._get_alive_players(role_enums if roles else None)

    def handle_death(self, player_name, reason):
        if not player_name or not self.players[player_name].is_alive:
            return
        self.players[player_name].is_alive = False
        self.announce(f"{player_name} {reason.value}", self.all_player_names)
        if self.players[player_name].role == Role.HUNTER.value:
            self._handle_hunter_shot(player_name)
        self.check_game_over()

    def _handle_hunter_shot(self, hunter_name):
        hunter = self.players[hunter_name]
        alive_players = self._get_alive_players()
        if not alive_players:
            return
        prompt = "猎人, 请选择要带走的玩家: "
        target = hunter.choose(prompt, alive_players)
        self.announce(f"猎人发动技能, 带走了 {target}", self.all_player_names)
        self.handle_death(target, DeathReason.SHOT_BY_HUNTER)

    def handle_hunter_shot(self, hunter_name):
        self._handle_hunter_shot(hunter_name)

    def check_game_over(self):
        werewolves = self._get_alive_players([Role.WEREWOLF])
        villagers = [n for n, p in self.players.items() if p.is_alive and p.role == Role.VILLAGER.value]
        gods = [n for n, p in self.players.items() if p.is_alive and p.role in [Role.SEER.value, Role.WITCH.value, Role.HUNTER.value, Role.GUARD.value]]

        if not werewolves:
            self.game_over = True
            self.winner = "Villagers"
            self.announce("游戏结束, 好人阵营胜利!", self.all_player_names, "#!")
            return True
        elif not villagers or not gods:
            self.game_over = True
            self.winner = "Werewolves"
            self.announce("游戏结束, 狼人阵营胜利!", self.all_player_names, "#!")
            return True
        return False

    def setup_game(self):
        role_config = []
        for role, count in self.roles.items():
            if count > 0:
                role_config.append(f"{role.capitalize()} {count}人")
        self.announce(f"本局游戏角色配置: {', '.join(role_config)}", self.all_player_names)
        self.announce(f"本局玩家人数: {len(self.players)}", self.all_player_names)
        self.announce("角色分配完成, 正在分发身份牌...", self.all_player_names)
        for name, player in self.players.items():
            time.sleep(0.3)
            self.announce(f"{name}, 你的身份是: {player.role.capitalize()}", [name])
        self.announce("游戏开始. 天黑, 请闭眼.", self.all_player_names, "#:")

if __name__ == "__main__":
    # Load config to get players
    game_dir = Path(__file__).resolve().parent
    config_path = game_dir / "config.json"

    try:
        with open(config_path, "r", encoding="utf-8") as f:
            config_data = json.load(f)
            # Construct players list for GameLogger
            # Assuming config has players with 'name'. UUID might be missing, so we generate or use name.
            init_players = []
            for p in config_data.get("players", []):
                init_players.append(
                    {
                        "player_name": p["name"],
                        "player_uuid": p.get(
                            "uuid", p["name"]
                        ),  # Use name as uuid if missing
                    }
                )
    except Exception as e:
        print(f"Error loading config for main: {e}")
        init_players = []

    game = WerewolfGame(init_players)
    game.run_game()

Game = WerewolfGame
