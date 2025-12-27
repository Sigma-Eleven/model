#include "generator.h"
#include <sstream>
#include <algorithm>

PythonGenerator::PythonGenerator(const WolfParseResult &result) : result(result) {}

std::string PythonGenerator::indent(int level)
{
    return std::string(level * 4, ' ');
}

// Helper utilities for generating valid Python code
static std::string trimStr(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos)
        return "";
    return s.substr(a, b - a + 1);
}

static bool looksLikeNumber(const std::string &s)
{
    if (s.empty())
        return false;
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+')
        i = 1;
    bool hasDigit = false;
    bool hasDot = false;
    for (; i < s.size(); ++i)
    {
        if (std::isdigit((unsigned char)s[i]))
            hasDigit = true;
        else if (s[i] == '.' && !hasDot)
            hasDot = true;
        else
            return false;
    }
    return hasDigit;
}

static std::string toPythonLiteral(const std::string &val)
{
    std::string t = trimStr(val);
    if (t.empty())
        return "None";
    std::string lower = t;
    for (auto &c : lower)
        c = (char)std::tolower(c);
    if (lower == "false")
        return "False";
    if (lower == "true")
        return "True";
    if (t.front() == '"' || t.front() == '\'')
        return t; // already a quoted string
    if (looksLikeNumber(t))
        return t;
    // if contains spaces or punctuation (except underscore), quote it
    bool needQuote = false;
    for (char c : t)
    {
        if (!(std::isalnum((unsigned char)c) || c == '_'))
        {
            needQuote = true;
            break;
        }
    }
    if (needQuote)
        return '"' + t + '"';
    return t;
}

std::string PythonGenerator::normalizeExpression(std::string expr)
{
    // protect '!='
    size_t pos = 0;
    while ((pos = expr.find("!=", pos)) != std::string::npos)
    {
        expr.replace(pos, 2, "__NE__");
        pos += 4;
    }

    // replace '!' with 'not ' when it's a standalone NOT
    // simple heuristic: replace "!" followed by space with "not "
    pos = 0;
    while ((pos = expr.find('!', pos)) != std::string::npos)
    {
        // skip != cases already protected
        expr.replace(pos, 1, "not ");
        pos += 4;
    }

    // restore '!='
    pos = 0;
    while ((pos = expr.find("__NE__", pos)) != std::string::npos)
    {
        expr.replace(pos, 6, "!=");
        pos += 2;
    }

    // true/false
    // replace standalone words
    std::string out;
    std::stringstream ss(expr);
    std::string token;
    bool first = true;
    while (ss >> token)
    {
        std::string low = token;
        for (auto &c : low)
            c = (char)std::tolower(c);
        if (low == "true")
            token = "True";
        else if (low == "false")
            token = "False";
        if (!first)
            out += " ";
        out += token;
        first = false;
    }
    return out;
}

std::string PythonGenerator::transformPrintContent(const std::string &inner)
{
    std::string s = trimStr(inner);
    // If contains +, build f-string
    size_t plusPos = s.find('+');
    if (plusPos != std::string::npos)
    {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true)
        {
            size_t p = s.find('+', start);
            std::string part = trimStr(s.substr(start, (p == std::string::npos) ? std::string::npos : p - start));
            parts.push_back(part);
            if (p == std::string::npos)
                break;
            start = p + 1;
        }
        std::string f;
        for (auto &part : parts)
        {
            if (part.empty())
                continue;
            // quoted string
            if ((part.front() == '"' && part.back() == '"') || (part.front() == '\'' && part.back() == '\''))
            {
                // strip quotes and append
                if (!f.empty())
                    f += " ";
                f += part.substr(1, part.size() - 2);
            }
            else
            {
                // if it's a simple identifier, treat as variable, otherwise treat as literal text
                bool ident = true;
                for (char c : part)
                    if (!(std::isalnum((unsigned char)c) || c == '_'))
                    {
                        ident = false;
                        break;
                    }
                if (ident)
                {
                    if (!f.empty())
                        f += " ";
                    f += "{" + part + "}";
                }
                else
                {
                    if (!f.empty())
                        f += " ";
                    f += part; // literal text
                }
            }
        }
        return "f\"" + f + "\"";
    }

    // If not quoted, quote it
    std::string t = s;
    if (t.empty())
        return "\"\"";
    if (t.front() == '"' || t.front() == '\'')
        return t; // already quoted
    // if looks like a single identifier (variable), print it directly
    bool ident = true;
    for (char c : t)
        if (!(std::isalnum((unsigned char)c) || c == '_'))
        {
            ident = false;
            break;
        }
    if (ident)
        return t;

    // else quote the whole content
    return '"' + t + '"';
}

std::string PythonGenerator::translateBody(const std::vector<std::string> &lines, int indentLevel)
{
    if (lines.empty())
    {
        return indent(indentLevel) + "pass\n";
    }

    struct GenLine
    {
        int indent;
        std::string content;
    };
    std::vector<GenLine> genLines;
    int currentIndent = indentLevel;

    for (const auto &line : lines)
    {
        std::string trimmed = line;
        // 移除末尾分号
        trimmed.erase(std::remove(trimmed.begin(), trimmed.end(), ';'), trimmed.end());
        trimmed = trimStr(trimmed);

        if (trimmed.empty())
            continue;

        // 处理大括号控制缩进
        if (trimmed == "{")
        {
            currentIndent++;
            continue;
        }
        if (trimmed == "}")
        {
            currentIndent--;
            if (currentIndent < indentLevel)
                currentIndent = indentLevel;
            continue;
        }

        // 处理变量声明 (num x = 10 -> x = 10)
        if (trimmed.compare(0, 4, "num ") == 0)
            trimmed = trimmed.substr(4);
        else if (trimmed.compare(0, 4, "str ") == 0)
            trimmed = trimmed.substr(4);
        else if (trimmed.compare(0, 5, "bool ") == 0)
            trimmed = trimmed.substr(5);
        else if (trimmed.compare(0, 4, "obj ") == 0)
            trimmed = trimmed.substr(4);

        // 处理 if (cond) -> if cond:
        if (trimmed.compare(0, 3, "if ") == 0 || trimmed.compare(0, 3, "if(") == 0)
        {
            size_t start = trimmed.find('(');
            size_t end = trimmed.find_last_of(')');
            if (start != std::string::npos && end != std::string::npos)
            {
                std::string cond = trimmed.substr(start + 1, end - start - 1);
                trimmed = "if " + normalizeExpression(cond) + ":";
            }
        }
        // 处理 elif (cond) -> elif cond:
        else if (trimmed.compare(0, 5, "elif ") == 0 || trimmed.compare(0, 5, "elif(") == 0)
        {
            size_t start = trimmed.find('(');
            size_t end = trimmed.find_last_of(')');
            if (start != std::string::npos && end != std::string::npos)
            {
                std::string cond = trimmed.substr(start + 1, end - start - 1);
                trimmed = "elif " + normalizeExpression(cond) + ":";
            }
        }
        // 处理 else -> else:
        else if (trimmed == "else")
        {
            trimmed = "else:";
        }
        // 处理 for (i, count) -> for i in range(count):
        else if (trimmed.compare(0, 4, "for ") == 0 || trimmed.compare(0, 4, "for(") == 0)
        {
            size_t start = trimmed.find('(');
            size_t end = trimmed.find_last_of(')');
            if (start != std::string::npos && end != std::string::npos)
            {
                std::string content = trimmed.substr(start + 1, end - start - 1);
                size_t comma = content.find(',');
                if (comma != std::string::npos)
                {
                    std::string var = trimStr(content.substr(0, comma));
                    std::string count = trimStr(content.substr(comma + 1));
                    trimmed = "for " + var + " in range(" + count + "):";
                }
            }
        }

        // 处理 print/println
        std::string remaining = trimmed;
        size_t scanPos = 0;
        bool hasPrint = false;
        while (true)
        {
            size_t pos = remaining.find("print", scanPos);
            if (pos == std::string::npos)
                break;

            size_t start = remaining.find('(', pos);
            if (start == std::string::npos)
            {
                scanPos = pos + 5;
                continue;
            }

            size_t end = remaining.find(')', start);
            if (end == std::string::npos)
            {
                scanPos = pos + 5;
                continue;
            }

            std::string inner = remaining.substr(start + 1, end - start - 1);
            std::string newInner = transformPrintContent(inner);

            if (trimStr(remaining) == remaining.substr(pos, end - pos + 1))
            {
                genLines.push_back({currentIndent, "print(" + newInner + ")"});
                hasPrint = true;
                remaining = "";
                break;
            }
            else
            {
                remaining.replace(pos, end - pos + 1, "print(" + newInner + ")");
                scanPos = pos + newInner.length() + 7;
            }
        }

        if (!remaining.empty())
        {
            genLines.push_back({currentIndent, normalizeExpression(remaining)});
        }
    }

    if (genLines.empty())
    {
        return indent(indentLevel) + "pass\n";
    }

    std::stringstream final_ss;
    for (size_t i = 0; i < genLines.size(); ++i)
    {
        final_ss << indent(genLines[i].indent) << genLines[i].content << "\n";
        // 如果当前行以 : 结尾，检查下一行是否缩进更深
        if (!genLines[i].content.empty() && genLines[i].content.back() == ':')
        {
            bool hasDeeper = false;
            if (i + 1 < genLines.size())
            {
                if (genLines[i + 1].indent > genLines[i].indent)
                {
                    hasDeeper = true;
                }
            }
            if (!hasDeeper)
            {
                final_ss << indent(genLines[i].indent + 1) << "pass\n";
            }
        }
    }

    return final_ss.str();
}

std::string PythonGenerator::generate()
{
    std::stringstream ss;

    // --- 1. Imports ---
    ss << "\"\"\"\n"
       << "Generated by Wolf DSL Translator\n"
       << "Game: " << result.gameName << "\n\"\"\"\n\n";
    ss << "import random\nimport time\nfrom pathlib import Path\nfrom typing import List, Dict, Optional\n\n";
    ss << "# Engine Imports\n";
    ss << "from game.engine import Game, GamePhase, GameStep, GameAction\n";
    ss << "from game.models import Role, DeathReason\n";
    ss << "from game.player import Player\n\n\n";

    // --- 2. Action Classes ---
    ss << "# ==========================================\n";
    ss << "# Action Classes\n";
    ss << "# ==========================================\n\n";
    for (const auto &action : result.actions)
    {
        std::string className = action.name;
        if (!className.empty())
            className[0] = std::toupper(className[0]);
        className += "Action";
        ss << "class " << className << "(GameAction):\n";
        ss << indent(1) << "\"\"\"Action class for DSL action: " << action.name << "\"\"\"\n";
        ss << indent(1) << "def execute(self, game, player, target=None):\n";
        ss << indent(2) << "game.action_" << action.name << "(target)\n\n";
    }

    // --- 3. Main Game Class ---
    ss << "# ==========================================\n";
    ss << "# Main Game Class\n";
    ss << "# ==========================================\n\n";
    ss << "class " << result.gameName << "(Game):\n";
    ss << indent(1) << "\"\"\"" << result.gameName << " implementation generated from DSL.\"\"\"\n\n";
    ss << indent(1) << "def __init__(self, players: List[Dict[str, str]], event_emitter=None, input_handler=None):\n";
    ss << indent(2) << "super().__init__(\"" << result.gameName << "\", players, event_emitter, input_handler)\n\n";

    ss << indent(2) << "# DSL Global Variables\n";
    for (const auto &pair : result.variables)
        ss << indent(2) << "self." << pair.first << " = " << toPythonLiteral(pair.second.value) << "\n";

    ss << "\n"
       << indent(2) << "self._init_phases()\n\n";

    // --- 4. Lifecycle Methods ---
    ss << indent(1) << "def _init_phases(self):\n";
    ss << indent(2) << "\"\"\"Initialize game phases and steps from DSL.\"\"\"\n";
    if (result.phases.empty())
    {
        ss << indent(2) << "pass\n";
    }
    else
    {
        for (const auto &phase : result.phases)
        {
            ss << indent(2) << phase.name << " = GamePhase(\"" << phase.name << "\")\n";
            for (const auto &step : phase.steps)
            {
                std::string actionClass = "None";
                if (!step.actionName.empty())
                {
                    actionClass = step.actionName;
                    actionClass[0] = std::toupper(actionClass[0]);
                    actionClass += "Action()";
                }
                ss << indent(2) << phase.name << ".add_step(GameStep(\"" << step.name << "\", [";
                for (size_t i = 0; i < step.rolesInvolved.size(); ++i)
                    ss << "Role." << step.rolesInvolved[i] << (i == step.rolesInvolved.size() - 1 ? "" : ", ");
                ss << "], " << actionClass << "))\n";
            }
            ss << indent(2) << "self.phases.append(" << phase.name << ")\n\n";
        }
    }

    ss << indent(1) << "def setup_game(self):\n";
    ss << indent(2) << "\"\"\"Custom game setup logic from DSL.\"\"\"\n";
    ss << indent(2) << "super().setup_game()  # Call engine base setup\n";
    ss << indent(2) << "# DSL Custom Logic\n"
       << translateBody(result.setup.bodyLines, 2) << "\n";

    // --- 5. DSL Logic (Actions & Methods) ---
    auto genMethod = [&](const std::string &prefix, const auto &items)
    {
        for (const auto &item : items)
        {
            ss << indent(1) << "def " << prefix << item.name << "(self, ";
            for (size_t i = 0; i < item.params.size(); ++i)
                ss << item.params[i].name << (i == item.params.size() - 1 ? "" : ", ");
            ss << "):\n";
            ss << translateBody(item.bodyLines, 2) << "\n";
        }
    };
    genMethod("action_", result.actions);
    genMethod("", result.methods);

    // --- 6. Utilities ---
    ss << indent(1) << "# ------------------------------------------\n";
    ss << indent(1) << "def get_alive_players(self, roles: Optional[List] = None) -> List[str]:\n";
    ss << indent(2) << "\"\"\"Get names of alive players, optionally filtered by roles.\"\"\"\n";
    ss << indent(2) << "return [name for name, p in self.players.items() \n";
    ss << indent(3) << "        if p.is_alive and (roles is None or p.role in [getattr(r, 'value', r) for r in roles])]\n\n";

    // --- 7. Entry Point ---
    ss << "if __name__ == \"__main__\":\n";
    ss << indent(1) << "# Simple test execution\n";
    ss << indent(1) << "players_data = [{'name': f'Player{i}'} for i in range(6)]\n";
    ss << indent(1) << "game_instance = " << result.gameName << "(players_data)\n";
    ss << indent(1) << "game_instance.setup_game()\n";
    ss << indent(1) << "print(f\"Game '{game_instance.name}' initialized with {len(game_instance.phases)} phases.\")\n";

    return ss.str();
}
