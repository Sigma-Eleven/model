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

from src.Logger import GameLogger
from src.Player import Player

from Game import ActionContext, GameAction, GameStep, GamePhase, Game

BASE = Path(__file__).resolve().parent.parent
SRC_DIR = BASE / "src"

sys.path.append(str(BASE))

class Role(Enum):
    CIVILIAN = "civilian"
    SPY = "spy"

class DeathReason(Enum):
    KILLED_BY_WEREWOLF = "在夜晚被杀害"
    POISONED_BY_WITCH = "被女巫毒杀"
    VOTED_OUT = "被投票出局"
    SHOT_BY_HUNTER = "被猎人带走"

# -----------------------------------------------------------------------------
# Concrete Actions Implementation
# -----------------------------------------------------------------------------

class DescribeAction(GameAction):
    def description(self) -> str:
        return "describe"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.announce(f"玩家描述了: {content}")

class VoteAction(GameAction):
    def description(self) -> str:
        return "vote"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.announce(f"玩家投给了: {target}")

class WhoIsTheSpy(Game):
    def __init__(self, players: List[Dict[str, str]], event_emitter=None, input_handler=None):
        super().__init__("whoisthespy", players, event_emitter, input_handler)
        self.roles: Dict[str, int] = {}
        self.killed_player: Optional[str] = None
        self.last_guarded: Optional[str] = None
        self.witch_save_used = False
        self.witch_poison_used = False
        self._init_phases()

    def _init_phases(self):
        # round_phase Phase
        round_phase = GamePhase("round_phase")
        round_phase.add_step(GameStep("description", [Role.CIVILIAN, Role.SPY], DescribeAction()))
        round_phase.add_step(GameStep("voting", [Role.CIVILIAN, Role.SPY], VoteAction()))
        self.phases.append(round_phase)

    def run_phase(self, phase: GamePhase):
        for step in phase.steps:
            if not self._running or self.check_game_over():
                return

            context = ActionContext(game=self)
            step.action.execute(context)

    def _get_alive_players(self, roles: Optional[List[Role]] = None) -> List[str]:
        alive_players = []
        role_values = []
        if roles:
            for r in roles:
                role_values.append(r.value)

        for name, p in self.players.items():
            if p.is_alive:
                if roles is None or p.role in role_values:
                    alive_players.append(name)
        return alive_players

    def _get_player_by_role(self, role: Role) -> Optional[Player]:
        for p in self.players.values():
            if p.role == role.value and p.is_alive:
                return p
        return None

    def _cancel(self):
        self.announce("游戏已取消.", prefix="#!")
        self.players.clear()
        self.roles.clear()
        self.all_player_names.clear()
        self.killed_player = None
        self.last_guarded = None
        self.witch_save_used = False
        self.witch_poison_used = False
        self.day_number = 0

    def setup_game(self):
        game_dir = Path(__file__).resolve().parent
        config_path = game_dir / "config.json"
        prompt_path = game_dir / "prompt.json"

        try:
            with open(config_path, "r", encoding="utf-8") as file:
                config = json.load(file)
                if not self._players_data:
                    player_configs = config.get("players", [])
                    self._players_data = []
                    for p in player_configs:
                        self._players_data.append({"player_name": p["name"], "player_uuid": p.get("uuid", p["name"]), **p})

                self.all_player_names = [p.get("player_name", "") for p in self._players_data]
                player_names = self.all_player_names
                self.player_config_map = {}
                for p in config.get("players", []): self.player_config_map[p["name"]] = p
                for p in self._players_data:
                    name = p.get("player_name")
                    if name:
                        if name not in self.player_config_map: self.player_config_map[name] = {}
                        self.player_config_map[name].update(p)
                player_config_map = self.player_config_map

            with open(prompt_path, "r", encoding="utf-8") as file:
                prompts = json.load(file)
        except (FileNotFoundError, KeyError, ValueError) as e:
            self.logger.system_logger.error(f"配置文件或提示词文件有错: {str(e)}")
            return

        player_count = len(self.all_player_names)
        if player_count == 6:
            self.roles = {Role.WEREWOLF.value: 2, Role.VILLAGER.value: 2, Role.SEER.value: 1, Role.WITCH.value: 1}
        else:
            self.roles = {Role.WEREWOLF.value: max(1, player_count // 4), Role.SEER.value: 1, Role.WITCH.value: 1, Role.HUNTER.value: 1, Role.GUARD.value: 1}
            self.roles[Role.VILLAGER.value] = player_count - sum(self.roles.values())

        self.announce("本局游戏角色配置", prefix="#@")
        role_config = []
        for role, count in self.roles.items():
            if count > 0: role_config.append(f"{role.capitalize()} {count}人")
        self.announce(", ".join(role_config), prefix="#@")

        self.announce(f"本局玩家人数: {player_count}", prefix="#@")
        self.announce(f"角色卡配置: {role_config}", prefix="#@")
        self.announce(f"玩家 {player_names} 已加入游戏.", prefix="#@")

        role_list = []
        for role, count in self.roles.items():
            for _ in range(count): role_list.append(role)
        random.shuffle(role_list)

        for name, role in zip(player_names, role_list):
            p_config = player_config_map.get(name, {})
            player = Player(name, role, p_config, prompts, self.logger)
            self.players[name] = player

        werewolves = self._get_alive_players([Role.WEREWOLF])
        self.announce("角色分配完成, 正在分发身份牌...", prefix="#@")
        for name, player in self.players.items():
            time.sleep(0.1)
            self.announce(f"你的身份是: {player.role.capitalize()}", visible_to=[player.name], prefix="#@")
            if player.role == Role.WEREWOLF.value:
                teammates = [w for w in werewolves if w != name]
                if teammates: self.announce(f"你的狼人同伴是: {', '.join(teammates)}", visible_to=[player.name], prefix="#@")
                else: self.announce("你是唯一的狼人", visible_to=[player.name], prefix="#@")

        self.announce("游戏开始. 天黑, 请闭眼.", prefix="#:")

    def handle_death(self, player_name: str, reason: DeathReason):
        if player_name and self.players[player_name].is_alive:
            self.players[player_name].is_alive = False
            self.announce(f"{player_name} 死了, 原因是 {reason.value}", prefix="#!")

            is_first_night_death = self.day_number == 1 and reason in [DeathReason.KILLED_BY_WEREWOLF, DeathReason.POISONED_BY_WITCH]
            can_have_last_words = reason in [DeathReason.VOTED_OUT, DeathReason.SHOT_BY_HUNTER] or is_first_night_death

            if can_have_last_words:
                player = self.players[player_name]
                last_words = player.speak(f"{player_name}, 请发表你的遗言: ")
                if last_words:
                    self.announce(f"[遗言] {player_name} 发言: {last_words}", prefix="#:")
                else:
                    self.announce(f"{player_name} 选择保持沉默, 没有留下遗言", prefix="#@")

            if self.players[player_name].role == Role.HUNTER.value:
                self.handle_hunter_shot(player_name)

    def handle_hunter_shot(self, hunter_name: str):
        self.announce(f"{hunter_name} 是猎人, 可以在临死前开枪带走一人", prefix="#@")
        alive_players_for_shot = [p for p in self._get_alive_players() if p != hunter_name]
        hunter_player = self.players[hunter_name]
        target = hunter_player.choose(f"{hunter_name}, 请选择你要带走的玩家: ", alive_players_for_shot, allow_skip=True)

        if target == "skip":
            self.announce("猎人放弃了开枪", prefix="#@")
        else:
            self.announce(f"猎人 {hunter_name} 开枪带走了 {target}", prefix="#@")
            self.handle_death(target, DeathReason.SHOT_BY_HUNTER)

    def check_game_over(self):
        alive_werewolves = self._get_alive_players([Role.WEREWOLF])
        alive_villagers = self._get_alive_players([Role.VILLAGER, Role.SEER, Role.WITCH, Role.HUNTER, Role.GUARD])

        if not alive_werewolves:
            self.announce("游戏结束, 好人阵营胜利!", prefix="#!")
            return True
        elif len(alive_werewolves) >= len(alive_villagers):
            self.announce("游戏结束, 狼人阵营胜利!", prefix="#!")
            return True
        return False

    def run_game(self):
        self.setup_game()
        while not self.check_game_over():
            for phase in self.phases:
                self.run_phase(phase)
                if self.check_game_over(): break

if __name__ == "__main__":
    game_dir = Path(__file__).resolve().parent
    config_path = game_dir / "config.json"

    try:
        with open(config_path, "r", encoding="utf-8") as f:
            config_data = json.load(f)
            init_players = []
            for p in config_data.get("players", []):
                init_players.append({"player_name": p["name"], "player_uuid": p.get("uuid", p["name"])})
    except Exception as e:
        print(f"Error loading config for main: {e}")
        init_players = []

    game = WhoIsTheSpy(init_players)
    game.run_game()

Game = WhoIsTheSpy
