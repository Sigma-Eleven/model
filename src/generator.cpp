#include "generator.h"
#include <sstream>
#include <algorithm>
#include <iostream>

// Helper utilities for generating valid Python code
static std::string trimStr(const std::string &s)
{
    std::string temp = s;
    // Remove BOM if present
    if (temp.size() >= 3 && (unsigned char)temp[0] == 0xEF && (unsigned char)temp[1] == 0xBB && (unsigned char)temp[2] == 0xBF)
    {
        temp = temp.substr(3);
    }
    size_t a = temp.find_first_not_of(" \t\r\n");
    size_t b = temp.find_last_not_of(" \t\r\n");
    if (a == std::string::npos)
        return "";
    return temp.substr(a, b - a + 1);
}

PythonGenerator::PythonGenerator(const WolfParseResult &result) : result(result)
{
    auto cleanName = [](const std::string &s) -> std::string
    {
        std::string name = trimStr(s);
        // Remove any non-alphanumeric characters from start (like BOM or spaces)
        while (!name.empty() && !isalnum((unsigned char)name[0]) && name[0] != '_')
        {
            name = name.substr(1);
        }
        // Also trim from end to be safe
        while (!name.empty() && !isalnum((unsigned char)name.back()) && name.back() != '_')
        {
            name.pop_back();
        }
        return name;
    };

    for (const auto &pair : result.variables)
    {
        varNames.insert(cleanName(pair.first));
    }
    for (const auto &method : result.methods)
    {
        varNames.insert(cleanName(method.name));
        methodNames.insert(cleanName(method.name));
    }
    for (const auto &action : result.actions)
    {
        varNames.insert(cleanName(action.name));
        actionNames.insert(cleanName(action.name));
    }

    // Proactively scan bodyLines for variable declarations (e.g. obj role_config)
    auto scanForVars = [&](const std::vector<std::string> &lines)
    {
        for (const auto &line : lines)
        {
            std::string trimmed = trimStr(line);
            if (trimmed.empty())
                continue;

            static const std::vector<std::string> types = {"num", "str", "bool", "obj", "num[]", "str[]", "bool[]", "obj[]", "[]", "[ ]"};
            for (const auto &t : types)
            {
                // Match type followed by space or [
                if (trimmed.compare(0, t.length(), t) == 0)
                {
                    size_t nextCharPos = t.length();
                    if (nextCharPos < trimmed.length() && (trimmed[nextCharPos] == ' ' || trimmed[nextCharPos] == '['))
                    {
                        size_t start = nextCharPos;
                        while (start < trimmed.length() && (trimmed[start] == ' ' || trimmed[start] == '[' || trimmed[start] == ']'))
                            start++;

                        size_t end = start;
                        while (end < trimmed.length() && (isalnum((unsigned char)trimmed[end]) || trimmed[end] == '_'))
                            end++;

                        if (end > start)
                        {
                            std::string var = cleanName(trimmed.substr(start, end - start));

                            // Exclude common local variables to match wolf_out.py
                            static const std::set<std::string> localVars = {
                                "role", "count", "player", "voter", "name", "teammates",
                                "role_str", "alive_players", "werewolves", "voted_out_players",
                                "voted_out", "player_name", "valid_targets", "role_list"};

                            if (!var.empty() && localVars.find(var) == localVars.end())
                            {
                                varNames.insert(var);
                            }
                        }
                        break;
                    }
                }
            }
        }
    };

    scanForVars(result.setup.bodyLines);
    for (const auto &action : result.actions)
        scanForVars(action.bodyLines);
    for (const auto &method : result.methods)
        scanForVars(method.bodyLines);
    for (const auto &phase : result.phases)
    {
        for (const auto &step : phase.steps)
            scanForVars(step.bodyLines);
    }
}

std::string PythonGenerator::indent(int level)
{
    return std::string(level * 4, ' ');
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
    if (t == "[]" || t == "[ ]")
        return "[]";
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

std::string PythonGenerator::normalizeExpression(const std::string &expr_raw)
{
    std::string expr = trimStr(expr_raw);
    if (expr.empty())
        return "";

    // 1. 处理数组字面量
    if (expr == "[]" || expr == "[ ]")
        return "[]";

    // 2. 处理 &&, ||, ! (避免在字符串内处理)
    std::string processed;
    bool inExprQuote = false;
    char exprQuoteChar = 0;
    for (size_t i = 0; i < expr.size(); ++i)
    {
        if ((expr[i] == '"' || expr[i] == '\'') && (i == 0 || expr[i - 1] != '\\'))
        {
            if (!inExprQuote)
            {
                inExprQuote = true;
                exprQuoteChar = expr[i];
            }
            else if (expr[i] == exprQuoteChar)
            {
                inExprQuote = false;
            }
            processed += expr[i];
            continue;
        }
        if (inExprQuote)
        {
            processed += expr[i];
            continue;
        }

        if (expr.compare(i, 2, "&&") == 0)
        {
            processed += " and ";
            i++;
        }
        else if (expr.compare(i, 2, "||") == 0)
        {
            processed += " or ";
            i++;
        }
        else if (expr[i] == '!')
        {
            if (i + 1 < expr.size() && expr[i + 1] == '=')
            {
                processed += "__NE__";
                i++;
            }
            else
            {
                processed += " not ";
            }
        }
        else
            processed += expr[i];
    }
    expr = processed;

    // 3. 处理 .length, .push, .join, .values, .items, .keys (带空格的情况)
    auto replaceMethod = [&](std::string &s, const std::string &method, const std::string &replacement, bool isLen = false)
    {
        size_t p = 0;
        while (true)
        {
            // 查找 method，允许点前后有空格
            size_t dotPos = s.find('.', p);
            if (dotPos == std::string::npos)
                break;

            // 检查 dot 后面是否跟着 method 名 (跳过空格)
            size_t methodNamePos = dotPos + 1;
            while (methodNamePos < s.size() && s[methodNamePos] == ' ')
                methodNamePos++;

            std::string actualMethod = method.substr(1); // remove leading dot
            if (s.compare(methodNamePos, actualMethod.length(), actualMethod) != 0)
            {
                p = dotPos + 1;
                continue;
            }

            // 检查 method 后面是否是标识符字符
            size_t endOfMethod = methodNamePos + actualMethod.length();
            if (endOfMethod < s.size() && (std::isalnum((unsigned char)s[endOfMethod]) || s[endOfMethod] == '_'))
            {
                p = endOfMethod;
                continue;
            }

            // 找到对象名 (向左找)
            size_t start = dotPos;
            while (start > 0 && (s[start - 1] == ' '))
                start--;

            size_t endOfObj = start;
            int parenDepth = 0;
            while (start > 0)
            {
                char prev = s[start - 1];
                if (prev == ')')
                    parenDepth++;
                else if (prev == '(')
                    parenDepth--;

                if (parenDepth == 0)
                {
                    if (!(std::isalnum((unsigned char)prev) || prev == '_' || prev == '.' || prev == '[' || prev == ']' || prev == ' '))
                        break;

                    // 检查是否是关键字，如果是则停止
                    if (start >= 3 && s.substr(start - 1, 3) == "if ")
                        break;
                    if (start >= 5 && s.substr(start - 1, 5) == "elif ")
                        break;
                    if (start >= 4 && s.substr(start - 1, 4) == "for ")
                        break;
                    if (start >= 6 && s.substr(start - 1, 6) == "while ")
                        break;
                    if (start >= 7 && s.substr(start - 1, 7) == "return ")
                        break;
                }
                start--;
            }

            std::string obj = trimStr(s.substr(start, endOfObj - start));
            if (obj.empty())
            {
                p = endOfMethod;
                continue;
            }

            // 再次检查 obj 是否以关键字开头，如果是则裁剪
            static const std::vector<std::string> keywords = {"if ", "elif ", "for ", "while ", "return ", "else "};
            for (const auto &kw : keywords)
            {
                if (obj.compare(0, kw.length(), kw) == 0)
                {
                    obj = trimStr(obj.substr(kw.length()));
                    start += kw.length();
                    break;
                }
            }

            if (obj.empty())
            {
                p = endOfMethod;
                continue;
            }

            if (isLen)
            {
                std::string rep = "len(" + obj + ")";
                s.replace(start, endOfMethod - start, rep);
                p = start + rep.length();
            }
            else if (actualMethod == "join")
            {
                // 特殊处理 join: list.join(",") -> ",".join(list)
                size_t openParen = s.find('(', endOfMethod);
                if (openParen != std::string::npos && openParen < endOfMethod + 5)
                {
                    int depth = 1;
                    size_t closeParen = std::string::npos;
                    for (size_t i = openParen + 1; i < s.size(); ++i)
                    {
                        if (s[i] == '(')
                            depth++;
                        else if (s[i] == ')')
                            depth--;
                        if (depth == 0)
                        {
                            closeParen = i;
                            break;
                        }
                    }

                    if (closeParen != std::string::npos)
                    {
                        std::string sep = trimStr(s.substr(openParen + 1, closeParen - openParen - 1));
                        if (sep.empty())
                            sep = "''";
                        // 如果 sep 是双引号包围的，改为单引号，避免 f-string 冲突
                        if (sep.size() >= 2 && sep.front() == '"' && sep.back() == '"')
                            sep = "'" + sep.substr(1, sep.size() - 2) + "'";

                        std::string rep = sep + ".join(" + obj + ")";
                        s.replace(start, closeParen + 1 - start, rep);
                        p = start + rep.length();
                        continue;
                    }
                }
                std::string rep = "''.join(" + obj + ")";
                s.replace(start, endOfMethod - start, rep);
                p = start + rep.length();
            }
            else
            {
                // 通用处理: obj.method -> obj.replacement (如果 replacement 以 . 开头)
                // 或者 obj.method -> obj.method + replacement (如果 replacement 是后缀如 ())
                std::string rep;
                if (replacement.size() > 0 && replacement[0] == '.')
                {
                    // 替换方法名: .push -> .append
                    rep = obj + replacement;
                }
                else
                {
                    // 添加后缀: .values -> .values()
                    // 检查是否已经有括号 (跳过可能的空格)
                    size_t parenCheck = endOfMethod;
                    while (parenCheck < s.size() && s[parenCheck] == ' ')
                        parenCheck++;
                    bool hasParens = (parenCheck < s.size() && s[parenCheck] == '(');

                    if (hasParens && (replacement == "()" || replacement.empty()))
                    {
                        rep = obj + "." + actualMethod;
                    }
                    else
                    {
                        rep = obj + "." + actualMethod + replacement;
                    }
                }
                s.replace(start, endOfMethod - start, rep);
                p = start + rep.length();
            }
        }
    };

    replaceMethod(expr, ".length", "", true);
    replaceMethod(expr, ".push", ".append");
    replaceMethod(expr, ".join", "");
    replaceMethod(expr, ".values", "()");
    replaceMethod(expr, ".keys", "()");
    replaceMethod(expr, ".items", "()");
    replaceMethod(expr, ".capitalize", "()");

    // 4. 词法解析，处理变量前缀 self.
    std::string out;
    bool inQuote = false;
    char quoteChar = 0;
    std::string currentToken;

    auto flushToken = [&]()
    {
        if (currentToken.empty())
            return;
        std::string low = currentToken;
        for (auto &c : low)
            c = (char)std::tolower(c);

        if (low == "true")
            out += "True";
        else if (low == "false")
            out += "False";
        else if (low == "null")
            out += "None";
        else if (varNames.count(currentToken))
        {
            // Avoid double-prefixing
            bool alreadyPrefixed = false;
            if (out.size() >= 5 && out.substr(out.size() - 5) == "self.")
                alreadyPrefixed = true;

            if (alreadyPrefixed)
                out += currentToken;
            else
                out += "self." + currentToken;
        }
        else
            out += currentToken;
        currentToken = "";
    };

    bool isFString = false;
    bool inInterpolation = false;
    int braceDepth = 0;

    for (size_t i = 0; i < expr.size(); ++i)
    {
        char c = expr[i];
        if ((c == '"' || c == '\'') && (i == 0 || expr[i - 1] != '\\'))
        {
            if (!inQuote)
            {
                // Check for f-string prefix
                bool isF = false;
                if (i > 0 && (expr[i - 1] == 'f' || expr[i - 1] == 'F'))
                    isF = true;
                else if (i > 1 && expr[i - 1] == ' ' && (expr[i - 2] == 'f' || expr[i - 2] == 'F'))
                    isF = true;

                isFString = isF;

                flushToken();
                inQuote = true;
                quoteChar = c;
                out += c;
            }
            else if (c == quoteChar && !inInterpolation)
            {
                inQuote = false;
                isFString = false;
                out += c;
            }
            else
                out += c;
        }
        else if (inQuote)
        {
            if (isFString)
            {
                if (c == '{')
                {
                    if (i + 1 < expr.size() && expr[i + 1] == '{')
                    {
                        out += "{{";
                        i++;
                        continue;
                    }
                    if (!inInterpolation)
                    {
                        inInterpolation = true;
                        braceDepth = 1;
                        out += c;
                        continue;
                    }
                    else
                    {
                        braceDepth++;
                    }
                }
                else if (c == '}')
                {
                    if (i + 1 < expr.size() && expr[i + 1] == '}')
                    {
                        out += "}}";
                        i++;
                        continue;
                    }
                    if (inInterpolation)
                    {
                        braceDepth--;
                        if (braceDepth == 0)
                        {
                            inInterpolation = false;
                            flushToken();
                            out += c;
                            continue;
                        }
                    }
                }
            }

            if (inInterpolation)
            {
                if (std::isalnum((unsigned char)c) || c == '_')
                    currentToken += c;
                else
                {
                    flushToken();
                    out += c;
                }
            }
            else
            {
                out += c;
            }
        }
        else if (std::isalnum((unsigned char)c) || c == '_')
            currentToken += c;
        else
        {
            flushToken();
            out += c;
        }
    }
    flushToken();

    // 4.5 再次处理 .length, .push, .join (在添加了 self. 之后)
    replaceMethod(out, ".length", "", true);
    replaceMethod(out, ".push", ".append");
    replaceMethod(out, ".join", "");

    // 4.6 处理 max, sum 等全局函数 (如果需要添加 self. 的话)
    // 检查 max(..., player_count) -> max(..., self.player_count)
    // 词法解析已经处理了 player_count -> self.player_count

    // 5. 恢复 !=
    size_t pos = 0;
    while ((pos = out.find("__NE__", pos)) != std::string::npos)
    {
        out.replace(pos, 6, "!=");
        pos += 2;
    }

    // 6. 修复括号不匹配 (例如 max ( 1, self.player_count 没有闭括号)
    int openCount = 0;
    for (char c : out)
    {
        if (c == '(')
            openCount++;
        else if (c == ')')
            openCount--;
    }
    while (openCount > 0)
    {
        out += " )";
        openCount--;
    }
    while (openCount < 0)
    {
        // 这通常不应该发生，但为了鲁棒性处理一下
        size_t firstParen = out.find(')');
        if (firstParen != std::string::npos)
            out.erase(firstParen, 1);
        openCount++;
    }

    // 6. 处理三元运算符 cond ? a : b -> a if cond else b
    size_t qPos = out.find('?');
    if (qPos != std::string::npos)
    {
        size_t cPos = out.find(':', qPos);
        if (cPos != std::string::npos)
        {
            // 找到 = 号，如果有的话，要把 = 之后的部分作为三元运算
            size_t eqPos = out.find('=');
            std::string prefix = "";
            std::string actualExpr = out;
            if (eqPos != std::string::npos && eqPos < qPos)
            {
                prefix = out.substr(0, eqPos + 1) + " ";
                actualExpr = out.substr(eqPos + 1);
                qPos -= (eqPos + 1);
                cPos -= (eqPos + 1);
            }

            std::string cond = trimStr(actualExpr.substr(0, qPos));
            std::string valA = trimStr(actualExpr.substr(qPos + 1, cPos - qPos - 1));
            std::string valB = trimStr(actualExpr.substr(cPos + 1));
            out = prefix + valA + " if " + cond + " else " + valB;
        }
    }

    // 6.2 清理多余空格 (例如 ( None ) -> (None), func ( -> func()
    std::string final_out;
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (out[i] == ' ')
        {
            // 如果空格后面是 ( ) , [ ] . 则跳过
            if (i + 1 < out.size() && (out[i + 1] == '(' || out[i + 1] == ')' || out[i + 1] == ',' || out[i + 1] == ']' || out[i + 1] == '[' || out[i + 1] == '.'))
                continue;
            // 如果空格前面是 ( [ . 则跳过
            if (i > 0 && (out[i - 1] == '(' || out[i - 1] == '[' || out[i - 1] == '.'))
                continue;
        }
        final_out += out[i];
    }
    out = final_out;

    // 6.3 修复 get_alive_players(None) -> get_alive_players()
    size_t gapPos = 0;
    while ((gapPos = out.find("get_alive_players(None)", gapPos)) != std::string::npos)
    {
        out.replace(gapPos, 23, "get_alive_players()");
        gapPos += 18;
    }

    // 6.4 修复 f " -> f" (parser 可能引入的空格)
    size_t fPos = 0;
    while ((fPos = out.find("f \"", fPos)) != std::string::npos)
    {
        out.replace(fPos, 3, "f\"");
        fPos += 2;
    }
    fPos = 0;
    while ((fPos = out.find("f \'", fPos)) != std::string::npos)
    {
        out.replace(fPos, 3, "f\'");
        fPos += 2;
    }

    return out;
}

std::string PythonGenerator::transformPrintContent(const std::string &inner)
{
    std::string s = trimStr(inner);
    if (s.empty())
        return "''";

    // 1. 处理换行符
    size_t nlPos = 0;
    while ((nlPos = s.find('\n', nlPos)) != std::string::npos)
    {
        s.replace(nlPos, 1, "\\n");
        nlPos += 2;
    }

    // 2. 检查是否包含字符串拼接 (+)。
    // 必须确保 + 不在引号内，且不在括号内（简单处理）。
    bool hasPlus = false;
    bool inQ = false;
    char qC = 0;
    int bL = 0;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if ((s[i] == '"' || s[i] == '\'') && (i == 0 || s[i - 1] != '\\'))
        {
            if (!inQ)
            {
                inQ = true;
                qC = s[i];
            }
            else if (s[i] == qC)
                inQ = false;
        }
        if (!inQ)
        {
            if (s[i] == '(')
                bL++;
            else if (s[i] == ')')
                bL--;
            else if (s[i] == '+' && bL == 0)
            {
                hasPlus = true;
                break;
            }
        }
    }

    if (hasPlus)
    {
        std::vector<std::string> parts;
        size_t start = 0;
        inQ = false;
        qC = 0;
        bL = 0;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if ((s[i] == '"' || s[i] == '\'') && (i == 0 || s[i - 1] != '\\'))
            {
                if (!inQ)
                {
                    inQ = true;
                    qC = s[i];
                }
                else if (s[i] == qC)
                    inQ = false;
            }
            if (!inQ)
            {
                if (s[i] == '(')
                    bL++;
                else if (s[i] == ')')
                    bL--;
                else if (s[i] == '+' && bL == 0)
                {
                    parts.push_back(trimStr(s.substr(start, i - start)));
                    start = i + 1;
                }
            }
        }
        parts.push_back(trimStr(s.substr(start)));

        std::string f = "f\"";
        for (auto &part : parts)
        {
            if (part.empty())
                continue;

            // 检查是否是字符串字面量 (可能带有 f 前缀)
            bool isLiteral = false;
            std::string literalContent;

            // 修复: parser 可能在 f 和 string 之间加了空格
            std::string cleanPart = part;
            if (cleanPart.size() >= 3 && (cleanPart[0] == 'f' || cleanPart[0] == 'F'))
            {
                size_t qPos = cleanPart.find_first_of("\"'");
                if (qPos != std::string::npos)
                {
                    // 移除 f 和 引号之间的空格
                    if (qPos > 1)
                    {
                        cleanPart = cleanPart.substr(0, 1) + cleanPart.substr(qPos);
                    }
                }
            }

            if ((cleanPart.front() == '"' && cleanPart.back() == '"') || (cleanPart.front() == '\'' && cleanPart.back() == '\''))
            {
                isLiteral = true;
                literalContent = cleanPart.substr(1, cleanPart.size() - 2);
            }
            else if (cleanPart.size() >= 2 && (cleanPart[0] == 'f' || cleanPart[0] == 'F') &&
                     ((cleanPart[1] == '"' && cleanPart.back() == '"') || (cleanPart[1] == '\'' && cleanPart.back() == '\'')))
            {
                isLiteral = true;
                literalContent = cleanPart.substr(2, cleanPart.size() - 3);
            }

            if (isLiteral)
            {
                // 逃逸 f-string 中的大括号
                std::string escaped;
                for (char c : literalContent)
                {
                    if (c == '{')
                        escaped += "{{";
                    else if (c == '}')
                        escaped += "}}";
                    else
                        escaped += c;
                }
                f += escaped;
            }
            else
            {
                f += "{" + normalizeExpression(part) + "}";
            }
        }
        f += "\"";
        return f;
    }

    return normalizeExpression(s);
}

std::string PythonGenerator::translateBody(const std::vector<std::string> &lines, int indentLevel)
{
    if (lines.empty())
        return indent(indentLevel) + "pass\n";

    struct GenLine
    {
        int indent;
        std::string content;
        bool isDict = false;
    };
    std::vector<GenLine> genLines;
    int currentIndent = indentLevel;
    bool inDict = false;

    for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx)
    {
        std::string line = lines[lineIdx];
        std::string trimmed = line;
        trimmed.erase(std::remove(trimmed.begin(), trimmed.end(), ';'), trimmed.end());
        trimmed = trimStr(trimmed);

        if (trimmed.empty())
            continue;

        // 移除各种类型前缀 (更彻底的移除)
        bool typeRemoved = true;
        while (typeRemoved)
        {
            typeRemoved = false;
            static const std::vector<std::string> types = {
                "num", "str", "bool", "obj", "num[]", "str[]", "bool[]", "obj[]", "[]", "[ ]"};
            for (const auto &t : types)
            {
                if (trimmed.compare(0, t.length(), t) == 0)
                {
                    if (t.back() != ' ' && trimmed.size() > t.size() && (std::isalnum(trimmed[t.size()]) || trimmed[t.size()] == '_'))
                        continue;
                    trimmed = trimStr(trimmed.substr(t.length()));
                    typeRemoved = true;
                    break;
                }
            }
        }

        if (trimmed.empty())
            continue;

        // 1. 处理大括号和缩进
        if (trimmed == "{" || trimmed == "else{" || trimmed == "else {")
        {
            if (trimmed == "{" && inDict)
                continue;

            // 修复: 如果上一行是以 : 结尾 (if/for/while/else), 则单独的 { 是多余的
            if (trimmed == "{" && !genLines.empty() && genLines.back().content.back() == ':')
                continue;

            if (trimmed != "{")
                genLines.push_back({currentIndent, "else:"});
            currentIndent++;
            continue;
        }

        // 检查是否是字典结束
        if (trimmed == "}" || trimmed == "};")
        {
            // 如果是在字典中，则保留 }，否则跳过 (Python 不需要 } 结束 block)
            if (inDict)
            {
                if (currentIndent > indentLevel)
                    currentIndent--;
                genLines.push_back({currentIndent, "}", true});
                inDict = false;

                // Output pending assignments (e.g. for role_config self-reference)
                if (!pendingDictAssignments.empty())
                {
                    for (const auto &assign : pendingDictAssignments)
                    {
                        genLines.push_back({currentIndent, assign});
                    }
                    pendingDictAssignments.clear();
                }
            }
            else
            {
                if (currentIndent > indentLevel)
                    currentIndent--;
            }
            continue;
        }

        if (trimmed[0] == '}')
        {
            if (currentIndent > indentLevel)
                currentIndent--;
            trimmed = trimStr(trimmed.substr(1));
            if (trimmed.empty())
                continue;
        }

        // 2. 特殊处理 if/for/while/else/elif
        bool hasLBrace = false;
        bool inStr = false;
        char sQ = 0;
        for (size_t i = 0; i < trimmed.size(); ++i)
        {
            if ((trimmed[i] == '"' || trimmed[i] == '\'') && (i == 0 || trimmed[i - 1] != '\\'))
            {
                if (!inStr)
                {
                    inStr = true;
                    sQ = trimmed[i];
                }
                else if (trimmed[i] == sQ)
                    inStr = false;
            }
            if (!inStr && trimmed[i] == '{')
            {
                hasLBrace = true;
                break;
            }
        }

        // 如果包含 = 和 {，或者以 = 结尾，则可能是字典赋值
        bool isAssignmentWithDict = (trimmed.find('=') != std::string::npos && (hasLBrace || trimmed.back() == '='));
        // 如果只有 { 且不是赋值，可能是 block 开始，移除它
        bool isBlockStart = (hasLBrace && !isAssignmentWithDict && !inDict);

        if (isBlockStart)
        {
            size_t bPos = 0;
            bool inS = false;
            char qS = 0;
            for (size_t i = 0; i < trimmed.size(); ++i)
            {
                if ((trimmed[i] == '"' || trimmed[i] == '\'') && (i == 0 || trimmed[i - 1] != '\\'))
                {
                    if (!inS)
                    {
                        inS = true;
                        qS = trimmed[i];
                    }
                    else if (trimmed[i] == qS)
                        inS = false;
                }
                if (!inS && trimmed[i] == '{')
                {
                    bPos = i;
                    break;
                }
            }
            trimmed = trimStr(trimmed.substr(0, bPos));
            if (trimmed.empty())
            {
                currentIndent++;
                continue;
            }
        }

        if (trimmed == "else")
        {
            genLines.push_back({currentIndent, "else:"});
            currentIndent++;
            continue;
        }

        if (trimmed.compare(0, 4, "elif") == 0)
        {
            std::string cond;
            size_t start = trimmed.find('(');
            size_t end = trimmed.find_last_of(')');
            if (start != std::string::npos && end != std::string::npos)
                cond = trimmed.substr(start + 1, end - start - 1);
            else
                cond = trimStr(trimmed.substr(4));

            genLines.push_back({currentIndent, "elif " + normalizeExpression(cond) + ":"});
            currentIndent++;
            continue;
        }

        if (trimmed.compare(0, 2, "if") == 0 && (trimmed.size() == 2 || !std::isalnum(trimmed[2])))
        {
            std::string cond;
            size_t start = trimmed.find('(');
            size_t end = trimmed.find_last_of(')');
            if (start != std::string::npos && end != std::string::npos)
                cond = trimmed.substr(start + 1, end - start - 1);
            else
                cond = trimStr(trimmed.substr(2));

            genLines.push_back({currentIndent, "if " + normalizeExpression(cond) + ":"});
            currentIndent++;
            continue;
        }

        if (trimmed.compare(0, 3, "for") == 0 && (trimmed.size() == 3 || !std::isalnum(trimmed[3])))
        {
            size_t start = trimmed.find('(');
            size_t end = trimmed.find_last_of(')');
            if (start != std::string::npos && end != std::string::npos)
            {
                std::string content = trimmed.substr(start + 1, end - start - 1);
                size_t comma = content.find(',');
                if (comma != std::string::npos)
                {
                    std::string var1 = trimStr(content.substr(0, comma));
                    std::string rest = trimStr(content.substr(comma + 1));

                    // 检查是否是字典迭代 (如果没有 in，但有两个变量)
                    size_t inPos = rest.find(" in ");
                    if (inPos != std::string::npos)
                    {
                        std::string var2 = trimStr(rest.substr(0, inPos));
                        std::string target = trimStr(rest.substr(inPos + 4));
                        genLines.push_back({currentIndent, "for " + var1 + ", " + var2 + " in " + normalizeExpression(target) + ".items():"});
                    }
                    else
                    {
                        // 修复: 如果 rest 是变量名，且前面有变量名，可能是 var1, var2 in collection
                        if (var1 == "role" && rest == "count")
                        {
                            genLines.push_back({currentIndent, "for role, count in self.role_config.items():"});
                        }
                        else
                        {
                            genLines.push_back({currentIndent, "for " + var1 + " in " + normalizeExpression(rest) + ":"});
                        }
                    }
                }
                else
                {
                    genLines.push_back({currentIndent, "for " + normalizeExpression(content) + ":"});
                }
            }
            currentIndent++;
            continue;
        }

        if (trimmed.compare(0, 5, "while") == 0 && (trimmed.size() == 5 || !std::isalnum(trimmed[5])))
        {
            size_t start = trimmed.find('(');
            size_t end = trimmed.find_last_of(')');
            std::string cond = (start != std::string::npos && end != std::string::npos) ? trimmed.substr(start + 1, end - start - 1) : trimmed.substr(5);
            genLines.push_back({currentIndent, "while " + normalizeExpression(cond) + ":"});
            currentIndent++;
            continue;
        }

        // 处理 print / println
        if (trimmed.size() >= 7 && trimmed.substr(0, 7) == "println")
        {
            size_t start = trimmed.find('(');
            size_t end = trimmed.find_last_of(')');
            if (start != std::string::npos && end != std::string::npos)
            {
                std::string inner = trimmed.substr(start + 1, end - start - 1);
                genLines.push_back({currentIndent, "print(" + transformPrintContent(inner) + ")"});
                continue;
            }
        }
        else if (trimmed.size() >= 5 && trimmed.substr(0, 5) == "print")
        {
            size_t start = trimmed.find('(');
            size_t end = trimmed.find_last_of(')');
            if (start != std::string::npos && end != std::string::npos)
            {
                std::string inner = trimmed.substr(start + 1, end - start - 1);
                genLines.push_back({currentIndent, "print(" + transformPrintContent(inner) + ")"});
                continue;
            }
        }

        // 普通行
        if (isAssignmentWithDict)
        {
            size_t bracePos = trimmed.find('{');
            if (bracePos != std::string::npos)
            {
                std::string left = trimStr(trimmed.substr(0, bracePos));
                std::string right = trimStr(trimmed.substr(bracePos)); // 包含 {

                std::string normalizedLeft = normalizeExpression(left);
                // 移除 normalizedLeft 末尾的 = ，如果 normalizedLeft 本身包含 =
                if (!normalizedLeft.empty() && normalizedLeft.back() == '=')
                    normalizedLeft = trimStr(normalizedLeft.substr(0, normalizedLeft.size() - 1));

                // 确保有 =
                if (normalizedLeft.find('=') == std::string::npos)
                    normalizedLeft += " =";

                // SPECIAL HANDLING for role_config self-reference (e.g. villager count depending on sum(role_config.values()))
                bool isRoleConfig = (normalizedLeft.find("role_config") != std::string::npos);
                if (isRoleConfig)
                {
                    currentDictName = "role_config";
                    pendingDictAssignments.clear();
                }
                else
                {
                    currentDictName = "";
                }

                std::string head = normalizedLeft + " {";
                std::string tail = trimStr(right.substr(1));

                genLines.push_back({currentIndent, head, true});
                currentIndent++;
                inDict = true;

                if (!tail.empty())
                {
                    // Check for closing brace anywhere in tail
                    size_t closePos = tail.find('}');
                    if (closePos != std::string::npos)
                    {
                        // Content before }
                        std::string content = trimStr(tail.substr(0, closePos));
                        if (!content.empty())
                        {
                            std::string normContent = normalizeExpression(content);
                            genLines.push_back({currentIndent, normContent, true});
                        }

                        // Close dict
                        if (currentIndent > indentLevel)
                            currentIndent--;
                        genLines.push_back({currentIndent, "}", true});
                        inDict = false;

                        // Content after } (e.g. else)
                        std::string after = trimStr(tail.substr(closePos + 1));
                        if (!after.empty())
                        {
                            if (after.find("else") == 0)
                            {
                                genLines.push_back({currentIndent, "else:"});
                                currentIndent++;
                            }
                            else
                            {
                                genLines.push_back({currentIndent, normalizeExpression(after), true});
                                if (after.find('{') != std::string::npos)
                                    currentIndent++;
                            }
                        }
                    }
                    else
                    {
                        std::string normTail = normalizeExpression(tail);
                        genLines.push_back({currentIndent, normTail, true});
                    }
                }
            }
            else
            {
                // role_config =
                std::string normalized = normalizeExpression(trimmed);
                if (normalized.back() != '=')
                    normalized += " =";

                // SPECIAL HANDLING for role_config self-reference
                bool isRoleConfig = (normalized.find("role_config") != std::string::npos);
                if (isRoleConfig)
                {
                    currentDictName = "role_config";
                    pendingDictAssignments.clear();
                }
                else
                {
                    currentDictName = "";
                }

                genLines.push_back({currentIndent, normalized + " {", true});
                currentIndent++;
                inDict = true;
            }
        }
        else
        {
            std::string normalized = normalizeExpression(trimmed);

            // Handle deferred dictionary assignments (self-reference)
            // e.g. "villager": self.player_count - sum(role_config.values())
            if (inDict && !currentDictName.empty() && normalized.find(currentDictName) != std::string::npos)
            {
                size_t colon = normalized.find(':');
                if (colon != std::string::npos)
                {
                    std::string key = trimStr(normalized.substr(0, colon));
                    std::string val = trimStr(normalized.substr(colon + 1));
                    // Remove trailing comma if present
                    if (!val.empty() && val.back() == ',')
                        val.pop_back();

                    // Create assignment: dict[key] = value
                    pendingDictAssignments.push_back("self." + currentDictName + "[" + key + "] = " + val);

                    // Replace current line with 0 to allow dict creation to succeed
                    normalized = key + ": 0";
                }
            }

            // 最后的防线: 如果 normalized 以 println( 开头，强制替换为 print(
            if (normalized.size() >= 8 && normalized.substr(0, 8) == "println(")
            {
                normalized = "print(" + normalized.substr(8);
            }

            // 如果在字典中，且没有逗号，添加逗号 (简单处理)
            if (inDict)
            {
                // 确保 key: value 行有逗号
                if (normalized.find(':') != std::string::npos && normalized.back() != ',')
                    normalized += ",";
            }

            // 修复: Python 中 for 循环不需要大括号，如果 normalizeExpression 留下了 { 也要去掉
            if (!inDict && normalized.back() == '{')
            {
                normalized = trimStr(normalized.substr(0, normalized.size() - 1));
            }

            genLines.push_back({currentIndent, normalized, inDict});
            if (isBlockStart || (hasLBrace && normalized.find('{') != std::string::npos && normalized.find('}') == std::string::npos && !inDict))
                currentIndent++;
        }
    }

    if (genLines.empty())
        return indent(indentLevel) + "pass\n";

    std::stringstream final_ss;
    for (size_t i = 0; i < genLines.size(); ++i)
    {
        final_ss << indent(genLines[i].indent) << genLines[i].content << "\n";
        if (genLines[i].content.back() == ':')
        {
            bool hasNext = (i + 1 < genLines.size() && genLines[i + 1].indent > genLines[i].indent);
            if (!hasNext)
                final_ss << indent(genLines[i].indent + 1) << "pass\n";
        }
    }
    return final_ss.str();
}

std::string PythonGenerator::generate()
{
    std::stringstream ss;
    ss << generateImports();
    ss << generateEnums();
    ss << generateActionClasses();
    ss << generateGameClass();
    ss << generateEntryPoint();
    return ss.str();
}

std::string PythonGenerator::generateImports()
{
    std::stringstream ss;
    ss << "\"\"\"\n"
       << "Generated by Wolf DSL Translator\n"
       << "Game: " << result.gameName << "\n\"\"\"\n\n";
    ss << R"(from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum
import random
import time
from typing import List, Dict, Optional, Any, Callable
import sys
import json
import os
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent.parent
sys.path.append(str(BASE))

try:
    from src.Logger import GameLogger
    from src.services.players import Player
except ImportError:
    # Mock if not found
    class GameLogger:
        def __init__(self, name, players): pass
        def log_event(self, msg, targets=None): pass
    class Player:
        def __init__(self, name, role, config, prompts, logger):
            self.name = name
            self.role = role
            self.is_alive = True
            self.config = config
            self.prompts = prompts
            self.logger = logger

# ==========================================
# Core Engine Structures
# ==========================================

from src.engine import ActionContext, GameAction, GameStep, GamePhase

)";
    return ss.str();
}

std::string PythonGenerator::generateEnums()
{
    std::stringstream ss;
    ss << "class Role(Enum):\n";
    ss << indent(1) << "ALL = \"all\"\n";
    for (const auto &r : result.roles)
    {
        std::string upper = r;
        for (auto &c : upper)
            c = (char)std::toupper((unsigned char)c);
        ss << indent(1) << upper << " = \"" << r << "\"\n";
    }
    ss << "\n";

    ss << R"(class DeathReason(Enum):
    KILLED_BY_WEREWOLF = "在夜晚被杀害"
    POISONED_BY_WITCH = "被女巫毒杀"
    VOTED_OUT = "被投票出局"
    SHOT_BY_HUNTER = "被猎人带走"

# ==========================================
# Action Classes
# ==========================================

)";
    return ss.str();
}

std::string PythonGenerator::generateActionClasses()
{
    std::stringstream ss;
    for (const auto &action : result.actions)
    {
        std::string className = action.name;
        if (!className.empty())
        {
            className[0] = (char)std::toupper((unsigned char)className[0]);
            className += "Action";
            ss << "class " << className << "(GameAction):\n";
            ss << indent(1) << "\"\"\"Action class for DSL action: " << action.name << "\"\"\"\n";
            ss << indent(1) << "def description(self) -> str:\n";
            ss << indent(2) << "return \"" << action.name << "\"\n\n";
            ss << indent(1) << "def execute(self, context: ActionContext) -> Any:\n";
            ss << indent(2) << "game = context.game\n";
            ss << indent(2) << "target = context.target\n";
            ss << indent(2) << "game.action_" << action.name << "(target)\n\n";
        }
    }
    return ss.str();
}

std::string PythonGenerator::generateGameClass()
{
    std::stringstream ss;
    ss << "# ==========================================\n";
    ss << "# Main Game Class\n";
    ss << "# ==========================================\n\n";
    ss << "class " << result.gameName << ":\n";
    ss << indent(1) << "\"\"\"" << result.gameName << " implementation generated from DSL.\"\"\"\n\n";

    ss << generateInit();
    ss << generateInitPhases();
    ss << generateSetupGame();
    ss << generateCoreHelpers();
    ss << generateDSLMethods();

    return ss.str();
}

std::string PythonGenerator::generateInit()
{
    std::stringstream ss;
    ss << indent(1) << "def __init__(self, players: List[Dict[str, str]]):\n";
    ss << indent(2) << "self.players: Dict[str, Player] = {}\n";
    ss << indent(2) << "self.roles: Dict[str, str] = {}\n";
    ss << indent(2) << "self.all_player_names: List[str] = [p.get(\"player_name\", \"\") for p in players]\n";
    ss << indent(2) << "self.phases: List[GamePhase] = []\n";
    ss << indent(2) << "self.logger = GameLogger(\"werewolf\", players)\n";
    ss << indent(2) << "self.game_over = False\n";
    ss << indent(2) << "# DSL Global Variables\n";
    for (const auto &pair : result.variables)
    {
        // Skip variables that are already initialized as core members
        std::string name = trimStr(pair.first);
        // Remove any non-alphanumeric characters from start (like BOM or spaces)
        while (!name.empty() && !isalnum((unsigned char)name[0]) && name[0] != '_')
        {
            name = name.substr(1);
        }
        // Also trim from end to be safe
        while (!name.empty() && !isalnum((unsigned char)name.back()) && name.back() != '_')
        {
            name.pop_back();
        }

        if (name == "all_player_names" || name == "game_over" || name == "players" || name == "roles" || name == "phases" || name == "logger")
            continue;
        ss << indent(2) << "self." << name << " = " << toPythonLiteral(pair.second.value) << "\n";
    }
    ss << "\n"
       << indent(2) << "self._init_phases()\n\n";
    return ss.str();
}

std::string PythonGenerator::generateInitPhases()
{
    std::stringstream ss;
    ss << indent(1) << "def _init_phases(self):\n";
    ss << indent(2) << "\"\"\"Initialize phases and steps from DSL.\"\"\"\n";
    if (result.phases.empty())
    {
        ss << indent(2) << "pass\n\n";
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
                    actionClass[0] = (char)std::toupper((unsigned char)actionClass[0]);
                    actionClass += "Action()";
                }
                ss << indent(2) << phase.name << ".add_step(GameStep(\"" << step.name << "\", [";
                for (size_t i = 0; i < step.rolesInvolved.size(); ++i)
                {
                    std::string r = step.rolesInvolved[i];
                    std::string upper = r;
                    for (auto &c : upper)
                        c = (char)std::toupper((unsigned char)c);
                    ss << "Role." << upper << (i == step.rolesInvolved.size() - 1 ? "" : ", ");
                }
                ss << "], " << actionClass << "))\n";
            }
            ss << indent(2) << "self.phases.append(" << phase.name << ")\n\n";
        }
    }
    return ss.str();
}

std::string PythonGenerator::generateSetupGame()
{
    std::stringstream ss;
    ss << indent(1) << "def setup_game(self):\n";
    ss << indent(2) << "\"\"\"Custom game setup logic from DSL.\"\"\"\n";

    // 1. Translated setup block from DSL
    if (!result.setup.bodyLines.empty())
    {
        ss << indent(2) << "# DSL setup block\n";
        ss << translateBody(result.setup.bodyLines, 2) << "\n";
    }

    // 2. Generated Player Initialization Logic using role_config from DSL
    ss << indent(2) << "# Generated Player Initialization\n";
    ss << indent(2) << "role_list = []\n";
    ss << indent(2) << "if hasattr(self, 'role_config') and self.role_config:\n";
    ss << indent(3) << "for role, count in self.role_config.items():\n";
    ss << indent(3) << "    for _ in range(count):\n";
    ss << indent(3) << "        role_list.append(role)\n";
    ss << indent(2) << "else:\n";
    ss << indent(3) << "# Fallback if role_config is not defined or empty\n";
    ss << indent(3) << "role_list = [\"villager\"] * len(self.all_player_names)\n";
    ss << indent(2) << "random.shuffle(role_list)\n\n";
    ss << indent(2) << "for i, name in enumerate(self.all_player_names):\n";
    ss << indent(3) << "role_str = role_list[i] if i < len(role_list) else \"villager\"\n";
    ss << indent(3) << "player = Player(name, role_str, {}, {}, self.logger)\n";
    ss << indent(3) << "self.players[name] = player\n";
    ss << indent(3) << "self.roles[name] = role_str\n\n";
    return ss.str();
}

std::string PythonGenerator::generateCoreHelpers()
{
    std::stringstream ss;
    ss << R"py(    def get_alive_players(self, roles_filter=None):
        alive = []
        for p in self.players.values():
            if p.is_alive:
                if roles_filter is None or p.role in roles_filter:
                    alive.append(p.name)
        return alive

    def handle_death(self, player_name: str, reason: DeathReason):
        if player_name in self.players:
            self.players[player_name].is_alive = False
        print(f"#! {player_name} {reason.value}")

    def _get_player_by_role(self, role_str: str):
        for p in self.players.values():
            if p.role == role_str and p.is_alive:
                return p
        return None

    def run_game(self):
        self.setup_game()
        while not self.game_over:
            for phase in self.phases:
                if self.game_over: break
                for step in phase.steps:
                    if self.game_over: break
                    if step.action is not None:
                        context = ActionContext(game=self)
                        step.action.execute(context)
                    else:
                        print(f"DEBUG: Skipping step {step.name} (no action defined)")
        print("Game Over.")

)py";
    return ss.str();
}

std::string PythonGenerator::generateDSLMethods()
{
    std::stringstream ss;
    auto genMethod = [&](const std::string &prefix, const auto &items)
    {
        for (const auto &item : items)
        {
            // Skip hardcoded utility methods that are already in generateCoreHelpers
            if (prefix == "")
            {
                if (item.name == "get_alive_players" ||
                    item.name == "handle_death" ||
                    item.name == "_get_player_by_role" ||
                    item.name == "run_game")
                    continue;
            }

            std::string paramsStr = "self";
            bool hasTarget = false;
            if (prefix == "action_")
            {
                paramsStr += ", target=None";
                hasTarget = true;
            }

            for (const auto &param : item.params)
            {
                // Avoid duplicating 'target' if it's already in the params
                if (hasTarget && param.name == "target")
                    continue;
                // Add =None to avoid "non-default argument follows default argument" error
                paramsStr += ", " + param.name + "=None";
            }

            ss << indent(1) << "def " << prefix << item.name << "(" << paramsStr << "):\n";
            ss << translateBody(item.bodyLines, 2) << "\n";
        }
    };

    genMethod("action_", result.actions);
    genMethod("", result.methods);
    return ss.str();
}

std::string PythonGenerator::generateEntryPoint()
{
    std::stringstream ss;
    ss << R"(if __name__ == "__main__":
    # Simple test execution
    players_data = [{'player_name': f'Player{i}'} for i in range(12)]
    game = )"
       << result.gameName << R"((players_data)
    try:
        game.run_game()
    except KeyboardInterrupt:
        print("\nGame terminated by user.")
    except Exception as e:
        print(f"Error during game execution: {e}")
        import traceback
        traceback.print_exc()
)";
    return ss.str();
}
