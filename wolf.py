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

from src.Game import ActionContext, GameAction, GameStep, GamePhase, Game, Player, GameLogger

BASE = Path(__file__).resolve().parent.parent
SRC_DIR = BASE / "src"

sys.path.append(str(BASE))

class RobotPlayer(Player):
    def speak(self, prompt: str) -> str:
        if "投票" in prompt: return "0"
        return f"我是{self.name}，我会努力找出狼人的。"

    def choose(self, prompt: str, candidates: List[str], allow_skip: bool = False) -> Optional[str]:
        filtered_candidates = [c for c in candidates if c != self.name]
        if not filtered_candidates:
            if not candidates: return "skip" if allow_skip else None
            filtered_candidates = candidates
        choice = random.choice(filtered_candidates)
        return choice

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
        guard = game.get_player_by_role(Role.GUARD)
        if not guard: return
        prompt = "守卫, 请选择你要守护的玩家 (不能连续两晚守护同一个人): "
        game.announce("守卫请睁眼" + prompt, visible_to=[guard.name], prefix="#@")
        alive_players = game.get_alive_players()
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
        werewolves = game.get_alive_players([Role.WEREWOLF])
        if not werewolves: return
        game.announce("狼人请睁眼" + f"现在的狼人有: {', '.join(werewolves)}", visible_to=werewolves, prefix="#@")
        if len(werewolves) == 1:
            game.announce("独狼无需讨论，直接进入投票阶段", visible_to=werewolves, prefix="#@")
        else:
            game.process_discussion(
                werewolves,
                {
                    'prompt': '{0}, 请发言或输入 \'0\' 准备投票: ',
                    'speech': '[狼人频道] {0} 发言: {1}',
                    'ready_msg': '({0} 已准备好投票 {1}/{2})',
                    'start': '狼人请开始讨论, 输入 \'0\' 表示发言结束, 准备投票'
                },
                max_rounds=5, enable_ready_check=True, visibility=werewolves, prefix="#:"
            )
        alive_players = game.get_alive_players()
        candidates = [p for p in alive_players if p not in werewolves]
        target = game.process_vote(
            werewolves, candidates,
            {
                'start': '狼人请投票',
                'prompt': '{0}, 请投票选择要击杀的目标: ',
                'result_out': '狼人投票决定击杀 {0}',
                'result_tie': '狼人投票出现平票, 请重新投票'
            },
            retry_on_tie=True, max_retries=2, visibility=werewolves
        )
        game.killed_player = target
        game.announce("狼人请闭眼", prefix="#@")

class SeerAction(GameAction):
    def description(self) -> str:
        return "预言家查验"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        seer = game.get_player_by_role(Role.SEER)
        if not seer: return
        prompt = "预言家, 请选择要查验的玩家: "
        game.announce("预言家请睁眼. 请选择要你要查验的玩家: ", visible_to=[seer.name], prefix="#@")
        alive_players = game.get_alive_players()
        candidates = [p for p in alive_players if p != seer.name]
        target = seer.choose(prompt, candidates)
        role = game.players[target].role
        identity = "狼人" if role == Role.WEREWOLF.value else "好人"
        game.announce(f"你查验了 {target} 的身份, 结果为 {identity}", visible_to=[seer.name], prefix="#@")
        game.announce("预言家请闭眼", prefix="#@")

class WitchAction(GameAction):
    def description(self) -> str:
        return "女巫毒药与解药"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        witch = game.get_player_by_role(Role.WITCH)
        if not witch: return
        game.announce("女巫请睁眼", visible_to=[witch.name], prefix="#@")
        alive_players = game.get_alive_players()
        actual_killed = None
        if game.killed_player:
            if game.players[game.killed_player].is_guarded:
                game.announce(f"今晚是个平安夜, {game.killed_player} 被守护了", prefix="#@")
            else:
                game.announce(f"今晚 {game.killed_player} 被杀害了", visible_to=[witch.name], prefix="#@")
                actual_killed = game.killed_player
        if not game.witch_save_used and actual_killed:
            prompt = "女巫, 你要使用解药吗? "
            # 机器人女巫倾向于救人
            use_save = witch.choose(prompt, ["y", "n"]) == "y" if not isinstance(witch, RobotPlayer) else True
            if use_save:
                actual_killed = None
                game.witch_save_used = True
                game.announce(f"你使用解药救了 {game.killed_player}", visible_to=[witch.name], prefix="#@")
        if not game.witch_poison_used:
            prompt = "女巫, 你要使用毒药吗? "
            # 如果没有救人，则更有可能使用毒药
            use_poison = witch.choose(prompt, ["y", "n"]) == "y" if not isinstance(witch, RobotPlayer) else (actual_killed is not None)
            if use_poison:
                poison_prompt = "请选择要毒杀的玩家: "
                candidates = [p for p in alive_players if p != witch.name]
                target = witch.choose(poison_prompt, candidates)
                if target:
                    game.handle_death(target, DeathReason.POISONED_BY_WITCH)
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
        alive_players = game.get_alive_players()
        game.process_discussion(
            alive_players,
            {
                'start': f"场上存活的玩家: {', '.join(alive_players)}",
                'prompt': '{0}, 请发言: ',
                'speech': '{0} 发言: {1}'
            }
        )

class DayVoteAction(GameAction):
    def description(self) -> str:
        return "白天投票"

    def execute(self, context: ActionContext) -> Any:
        game = context.game
        alive_players = game.get_alive_players()
        target = game.process_vote(
            alive_players, alive_players,
            {
                'start': '请开始投票',
                'prompt': '{0}, 请投票: ',
                'action': '{0} 投票给 {1}',
                'result_out': '投票结果: {0} 被投票出局',
                'result_tie': '投票平票, 无人出局'
            }
        )
        if target:
            game.handle_death(target, DeathReason.VOTED_OUT)

class WerewolfGame(Game):
    def __init__(self, players: List[Dict[str, str]], event_emitter=None, input_handler=None):
        super().__init__("werewolfgame", players, event_emitter, input_handler)
        self.roles: Dict[str, int] = {}
        self.killed_player: Optional[str] = None
        self.last_guarded: Optional[str] = None
        self.witch_save_used = False
        self.witch_poison_used = False
        self._game_over_announced = False

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

    def stop_game(self):
        super().stop_game()
        self.announce("游戏已停止/取消.", prefix="#!")
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
        config, prompts, player_config_map = self.load_basic_config(game_dir)

        player_count = len(self.all_player_names)
        player_names = self.all_player_names

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
            player = RobotPlayer(name, role, p_config, prompts, self.logger)
            self.players[name] = player

        werewolves = self.get_alive_players([Role.WEREWOLF])
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
        alive_players_for_shot = [p for p in self.get_alive_players() if p != hunter_name]
        hunter_player = self.players[hunter_name]
        target = hunter_player.choose(f"{hunter_name}, 请选择你要带走的玩家: ", alive_players_for_shot, allow_skip=True)

        if target == "skip":
            self.announce("猎人放弃了开枪", prefix="#@")
        else:
            self.announce(f"猎人 {hunter_name} 开枪带走了 {target}", prefix="#@")
            self.handle_death(target, DeathReason.SHOT_BY_HUNTER)

    def check_game_over(self) -> bool:
        alive_werewolves = self.get_alive_players([Role.WEREWOLF])
        alive_villagers = self.get_alive_players([Role.VILLAGER, Role.SEER, Role.WITCH, Role.HUNTER, Role.GUARD])

        if not alive_werewolves:
            if not self._game_over_announced:
                self.announce("游戏结束, 好人阵营胜利!", prefix="#!")
                self._game_over_announced = True
            return True
        elif len(alive_werewolves) >= len(alive_villagers):
            if not self._game_over_announced:
                self.announce("游戏结束, 狼人阵营胜利!", prefix="#!")
                self._game_over_announced = True
            return True
        return False

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
