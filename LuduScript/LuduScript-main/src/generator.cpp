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
                    if (!v.empty())
                        varNames.insert(v);
                    break;
                }
            }
        }
    };

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
            if (i + 1 < out.size() && (out[i + 1] == '(' || out[i + 1] == ')' || out[i + 1] == ',' || out[i + 1] == ']' || out[i + 1] == ']' || out[i + 1] == '[' || out[i + 1] == '.'))
                continue;
            if (i > 0 && (out[i - 1] == '(' || out[i - 1] == '[' || out[i - 1] == '.'))
                continue;
        }
        fin += out[i];
    }
    out = fin;
    auto fxc = [&](std::string s, std::string r)
    { size_t p=0; while((p=out.find(s,p))!=std::string::npos) out.replace(p,s.size(),r), p+=r.size(); };
    fxc("_cancel(", "stop_game(");
    fxc("f \"", "f\"");
    fxc("f \'", "f\'");

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
    ss << generateBaseStructures();
    ss << generateEnums();
    ss << generateActionClasses();
    ss << generateGameClass();
    ss << generateEntryPoint();
    return ss.str();
}

std::string PythonGenerator::mapActionToClassName(const std::string &name)
{
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
    return "";
}

std::string PythonGenerator::generateCoreStructures()
{
    return "";
}

std::string PythonGenerator::generateBaseStructures()
{
    return "";
}

std::string PythonGenerator::generateEnums()
{
    return "";
}

std::string PythonGenerator::generateActionClasses()
{
    return "";
}

std::string PythonGenerator::generateActionBody(const std::string &actionName)
{
    return "";
}
std::string PythonGenerator::generateCancel() { return ""; }

std::string PythonGenerator::generateGameClass()
{
    std::stringstream ss;
    std::string className = result.gameName;
    if (className.empty())
        className = "WolfGame";
    ss << "class " << className << "(Game):\n";
    ss << generateInit();
    ss << generateInitPhases();
    ss << generateSetupGame();
    ss << generateHandleDeath();
    ss << generateHandleHunterShot();
    ss << generateCheckGameOver();
    ss << generateDSLMethods();
    return ss.str();
}

std::string PythonGenerator::generateInit()
{
    std::stringstream ss;
    ss << indent(1) << "def __init__(self, players_data: List[Dict[str, str]], event_emitter=None, input_handler=None):\n"
       << indent(2) << "super().__init__(\"" << result.gameName << "\", players_data, event_emitter, input_handler)\n";
    for (auto &v : result.variables)
    {
        ss << indent(2) << "self." << v.first << " = " << toPythonLiteral(v.second.value) << "\n";
    }
    ss << "\n";
    return ss.str();
}

std::string PythonGenerator::generateInitPhases()
{
    std::stringstream ss;
    ss << indent(1) << "def _init_phases(self):\n";
    if (result.phases.empty())
    {
        ss << indent(2) << "pass\n";
    }
    else
    {
        for (auto &p : result.phases)
        {
            ss << indent(2) << "phase = GamePhase(\"" << p.name << "\")\n";
            for (auto &s : p.steps)
            {
                ss << indent(2) << "phase.add_step(GameStep(\"" << s.name << "\", " << mapActionToClassName(s.actionName) << "()))\n";
            }
            ss << indent(2) << "self.phases.append(phase)\n";
        }
    }
    ss << "\n";
    return ss.str();
}

std::string PythonGenerator::generateSetupGame()
{
    std::stringstream ss;
    ss << indent(1) << "def setup_game(self):\n";
    ss << translateBody(result.setup.bodyLines, 2);
    ss << "\n";
    return ss.str();
}

std::string PythonGenerator::generateHandleDeath() { return ""; }
std::string PythonGenerator::generateHandleHunterShot() { return ""; }
std::string PythonGenerator::generateCheckGameOver()
{
    std::stringstream ss;
    ss << indent(1) << "def check_game_over(self) -> bool:\n"
       << indent(2) << "return False\n\n";
    return ss.str();
}

std::string PythonGenerator::generateDSLMethods()
{
    std::stringstream ss;
    for (auto &m : result.methods)
    {
        ss << indent(1) << "def " << m.name << "(self";
        for (auto &arg : m.params)
            ss << ", " << arg.name;
        ss << "):\n";
        ss << translateBody(m.bodyLines, 2);
        ss << "\n";
    }
    return ss.str();
}

std::string PythonGenerator::generateEntryPoint()
{
    std::stringstream ss;
    std::string className = result.gameName;
    if (className.empty())
        className = "WolfGame";
    ss << "if __name__ == \"__main__\":\n"
       << indent(1) << "players_data = [\n"
       << indent(2) << "{\"name\": f\"Player {i}\", \"type\": \"robot\"} for i in range(1, 10)\n"
       << indent(1) << "]\n"
       << indent(1) << "game = " << className << "(players_data)\n"
       << indent(1) << "game.run_game()\n";
    return ss.str();
}
