#include "codegen.h"

PythonGenerator::PythonGenerator(std::ostream &out) : out(out) {}

void PythonGenerator::indent()
{
    for (int i = 0; i < indentLevel; ++i)
    {
        out << "    ";
    }
}

void PythonGenerator::generate(GameDecl &game)
{
    game.accept(*this);
}

void PythonGenerator::emitImports()
{
    out << "from src.Game import Game, GameAction, GamePhase, GameStep, ActionContext\n";
    out << "from src.Player import Player\n";
    out << "from enum import Enum\n";
    out << "from typing import List, Dict, Any, Optional\n";
    out << "from pathlib import Path\n";
    out << "import random\n";
    out << "import time\n";
    out << "\n";
}

void PythonGenerator::emitHelpers()
{
    out << "# Helper Functions\n";
    out << "def get_players(game, role=None, status='alive'):\n";
    out << "    target_role = role.value if hasattr(role, 'value') else role\n";
    out << "    players = []\n";
    out << "    for p in game.players.values():\n";
    out << "        # If status is not 'all', only include players with a valid role\n";
    out << "        if status != 'all' and not p.role:\n";
    out << "            continue\n";
    out << "        if status == 'alive' and not p.is_alive: continue\n";
    out << "        if status == 'dead' and p.is_alive: continue\n";
    out << "        if target_role and p.role != target_role: continue\n";
    out << "        players.append(p.name)\n";
    out << "    return players\n\n";

    out << "def get_role(game, player_name):\n";
    out << "    if player_name in game.players:\n";
    out << "        return game.players[player_name].role\n";
    out << "    return None\n\n";

    out << "def kill(game, player_name):\n";
    out << "    if player_name in game.players:\n";
    out << "        game.players[player_name].is_alive = False\n\n";

    out << "def stop_game(game, msg=\"\"):\n";
    out << "    if msg: game.announce(msg)\n";
    out << "    game.stop_game()\n\n";

    out << "def set_data(game, player_name, key, value):\n";
    out << "    if player_name in game.players:\n";
    out << "        p = game.players[player_name]\n";
    out << "        if key == \"role\":\n";
    out << "            p.role = value\n";
    out << "        else:\n";
    out << "            setattr(p, key, value)\n\n";

    out << "def get_data(game, player_name, key):\n";
    out << "    if player_name in game.players:\n";
    out << "        p = game.players[player_name]\n";
    out << "        if key == \"role\":\n";
    out << "            return p.role\n";
    out << "        return getattr(p, key, None)\n";
    out << "    return None\n\n";

    out << "def shuffle(game, lst):\n";
    out << "    new_list = list(lst)\n";
    out << "    random.shuffle(new_list)\n";
    out << "    return new_list\n\n";

    out << "def dsl_vote(game, voters, candidates, prompts=None):\n";
    out << "    if prompts is None:\n";
    out << "        prompts = {\n";
    out << "            \"start\": \"Start Voting\",\n";
    out << "            \"prompt\": \"{0}, please choose your target\",\n";
    out << "            \"action\": \"{0} voted for {1}\",\n";
    out << "            \"result_out\": \"Voting result: {0} is out\",\n";
    out << "            \"result_tie\": \"Voting tie, no one is out\"\n";
    out << "        }\n";
    out << "    if \"start\" in prompts: game.announce(prompts[\"start\"], None, \"#@\")\n";
    out << "    votes = {name: 0 for name in candidates}\n";
    out << "    player_candidate_mode = any((c in game.players) for c in candidates)\n";
    out << "    for voter_name in voters:\n";
    out << "        voter = game.players.get(voter_name)\n";
    out << "        if not voter or not voter.is_alive: continue\n";
    out << "        prompt = prompts[\"prompt\"].format(voter_name)\n";
    out << "        if player_candidate_mode:\n";
    out << "            voter_candidates = []\n";
    out << "            for c in candidates:\n";
    out << "                p_target = game.players.get(c)\n";
    out << "                if not p_target: continue\n";
    out << "                if not p_target.is_alive or not p_target.role: continue\n";
    out << "                if c == voter_name: continue\n";
    out << "                voter_candidates.append(c)\n";
    out << "        else:\n";
    out << "            voter_candidates = list(candidates)\n";
    out << "        if not voter_candidates: continue\n";
    out << "        target = voter.choose(prompt, voter_candidates)\n";
    out << "        if target in votes: votes[target] += 1\n";
    out << "        if \"action\" in prompts: game.announce(prompts[\"action\"].format(voter_name, target), None, \"#:\")\n";
    out << "    max_votes = max(votes.values()) if votes else 0\n";
    out << "    targets = [name for name, count in votes.items() if count == max_votes and count > 0]\n";
    out << "    if len(targets) == 1:\n";
    out << "        winner = targets[0]\n";
    out << "        if \"result_out\" in prompts: game.announce(prompts[\"result_out\"].format(winner), None, \"#!\")\n";
    out << "        return winner\n";
    out << "    if \"result_tie\" in prompts: game.announce(prompts[\"result_tie\"], None, \"#@\")\n";
    out << "    return None\n\n";

    out << "def dsl_discussion(game, participants, prompts=None):\n";
    out << "    if prompts is None:\n";
    out << "        prompts = {\n";
    out << "            \"start\": \"Start Discussion\",\n";
    out << "            \"prompt\": \"It is {0}'s turn to speak\",\n";
    out << "            \"speech\": \"{0}: {1}\",\n";
    out << "            \"ready_msg\": \"{0} is ready ({1}/{2})\",\n";
    out << "            \"timeout\": \"Discussion timeout\",\n";
    out << "            \"alive_players\": \"Current alive: {0}\"\n";
    out << "        }\n";
    out << "    return game.process_discussion(participants, prompts)\n\n";
}

//开始开始----
void PythonGenerator::visit(GameDecl &node)
{
    className = node.name + "Game";

    emitImports();

//枚举角色
    out << "class Role(str, Enum):\n";
    for (const auto &role : node.roles)
    {
        out << "    " << role->name << " = \"" << role->name << "\"\n";
    }
    out << "\n";

    emitHelpers();

//生成所有action类    
    for (const auto &action : node.actions)
    {
        action->accept(*this);
    }

//生成主游戏类
    out << "class " << className << "(Game):\n";
    indentLevel++;

//初始化game 的基类还有dsl里面的全局变量
    indent();
    out << "def __init__(self, players_data, event_emitter=None, input_handler=None):\n";
    indentLevel++;
    indent();
    out << "super().__init__(\"" << node.name << "\", players_data, event_emitter, input_handler)\n";

    for (const auto &var : node.vars)
    {
        indent();
        out << "self." << var->name << " = ";
        if (var->initial)
        {
            var->initial->accept(*this);
        }
        else
        {
            out << "None";
        }
        out << "\n";
    }
    indentLevel--;
    out << "\n";

//游戏流程开始
    indent();
    out << "def _init_phases(self):\n";
    indentLevel++;
    for (const auto &phase : node.phases)
    {
        phase->accept(*this);
    }
    indentLevel--;
    out << "\n";

//未来适配其他游戏需要更改这里
    indent();
    out << "def check_game_over(self):\n";
    indentLevel++;
    indent();
    out << "return False # TODO: Implement game over logic\n";
    indentLevel--;
    out << "\n";

//初始化玩家，执行set up语句块
    indent();
    out << "def setup_game(self):\n";
    indentLevel++;
    indent();
    out << "game = self\n";
    indent();
    out << "config, prompts, player_config_map = self.load_basic_config(Path(__file__).parent)\n";
    indent();
    out << "# Initialize players from players_data if not already in player_config_map\n";
    indent();
    out << "for p_data in self._players_data:\n";
    indentLevel++;
    indent();
    out << "name = p_data.get('player_name') or p_data.get('name')\n";
    indent();
    out << "if name and name not in player_config_map:\n";
    indentLevel++;
    indent();
    out << "player_config_map[name] = p_data\n";
    indentLevel--;
    indentLevel--;
    out << "\n";
    indent();
    out << "for name, p_data in player_config_map.items():\n";
    indentLevel++;
    indent();
    out << "p_prompts = prompts.get('system', {}).copy()\n";
    indent();
    out << "if 'prompt' in p_prompts: p_prompts['PROMPT'] = p_prompts['prompt']\n";
    indent();
    out << "if 'reminder' in p_prompts: p_prompts['REMINDER'] = p_prompts['reminder']\n";
    indent();
    out << "self.players[name] = Player(name, None, p_data, p_prompts, self.logger, self.input_handler, self.event_emitter)\n";
    indentLevel--;
    if (node.setup)
    {
        node.setup->accept(*this);
    }
    else
    {
        indent();
        out << "pass # TODO: Implement setup logic (roles distribution)\n";
    }
    if (node.config)
    {
        node.config->accept(*this);
    }
    indentLevel--;
    out << "\n";

    indentLevel--;

    out << "Game = " << className << "\n";
}

//没实现，只有注释
void PythonGenerator::visit(ConfigDecl &node)
{
    indent();
    out << "# Config: min=" << node.minPlayers << ", max=" << node.maxPlayers << "\n";
    indent();
    out << "self.announce(f\"Game started with {len(self.players)} players.\")\n";
}

//上面已经解决喽
void PythonGenerator::visit(RoleDecl &node)
{
}

void PythonGenerator::visit(VarDecl &node)
{
}

void PythonGenerator::visit(ActionDecl &node)
{
    out << "class " << node.name << "(GameAction):\n";
    indentLevel++;

    indent();
    out << "def description(self):\n";
    indentLevel++;
    indent();
    out << "return \"" << (node.description.empty() ? node.displayName : node.description) << "\"\n";
    indentLevel--;

    out << "\n";
    indent();
    out << "def execute(self, context):\n";
    indentLevel++;
    indent();
    out << "game = context.game\n";
    if (node.body)
    {
        node.body->accept(*this);
    }
    else
    {
        indent();
        out << "pass\n";
    }
    indentLevel--;
    indentLevel--;
    out << "\n";
}

void PythonGenerator::visit(PhaseDecl &node)
{
    indent();
    out << node.name << " = GamePhase(\"" << node.displayName << "\")\n";
    for (const auto &step : node.steps)
    {
        step->accept(*this);
        indent();
        out << node.name << ".add_step(" << step->name << ")\n";
    }
    indent();
    out << "self.phases.append(" << node.name << ")\n";
}

void PythonGenerator::visit(StepDecl &node)
{
    indent();
    out << node.name << " = GameStep(\"" << node.name << "\", ";

    out << "[";
    for (size_t i = 0; i < node.roles.size(); ++i)
    {
        out << "Role." << node.roles[i];
        if (i < node.roles.size() - 1)
            out << ", ";
    }
    out << "], ";

    out << node.actionName << "()";
    out << ")\n";
}

//语句节点：dsl逻辑->python语句
void PythonGenerator::visit(BlockStmt &node)
{
    if (node.statements.empty())
    {
        indent();
        out << "pass\n";
        return;
    }
    for (const auto &stmt : node.statements)
    {
        stmt->accept(*this);
    }
}

void PythonGenerator::visit(LetStmt &node)
{
    indent();
    out << node.name << " = ";
    if (node.initial)
        node.initial->accept(*this);
    else
        out << "None";
    out << "\n";
}

void PythonGenerator::visit(AssignStmt &node)
{
    indent();
    node.target->accept(*this);
    out << " = ";
    node.value->accept(*this);
    out << "\n";
}

void PythonGenerator::visit(IfStmt &node)
{
    indent();
    out << "if ";
    node.condition->accept(*this);
    out << ":\n";
    indentLevel++;
    node.thenBranch->accept(*this);
    indentLevel--;
    if (node.elseBranch)
    {
        indent();
        out << "else:\n";
        indentLevel++;
        node.elseBranch->accept(*this);
        indentLevel--;
    }
}

void PythonGenerator::visit(ForStmt &node)
{
    indent();
    out << "for " << node.iterator << " in ";
    node.iterable->accept(*this);
    out << ":\n";
    indentLevel++;
    node.body->accept(*this);
    indentLevel--;
}

void PythonGenerator::visit(ReturnStmt &node)
{
    indent();
    out << "return";
    if (node.value)
    {
        out << " ";
        node.value->accept(*this);
    }
    out << "\n";
}

void PythonGenerator::visit(ExpressionStmt &node)
{
    indent();
    node.expression->accept(*this);
    out << "\n";
}

//表达式节点，包括字面量、变量、二元表达式、一元表达式
void PythonGenerator::visit(LiteralExpr &node)
{
    if (node.type == "string")
        out << "\"" << node.value << "\"";
    else
        out << node.value;
}

//没有办法实现区分全局或者局部的变量，也没法识别角色标识符
void PythonGenerator::visit(VariableExpr &node)
{
    out << node.name;
}

void PythonGenerator::visit(BinaryExpr &node)
{
    node.left->accept(*this);
    if (node.op == "and")
        out << " and ";
    else if (node.op == "or")
        out << " or ";
    else
        out << " " << node.op << " ";
    node.right->accept(*this);
}

void PythonGenerator::visit(UnaryExpr &node)
{
    if (node.op == "not")
        out << "not ";
    else
        out << node.op;
    node.right->accept(*this);
}

//映射内联函数
void PythonGenerator::visit(CallExpr &node)
{
    if (node.callName == "announce")
    {
        out << "game.announce(";
    }
    else if (node.callName == "get_players")
    {
        out << "get_players(game, ";
    }
    else if (node.callName == "get_role")
    {
        out << "get_role(game, ";
    }
    else if (node.callName == "kill")
    {
        out << "kill(game, ";
    }
    else if (node.callName == "stop_game")
    {
        out << "stop_game(game, ";
    }
    else if (node.callName == "set_data")
    {
        out << "set_data(game, ";
    }
    else if (node.callName == "get_data")
    {
        out << "get_data(game, ";
    }
    else if (node.callName == "shuffle")
    {
        out << "shuffle(game, ";
    }
    else if (node.callName == "len")
    {
        out << "len(";
    }
    else if (node.callName == "vote")
    {
        out << "dsl_vote(game, ";
    }
    else if (node.callName == "discussion")
    {
        out << "dsl_discussion(game, ";
    }
    else if (node.callName == "announce")
    {
        out << "game.announce(";
    }
    else
    {
        out << "game." << node.callName << "(";
    }

    //目前解析器不支持命名参数，所以直接打印参数
    for (size_t i = 0; i < node.args.size(); ++i)
    {
        node.args[i]->accept(*this);
        if (i < node.args.size() - 1)
            out << ", ";
    }
    out << ")";
}

//表达式节点：dsl->python
void PythonGenerator::visit(ListExpr &node)
{
    out << "[";
    for (size_t i = 0; i < node.elements.size(); ++i)
    {
        node.elements[i]->accept(*this);
        if (i < node.elements.size() - 1)
            out << ", ";
    }
    out << "]";
}

void PythonGenerator::visit(MemberExpr &node)
{
    node.object->accept(*this);
    out << "." << node.member;
}

void PythonGenerator::visit(IndexExpr &node)
{
    node.object->accept(*this);
    out << "[";
    node.index->accept(*this);
    out << "]";
}
