#pragma once

#include <string>
#include <vector>
#include <iostream>

enum class TokenType
{
    KW_GAME,
    KW_CONFIG,
    KW_ROLE,
    KW_VAR,
    KW_ACTION,
    KW_PHASE,
    KW_STEP,
    KW_EXECUTE,
    KW_SETUP,
    KW_LET,
    KW_IF,
    KW_ELSE,
    KW_FOR,
    KW_IN,
    KW_RETURN,
    KW_TRUE,
    KW_FALSE,
    KW_NULL,
    KW_AND,
    KW_OR,
    KW_NOT,

    IDENTIFIER,
    STRING,
    NUMBER,

    LBRACE,
    RBRACE,
    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    COLON,
    COMMA,
    DOT,
    ASSIGN,
    EQ,
    NEQ,
    LT,
    GT,
    LE,
    GE,
    PLUS,
    MINUS,
    STAR,
    SLASH,

    EOF_TOKEN,
    UNKNOWN
};

struct Token
{
    TokenType type;
    std::string text;
    int line;
    int column;

    std::string toString() const
    {
        return "Token(" + std::to_string(static_cast<int>(type)) + ", '" + text + "', " + std::to_string(line) + ")";
    }
};

class Lexer
{
public:
    Lexer(const std::string &source);
    std::vector<Token> tokenize();

private:
    std::string source;
    int pos = 0;
    int line = 1;
    int column = 1;

    char peek(int set = 0) const;
    char advance();
    bool match(char expected);
    void skipWhitespace();

    //解析字符串字面量
    Token stringLiteral();
    //解析数字字面量
    Token numberLiteral();
    //解析标识符或关键字
    Token identifierOrKeyword();
};
