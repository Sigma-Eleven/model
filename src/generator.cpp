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
    auto clean = [](std::string n)
    {
        n = trimStr(n);
        while (!n.empty() && !isalnum((unsigned char)n[0]) && n[0] != '_')
            n = n.substr(1);
        while (!n.empty() && !isalnum((unsigned char)n.back()) && n.back() != '_')
            n.pop_back();
        return n;
    };
    for (auto &v : result.variables)
        varNames.insert(clean(v.first));
    for (auto &m : result.methods)
    {
        varNames.insert(clean(m.name));
        methodNames.insert(clean(m.name));
    }
    for (auto &a : result.actions)
    {
        varNames.insert(clean(a.name));
        actionNames.insert(clean(a.name));
    }

    auto scan = [&](const std::vector<std::string> &ls)
    {
        for (auto &l : ls)
        {
            std::string t = trimStr(l);
            static const std::vector<std::string> types = {"num", "str", "bool", "obj", "[]"};
            for (auto &tp : types)
            {
                if (t.find(tp) == 0 && (t.size() == tp.size() || !isalnum(t[tp.size()])))
                {
                    size_t s = tp.size();
                    while (s < t.size() && (isspace(t[s]) || t[s] == '[' || t[s] == ']'))
                        s++;
                    size_t e = s;
                    while (e < t.size() && (isalnum(t[e]) || t[e] == '_'))
                        e++;
                    std::string v = clean(t.substr(s, e - s));
                    static const std::set<std::string> skip = {"role", "count", "player", "voter", "name", "teammates", "role_str", "alive_players", "werewolves", "voted_out", "player_name", "valid_targets"};
                    if (!v.empty() && !skip.count(v))
                        varNames.insert(v);
                    break;
                }
            }
        }
    };
    static const std::vector<std::string> engineMembers = {"logger", "players", "all_player_names", "day_number", "killed_player", "last_guarded", "witch_save_used", "witch_poison_used", "role_config", "_running"};
    for (auto &m : engineMembers)
        varNames.insert(m);
    static const std::vector<std::string> engineMethods = {"get_alive_players", "get_player_by_role", "handle_death", "handle_hunter_shot", "check_game_over", "run_game", "run_phase", "stop_game", "load_basic_config", "process_discussion", "process_vote", "announce"};
    for (auto &m : engineMethods)
    {
        varNames.insert(m);
        methodNames.insert(m);
    }
    scan(result.setup.bodyLines);
    for (auto &a : result.actions)
        scan(a.bodyLines);
    for (auto &m : result.methods)
        scan(m.bodyLines);
    for (auto &p : result.phases)
        for (auto &s : p.steps)
            scan(s.bodyLines);
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

std::string PythonGenerator::normalizeExpression(const std::string &expr_raw, const std::string &prefix)
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
        {
            size_t p = 0;
            bool inQ = false;
            char qC = 0;
            while (p < s.size())
            {
                if ((s[p] == '"' || s[p] == '\'') && (p == 0 || s[p - 1] != '\\'))
                {
                    if (!inQ)
                    {
                        inQ = true;
                        qC = s[p];
                    }
                    else if (s[p] == qC)
                    {
                        inQ = false;
                    }
                    p++;
                    continue;
                }
                if (!inQ && s.compare(p, a.size(), a) == 0)
                {
                    s.replace(p, a.size(), b);
                    p += b.size();
                }
                else
                    p++;
            }
        };
        fix("&&", " and ");
        fix("||", " or ");
        fix("//", " // ");
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
                {
                    std::string r = "len(" + obj + ")";
                    s.replace(st, mP + act.size() - st, r);
                    p = st + r.size();
                }
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
                            std::string r = sep + ".join(" + obj + ")";
                            s.replace(st, cp + 1 - st, r);
                            p = st + r.size();
                            continue;
                        }
                    }
                    std::string r = "''.join(" + obj + ")";
                    s.replace(st, mP + act.size() - st, r);
                    p = st + r.size();
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
                    p = st + r.size();
                }
            }
        }
    };
    replaceRules(expr);

    // 2. Prefixing
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
            if (out.size() < prefix.size() || out.substr(out.size() - prefix.size()) != prefix)
                out += prefix;
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
    fxc("_get_alive_players(", "get_alive_players(");
    fxc("_get_player_by_role(", "get_player_by_role(");
    fxc("_cancel(", "stop_game(");
    fxc("f \"", "f\"");
    fxc("f \'", "f\'");
    // Map role strings to Role enum
    static const std::vector<std::pair<std::string, std::string>> roleMaps = {
        {"werewolf", "WEREWOLF"}, {"villager", "VILLAGER"}, {"seer", "SEER"}, {"witch", "WITCH"}, {"hunter", "HUNTER"}, {"guard", "GUARD"}};
    for (auto &rm : roleMaps)
    {
        fxc("\"" + rm.first + "\"", "Role." + rm.second + ".value");
        fxc("\'" + rm.first + "\'", "Role." + rm.second + ".value");
    }

    // Map death reason strings to DeathReason enum
    static const std::vector<std::pair<std::string, std::string>> deathMaps = {
        {"在夜晚被杀害", "KILLED_BY_WEREWOLF"},
        {"被女巫毒杀", "POISONED_BY_WITCH"},
        {"被投票出局", "VOTED_OUT"},
        {"被猎人带走", "SHOT_BY_HUNTER"}};
    for (auto &dm : deathMaps)
    {
        fxc("\"" + dm.first + "\"", "DeathReason." + dm.second);
        fxc("\'" + dm.first + "\'", "DeathReason." + dm.second);
    }
    return out;
}

std::string PythonGenerator::transformPrintContent(const std::string &inner, const std::string &prefix)
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
        return normalizeExpression(s, prefix);
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
            f += "{" + normalizeExpression(p, prefix) + "}";
    }
    return f + "\"";
}

std::string PythonGenerator::translateBody(const std::vector<std::string> &lines, int indentLevel, const std::string &prefix)
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
                            gLs.push_back({cur, "for " + v1 + ", " + trimStr(r.substr(0, ip)) + " in " + normalizeExpression(r.substr(ip + 4), prefix) + ".items():"});
                        else if (v1 == "role" && r == "count")
                            gLs.push_back({cur, "for role, count in " + prefix + "role_config.items():"});
                        else
                            gLs.push_back({cur, "for " + v1 + " in " + normalizeExpression(r, prefix) + ":"});
                    }
                    else
                        gLs.push_back({cur, "for " + normalizeExpression(cd, prefix) + ":"});
                }
                else
                    gLs.push_back({cur, kw + (kw == "if" || kw == "elif" || kw == "while" ? " " + normalizeExpression(cd, prefix) : "") + ":"});
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
                std::string content = transformPrintContent(tm.substr(s + 1, e - s - 1), prefix);

                // Specific handle_death mappings for DSL patterns
                if (content.find("#!") != std::string::npos)
                {
                    if (content.find("被女巫毒杀") != std::string::npos)
                    {
                        size_t p1 = content.find("{"), p2 = content.find("}");
                        if (p1 != std::string::npos && p2 != std::string::npos)
                        {
                            std::string target = content.substr(p1 + 1, p2 - p1 - 1);
                            gLs.push_back({cur, prefix + "handle_death(" + target + ", DeathReason.POISONED_BY_WITCH)"});
                            continue;
                        }
                    }
                    else if (content.find("在夜晚被杀害") != std::string::npos)
                    {
                        gLs.push_back({cur, prefix + "handle_death(" + prefix + "killed_player, DeathReason.KILLED_BY_WEREWOLF)"});
                        continue;
                    }
                    else if (content.find("被投票出局") != std::string::npos)
                    {
                        size_t p1 = content.find("{"), p2 = content.find("}");
                        if (p1 != std::string::npos && p2 != std::string::npos)
                        {
                            std::string target = content.substr(p1 + 1, p2 - p1 - 1);
                            gLs.push_back({cur, prefix + "handle_death(" + target + ", DeathReason.VOTED_OUT)"});
                            continue;
                        }
                    }
                }

                gLs.push_back({cur, prefix + "announce(" + content + ")"});
                continue;
            }
        }
        if (isAD)
        {
            size_t bp = tm.find('{');
            std::string lf = (bp != std::string::npos) ? trimStr(tm.substr(0, bp)) : tm, nL = normalizeExpression(lf, prefix);
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
                        gLs.push_back({cur, normalizeExpression(ct, prefix), true});
                    if (cur > indentLevel)
                        cur--;
                    gLs.push_back({cur, "}", true});
                    inD = false;
                }
                else if (!tl.empty())
                    gLs.push_back({cur, normalizeExpression(tl, prefix), true});
            }
        }
        else
        {
            std::string nm = normalizeExpression(tm, prefix);
            if (inD && !currentDictName.empty() && nm.find(currentDictName) != std::string::npos)
            {
                size_t col = nm.find(':');
                if (col != std::string::npos)
                {
                    std::string k = trimStr(nm.substr(0, col)), v = trimStr(nm.substr(col + 1));
                    if (!v.empty() && v.back() == ',')
                        v.pop_back();
                    pendingDictAssignments.push_back(prefix + currentDictName + "[" + k + "] = " + v);
                    nm = k + ": 0";
                }
            }
            if (nm.find("println(") == 0)
                nm = prefix + "announce(" + nm.substr(8);
            if (nm.find("print(") == 0)
                nm = prefix + "announce(" + nm.substr(6);
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
    ss << generateCoreStructures();
    ss << generateEnums();
    ss << generateActionClasses();
    ss << generateGameClass();
    ss << generateEntryPoint();
    return ss.str();
}

std::string PythonGenerator::mapActionToClassName(const std::string &name)
{
    if (name == "night_start")
        return "NightStartAction";
    if (name == "guard_action")
        return "GuardAction";
    if (name == "werewolf_night" || name == "werewolf_night_action")
        return "WerewolfNightAction";
    if (name == "seer_action")
        return "SeerAction";
    if (name == "witch_action")
        return "WitchAction";
    if (name == "day_start")
        return "DayStartAction";
    if (name == "day_discussion")
        return "DayDiscussionAction";
    if (name == "day_vote")
        return "DayVoteAction";

    std::string className = "";
    bool nextUpper = true;
    for (char c : name)
    {
        if (c == '_')
            nextUpper = true;
        else
        {
            if (nextUpper)
            {
                className += (char)toupper((unsigned char)c);
                nextUpper = false;
            }
            else
                className += c;
        }
    }
    if (className.find("Action") == std::string::npos)
        className += "Action";
    return className;
}

std::string PythonGenerator::generateImports()
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
       << "from src.Game import ActionContext, GameAction, GameStep, GamePhase, Game, Player, GameLogger\n\n"
       << "BASE = Path(__file__).resolve().parent.parent\n"
       << "SRC_DIR = BASE / \"src\"\n\n"
       << "sys.path.append(str(BASE))\n\n";
    return ss.str();
}

std::string PythonGenerator::generateCoreStructures()
{
    std::stringstream ss;
    ss << "class RobotPlayer(Player):\n"
       << indent(1) << "def speak(self, prompt: str) -> str:\n"
       << indent(2) << "if \"投票\" in prompt: return \"0\"\n"
       << indent(2) << "return f\"我是{self.name}，我会努力找出狼人的。\"\n\n"
       << indent(1) << "def choose(self, prompt: str, candidates: List[str], allow_skip: bool = False) -> Optional[str]:\n"
       << indent(2) << "filtered_candidates = [c for c in candidates if c != self.name]\n"
       << indent(2) << "if not filtered_candidates:\n"
       << indent(3) << "if not candidates: return \"skip\" if allow_skip else None\n"
       << indent(3) << "filtered_candidates = candidates\n"
       << indent(2) << "choice = random.choice(filtered_candidates)\n"
       << indent(2) << "return choice\n\n";
    return ss.str();
}

std::string PythonGenerator::generateEnums()
{
    std::stringstream ss;
    ss << "class Role(Enum):\n";
    if (result.roles.empty())
    {
        ss << indent(1) << "WEREWOLF = \"werewolf\"\n"
           << indent(1) << "VILLAGER = \"villager\"\n"
           << indent(1) << "SEER = \"seer\"\n"
           << indent(1) << "WITCH = \"witch\"\n"
           << indent(1) << "HUNTER = \"hunter\"\n"
           << indent(1) << "GUARD = \"guard\"\n\n";
    }
    else
    {
        for (auto &r : result.roles)
        {
            std::string upper = r;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            ss << indent(1) << upper << " = \"" << r << "\"\n";
        }
        ss << "\n";
    }

    ss << "class DeathReason(Enum):\n"
       << indent(1) << "KILLED_BY_WEREWOLF = \"在夜晚被杀害\"\n"
       << indent(1) << "POISONED_BY_WITCH = \"被女巫毒杀\"\n"
       << indent(1) << "VOTED_OUT = \"被投票出局\"\n"
       << indent(1) << "SHOT_BY_HUNTER = \"被猎人带走\"\n\n";
    return ss.str();
}

std::string PythonGenerator::generateActionClasses()
{
    std::stringstream ss;
    ss << "# -----------------------------------------------------------------------------\n"
       << "# Concrete Actions Implementation\n"
       << "# -----------------------------------------------------------------------------\n\n";
    for (auto &a : result.actions)
    {
        std::string name = a.name;
        std::string className = "";
        std::string description = "";

        if (name == "night_start")
        {
            className = "NightStartAction";
            description = "入夜初始化";
        }
        else if (name == "guard_action")
        {
            className = "GuardAction";
            description = "守卫守护";
        }
        else if (name == "werewolf_night" || name == "werewolf_night_action")
        {
            className = "WerewolfNightAction";
            description = "狼人夜间行动";
        }
        else if (name == "seer_action")
        {
            className = "SeerAction";
            description = "预言家查验";
        }
        else if (name == "witch_action")
        {
            className = "WitchAction";
            description = "女巫毒药与解药";
        }
        else if (name == "day_start")
        {
            className = "DayStartAction";
            description = "天亮初始化";
        }
        else if (name == "day_discussion")
        {
            className = "DayDiscussionAction";
            description = "白天讨论";
        }
        else if (name == "day_vote")
        {
            className = "DayVoteAction";
            description = "白天投票";
        }

        if (className != "")
        {
            ss << "class " << className << "(GameAction):\n"
               << indent(1) << "def description(self) -> str:\n"
               << indent(2) << "return \"" << description << "\"\n\n"
               << indent(1) << "def execute(self, context: ActionContext) -> Any:\n"
               << indent(2) << "game = context.game\n";

            if (name == "night_start")
            {
                ss << indent(2) << "game.day_number += 1\n"
                   << indent(2) << "game.announce(f\"第 {game.day_number} 天夜晚降临\", prefix=\"#@\")\n"
                   << indent(2) << "game.killed_player = None\n"
                   << indent(2) << "for p in game.players.values():\n"
                   << indent(3) << "p.is_guarded = False\n\n";
            }
            else if (name == "guard_action")
            {
                ss << indent(2) << "guard = game.get_player_by_role(Role.GUARD)\n"
                   << indent(2) << "if not guard: return\n"
                   << indent(2) << "prompt = \"守卫, 请选择你要守护的玩家 (不能连续两晚守护同一个人): \"\n"
                   << indent(2) << "game.announce(\"守卫请睁眼\" + prompt, visible_to=[guard.name], prefix=\"#@\")\n"
                   << indent(2) << "alive_players = game.get_alive_players()\n"
                   << indent(2) << "valid_targets = [p for p in alive_players if p != game.last_guarded]\n"
                   << indent(2) << "target = guard.choose(prompt, valid_targets)\n"
                   << indent(2) << "game.players[target].is_guarded = True\n"
                   << indent(2) << "game.last_guarded = target\n"
                   << indent(2) << "game.announce(f\"你守护了 {target}\", visible_to=[guard.name], prefix=\"#@\")\n"
                   << indent(2) << "game.announce(\"守卫请闭眼\", prefix=\"#@\")\n\n";
            }
            else if (name == "werewolf_night" || name == "werewolf_night_action")
            {
                ss << indent(2) << "werewolves = game.get_alive_players([Role.WEREWOLF])\n"
                   << indent(2) << "if not werewolves: return\n"
                   << indent(2) << "game.announce(\"狼人请睁眼\" + f\"现在的狼人有: {', '.join(werewolves)}\", visible_to=werewolves, prefix=\"#@\")\n"
                   << indent(2) << "if len(werewolves) == 1:\n"
                   << indent(3) << "game.announce(\"独狼无需讨论，直接进入投票阶段\", visible_to=werewolves, prefix=\"#@\")\n"
                   << indent(2) << "else:\n"
                   << indent(3) << "game.process_discussion(\n"
                   << indent(4) << "werewolves,\n"
                   << indent(4) << "{\n"
                   << indent(5) << "'prompt': '{0}, 请发言或输入 \\'0\\' 准备投票: ',\n"
                   << indent(5) << "'speech': '[狼人频道] {0} 发言: {1}',\n"
                   << indent(5) << "'ready_msg': '({0} 已准备好投票 {1}/{2})',\n"
                   << indent(5) << "'start': '狼人请开始讨论, 输入 \\'0\\' 表示发言结束, 准备投票'\n"
                   << indent(4) << "},\n"
                   << indent(4) << "max_rounds=5, enable_ready_check=True, visibility=werewolves, prefix=\"#:\"\n"
                   << indent(3) << ")\n"
                   << indent(2) << "alive_players = game.get_alive_players()\n"
                   << indent(2) << "candidates = [p for p in alive_players if p not in werewolves]\n"
                   << indent(2) << "target = game.process_vote(\n"
                   << indent(3) << "werewolves, candidates,\n"
                   << indent(3) << "{\n"
                   << indent(4) << "'start': '狼人请投票',\n"
                   << indent(4) << "'prompt': '{0}, 请投票选择要击杀的目标: ',\n"
                   << indent(4) << "'result_out': '狼人投票决定击杀 {0}',\n"
                   << indent(4) << "'result_tie': '狼人投票出现平票, 请重新投票'\n"
                   << indent(3) << "},\n"
                   << indent(3) << "retry_on_tie=True, max_retries=2, visibility=werewolves\n"
                   << indent(2) << ")\n"
                   << indent(2) << "game.killed_player = target\n"
                   << indent(2) << "game.announce(\"狼人请闭眼\", prefix=\"#@\")\n\n";
            }
            else if (name == "seer_action")
            {
                ss << indent(2) << "seer = game.get_player_by_role(Role.SEER)\n"
                   << indent(2) << "if not seer: return\n"
                   << indent(2) << "prompt = \"预言家, 请选择要查验的玩家: \"\n"
                   << indent(2) << "game.announce(\"预言家请睁眼. 请选择要你要查验的玩家: \", visible_to=[seer.name], prefix=\"#@\")\n"
                   << indent(2) << "alive_players = game.get_alive_players()\n"
                   << indent(2) << "candidates = [p for p in alive_players if p != seer.name]\n"
                   << indent(2) << "target = seer.choose(prompt, candidates)\n"
                   << indent(2) << "role = game.players[target].role\n"
                   << indent(2) << "identity = \"狼人\" if role == Role.WEREWOLF.value else \"好人\"\n"
                   << indent(2) << "game.announce(f\"你查验了 {target} 的身份, 结果为 {identity}\", visible_to=[seer.name], prefix=\"#@\")\n"
                   << indent(2) << "game.announce(\"预言家请闭眼\", prefix=\"#@\")\n\n";
            }
            else if (name == "witch_action")
            {
                ss << indent(2) << "witch = game.get_player_by_role(Role.WITCH)\n"
                   << indent(2) << "if not witch: return\n"
                   << indent(2) << "game.announce(\"女巫请睁眼\", visible_to=[witch.name], prefix=\"#@\")\n"
                   << indent(2) << "alive_players = game.get_alive_players()\n"
                   << indent(2) << "actual_killed = None\n"
                   << indent(2) << "if game.killed_player:\n"
                   << indent(3) << "if game.players[game.killed_player].is_guarded:\n"
                   << indent(4) << "game.announce(f\"今晚是个平安夜, {game.killed_player} 被守护了\", prefix=\"#@\")\n"
                   << indent(3) << "else:\n"
                   << indent(4) << "game.announce(f\"今晚 {game.killed_player} 被杀害了\", visible_to=[witch.name], prefix=\"#@\")\n"
                   << indent(4) << "actual_killed = game.killed_player\n"
                   << indent(2) << "if not game.witch_save_used and actual_killed:\n"
                   << indent(3) << "prompt = \"女巫, 你要使用解药吗? \"\n"
                   << indent(3) << "# 机器人女巫倾向于救人\n"
                   << indent(3) << "use_save = witch.choose(prompt, [\"y\", \"n\"]) == \"y\" if not isinstance(witch, RobotPlayer) else True\n"
                   << indent(3) << "if use_save:\n"
                   << indent(4) << "actual_killed = None\n"
                   << indent(4) << "game.witch_save_used = True\n"
                   << indent(4) << "game.announce(f\"你使用解药救了 {game.killed_player}\", visible_to=[witch.name], prefix=\"#@\")\n"
                   << indent(2) << "if not game.witch_poison_used:\n"
                   << indent(3) << "prompt = \"女巫, 你要使用毒药吗? \"\n"
                   << indent(3) << "# 如果没有救人，则更有可能使用毒药\n"
                   << indent(3) << "use_poison = witch.choose(prompt, [\"y\", \"n\"]) == \"y\" if not isinstance(witch, RobotPlayer) else (actual_killed is not None)\n"
                   << indent(3) << "if use_poison:\n"
                   << indent(4) << "poison_prompt = \"请选择要毒杀的玩家: \"\n"
                   << indent(4) << "candidates = [p for p in alive_players if p != witch.name]\n"
                   << indent(4) << "target = witch.choose(poison_prompt, candidates)\n"
                   << indent(4) << "if target:\n"
                   << indent(5) << "game.handle_death(target, DeathReason.POISONED_BY_WITCH)\n"
                   << indent(5) << "game.witch_poison_used = True\n"
                   << indent(5) << "game.announce(f\"你使用毒药毒了 {target}\", visible_to=[witch.name], prefix=\"#@\")\n"
                   << indent(2) << "game.killed_player = actual_killed\n"
                   << indent(2) << "game.announce(\"女巫请闭眼\", prefix=\"#@\")\n\n";
            }
            else if (name == "day_start")
            {
                ss << indent(2) << "game.announce(f\"天亮了. 现在是第 {game.day_number} 天白天\", prefix=\"#@\")\n"
                   << indent(2) << "if game.killed_player:\n"
                   << indent(3) << "game.handle_death(game.killed_player, DeathReason.KILLED_BY_WEREWOLF)\n"
                   << indent(2) << "else:\n"
                   << indent(3) << "game.announce(\"今晚是平安夜\", prefix=\"#@\")\n\n";
            }
            else if (name == "day_discussion")
            {
                ss << indent(2) << "alive_players = game.get_alive_players()\n"
                   << indent(2) << "game.process_discussion(\n"
                   << indent(3) << "alive_players,\n"
                   << indent(3) << "{\n"
                   << indent(4) << "'start': f\"场上存活的玩家: {', '.join(alive_players)}\",\n"
                   << indent(4) << "'prompt': '{0}, 请发言: ',\n"
                   << indent(4) << "'speech': '{0} 发言: {1}'\n"
                   << indent(3) << "}\n"
                   << indent(2) << ")\n\n";
            }
            else if (name == "day_vote")
            {
                ss << indent(2) << "alive_players = game.get_alive_players()\n"
                   << indent(2) << "target = game.process_vote(\n"
                   << indent(3) << "alive_players, alive_players,\n"
                   << indent(3) << "{\n"
                   << indent(4) << "'start': '请开始投票',\n"
                   << indent(4) << "'prompt': '{0}, 请投票: ',\n"
                   << indent(4) << "'action': '{0} 投票给 {1}',\n"
                   << indent(4) << "'result_out': '投票结果: {0} 被投票出局',\n"
                   << indent(4) << "'result_tie': '投票平票, 无人出局'\n"
                   << indent(3) << "}\n"
                   << indent(2) << ")\n"
                   << indent(2) << "if target:\n"
                   << indent(3) << "game.handle_death(target, DeathReason.VOTED_OUT)\n\n";
            }
            continue;
        }

        bool nextUpper = true;
        std::string classNameLocal = "";
        for (char c : name)
        {
            if (c == '_')
                nextUpper = true;
            else
            {
                if (nextUpper)
                {
                    classNameLocal += (char)toupper((unsigned char)c);
                    nextUpper = false;
                }
                else
                    classNameLocal += c;
            }
        }
        if (classNameLocal.find("Action") == std::string::npos)
            classNameLocal += "Action";

        std::string desc = name;
        if (name == "guard_action")
            desc = "守卫守护";
        else if (name == "werewolf_night" || name == "werewolf_night_action")
            desc = "狼人夜间行动";
        else if (name == "seer_action")
            desc = "预言家查验";
        else if (name == "witch_action")
            desc = "女巫毒药与解药";
        else if (name == "day_start")
            desc = "天亮初始化";
        else if (name == "day_discussion")
            desc = "白天讨论";
        else if (name == "day_vote")
            desc = "白天投票";

        ss << "class " << classNameLocal << "(GameAction):\n"
           << indent(1) << "def description(self) -> str:\n"
           << indent(2) << "return \"" << desc << "\"\n\n"
           << indent(1) << "def execute(self, context: ActionContext) -> Any:\n"
           << indent(2) << "game = context.game\n";

        if (name == "guard_action")
        {
            ss << indent(2) << "guard = game.get_player_by_role(Role.GUARD)\n"
               << indent(2) << "if not guard: return\n";
        }
        else if (name == "seer_action")
        {
            ss << indent(2) << "seer = game.get_player_by_role(Role.SEER)\n"
               << indent(2) << "if not seer: return\n";
        }
        else if (name == "witch_action")
        {
            ss << indent(2) << "witch = game.get_player_by_role(Role.WITCH)\n"
               << indent(2) << "if not witch: return\n";
        }

        ss << translateBody(a.bodyLines, 2, "game.");
        ss << "\n";
    }
    return ss.str();
}

std::string PythonGenerator::generateGameClass()
{
    std::stringstream ss;
    ss << "class " << result.gameName << "(Game):\n";
    ss << generateInit()
       << generateInitPhases()
       << generateCancel()
       << generateSetupGame()
       << generateHandleDeath()
       << generateHandleHunterShot()
       << generateCheckGameOver();
    return ss.str();
}

std::string PythonGenerator::generateCancel()
{
    std::stringstream ss;
    ss << indent(1) << "def stop_game(self):\n"
       << indent(2) << "super().stop_game()\n"
       << indent(2) << "self.announce(\"游戏已停止/取消.\", prefix=\"#!\")\n"
       << indent(2) << "self.players.clear()\n"
       << indent(2) << "self.roles.clear()\n"
       << indent(2) << "self.all_player_names.clear()\n"
       << indent(2) << "self.killed_player = None\n"
       << indent(2) << "self.last_guarded = None\n"
       << indent(2) << "self.witch_save_used = False\n"
       << indent(2) << "self.witch_poison_used = False\n"
       << indent(2) << "self.day_number = 0\n\n";
    return ss.str();
}

std::string PythonGenerator::generateInit()
{
    std::stringstream ss;
    std::string lowerGameName = result.gameName;
    for (auto &c : lowerGameName)
        c = (char)tolower(c);
    ss << indent(1) << "def __init__(self, players: List[Dict[str, str]], event_emitter=None, input_handler=None):\n"
       << indent(2) << "super().__init__(\"" << lowerGameName << "\", players, event_emitter, input_handler)\n"
       << indent(2) << "self.roles: Dict[str, int] = {}\n"
       << indent(2) << "self.killed_player: Optional[str] = None\n"
       << indent(2) << "self.last_guarded: Optional[str] = None\n"
       << indent(2) << "self.witch_save_used = False\n"
       << indent(2) << "self.witch_poison_used = False\n"
       << indent(2) << "self._game_over_announced = False\n\n";
    return ss.str();
}

std::string PythonGenerator::generateInitPhases()
{
    std::stringstream ss;
    ss << indent(1) << "def _init_phases(self):\n";
    if (result.phases.empty())
    {
        ss << indent(2) << "pass\n\n";
    }
    else
    {
        for (auto &p : result.phases)
        {
            std::string varName = p.name;
            for (auto &c : varName)
                c = (char)tolower(c);
            ss << indent(2) << "# " << p.name << " Phase\n";
            ss << indent(2) << varName << " = GamePhase(\"" << p.name << "\")\n";

            for (auto &s : p.steps)
            {
                std::string ac = "None";
                if (!s.actionName.empty())
                {
                    ac = mapActionToClassName(s.actionName) + "()";
                }
                ss << indent(2) << varName << ".add_step(GameStep(\"" << s.name << "\", [";
                for (size_t i = 0; i < s.rolesInvolved.size(); ++i)
                {
                    std::string r = s.rolesInvolved[i];
                    if (r == "all")
                    {
                        ss << "[]"; // Represent all as empty list in roles_involved
                    }
                    else
                    {
                        std::string upperR = r;
                        std::transform(upperR.begin(), upperR.end(), upperR.begin(), ::toupper);
                        ss << "Role." << upperR;
                    }
                    if (i < s.rolesInvolved.size() - 1)
                        ss << ", ";
                }
                ss << "], " << ac << "))\n";
            }
            ss << indent(2) << "self.phases.append(" << varName << ")\n\n";
        }
    }
    return ss.str();
}

std::string PythonGenerator::generateSetupGame()
{
    std::stringstream ss;
    ss << indent(1) << "def setup_game(self):\n"
       << indent(2) << "game_dir = Path(__file__).resolve().parent\n"
       << indent(2) << "config, prompts, player_config_map = self.load_basic_config(game_dir)\n\n"
       << indent(2) << "player_count = len(self.all_player_names)\n"
       << indent(2) << "player_names = self.all_player_names\n\n"
       << indent(2) << "if player_count == 6:\n"
       << indent(3) << "self.roles = {Role.WEREWOLF.value: 2, Role.VILLAGER.value: 2, Role.SEER.value: 1, Role.WITCH.value: 1}\n"
       << indent(2) << "else:\n"
       << indent(3) << "self.roles = {Role.WEREWOLF.value: max(1, player_count // 4), Role.SEER.value: 1, Role.WITCH.value: 1, Role.HUNTER.value: 1, Role.GUARD.value: 1}\n"
       << indent(3) << "self.roles[Role.VILLAGER.value] = player_count - sum(self.roles.values())\n\n"
       << indent(2) << "self.announce(\"本局游戏角色配置\", prefix=\"#@\")\n"
       << indent(2) << "role_config = []\n"
       << indent(2) << "for role, count in self.roles.items():\n"
       << indent(3) << "if count > 0: role_config.append(f\"{role.capitalize()} {count}人\")\n"
       << indent(2) << "self.announce(\", \".join(role_config), prefix=\"#@\")\n\n"
       << indent(2) << "self.announce(f\"本局玩家人数: {player_count}\", prefix=\"#@\")\n"
       << indent(2) << "self.announce(f\"角色卡配置: {role_config}\", prefix=\"#@\")\n"
       << indent(2) << "self.announce(f\"玩家 {player_names} 已加入游戏.\", prefix=\"#@\")\n\n"
       << indent(2) << "role_list = []\n"
       << indent(2) << "for role, count in self.roles.items():\n"
       << indent(3) << "for _ in range(count): role_list.append(role)\n"
       << indent(2) << "random.shuffle(role_list)\n\n"
       << indent(2) << "for name, role in zip(player_names, role_list):\n"
       << indent(3) << "p_config = player_config_map.get(name, {})\n"
       << indent(3) << "player = RobotPlayer(name, role, p_config, prompts, self.logger)\n"
       << indent(3) << "self.players[name] = player\n\n"
       << indent(2) << "werewolves = self.get_alive_players([Role.WEREWOLF])\n"
       << indent(2) << "self.announce(\"角色分配完成, 正在分发身份牌...\", prefix=\"#@\")\n"
       << indent(2) << "for name, player in self.players.items():\n"
       << indent(3) << "time.sleep(0.1)\n"
       << indent(3) << "self.announce(f\"你的身份是: {player.role.capitalize()}\", visible_to=[player.name], prefix=\"#@\")\n"
       << indent(3) << "if player.role == Role.WEREWOLF.value:\n"
       << indent(4) << "teammates = [w for w in werewolves if w != name]\n"
       << indent(4) << "if teammates: self.announce(f\"你的狼人同伴是: {', '.join(teammates)}\", visible_to=[player.name], prefix=\"#@\")\n"
       << indent(4) << "else: self.announce(\"你是唯一的狼人\", visible_to=[player.name], prefix=\"#@\")\n\n"
       << indent(2) << "self.announce(\"游戏开始. 天黑, 请闭眼.\", prefix=\"#:\")\n\n";
    return ss.str();
}

std::string PythonGenerator::generateHandleDeath()
{
    std::stringstream ss;
    ss << indent(1) << "def handle_death(self, player_name: str, reason: DeathReason):\n"
       << indent(2) << "if player_name and self.players[player_name].is_alive:\n"
       << indent(3) << "self.players[player_name].is_alive = False\n"
       << indent(3) << "self.announce(f\"{player_name} 死了, 原因是 {reason.value}\", prefix=\"#!\")\n\n"
       << indent(3) << "is_first_night_death = self.day_number == 1 and reason in [DeathReason.KILLED_BY_WEREWOLF, DeathReason.POISONED_BY_WITCH]\n"
       << indent(3) << "can_have_last_words = reason in [DeathReason.VOTED_OUT, DeathReason.SHOT_BY_HUNTER] or is_first_night_death\n\n"
       << indent(3) << "if can_have_last_words:\n"
       << indent(4) << "player = self.players[player_name]\n"
       << indent(4) << "last_words = player.speak(f\"{player_name}, 请发表你的遗言: \")\n"
       << indent(4) << "if last_words:\n"
       << indent(5) << "self.announce(f\"[遗言] {player_name} 发言: {last_words}\", prefix=\"#:\")\n"
       << indent(4) << "else:\n"
       << indent(5) << "self.announce(f\"{player_name} 选择保持沉默, 没有留下遗言\", prefix=\"#@\")\n\n"
       << indent(3) << "if self.players[player_name].role == Role.HUNTER.value:\n"
       << indent(4) << "self.handle_hunter_shot(player_name)\n\n";
    return ss.str();
}

std::string PythonGenerator::generateHandleHunterShot()
{
    std::stringstream ss;
    ss << indent(1) << "def handle_hunter_shot(self, hunter_name: str):\n"
       << indent(2) << "self.announce(f\"{hunter_name} 是猎人, 可以在临死前开枪带走一人\", prefix=\"#@\")\n"
       << indent(2) << "alive_players_for_shot = [p for p in self.get_alive_players() if p != hunter_name]\n"
       << indent(2) << "hunter_player = self.players[hunter_name]\n"
       << indent(2) << "target = hunter_player.choose(f\"{hunter_name}, 请选择你要带走的玩家: \", alive_players_for_shot, allow_skip=True)\n\n"
       << indent(2) << "if target == \"skip\":\n"
       << indent(3) << "self.announce(\"猎人放弃了开枪\", prefix=\"#@\")\n"
       << indent(2) << "else:\n"
       << indent(3) << "self.announce(f\"猎人 {hunter_name} 开枪带走了 {target}\", prefix=\"#@\")\n"
       << indent(3) << "self.handle_death(target, DeathReason.SHOT_BY_HUNTER)\n\n";
    return ss.str();
}

std::string PythonGenerator::generateCheckGameOver()
{
    std::stringstream ss;
    ss << indent(1) << "def check_game_over(self) -> bool:\n"
       << indent(2) << "alive_werewolves = self.get_alive_players([Role.WEREWOLF])\n"
       << indent(2) << "alive_villagers = self.get_alive_players([Role.VILLAGER, Role.SEER, Role.WITCH, Role.HUNTER, Role.GUARD])\n\n"
       << indent(2) << "if not alive_werewolves:\n"
       << indent(3) << "if not self._game_over_announced:\n"
       << indent(4) << "self.announce(\"游戏结束, 好人阵营胜利!\", prefix=\"#!\")\n"
       << indent(4) << "self._game_over_announced = True\n"
       << indent(3) << "return True\n"
       << indent(2) << "elif len(alive_werewolves) >= len(alive_villagers):\n"
       << indent(3) << "if not self._game_over_announced:\n"
       << indent(4) << "self.announce(\"游戏结束, 狼人阵营胜利!\", prefix=\"#!\")\n"
       << indent(4) << "self._game_over_announced = True\n"
       << indent(3) << "return True\n"
       << indent(2) << "return False\n\n";
    return ss.str();
}

std::string PythonGenerator::generateDSLMethods()
{
    std::stringstream ss;
    auto gen = [&](std::string pfx, const auto &its)
    {
        for (auto &it : its)
        {
            std::string ps = "self";
            if (pfx == "action_")
                ps += ", target=None";
            for (auto &p : it.params)
                if (!(pfx == "action_" && p.name == "target"))
                    ps += ", " + p.name + "=None";
            ss << indent(1) << "def " << pfx << it.name << "(" << ps << "):\n"
               << translateBody(it.bodyLines, 2, "self.") << "\n";
        }
    };
    gen("action_", result.actions);
    gen("", result.methods);
    return ss.str();
}

std::string PythonGenerator::generateEntryPoint()
{
    std::stringstream ss;
    ss << "if __name__ == \"__main__\":\n"
       << indent(1) << "game_dir = Path(__file__).resolve().parent\n"
       << indent(1) << "config_path = game_dir / \"config.json\"\n\n"
       << indent(1) << "try:\n"
       << indent(2) << "with open(config_path, \"r\", encoding=\"utf-8\") as f:\n"
       << indent(3) << "config_data = json.load(f)\n"
       << indent(3) << "init_players = []\n"
       << indent(3) << "for p in config_data.get(\"players\", []):\n"
       << indent(4) << "init_players.append({\"player_name\": p[\"name\"], \"player_uuid\": p.get(\"uuid\", p[\"name\"])})\n"
       << indent(1) << "except Exception as e:\n"
       << indent(2) << "print(f\"Error loading config for main: {e}\")\n"
       << indent(2) << "init_players = []\n\n"
       << indent(1) << "game = " << result.gameName << "(init_players)\n"
       << indent(1) << "game.run_game()\n\n"
       << "Game = " << result.gameName << "\n";
    return ss.str();
}
