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

    IDENTIFIER,//标识符，包括变量名、函数名、类名等
    STRING,//字符串字面量
    NUMBER,//数字字面量

    LBRACE,//左大括号
    RBRACE,//右大括号
    LPAREN,//左小括号
    RPAREN,//右小括号
    LBRACKET,//左中括号
    RBRACKET,//右中括号
    COLON,//冒号
    COMMA,//逗号
    DOT,//点号
    ASSIGN,//赋值运算符
    EQ,//等于运算符
    NEQ,//不等于运算符
    LT,//小于运算符
    GT,//大于运算符
    LE,//小于等于运算符
    GE,//大于等于运算符
    PLUS,//加号
    MINUS,//减号
    STAR,//星号
    SLASH,//斜杠

    EOF_TOKEN,//文件结束标记
    UNKNOWN,//未知 token 类型
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
