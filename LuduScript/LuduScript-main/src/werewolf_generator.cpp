#include "werewolf_generator.h"
#include <sstream>
#include <algorithm>

WerewolfGenerator::WerewolfGenerator(const WolfParseResult &result) : PythonGenerator(result) {}

std::string WerewolfGenerator::generateImports()
{
    std::stringstream ss;
    ss << "from abc import ABC, abstractmethod\n"
       << "from dataclasses import dataclass, field\n"
       << "from datetime import datetime\n"
       << "from enum import Enum\n"
       << "import json\n"
       << "import os\n"
       << "from pathlib import Path\n"
       << "import random\n"
       << "import sys\n"
       << "import time\n"
       << "from typing import Any, Callable, Dict, List, Optional, Union\n\n"
       << "# Import base Game classes\n"
       << "try:\n"
       << indent(1) << "from src.Game import Game, ActionContext, GameAction, GameStep, GamePhase\n"
       << "except ImportError:\n"
       << indent(1) << "# Fallback if Game.py is not found (for standalone testing)\n"
       << indent(1) << "base_dir = Path(__file__).resolve().parent\n"
       << indent(1) << "sys.path.append(str(base_dir))\n"
       << indent(1) << "sys.path.append(str(base_dir / 'src'))\n"
       << indent(1) << "try:\n"
       << indent(2) << "from src.Game import Game, ActionContext, GameAction, GameStep, GamePhase\n"
       << indent(1) << "except Exception:\n"
       << indent(2) << "from Game import Game, ActionContext, GameAction, GameStep, GamePhase\n\n";
    return ss.str();
}

std::string WerewolfGenerator::generateCoreStructures()
{
    // Integrated from Game.py
    return "";
}

std::string WerewolfGenerator::generateBaseStructures()
{
    return "";
}
// 这里这个gen法有点抽象了
std::string WerewolfGenerator::generateEnums()
{
    std::stringstream ss;
    ss << "# models.py\n"
       << "class Role(Enum):\n";

    if (result.roles.empty())
    {
        // Fallback roles if not defined in DSL
        ss << indent(1) << "WEREWOLF = \"werewolf\"\n"
           << indent(1) << "VILLAGER = \"villager\"\n"
           << indent(1) << "SEER = \"seer\"\n"
           << indent(1) << "WITCH = \"witch\"\n"
           << indent(1) << "HUNTER = \"hunter\"\n"
           << indent(1) << "GUARD = \"guard\"\n";
    }
    else
    {
        for (const auto &role : result.roles)
        {
            std::string upperRole = role;
            std::transform(upperRole.begin(), upperRole.end(), upperRole.begin(), ::toupper);
            ss << indent(1) << upperRole << " = \"" << role << "\"\n";
        }
    }

    ss << "\n\n"
       << "class DeathReason(Enum):\n"
       << indent(1) << "KILLED_BY_WEREWOLF = \"在夜晚被杀害\"\n"
       << indent(1) << "POISONED_BY_WITCH = \"被女巫毒杀\"\n"
       << indent(1) << "VOTED_OUT = \"被投票出局\"\n"
       << indent(1) << "SHOT_BY_HUNTER = \"被猎人带走\"\n\n";
    return ss.str();
}
// 问号
std::string WerewolfGenerator::generateGameClass()
{
    std::stringstream ss;
    ss << PythonGenerator::generateGameClass();

    // Add Werewolf-specific infrastructure methods
    ss << "\n"
       << indent(1) << "def announce(self, message: str, visible_to: list = None, prefix: str = \"#@\") -> None:\n"
       << indent(2) << "super().announce(message, visible_to, prefix)\n\n"

       << indent(1) << "def _init_players(self, players_data):\n"
       << indent(2) << "roles_list = []\n"
       << indent(2) << "for role_name in [r.value for r in Role]:\n"
       << indent(3) << "count = self.roles.get(role_name, 0)\n"
       << indent(3) << "roles_list.extend([role_name] * count)\n\n"
       << indent(2) << "# Adjust roles if player count mismatch (simple logic)\n"
       << indent(2) << "if len(players_data) != len(roles_list):\n"
       << indent(3) << "if len(players_data) > len(roles_list):\n"
       << indent(4) << "roles_list.extend([Role.VILLAGER.value] * (len(players_data) - len(roles_list)))\n"
       << indent(3) << "else:\n"
       << indent(4) << "roles_list = roles_list[:len(players_data)]\n"
       << indent(2) << "random.shuffle(roles_list)\n\n"
       << indent(2) << "for i, p_data in enumerate(players_data):\n"
       << indent(3) << "name = p_data['player_name']\n"
       << indent(3) << "role = roles_list[i]\n"
       << indent(3) << "# Create Player instance (using inner class)\n"
       << indent(3) << "player = self._create_player(name, role)\n"
       << indent(3) << "self.players[name] = player\n\n"

       << indent(1) << "def _create_player(self, name, role):\n"
       << indent(2) << "game_instance = self\n"
       << indent(2) << "class GamePlayer:\n"
       << indent(3) << "def __init__(self, name, role):\n"
       << indent(4) << "self.name = name\n"
       << indent(4) << "self.role = role\n"
       << indent(4) << "self.is_alive = True\n"
       << indent(4) << "self.is_guarded = False\n"
       << indent(3) << "def speak(self, prompt: str) -> str:\n"
       << indent(4) << "if game_instance.input_handler:\n"
       << indent(5) << "return game_instance.input_handler(game_instance.game_name, self.name, prompt, [], False)\n"
       << indent(4) << "return input(prompt)\n"
       << indent(3) << "def choose(self, prompt: str, candidates: List[str], allow_skip: bool = False) -> Optional[str]:\n"
       << indent(4) << "if game_instance.input_handler:\n"
       << indent(5) << "return game_instance.input_handler(game_instance.game_name, self.name, prompt, candidates, allow_skip)\n"
       << indent(4) << "retries = 0\n"
       << indent(4) << "max_retries = 3\n"
       << indent(4) << "while retries < max_retries:\n"
       << indent(5) << "game_instance.announce(f\"\\n{prompt}\", [self.name])\n"
       << indent(5) << "game_instance.announce(f\"候选项: {candidates}\", [self.name])\n"
       << indent(5) << "choice = input(\"请输入选择: \").strip()\n"
       << indent(5) << "if allow_skip and not choice:\n"
       << indent(6) << "return None\n"
       << indent(5) << "if choice in candidates:\n"
       << indent(6) << "return choice\n"
       << indent(5) << "game_instance.announce(\"无效的选择，请重试。\", [self.name])\n"
       << indent(5) << "retries += 1\n"
       << indent(4) << "game_instance.announce(\"重试次数已达上限。正在随机选择。\", [self.name])\n"
       << indent(4) << "if candidates:\n"
       << indent(5) << "selection = random.choice(candidates)\n"
       << indent(5) << "game_instance.announce(f\"随机选择了: {selection}\", [self.name])\n"
       << indent(5) << "return selection\n"
       << indent(4) << "return None\n\n"
       << indent(2) << "return GamePlayer(name, role)\n\n"

       << indent(1) << "def _get_player_by_role(self, role: Role):\n"
       << indent(2) << "for p in self.players.values():\n"
       << indent(3) << "if p.role == role.value and p.is_alive:\n"
       << indent(4) << "return p\n"
       << indent(2) << "return None\n\n"

       << indent(1) << "def _get_alive_players(self, roles: List[Role] = None):\n"
       << indent(2) << "if roles:\n"
       << indent(3) << "role_values = [r.value for r in roles]\n"
       << indent(3) << "return [n for n, p in self.players.items() if p.is_alive and p.role in role_values]\n"
       << indent(2) << "return [n for n, p in self.players.items() if p.is_alive]\n\n"

       << indent(1) << "def get_alive_players(self, roles: List[str] = None):\n"
       << indent(2) << "role_enums = []\n"
       << indent(2) << "if roles:\n"
       << indent(3) << "for r in roles:\n"
       << indent(4) << "try:\n"
       << indent(5) << "role_enums.append(Role(r))\n"
       << indent(4) << "except ValueError:\n"
       << indent(5) << "pass\n"
       << indent(2) << "return self._get_alive_players(role_enums if roles else None)\n\n"

       << indent(1) << "def handle_death(self, player_name, reason):\n"
       << indent(2) << "if not player_name or not self.players[player_name].is_alive:\n"
       << indent(3) << "return\n"
       << indent(2) << "self.players[player_name].is_alive = False\n"
       << indent(2) << "self.announce(f\"{player_name} {reason.value}\", self.all_player_names)\n"
       << indent(2) << "self.check_game_over()\n\n";

    return ss.str();
}

std::string WerewolfGenerator::generateActionClasses()
{
    std::stringstream ss;
    ss << "# -----------------------------------------------------------------------------\n"
       << "# Generated Actions from DSL\n"
       << "# -----------------------------------------------------------------------------\n\n";

    for (const auto &action : result.actions)
    {
        std::string className = mapActionToClassName(action.name);
        ss << "class " << className << "(GameAction):\n"
           << indent(1) << "def description(self) -> str:\n"
           << indent(2) << "return \"" << action.name << "\"\n\n"
           << indent(1) << "def execute(self, context: ActionContext) -> Any:\n"
           << indent(2) << "game = context.game\n";

        // Use translateBody with "game." prefix to access game state
        ss << translateBody(action.bodyLines, 2, "game.");
        ss << "\n\n";
    }

    return ss.str();
}

std::string WerewolfGenerator::generateEntryPoint()
{
    std::stringstream ss;
    ss << "if __name__ == \"__main__\":\n"
       << indent(1) << "# Load config to get players\n"
       << indent(1) << "game_dir = Path(__file__).resolve().parent\n"
       << indent(1) << "config_path = game_dir / \"config.json\"\n\n"
       << indent(1) << "try:\n"
       << indent(2) << "with open(config_path, \"r\", encoding=\"utf-8\") as f:\n"
       << indent(3) << "config_data = json.load(f)\n"
       << indent(3) << "# Construct players list for GameLogger\n"
       << indent(3) << "# Assuming config has players with 'name'. UUID might be missing, so we generate or use name.\n"
       << indent(3) << "init_players = []\n"
       << indent(3) << "for p in config_data.get(\"players\", []):\n"
       << indent(4) << "init_players.append(\n"
       << indent(5) << "{\n"
       << indent(6) << "\"player_name\": p[\"name\"],\n"
       << indent(6) << "\"player_uuid\": p.get(\n"
       << indent(7) << "\"uuid\", p[\"name\"]\n"
       << indent(6) << "),  # Use name as uuid if missing\n"
       << indent(5) << "}\n"
       << indent(4) << ")\n"
       << indent(1) << "except Exception as e:\n"
       << indent(2) << "print(f\"Error loading config for main: {e}\")\n"
       << indent(2) << "init_players = []\n\n"
       << indent(1) << "game = WerewolfGame(init_players)\n"
       << indent(1) << "game.run_game()\n\n"
       << "Game = WerewolfGame\n";
    return ss.str();
}

std::string WerewolfGenerator::generateInit()
{
    std::stringstream ss;
    ss << PythonGenerator::generateInit();

    // Add Werewolf-specific initialization
    // We need to find where the last line of super().__init__ is and insert there,
    // or just append to the end of the method.
    // Actually, PythonGenerator::generateInit() ends with the variable initializations.

    ss << indent(2) << "self.roles = {}\n";

    // Extract role counts from result if possible, or use defaults
    // In wolf.game, roles are just an enum. The counts are usually in setup.
    // For now, we'll initialize them to 0 and let setup_game handle it.
    for (const auto &role : result.roles)
    {
        ss << indent(2) << "self.roles[\"" << role << "\"] = 0\n";
    }

    ss << indent(2) << "self._init_players(players_data)\n";
    return ss.str();
}

std::string WerewolfGenerator::generateInitPhases()
{
    std::stringstream ss;
    ss << indent(1) << "def _init_phases(self):\n";
    for (const auto &phase : result.phases)
    {
        std::string phaseVar = phase.name;
        std::transform(phaseVar.begin(), phaseVar.end(), phaseVar.begin(), ::tolower);
        ss << indent(2) << phaseVar << " = GamePhase(\"" << phase.name << "\")\n";
        for (const auto &step : phase.steps)
        {
            std::string actionClass = mapActionToClassName(step.actionName);
            std::string rolesStr = "[]";
            if (!step.rolesInvolved.empty())
            {
                rolesStr = "[";
                for (size_t i = 0; i < step.rolesInvolved.size(); ++i)
                {
                    if (step.rolesInvolved[i] == "all")
                    {
                        rolesStr += "\"all\"";
                    }
                    else
                    {
                        rolesStr += "Role(\"" + step.rolesInvolved[i] + "\")";
                    }
                    if (i < step.rolesInvolved.size() - 1)
                    {
                        rolesStr += ", ";
                    }
                }
                rolesStr += "]";
            }

            ss << indent(2) << phaseVar << ".add_step(GameStep(\n"
               << indent(3) << "name=\"" << step.name << "\",\n"
               << indent(3) << "roles_involved=" << rolesStr << ",\n"
               << indent(3) << "action=" << actionClass << "()))\n";
        }
        ss << indent(2) << "self.phases.append(" << phaseVar << ")\n\n";
    }
    return ss.str();
}

std::string WerewolfGenerator::generateCancel()
{
    return PythonGenerator::generateCancel();
}

std::string WerewolfGenerator::generateSetupGame()
{
    return PythonGenerator::generateSetupGame();
}

std::string WerewolfGenerator::generateHandleDeath()
{
    // This is now handled by the infrastructure added in generateGameClass
    return "";
}

std::string WerewolfGenerator::generateHandleHunterShot()
{
    // This should be in the DSL. If it is, generateDSLMethods will handle it.
    return "";
}

std::string WerewolfGenerator::generateCheckGameOver()
{
    // The base class will generate this if it's in the DSL methods,
    // but the abstract method in Game.py requires it to be present.
    // Let's see if check_game_over is in result.methods.
    bool found = false;
    for (const auto &m : result.methods)
    {
        if (m.name == "check_game_over")
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        // It will be generated by generateDSLMethods
        return "";
    }

    // Default implementation if not in DSL
    std::stringstream ss;
    ss << indent(1) << "def check_game_over(self) -> bool:\n"
       << indent(2) << "return self.game_over\n\n";
    return ss.str();
}

std::string WerewolfGenerator::generateDSLMethods()
{
    return PythonGenerator::generateDSLMethods();
}

std::string WerewolfGenerator::generateActionBody(const std::string &actionName)
{
    // This method is not used as we generate full action classes in generateActionClasses
    return "";
}
