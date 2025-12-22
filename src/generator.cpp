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

static std::string normalizeExpression(std::string expr)
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

static std::string transformPrintContent(const std::string &inner)
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

    std::stringstream ss;
    for (const auto &line : lines)
    {
        std::string trimmed = line;
        // 移除末尾分号和空白
        trimmed.erase(std::remove(trimmed.begin(), trimmed.end(), ';'), trimmed.end());
        trimmed = trimStr(trimmed);

        // 分离并处理行内的多个 print/println 调用
        std::string remaining = trimmed;
        size_t scanPos = 0;
        bool foundPrint = false;
        while (true)
        {
            size_t pos = remaining.find("print", scanPos);
            if (pos == std::string::npos)
                break;
            // 找到 '('
            size_t start = remaining.find('(', pos);
            if (start == std::string::npos)
                break;
            // 找到下一个 ')' 对应
            size_t end = remaining.find(')', start);
            if (end == std::string::npos)
                break;
            std::string inner = remaining.substr(start + 1, end - start - 1);
            std::string newInner = transformPrintContent(inner);
            ss << indent(indentLevel) << "print(" << newInner << ")\n";
            // 删除已处理的 print(...) 段以便继续扫描剩余内容
            remaining.erase(pos, end - pos + 1);
            foundPrint = true;
            scanPos = pos; // 继续从删除处扫描
        }

        // 剩余代码（非 print 部分）也需要正常化和输出
        remaining = trimStr(remaining);
        if (!remaining.empty())
        {
            remaining = normalizeExpression(remaining);
            ss << indent(indentLevel) << remaining << "\n";
        }
        else if (!foundPrint)
        {
            // 如果既没有 print 也没有其他内容，输出 pass
            ss << indent(indentLevel) << "pass\n";
        }
    }
    return ss.str();
}

std::string PythonGenerator::generate()
{
    std::stringstream ss;

    ss << "# Generated Python code from Wolf DSL\n";
    ss << "import time\n\n";

    // 1. 角色定义
    ss << "ROLES = " << "[";
    for (size_t i = 0; i < result.roles.size(); ++i)
    {
        ss << "\"" << result.roles[i] << "\"" << (i == result.roles.size() - 1 ? "" : ", ");
    }
    ss << "]\n\n";

    // 2. 全局变量
    ss << "# Global Variables\n";
    for (const auto &pair : result.variables)
    {
        ss << pair.first << " = " << toPythonLiteral(pair.second.value) << "\n";
    }
    ss << "\n";

    // 3. 动作定义 (作为类或函数)
    ss << "# Actions\n";
    for (const auto &action : result.actions)
    {
        ss << "def action_" << action.name << "(";
        for (size_t i = 0; i < action.params.size(); ++i)
        {
            ss << action.params[i].name << (i == action.params.size() - 1 ? "" : ", ");
        }
        ss << "):\n";
        ss << translateBody(action.bodyLines, 1);
        ss << "\n";
    }

    // 4. 阶段与步骤逻辑
    ss << "class GameFlow:\n";
    ss << indent(1) << "def __init__(self):\n";
    ss << indent(2) << "self.current_phase = None\n\n";

    for (const auto &phase : result.phases)
    {
        ss << indent(1) << "def phase_" << phase.name << "(self):\n";
        ss << indent(2) << "print(f\"--- Phase: " << phase.name << " ---\")\n";
        for (const auto &step : phase.steps)
        {
            ss << indent(2) << "# Step: " << step.name << "\n";
            std::string cond = step.condition.empty() ? "" : normalizeExpression(step.condition);
            if (!cond.empty())
            {
                ss << indent(2) << "if " << cond << ":\n";
                ss << indent(3) << "print(f\"Executing step: " << step.name << "\")\n";
                if (!step.actionName.empty())
                {
                    const WolfParseResult::ActionDef *act = nullptr;
                    for (const auto &a : result.actions)
                        if (a.name == step.actionName)
                        {
                            act = &a;
                            break;
                        }
                    if (act && !act->params.empty())
                    {
                        ss << indent(3) << "action_" << step.actionName << "(";
                        for (size_t i = 0; i < act->params.size(); ++i)
                        {
                            if (i)
                                ss << ", ";
                            std::string pname = act->params[i].name;
                            std::string joined;
                            for (const auto &l : step.bodyLines)
                                joined += l + " ";
                            if (joined.find(pname) != std::string::npos)
                                ss << pname;
                            else
                                ss << "None";
                        }
                        ss << ")\n";
                    }
                    else
                    {
                        ss << indent(3) << "action_" << step.actionName << "()\n";
                    }
                }
                ss << translateBody(step.bodyLines, 3);
            }
            else
            {
                ss << indent(2) << "print(f\"Executing step: " << step.name << "\")\n";
                if (!step.actionName.empty())
                {
                    const WolfParseResult::ActionDef *act = nullptr;
                    for (const auto &a : result.actions)
                        if (a.name == step.actionName)
                        {
                            act = &a;
                            break;
                        }
                    if (act && !act->params.empty())
                    {
                        ss << indent(2) << "action_" << step.actionName << "(";
                        for (size_t i = 0; i < act->params.size(); ++i)
                        {
                            if (i)
                                ss << ", ";
                            std::string pname = act->params[i].name;
                            std::string joined;
                            for (const auto &l : step.bodyLines)
                                joined += l + " ";
                            if (joined.find(pname) != std::string::npos)
                                ss << pname;
                            else
                                ss << "None";
                        }
                        ss << ")\n";
                    }
                    else
                    {
                        ss << indent(2) << "action_" << step.actionName << "()\n";
                    }
                }
                ss << translateBody(step.bodyLines, 2);
            }
        }
        ss << "\n";
    }

    // 5. Setup与启动
    ss << "def run_game():\n";
    ss << indent(1) << "print(\"=== Starting " << result.gameName << " ===\")\n";
    ss << translateBody(result.setup.bodyLines, 1);
    ss << indent(1) << "flow = GameFlow()\n";
    for (const auto &phase : result.phases)
    {
        ss << indent(1) << "flow.phase_" << phase.name << "()\n";
    }

    ss << "\nif __name__ == \"__main__\":\n";
    ss << indent(1) << "run_game()\n";

    return ss.str();
}
