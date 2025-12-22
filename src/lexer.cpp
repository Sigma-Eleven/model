#include "lexer.h"
#include <cctype>

Token::Token(TokenKind k, std::string t, int l) : kind(k), text(std::move(t)), line(l) {}
Lexer::Lexer(const std::string &source) : source(source), pos(0), line(1) {}

char Lexer::peek() const
{
    if (pos >= source.size())
        return '\0';
    return source[pos];
}

char Lexer::get()
{
    if (pos >= source.size())
        return '\0';
    return source[pos++];
}

void Lexer::skipWhitespace()
{
    while (true)
    {
        char c = peek();
        if (std::isspace(c))
        {
            if (c == '\n')
                line++;
            get();
        }
        else if (c == '/' && pos + 1 < source.size() && source[pos + 1] == '/')
        {
            // 跳过注释
            while (peek() != '\n' && peek() != '\0')
                get();
        }
        else
        {
            break;
        }
    }
}

// 标识符和关键字处理
Token Lexer::identifier()
{
    std::string s;
    int i = line;
    while (std::isalnum(peek()) || peek() == '_')
        s += get();

    return Token(TokenKind::IDENT, s, i);
}

// 数字处理
Token Lexer::number()
{
    std::string s;
    int i = line;
    while (std::isdigit(peek()))
    {
        s += get();
        if (peek() == '.')
        {
            s += get();
            while (std::isdigit(peek()))
            {
                s += get();
            }
            break;
        }
    }
    return Token(TokenKind::NUMBER, s, i);
}

// 字符串处理
Token Lexer::string()
{
    std::string s;
    int i = line;
    get();
    while (true)
    {
        char current = peek();
        if (current == '\0' || current == '\n')
            break;
        if (current == '"')
        {
            get();
            break;
        }
        if (current == '\\')
        {
            get();
            char next = peek();
            switch (next)
            {
            case 'n':
                s += '\n';
                break;
            case 't':
                s += '\t';
                break;
            case '"':
                s += '"';
                break;
            case '\\':
                s += '\\';
                break;
            default:
                s += next;
                break;
            }
            get();
            continue;
        }
        s += get();
    }
    return Token(TokenKind::STRING, s, i);
}

// 获取下一个Token
Token Lexer::getNextToken()
{
    skipWhitespace();

    char current = peek();
    if (current == '\0')
        return Token(TokenKind::END, "", line);

    if (std::isalpha(current) || current == '_')
        return identifier();

    if (std::isdigit(current))
        return number();

    if (current == '"')
        return string();

    int i = line;
    switch (current)
    {
    case '(':
        get();
        return Token(TokenKind::LPAREN, "(", i);
    case ')':
        get();
        return Token(TokenKind::RPAREN, ")", i);
    case '{':
        get();
        return Token(TokenKind::LBRACE, "{", i);
    case '}':
        get();
        return Token(TokenKind::RBRACE, "}", i);
    case ',':
        get();
        return Token(TokenKind::COMMA, ",", i);
    case ';':
        get();
        return Token(TokenKind::SEMI, ";", i);
    case '.':
        get();
        return Token(TokenKind::DOT, ".", i);
    case '+':
        get();
        return Token(TokenKind::PLUS, "+", i);
    case '-':
        get();
        return Token(TokenKind::MINUS, "-", i);
    case '*':
        get();
        return Token(TokenKind::MUL, "*", i);
    case '/':
        get();
        return Token(TokenKind::DIV, "/", i);
    case '%':
        get();
        return Token(TokenKind::MOD, "%", i);
    case '=':
        get();
        if (peek() == '=')
        {
            get();
            return Token(TokenKind::EQ, "==", i);
        }
        return Token(TokenKind::ASSIGN, "=", i);
    case '!':
        get();
        if (peek() == '=')
        {
            get();
            return Token(TokenKind::NEQ, "!=", i);
        }
        return Token(TokenKind::NOT, "!", i);
    case '<':
        get();
        if (peek() == '=')
        {
            get();
            return Token(TokenKind::LE, "<=", i);
        }
        return Token(TokenKind::LT, "<", i);
    case '>':
        get();
        if (peek() == '=')
        {
            get();
            return Token(TokenKind::GE, ">=", i);
        }
        return Token(TokenKind::GT, ">", i);
    case '&':
        get();
        if (peek() == '&')
        {
            get();
            return Token(TokenKind::AND, "&&", i);
        }
        break;
    case '|':
        get();
        if (peek() == '|')
        {
            get();
            return Token(TokenKind::OR, "||", i);
        }
        break;
    default:
        get();
        return Token(TokenKind::UNKNOWN, std::string(1, current), i);
    }
    return Token(TokenKind::UNKNOWN, std::string(1, current), i);
}