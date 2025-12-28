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

BASE = Path(__file__).resolve().parent.parent.parent
SRC_DIR = BASE / "src"

sys.path.append(str(BASE))

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
# Concrete Actions Implementation
# -----------------------------------------------------------------------------

class NightStartAction(GameAction):
    def description(self) -> str:
        return "入夜初始化"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.day_number += 1
        game.announce(f"第 {game.day_number} 天夜晚降临", prefix="#@")
        game.killed_player = None
        for p in game.players.values():
            p.is_guarded = False

class GuardAction(GameAction):
    def description(self) -> str:
        return "守卫守护"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        guard = game._get_player_by_role(Role.GUARD)
        if not guard: return
        prompt = "守卫, 请选择你要守护的玩家 (不能连续两晚守护同一个人): "
        game.announce("守卫请睁眼" + prompt, visible_to=[guard.name], prefix="#@")
        alive_players = game._get_alive_players()
        valid_targets = [p for p in alive_players if p != game.last_guarded]
        target = guard.choose(prompt, valid_targets)
        game.players[target].is_guarded = True
        game.last_guarded = target
        game.announce(f"你守护了 {target}", visible_to=[guard.name], prefix="#@")
        game.announce("守卫请闭眼", prefix="#@")

class WerewolfNightAction(GameAction):
    def description(self) -> str:
        return "狼人夜间行动"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        werewolves = game._get_alive_players([Role.WEREWOLF])
        if not werewolves: return
        game.announce("狼人请睁眼" + f"现在的狼人有: {', '.join(werewolves)}", visible_to=werewolves, prefix="#@")
        if len(werewolves) == 1:
            game.announce("独狼无需讨论，直接进入投票阶段", visible_to=werewolves, prefix="#@")
        else:
            self._handle_discussion(game, werewolves)
        self._handle_voting(game, werewolves)
        game.announce("狼人请闭眼", prefix="#@")

    def _handle_discussion(self, game, werewolves):
        game.announce("狼人请开始讨论, 输入 '0' 表示发言结束, 准备投票", visible_to=werewolves, prefix="#@")
        ready_to_vote = set()
        discussion_rounds = 0
        max_discussion_rounds = 5
        while len(ready_to_vote) < len(werewolves) and discussion_rounds < max_discussion_rounds:
            discussion_rounds += 1
            for wolf in werewolves:
                if wolf in ready_to_vote: continue
                wolf_player = game.players[wolf]
                action = wolf_player.speak(f"{wolf}, 请发言或输入 '0' 准备投票: ")
                if action == "0":
                    ready_to_vote.add(wolf)
                    msg = f"({wolf} 已准备好投票 {len(ready_to_vote)}/{len(werewolves)})"
                    game.announce(msg, visible_to=werewolves, prefix="#@")
                elif action:
                    game.announce(f"[狼人频道] {wolf} 发言: {action}", visible_to=werewolves, prefix="#:")
        if discussion_rounds >= max_discussion_rounds and len(ready_to_vote) < len(werewolves):
            msg = f"讨论已达到最大轮次({max_discussion_rounds}轮)，强制进入投票阶段"
            game.announce(msg, visible_to=werewolves, prefix="#@")

    def _handle_voting(self, game, werewolves):
        alive_players = game._get_alive_players()
        game.announce("狼人请投票", visible_to=werewolves, prefix="#@")
        while True:
            votes = {name: 0 for name in alive_players}
            for wolf_name in werewolves:
                wolf_player = game.players[wolf_name]
                prompt = f"{wolf_name}, 请投票选择要击杀的目标: "
                target = wolf_player.choose(prompt, alive_players)
                votes[target] += 1
            max_votes = max(votes.values())
            kill_targets = [name for name, count in votes.items() if count == max_votes]
            if len(kill_targets) == 1:
                game.killed_player = kill_targets[0]
                game.announce(f"狼人投票决定击杀 {game.killed_player}", visible_to=werewolves, prefix="#@")
                break
            else:
                game.announce("狼人投票出现平票, 请重新商议并投票", visible_to=werewolves, prefix="#@")

class SeerAction(GameAction):
    def description(self) -> str:
        return "预言家查验"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        seer = game._get_player_by_role(Role.SEER)
        if not seer: return
        prompt = "预言家, 请选择要查验的玩家: "
        game.announce("预言家请睁眼. 请选择要你要查验的玩家: ", visible_to=[seer.name], prefix="#@")
        alive_players = game._get_alive_players()
        target = seer.choose(prompt, alive_players)
        role = game.players[target].role
        identity = "狼人" if role == Role.WEREWOLF.value else "好人"
        game.announce(f"你查验了 {target} 的身份, 结果为 {identity}", visible_to=[seer.name], prefix="#@")
        game.announce("预言家请闭眼", prefix="#@")

class WitchAction(GameAction):
    def description(self) -> str:
        return "女巫毒药与解药"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        witch = game._get_player_by_role(Role.WITCH)
        if not witch: return
        game.announce("女巫请睁眼", visible_to=[witch.name], prefix="#@")
        alive_players = game._get_alive_players()
        actual_killed = None
        if game.killed_player:
            if game.players[game.killed_player].is_guarded:
                game.announce(f"今晚是个平安夜, {game.killed_player} 被守护了", prefix="#@")
            else:
                game.announce(f"今晚 {game.killed_player} 被杀害了", visible_to=[witch.name], prefix="#@")
                actual_killed = game.killed_player
        if not game.witch_save_used and actual_killed:
            prompt = "女巫, 你要使用解药吗? "
            if witch.choose(prompt, ["y", "n"]) == "y":
                actual_killed = None
                game.witch_save_used = True
                game.announce(f"你使用解药救了 {game.killed_player}", visible_to=[witch.name], prefix="#@")
        if not game.witch_poison_used:
            prompt = "女巫, 你要使用毒药吗? "
            if witch.choose(prompt, ["y", "n"]) == "y":
                poison_prompt = "请选择要毒杀的玩家: "
                target = witch.choose(poison_prompt, alive_players)
                if actual_killed is None: actual_killed = target
                else: game.handle_death(target, DeathReason.POISONED_BY_WITCH)
                game.witch_poison_used = True
                game.announce(f"你使用毒药毒了 {target}", visible_to=[witch.name], prefix="#@")
        game.killed_player = actual_killed
        game.announce("女巫请闭眼", prefix="#@")

class DayStartAction(GameAction):
    def description(self) -> str:
        return "天亮初始化"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        game.announce(f"天亮了. 现在是第 {game.day_number} 天白天", prefix="#@")
        if game.killed_player:
            game.handle_death(game.killed_player, DeathReason.KILLED_BY_WEREWOLF)
        else:
            game.announce("今晚是平安夜", prefix="#@")

class DayDiscussionAction(GameAction):
    def description(self) -> str:
        return "白天讨论"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        alive_players = game._get_alive_players()
        game.announce(f"场上存活的玩家: {', '.join(alive_players)}", prefix="#@")
        for player_name in alive_players:
            player = game.players[player_name]
            speech = player.speak(f"{player_name}, 请发言: ")
            game.announce(f"{player_name} 发言: {speech}", prefix="#:")

class DayVoteAction(GameAction):
    def description(self) -> str:
        return "白天投票"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        alive_players = game._get_alive_players()
        game.announce("请开始投票", prefix="#@")
        votes = {name: 0 for name in alive_players}
        for voter_name in alive_players:
            voter = game.players[voter_name]
            prompt = f"{voter_name}, 请投票: "
            target = voter.choose(prompt, alive_players)
            votes[target] += 1
            game.announce(f"{voter_name} 投票给 {target}", prefix="#:")
        max_votes = max(votes.values())
        voted_out_players = [name for name, count in votes.items() if count == max_votes]
        if len(voted_out_players) == 1:
            voted_out_player = voted_out_players[0]
            game.announce(f"投票结果: {voted_out_player} 被投票出局", prefix="#!")
            game.handle_death(voted_out_player, DeathReason.VOTED_OUT)
        else:
            game.announce("投票平票, 无人出局", prefix="#@")

class WerewolfGame(Game):
    def __init__(self, players: List[Dict[str, str]]):
        super().__init__("werewolf", players)
        self.roles: Dict[str, int] = {}
        self.killed_player: Optional[str] = None
        self.last_guarded: Optional[str] = None
        self.witch_save_used = False
        self.witch_poison_used = False
        self._init_phases()

    def _init_phases(self):
        # Night Phase
        night = GamePhase("Night")
        night.add_step(GameStep("入夜初始化", [[]], NightStartAction()))
        night.add_step(GameStep("守卫守护", [Role.GUARD], GuardAction()))
        night.add_step(GameStep("狼人行动", [Role.WEREWOLF], WerewolfNightAction()))
        night.add_step(GameStep("预言家查验", [Role.SEER], SeerAction()))
        night.add_step(GameStep("女巫用药", [Role.WITCH], WitchAction()))
        self.phases.append(night)

        # Day Phase
        day = GamePhase("Day")
        day.add_step(GameStep("天亮结算", [[]], DayStartAction()))
        day.add_step(GameStep("白天讨论", [[]], DayDiscussionAction()))
        day.add_step(GameStep("白天投票", [[]], DayVoteAction()))
        self.phases.append(day)

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
                if not self._players:
                    player_configs = config.get("players", [])
                    self._players = []
                    for p in player_configs:
                        self._players.append({"player_name": p["name"], "player_uuid": p.get("uuid", p["name"]), **p})

                self.all_player_names = [p.get("player_name", "") for p in self._players]
                player_names = self.all_player_names
                self.player_config_map = {}
                for p in config.get("players", []): self.player_config_map[p["name"]] = p
                for p in self._players:
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

    game = WerewolfGame(init_players)
    game.run_game()

Game = WerewolfGame
