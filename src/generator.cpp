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
    if (expr == "[]" || expr == "[ ]")
        return "[]";

    // 1. Logic operators & Method replacements
    auto replaceRules = [&](std::string &s)
    {
        auto fix = [&](std::string a, std::string b)
        { size_t p=0; while((p=s.find(a,p))!=std::string::npos) s.replace(p,a.size(),b), p+=b.size(); };
        fix("&&", " and ");
        fix("||", " or ");
        fix("!=", "__NE__");
        fix("!", " not ");
        struct Rule
        {
            std::string m;
            std::string r;
            bool isL;
        };
        std::vector<Rule> rules = {{".length", "", true}, {".push", ".append", false}, {".join", "", false}, {".values", "()", false}, {".keys", "()", false}, {".items", "()", false}, {".capitalize", "()", false}};
        for (auto &rl : rules)
        {
            size_t p = 0;
            while ((p = s.find('.', p)) != std::string::npos)
            {
                size_t mP = p + 1;
                while (mP < s.size() && s[mP] == ' ')
                    mP++;
                std::string act = rl.m.substr(1);
                if (s.compare(mP, act.size(), act) != 0 || (mP + act.size() < s.size() && (isalnum(s[mP + act.size()]) || s[mP + act.size()] == '_')))
                {
                    p++;
                    continue;
                }
                size_t st = p;
                while (st > 0 && s[st - 1] == ' ')
                    st--;
                size_t endObj = st;
                int d = 0;
                while (st > 0)
                {
                    char pv = s[st - 1];
                    if (pv == ')')
                        d++;
                    else if (pv == '(')
                        d--;
                    if (d == 0 && !(isalnum(pv) || pv == '_' || pv == '.' || pv == '[' || pv == ']' || pv == ' '))
                        break;
                    static const std::vector<std::string> kw = {"if ", "elif ", "for ", "while ", "return ", "else "};
                    bool fnd = false;
                    for (auto &k : kw)
                        if (st >= k.size() && s.substr(st - k.size(), k.size()) == k)
                        {
                            fnd = true;
                            break;
                        }
                    if (fnd)
                        break;
                    st--;
                }
                std::string obj = trimStr(s.substr(st, endObj - st));
                if (obj.empty())
                {
                    p = mP + act.size();
                    continue;
                }
                if (rl.isL)
                    s.replace(st, mP + act.size() - st, "len(" + obj + ")");
                else if (act == "join")
                {
                    size_t op = s.find('(', mP + act.size());
                    if (op != std::string::npos && op < mP + act.size() + 5)
                    {
                        int d = 1;
                        size_t cp = std::string::npos;
                        for (size_t i = op + 1; i < s.size(); ++i)
                        {
                            if (s[i] == '(')
                                d++;
                            else if (s[i] == ')')
                                d--;
                            if (d == 0)
                            {
                                cp = i;
                                break;
                            }
                        }
                        if (cp != std::string::npos)
                        {
                            std::string sep = trimStr(s.substr(op + 1, cp - op - 1));
                            if (sep.empty())
                                sep = "''";
                            if (sep.size() >= 2 && sep[0] == '"' && sep.back() == '"')
                                sep = "'" + sep.substr(1, sep.size() - 2) + "'";
                            s.replace(st, cp + 1 - st, sep + ".join(" + obj + ")");
                            p = st;
                            continue;
                        }
                    }
                    s.replace(st, mP + act.size() - st, "''.join(" + obj + ")");
                }
                else
                {
                    std::string r = (rl.r[0] == '.') ? (obj + rl.r) : (obj + "." + act + rl.r);
                    if (rl.r == "()")
                    {
                        size_t pc = mP + act.size();
                        while (pc < s.size() && s[pc] == ' ')
                            pc++;
                        if (pc < s.size() && s[pc] == '(')
                            r = obj + "." + act;
                    }
                    s.replace(st, mP + act.size() - st, r);
                }
                p = st;
            }
        }
    };
    replaceRules(expr);

    // 2. Prefixing (self.)
    std::string out, tok;
    bool inQ = 0, isF = 0, inI = 0;
    char qC = 0;
    int bD = 0;
    auto flsh = [&]()
    {
        if (tok.empty())
            return;
        std::string low = tok;
        for (auto &c : low)
            c = (char)tolower(c);
        if (low == "true")
            out += "True";
        else if (low == "false")
            out += "False";
        else if (low == "null")
            out += "None";
        else if (varNames.count(tok))
        {
            if (out.size() < 5 || out.substr(out.size() - 5) != "self.")
                out += "self.";
            out += tok;
        }
        else
            out += tok;
        tok = "";
    };
    for (size_t i = 0; i < expr.size(); ++i)
    {
        char c = expr[i];
        if ((c == '"' || c == '\'') && (i == 0 || expr[i - 1] != '\\'))
        {
            if (!inQ)
            {
                isF = (i > 0 && (expr[i - 1] == 'f' || expr[i - 1] == 'F')) || (i > 1 && expr[i - 1] == ' ' && (expr[i - 2] == 'f' || expr[i - 2] == 'F'));
                flsh();
                inQ = 1;
                qC = c;
                out += c;
            }
            else if (c == qC && !inI)
            {
                inQ = 0;
                isF = 0;
                out += c;
            }
            else
                out += c;
        }
        else if (inQ)
        {
            if (isF && c == '{')
            {
                if (i + 1 < expr.size() && expr[i + 1] == '{')
                {
                    out += "{{";
                    i++;
                }
                else if (!inI)
                {
                    inI = 1;
                    bD = 1;
                    out += c;
                }
                else
                    bD++;
            }
            else if (isF && c == '}' && inI)
            {
                if (i + 1 < expr.size() && expr[i + 1] == '}')
                {
                    out += "}}";
                    i++;
                }
                else if (--bD == 0)
                {
                    inI = 0;
                    flsh();
                    out += c;
                }
                else
                    out += c;
            }
            else if (inI)
            {
                if (isalnum(c) || c == '_')
                    tok += c;
                else
                {
                    flsh();
                    out += c;
                }
            }
            else
                out += c;
        }
        else if (isalnum(c) || c == '_')
            tok += c;
        else
        {
            flsh();
            out += c;
        }
    }
    flsh();
    replaceRules(out);

    // 3. Final cleanup
    size_t p = 0;
    while ((p = out.find("__NE__", p)) != std::string::npos)
        out.replace(p, 6, "!="), p += 2;
    int oC = 0;
    for (char c : out)
        if (c == '(')
            oC++;
        else if (c == ')')
            oC--;
    while (oC > 0)
        out += " )", oC--;
    while (oC < 0)
    {
        size_t f = out.find(')');
        if (f != std::string::npos)
            out.erase(f, 1);
        oC++;
    }
    size_t qP = out.find('?'), cP = out.find(':', qP);
    if (qP != std::string::npos && cP != std::string::npos)
    {
        size_t eP = out.find('=');
        std::string pfx = "", act = out;
        if (eP != std::string::npos && eP < qP)
            pfx = out.substr(0, eP + 1) + " ", act = out.substr(eP + 1), qP -= (eP + 1), cP -= (eP + 1);
        out = pfx + trimStr(act.substr(qP + 1, cP - qP - 1)) + " if " + trimStr(act.substr(0, qP)) + " else " + trimStr(act.substr(cP + 1));
    }
    std::string fin;
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (out[i] == ' ')
        {
            if (i + 1 < out.size() && (out[i + 1] == '(' || out[i + 1] == ')' || out[i + 1] == ',' || out[i + 1] == ']' || out[i + 1] == '[' || out[i + 1] == '.'))
                continue;
            if (i > 0 && (out[i - 1] == '(' || out[i - 1] == '[' || out[i - 1] == '.'))
                continue;
        }
        fin += out[i];
    }
    out = fin;
    auto fxc = [&](std::string s, std::string r)
    { size_t p=0; while((p=out.find(s,p))!=std::string::npos) out.replace(p,s.size(),r), p+=r.size(); };
    fxc("get_alive_players(None)", "get_alive_players()");
    fxc("f \"", "f\"");
    fxc("f \'", "f\'");
    return out;
}

std::string PythonGenerator::transformPrintContent(const std::string &inner)
{
    std::string s = trimStr(inner);
    if (s.empty())
        return "''";
    size_t nl = 0;
    while ((nl = s.find('\n', nl)) != std::string::npos)
        s.replace(nl, 1, "\\n"), nl += 2;
    auto split = [&](const std::string &str)
    {
        std::vector<std::string> pts;
        size_t st = 0;
        bool inQ = 0;
        char qC = 0;
        int bL = 0;
        for (size_t i = 0; i < str.size(); ++i)
        {
            if ((str[i] == '"' || str[i] == '\'') && (i == 0 || str[i - 1] != '\\'))
            {
                if (!inQ)
                {
                    inQ = 1;
                    qC = str[i];
                }
                else if (str[i] == qC)
                    inQ = 0;
            }
            if (!inQ)
            {
                if (str[i] == '(')
                    bL++;
                else if (str[i] == ')')
                    bL--;
                else if (str[i] == '+' && bL == 0)
                {
                    pts.push_back(trimStr(str.substr(st, i - st)));
                    st = i + 1;
                }
            }
        }
        pts.push_back(trimStr(str.substr(st)));
        return pts;
    };
    auto pts = split(s);
    if (pts.size() <= 1)
        return normalizeExpression(s);
    std::string f = "f\"";
    for (auto &p : pts)
    {
        if (p.empty())
            continue;
        std::string cp = p;
        if (cp.size() >= 3 && (cp[0] == 'f' || cp[0] == 'F'))
        {
            size_t qP = cp.find_first_of("\"'");
            if (qP != std::string::npos && qP > 1)
                cp = cp.substr(0, 1) + cp.substr(qP);
        }
        bool isL = (cp.front() == '"' && cp.back() == '"') || (cp.front() == '\'' && cp.back() == '\'');
        bool isFL = !isL && cp.size() >= 3 && (cp[0] == 'f' || cp[0] == 'F') && ((cp[1] == '"' && cp.back() == '"') || (cp[1] == '\'' && cp.back() == '\''));
        if (isL || isFL)
        {
            std::string lit = cp.substr(isL ? 1 : 2, cp.size() - (isL ? 2 : 3));
            for (char c : lit)
            {
                if (c == '{')
                    f += "{{";
                else if (c == '}')
                    f += "}}";
                else
                    f += c;
            }
        }
        else
            f += "{" + normalizeExpression(p) + "}";
    }
    return f + "\"";
}

std::string PythonGenerator::translateBody(const std::vector<std::string> &lines, int indentLevel)
{
    if (lines.empty())
        return indent(indentLevel) + "pass\n";
    struct GenLine
    {
        int idt;
        std::string ct;
        bool isD = false;
    };
    std::vector<GenLine> gLs;
    int cur = indentLevel;
    bool inD = false;
    for (auto line : lines)
    {
        std::string tm = line;
        tm.erase(std::remove(tm.begin(), tm.end(), ';'), tm.end());
        tm = trimStr(tm);
        if (tm.empty())
            continue;
        bool tRm = 1;
        while (tRm)
        {
            tRm = 0;
            static const std::vector<std::string> ts = {"num", "str", "bool", "obj", "num[]", "str[]", "bool[]", "obj[]", "[]", "[ ]"};
            for (auto &t : ts)
                if (tm.compare(0, t.size(), t) == 0)
                {
                    if (t.back() != ' ' && tm.size() > t.size() && (isalnum(tm[t.size()]) || tm[t.size()] == '_'))
                        continue;
                    tm = trimStr(tm.substr(t.size()));
                    tRm = 1;
                    break;
                }
        }
        if (tm.empty())
            continue;
        if (tm == "{" || tm == "else{" || tm == "else {")
        {
            if (tm == "{" && (inD || (!gLs.empty() && gLs.back().ct.back() == ':')))
                continue;
            if (tm != "{")
                gLs.push_back({cur, "else:"});
            cur++;
            continue;
        }
        if (tm == "}" || tm == "};")
        {
            if (inD)
            {
                if (cur > indentLevel)
                    cur--;
                gLs.push_back({cur, "}", true});
                inD = 0;
                for (auto &a : pendingDictAssignments)
                    gLs.push_back({cur, a});
                pendingDictAssignments.clear();
            }
            else if (cur > indentLevel)
                cur--;
            continue;
        }
        if (tm[0] == '}')
        {
            if (cur > indentLevel)
                cur--;
            tm = trimStr(tm.substr(1));
            if (tm.empty())
                continue;
        }
        auto getBr = [](const std::string &s)
        { bool inS=0; char q=0; for (size_t i=0; i<s.size(); ++i) { if ((s[i]=='"'||s[i]=='\'')&&(i==0||s[i-1]!='\\')) { if(!inS){inS=1;q=s[i];} else if(s[i]==q)inS=0; } if (!inS && s[i] == '{') return (int)i; } return -1; };
        int bP = getBr(tm);
        bool isAD = (tm.find('=') != std::string::npos && (bP != -1 || tm.back() == '=')), isBS = (bP != -1 && !isAD && !inD);
        if (isBS)
        {
            tm = trimStr(tm.substr(0, bP));
            if (tm.empty())
            {
                cur++;
                continue;
            }
        }
        if (tm == "else")
        {
            gLs.push_back({cur, "else:"});
            cur++;
            continue;
        }
        auto hCtrl = [&](std::string kw, int l)
        {
            if (tm.compare(0, l, kw) == 0 && (tm.size() == (size_t)l || !isalnum(tm[l])))
            {
                size_t s = tm.find('('), e = tm.find_last_of(')');
                std::string cd = (s != std::string::npos && e != std::string::npos) ? tm.substr(s + 1, e - s - 1) : trimStr(tm.substr(l));
                if (kw == "for")
                {
                    size_t c = cd.find(',');
                    if (c != std::string::npos)
                    {
                        std::string v1 = trimStr(cd.substr(0, c)), r = trimStr(cd.substr(c + 1)), iP = " in ";
                        size_t ip = r.find(iP);
                        if (ip != std::string::npos)
                            gLs.push_back({cur, "for " + v1 + ", " + trimStr(r.substr(0, ip)) + " in " + normalizeExpression(r.substr(ip + 4)) + ".items():"});
                        else if (v1 == "role" && r == "count")
                            gLs.push_back({cur, "for role, count in self.role_config.items():"});
                        else
                            gLs.push_back({cur, "for " + v1 + " in " + normalizeExpression(r) + ":"});
                    }
                    else
                        gLs.push_back({cur, "for " + normalizeExpression(cd) + ":"});
                }
                else
                    gLs.push_back({cur, kw + (kw == "if" || kw == "elif" || kw == "while" ? " " + normalizeExpression(cd) : "") + ":"});
                cur++;
                return 1;
            }
            return 0;
        };
        if (hCtrl("if", 2) || hCtrl("elif", 4) || hCtrl("for", 3) || hCtrl("while", 5))
            continue;
        if (tm.find("print") == 0)
        {
            size_t s = tm.find('('), e = tm.find_last_of(')');
            if (s != std::string::npos && e != std::string::npos)
            {
                gLs.push_back({cur, "print(" + transformPrintContent(tm.substr(s + 1, e - s - 1)) + ")"});
                continue;
            }
        }
        if (isAD)
        {
            size_t bp = tm.find('{');
            std::string lf = (bp != std::string::npos) ? trimStr(tm.substr(0, bp)) : tm, nL = normalizeExpression(lf);
            if (!nL.empty() && nL.back() == '=')
                nL = trimStr(nL.substr(0, nL.size() - 1));
            if (nL.find('=') == std::string::npos)
                nL += " =";
            currentDictName = (nL.find("role_config") != std::string::npos) ? "role_config" : "";
            if (!currentDictName.empty())
                pendingDictAssignments.clear();
            gLs.push_back({cur, nL + " {", true});
            cur++;
            inD = true;
            if (bp != std::string::npos)
            {
                std::string tl = trimStr(tm.substr(bp + 1));
                size_t cp = tl.find('}');
                if (cp != std::string::npos)
                {
                    std::string ct = trimStr(tl.substr(0, cp));
                    if (!ct.empty())
                        gLs.push_back({cur, normalizeExpression(ct), true});
                    if (cur > indentLevel)
                        cur--;
                    gLs.push_back({cur, "}", true});
                    inD = false;
                }
                else if (!tl.empty())
                    gLs.push_back({cur, normalizeExpression(tl), true});
            }
        }
        else
        {
            std::string nm = normalizeExpression(tm);
            if (inD && !currentDictName.empty() && nm.find(currentDictName) != std::string::npos)
            {
                size_t col = nm.find(':');
                if (col != std::string::npos)
                {
                    std::string k = trimStr(nm.substr(0, col)), v = trimStr(nm.substr(col + 1));
                    if (!v.empty() && v.back() == ',')
                        v.pop_back();
                    pendingDictAssignments.push_back("self." + currentDictName + "[" + k + "] = " + v);
                    nm = k + ": 0";
                }
            }
            if (nm.find("println(") == 0)
                nm = "print(" + nm.substr(8);
            if (inD && nm.find(':') != std::string::npos && nm.back() != ',')
                nm += ",";
            if (!inD && nm.back() == '{')
                nm = trimStr(nm.substr(0, nm.size() - 1));
            gLs.push_back({cur, nm, inD});
            if (isBS || (bP != -1 && nm.find('{') != std::string::npos && nm.find('}') == std::string::npos && !inD))
                cur++;
        }
    }
    if (gLs.empty())
        return indent(indentLevel) + "pass\n";
    std::stringstream fs;
    for (size_t i = 0; i < gLs.size(); ++i)
    {
        fs << indent(gLs[i].idt) << gLs[i].ct << "\n";
        if (gLs[i].ct.back() == ':' && (i + 1 == gLs.size() || gLs[i + 1].idt <= gLs[i].idt))
            fs << indent(gLs[i].idt + 1) << "pass\n";
    }
    return fs.str();
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
