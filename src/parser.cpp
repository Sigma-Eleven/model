#include "parser.h"
#include <iostream>
#include <sstream>
#include <algorithm>

WolfParser::WolfParser(std::string src) : lexer(std::move(src))
{
    current = lexer.getNextToken();
}

Token WolfParser::consume()
{
    Token t = current;
    current = lexer.getNextToken();
    return t;
}

bool WolfParser::match(TokenKind kind)
{
    if (current.kind == kind)
    {
        consume();
        return true;
    }
    return false;
}

bool WolfParser::matchIdent(const std::string &ident)
{
    if (current.kind == TokenKind::IDENT && current.text == ident)
    {
        consume();
        return true;
    }
    return false;
}

void WolfParser::expect(TokenKind kind, const std::string &msg)
{
    if (current.kind != kind)
    {
        error(msg);
    }
    consume();
}

void WolfParser::expectIdent(const std::string &ident, const std::string &msg)
{
    if (current.kind != TokenKind::IDENT || current.text != ident)
    {
        error(msg);
    }
    consume();
}

void WolfParser::error(const std::string &msg)
{
    std::cout << "Parse error (line " << current.line << "): "
              << msg << " but got '" << current.text << "'" << std::endl;
    result.hasError = true;
    result.errorMessage = msg;
}

void WolfParser::checkInGameContext(const std::string &statementType)
{
    if (!inGameBlock)
    {
        error(statementType + " must be inside a game definition");
    }
}

void WolfParser::checkNotInTopLevel(const std::string &statementType)
{
    if (currentContext == ParseContext::TOP_LEVEL)
    {
        error(statementType + " cannot be at top level");
    }
}

WolfParseResult WolfParser::parse()
{
    result = WolfParseResult();

    try
    {
        parseTopLevel();
    }
    catch (const std::exception &e)
    {
        result.hasError = true;
        result.errorMessage = e.what();
    }

    return result;
}

void WolfParser::parseTopLevel()
{
    while (current.kind != TokenKind::END)
    {
        if (current.kind == TokenKind::IDENT && current.text == "game")
        {
            parseGameDefinition();
        }
        else if (current.kind == TokenKind::IDENT)
        {
            error("Only 'game' definition is allowed at top level");
            consume();
        }
        else
        {
            consume();
        }
    }
}

void WolfParser::parseGameDefinition()
{
    if (inGameBlock)
    {
        error("Nested game definitions are not allowed");
        return;
    }

    expectIdent("game", "Expected 'game'");

    if (current.kind != TokenKind::IDENT)
    {
        error("Expected game name");
        return;
    }

    result.gameName = current.text;
    consume();

    expect(TokenKind::LBRACE, "Expected '{' after game name");

    inGameBlock = true;
    currentContext = ParseContext::IN_GAME;

    while (current.kind != TokenKind::RBRACE && current.kind != TokenKind::END)
    {
        parseInGameBlock();
    }

    expect(TokenKind::RBRACE, "Expected '}' to close game definition");
    inGameBlock = false;
    currentContext = ParseContext::TOP_LEVEL;
}

void WolfParser::parseInGameBlock()
{
    Token before = current;

    if (current.kind == TokenKind::IDENT)
    {
        std::string keyword = current.text;

        if (keyword == "enum")
        {
            parseEnumDefinition();
        }
        else if (keyword == "action")
        {
            parseActionDefinition();
        }
        else if (keyword == "phase")
        {
            parsePhaseDefinition();
        }
        else if (keyword == "def")
        {
            parseMethodDefinition();
        }
        else if (keyword == "setup")
        {
            parseSetupDefinition();
        }
        else if (keyword == "num" || keyword == "str" || keyword == "bool" || keyword == "obj")
        {
            parseVariableDefinition();
        }
        else if (keyword == "if")
        {
            parseIfStatement();
        }
        else if (keyword == "for")
        {
            parseForStatement();
        }
        else
        {
            parseExpressionStatement();
        }
    }
    else if (current.kind == TokenKind::LBRACE)
    {
        consume();
        parseStatementList();
        expect(TokenKind::RBRACE, "Expected '}' to close block");
    }
    else
    {
        parseExpressionStatement();
    }

    // 强制消耗一个Token以防死循环
    if (current.kind == before.kind && current.line == before.line && current.text == before.text && current.kind != TokenKind::END)
    {
        consume();
    }
}

void WolfParser::parseEnumDefinition()
{
    checkInGameContext("enum");

    expectIdent("enum", "Expected 'enum'");

    expect(TokenKind::LBRACE, "Expected '{' after enum");

    while (current.kind != TokenKind::RBRACE && current.kind != TokenKind::END)
    {
        if (current.kind == TokenKind::IDENT)
        {
            std::string role = current.text;
            result.roles.push_back(role);
            consume();

            if (match(TokenKind::COMMA))
            {
                continue;
            }
        }
        else
        {
            consume();
        }
    }

    expect(TokenKind::RBRACE, "Expected '}' to close enum");
}

void WolfParser::parseActionDefinition()
{
    checkInGameContext("action");

    WolfParseResult::ActionDef action;
    action.line = current.line;

    expectIdent("action", "Expected 'action'");

    if (current.kind != TokenKind::IDENT)
    {
        error("Expected action name");
        return;
    }

    action.name = current.text;
    consume();

    expect(TokenKind::LPAREN, "Expected '(' after action name");

    action.params = parseParamList();

    expect(TokenKind::RPAREN, "Expected ')' after parameters");

    expect(TokenKind::LBRACE, "Expected '{' after action parameters");

    action.bodyLines = parseCodeBlock();

    expect(TokenKind::RBRACE, "Expected '}' after action body");

    result.actions.push_back(action);
}

std::vector<WolfParseResult::Param> WolfParser::parseParamList()
{
    std::vector<WolfParseResult::Param> params;

    if (current.kind != TokenKind::RPAREN)
    {
        while (true)
        {
            if (current.kind == TokenKind::IDENT)
            {
                WolfParseResult::Param param;
                param.name = current.text;
                consume();

                params.push_back(param);

                if (!match(TokenKind::COMMA))
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }

    return params;
}

void WolfParser::parsePhaseDefinition()
{
    checkInGameContext("phase");

    WolfParseResult::PhaseDef phase;
    phase.line = current.line;

    expectIdent("phase", "Expected 'phase'");

    if (current.kind != TokenKind::IDENT)
    {
        error("Expected phase name");
        return;
    }

    phase.name = current.text;
    consume();

    expect(TokenKind::LBRACE, "Expected '{' after phase name");

    result.phases.push_back(phase);

    currentContext = ParseContext::IN_PHASE;

    while (current.kind != TokenKind::RBRACE && current.kind != TokenKind::END)
    {
        if (current.kind == TokenKind::IDENT && current.text == "step")
        {
            parseStepDefinition();
        }
        else if (current.kind == TokenKind::IDENT)
        {
            std::string type = current.text;
            if (type == "num" || type == "str" || type == "bool" || type == "obj")
            {
                parseVariableDefinition();
            }
            else
            {
                parseExpressionStatement();
            }
        }
        else
        {
            parseExpressionStatement();
        }
    }

    currentContext = ParseContext::IN_GAME;

    expect(TokenKind::RBRACE, "Expected '}' to close phase");
}

void WolfParser::parseStepDefinition()
{
    if (currentContext != ParseContext::IN_PHASE)
    {
        error("step must be inside a phase");
        return;
    }

    WolfParseResult::PhaseDef::StepDef step;
    step.line = current.line;

    expectIdent("step", "Expected 'step'");

    if (current.kind != TokenKind::STRING)
    {
        error("Expected step name string");
        return;
    }
    step.name = current.text;
    consume();

    if (matchIdent("for"))
    {
        while (current.kind == TokenKind::IDENT)
        {
            // 如果遇到 with, if 或 {，说明角色列表结束了
            if (current.text == "with" || current.text == "if")
            {
                break;
            }

            step.rolesInvolved.push_back(current.text);
            consume();

            if (!match(TokenKind::COMMA))
            {
                break;
            }
        }
    }

    if (matchIdent("with"))
    {
        if (current.kind != TokenKind::IDENT)
        {
            error("Expected action name after 'with'");
            return;
        }
        step.actionName = current.text;
        consume();
    }

    if (matchIdent("if"))
    {
        expect(TokenKind::LPAREN, "Expected '(' after 'if'");
        step.condition = parseExpression();
        expect(TokenKind::RPAREN, "Expected ')' after condition");
    }

    expect(TokenKind::LBRACE, "Expected '{' after step definition");

    step.bodyLines = parseStatementList();

    expect(TokenKind::RBRACE, "Expected '}' to close step");

    if (!result.phases.empty())
    {
        result.phases.back().steps.push_back(step);
    }
}

void WolfParser::parseVariableDefinition()
{
    checkNotInTopLevel("Variable definition");

    WolfParseResult::VariableDef var;
    var.line = current.line;
    var.type_keyword = current.text;
    consume();

    // 支持 num(var) 和 num var 两种写法
    if (match(TokenKind::LPAREN))
    {
        if (current.kind != TokenKind::IDENT)
        {
            error("Expected variable name");
            return;
        }

        var.name = current.text;
        consume();

        expect(TokenKind::RPAREN, "Expected ')' after variable name");
    }
    else
    {
        if (current.kind != TokenKind::IDENT)
        {
            error("Expected variable name");
            return;
        }

        var.name = current.text;
        consume();
    }

    if (match(TokenKind::ASSIGN))
    {
        var.value = parseExpression();
    }

    if (current.kind == TokenKind::SEMI)
    {
        consume();
    }

    result.variables[var.name] = var;
}

void WolfParser::parseMethodDefinition()
{
    checkInGameContext("method definition");

    WolfParseResult::MethodDef method;
    method.line = current.line;

    expectIdent("def", "Expected 'def'");

    if (current.kind != TokenKind::IDENT)
    {
        error("Expected method name");
        return;
    }

    method.name = current.text;
    consume();

    expect(TokenKind::LPAREN, "Expected '(' after method name");

    method.params = parseParamList();

    expect(TokenKind::RPAREN, "Expected ')' after parameters");

    expect(TokenKind::LBRACE, "Expected '{' after method parameters");

    method.bodyLines = parseCodeBlock();

    expect(TokenKind::RBRACE, "Expected '}' after method body");

    result.methods.push_back(method);
}

void WolfParser::parseSetupDefinition()
{
    checkInGameContext("setup");

    WolfParseResult::SetupDef setup;
    setup.line = current.line;

    expectIdent("setup", "Expected 'setup'");
    expect(TokenKind::LBRACE, "Expected '{' after setup");

    setup.bodyLines = parseCodeBlock();

    expect(TokenKind::RBRACE, "Expected '}' after setup body");

    result.setup = setup;
}

void WolfParser::parseIfStatement()
{
    checkNotInTopLevel("if statement");
    expectIdent("if", "Expected 'if'");
    expect(TokenKind::LPAREN, "Expected '(' after 'if'");

    parseExpression();

    expect(TokenKind::RPAREN, "Expected ')' after condition");
    expect(TokenKind::LBRACE, "Expected '{' for if body");

    parseStatementList();

    expect(TokenKind::RBRACE, "Expected '}' to close if body");

    while (current.kind == TokenKind::IDENT && current.text == "elif")
    {
        consume();
        expect(TokenKind::LPAREN, "Expected '(' after 'elif'");

        parseExpression();
        expect(TokenKind::RPAREN, "Expected ')' after elif condition");
        expect(TokenKind::LBRACE, "Expected '{' for elif body");

        parseStatementList();
        expect(TokenKind::RBRACE, "Expected '}' to close elif body");
    }

    if (matchIdent("else"))
    {
        expect(TokenKind::LBRACE, "Expected '{' for else body");

        parseStatementList();
        expect(TokenKind::RBRACE, "Expected '}' to close else body");
    }
}

void WolfParser::parseForStatement()
{
    checkNotInTopLevel("for statement");

    expectIdent("for", "Expected 'for'");
    expect(TokenKind::LPAREN, "Expected '(' after 'for'");

    if (current.kind != TokenKind::IDENT)
    {
        error("Expected iterator variable name");
        return;
    }

    std::string iterator = current.text;
    consume();

    expect(TokenKind::COMMA, "Expected ',' after iterator variable");

    parseExpression();

    expect(TokenKind::RPAREN, "Expected ')' after for arguments");
    expect(TokenKind::LBRACE, "Expected '{' for for body");

    parseStatementList();

    expect(TokenKind::RBRACE, "Expected '}' to close for body");
}

void WolfParser::parseExpressionStatement()
{
    checkNotInTopLevel("Expression statement");

    parseExpression();

    if (current.kind == TokenKind::SEMI)
    {
        consume();
    }
}

bool WolfParser::isKeyword(const std::string &text)
{
    // 只有那些代表“新块开始”的关键字才应该强制截断表达式
    // 像 for, if, return 等可能出现在表达式或语句中间的关键字不应在此列
    static const std::vector<std::string> breakKeywords = {
        "game", "enum", "action", "phase", "step", "def", "setup",
        "num", "str", "bool", "obj"};
    return std::find(breakKeywords.begin(), breakKeywords.end(), text) != breakKeywords.end();
}

std::string WolfParser::parseExpression()
{
    std::stringstream expr;

    while (current.kind != TokenKind::SEMI &&
           current.kind != TokenKind::RPAREN &&
           current.kind != TokenKind::RBRACE &&
           current.kind != TokenKind::COMMA &&
           current.kind != TokenKind::END)
    {
        // 如果遇到下一个关键字且不是在括号内，则停止解析表达式
        if (current.kind == TokenKind::IDENT && isKeyword(current.text))
        {
            break;
        }

        expr << current.text << " ";
        consume();
    }

    return expr.str();
}

std::string WolfParser::parseType()
{
    if (current.kind == TokenKind::IDENT)
    {
        std::string type = current.text;
        consume();
        return type;
    }
    error("Expected type name");
    return "";
}

std::vector<std::string> WolfParser::parseCodeBlock()
{
    return parseStatementList();
}

std::vector<std::string> WolfParser::parseStatementList()
{
    std::vector<std::string> lines;

    while (current.kind != TokenKind::RBRACE && current.kind != TokenKind::END)
    {
        if (current.kind == TokenKind::SEMI)
        {
            consume();
            continue;
        }

        std::stringstream line;

        while (current.kind != TokenKind::SEMI &&
               current.kind != TokenKind::RBRACE &&
               current.kind != TokenKind::END)
        {
            // 如果遇到下一个定义的关键字，停止当前行解析
            if (current.kind == TokenKind::IDENT && isKeyword(current.text))
            {
                // 排除掉 if/for 等可以在语句块内部出现的关键字
                if (current.text != "if" && current.text != "for" && current.text != "return")
                {
                    break;
                }
            }

            line << current.text << " ";
            consume();
        }

        std::string lineStr = line.str();
        if (!lineStr.empty())
        {
            lines.push_back(lineStr);
        }

        if (current.kind == TokenKind::SEMI)
        {
            consume();
        }
    }

    return lines;
}