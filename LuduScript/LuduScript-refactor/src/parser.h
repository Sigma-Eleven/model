#pragma once

#include "lexer.h"
#include "ast.h"
#include <vector>
#include <memory>

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<GameDecl> parse();

private:
    std::vector<Token> tokens;
    int current = 0;

    Token peek() const;
    Token previous() const;
    bool isAtEnd() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    void error(const Token& token, const std::string& message);

    // Declaration Parsing
    std::unique_ptr<GameDecl> parseGame();
    std::unique_ptr<ConfigDecl> parseConfig();
    std::unique_ptr<RoleDecl> parseRole();
    std::unique_ptr<VarDecl> parseVar();
    std::unique_ptr<ActionDecl> parseAction();
    std::unique_ptr<PhaseDecl> parsePhase();
    std::unique_ptr<StepDecl> parseStep();

    // Statement Parsing
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<BlockStmt> parseBlock();
    std::unique_ptr<LetStmt> parseLet();
    std::unique_ptr<IfStmt> parseIf();
    std::unique_ptr<Statement> parseFor();
    std::unique_ptr<ReturnStmt> parseReturn();
    std::unique_ptr<Statement> parseExpressionStatement();

    // Expression Parsing
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parseEquality();
    std::unique_ptr<Expression> parseComparison();
    std::unique_ptr<Expression> parseTerm();
    std::unique_ptr<Expression> parseFactor();
    std::unique_ptr<Expression> parseUnary();
    std::unique_ptr<Expression> parsePrimary();
};
