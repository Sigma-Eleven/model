#pragma once

#include<vector>
#include<string>

enum class TokenKind
{
    END,
    IDENT,
    NUMBER,
    STRING,
    BOOL,
    KW_IF,
    KW_ELIF,
    KW_ELSE,
    KW_FOR,
    KW_BREAK,
    KW_CONTINUE,
    KW_OBJ,
    KW_NUM,
    KW_STR,
    KW_BOOL,
    KW_TRUE,
    KW_FALSE,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    COMMA,
    SEMI,
    DOT,
    PLUS,
    MINUS,
    MUL,
    DIV,
    MOD,
    EQ,
    ASSIGN,
    NEQ,
    LT,
    GT,
    LE,
    GE,
    AND,
    OR,
    NOT,
    LBRACKET, 
    RBRACKET, 
    UNKNOWN
};

struct Token
{
    TokenKind kind;
    std::string text;
    int line;

    Token(TokenKind k = TokenKind::UNKNOWN, std::string t = "", int l = 1);
};

class Lexer
{
private:
    std::string source;
    size_t pos = 0;
    int line;

    char peek() const;
    char get();
    void skipWhitespace();
    Token identifier();
    Token number();
    Token string();

public:
    explicit Lexer(const std::string &source);
    Token getNextToken();
};