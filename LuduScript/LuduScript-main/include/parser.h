#pragma once

#include "lexer.h"
#include <string>
#include <vector>
#include <map>

struct WolfParseResult
{
    std::string gameName;
    std::vector<std::string> roles;

    // 参数结构
    struct Param
    {
        std::string name;
    };

    // 动作定义结构
    struct ActionDef
    {
        std::string name;
        std::vector<Param> params;
        std::vector<std::string> bodyLines; 
        int line;
    };

    // 变量定义
    struct VariableDef
    {
        std::string name;
        std::string type_keyword; 
        std::string value;       
        int line;
    };

    // 阶段定义结构
    struct PhaseDef
    {
        struct StepDef
        {
            std::string name;
            std::vector<std::string> rolesInvolved;
            std::string actionName;
            std::string condition;              
            std::vector<std::string> bodyLines; 
            int line;
        };

        std::string name;
        std::vector<StepDef> steps;
        int line;
    };

    // 方法定义
    struct MethodDef
    {
        std::string name;
        std::vector<Param> params;
        std::vector<std::string> bodyLines; 
        int line;
    };

    // setup定义
    struct SetupDef
    {
        std::vector<std::string> bodyLines; 
        int line;
    };

    // 存储所有定义
    std::vector<ActionDef> actions;
    std::vector<PhaseDef> phases;
    std::map<std::string, VariableDef> variables;
    std::vector<MethodDef> methods;
    SetupDef setup;

    // 错误信息
    bool hasError = false;
    std::string errorMessage;

    WolfParseResult() = default;
};

class WolfParser
{
private:
    Lexer lexer;
    Token current;
    WolfParseResult result;

    enum class ParseContext
    {
        TOP_LEVEL,
        IN_GAME,
        IN_ACTION,
        IN_PHASE,
        IN_STEP,
        IN_SETUP
    };

    ParseContext currentContext = ParseContext::TOP_LEVEL;
    bool inGameBlock = false;

    Token peek() { return current; }
    Token consume();
    bool match(TokenKind kind);
    bool matchIdent(const std::string &ident);
    bool isKeyword(const std::string &text);
    void expect(TokenKind kind, const std::string &msg);
    void expectIdent(const std::string &ident, const std::string &msg);
    void error(const std::string &msg);

    // 顶层解析方法
    void parseTopLevel();
    void parseGameDefinition();

    // game内部的解析方法
    void parseInGameBlock();
    void parseEnumDefinition();
    void parseActionDefinition();
    void parsePhaseDefinition();
    void parseStepDefinition();
    void parseVariableDefinition();
    void parseMethodDefinition();
    void parseSetupDefinition();
    void parseIfStatement();
    void parseForStatement();
    void parseExpressionStatement();

    // 解析方法
    std::vector<WolfParseResult::Param> parseParamList();
    std::string parseType(); 
    std::string parseExpression();
    std::vector<std::string> parseCodeBlock();
    std::vector<std::string> parseStatementList();

    // 上下文检查
    void checkInGameContext(const std::string &statementType);
    void checkNotInTopLevel(const std::string &statementType);

public:
    explicit WolfParser(std::string src);
    WolfParseResult parse();
};