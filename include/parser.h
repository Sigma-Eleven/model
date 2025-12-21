#pragma once

#include "lexer.h"
#include <string>
#include <vector>
#include <map>

// Wolf语言解析结果（纯结构）
struct WolfParseResult
{
    std::string gameName;
    std::vector<std::string> roles;

    // 参数结构
    struct Param
    {
        std::string name;
        std::string type; // 类型名（如果有）
    };

    // 动作定义结构
    struct ActionDef
    {
        std::string name;
        std::vector<Param> params;
        std::vector<std::string> bodyLines; // 原始DSL代码
        int line;
    };

    // 变量定义
    struct VariableDef
    {
        std::string name;
        std::string type_keyword; // "num", "str", "bool"
        std::string value;        // 原始表达式
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
            std::string condition;              // 原始条件表达式
            std::vector<std::string> bodyLines; // 步骤体
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
        std::vector<std::string> bodyLines; // 原始代码
        int line;
    };

    // setup定义
    struct SetupDef
    {
        std::vector<std::string> bodyLines; // 原始代码
        int line;
    };

    // 存储所有定义
    std::vector<ActionDef> actions;
    std::vector<PhaseDef> phases;
    std::map<std::string, VariableDef> variables;
    std::vector<MethodDef> methods;
    SetupDef setup;

    // 错误标志
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

    // 解析上下文
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

    // 基础工具方法
    Token peek() { return current; }
    Token consume();
    bool match(TokenKind kind);
    bool matchIdent(const std::string &ident);
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

    // 辅助解析方法
    std::vector<WolfParseResult::Param> parseParamList();
    std::string parseType(); // 简化版类型解析
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