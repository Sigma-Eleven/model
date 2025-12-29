#include "lexer.h"
#include <unordered_map>
#include <cctype>

Lexer::Lexer(const std::string& source) : source(source) {}

char Lexer::peek(int offset) const {
    if (pos + offset >= source.length()) return '\0';
    return source[pos + offset];
}

char Lexer::advance() {
    if (pos >= source.length()) return '\0';
    char c = source[pos];
    pos++;
    column++;
    if (c == '\n') {
        line++;
        column = 1;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (peek() == expected) {
        advance();
        return true;
    }
    return false;
}

void Lexer::skipWhitespace() {
    while (true) {
        char c = peek();
        if (isspace(c)) {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            // Comment
            while (peek() != '\n' && peek() != '\0') {
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::stringLiteral() {
    int startCol = column;
    advance(); // Skip "
    std::string value;
    while (peek() != '"' && peek() != '\0') {
        value += advance();
    }
    if (peek() == '"') advance();
    return {TokenType::STRING, value, line, startCol};
}

Token Lexer::numberLiteral() {
    int startCol = column;
    std::string value;
    while (isdigit(peek())) {
        value += advance();
    }
    return {TokenType::NUMBER, value, line, startCol};
}

Token Lexer::identifierOrKeyword() {
    int startCol = column;
    std::string text;
    while (isalnum(peek()) || peek() == '_') {
        text += advance();
    }

    static std::unordered_map<std::string, TokenType> keywords = {
        {"game", TokenType::KW_GAME},
        {"config", TokenType::KW_CONFIG},
        {"role", TokenType::KW_ROLE},
        {"var", TokenType::KW_VAR},
        {"action", TokenType::KW_ACTION},
        {"phase", TokenType::KW_PHASE},
        {"step", TokenType::KW_STEP},
        {"execute", TokenType::KW_EXECUTE},
        {"setup", TokenType::KW_SETUP},
        {"let", TokenType::KW_LET},
        {"if", TokenType::KW_IF},
        {"else", TokenType::KW_ELSE},
        {"for", TokenType::KW_FOR},
        {"in", TokenType::KW_IN},
        {"return", TokenType::KW_RETURN},
        {"true", TokenType::KW_TRUE},
        {"false", TokenType::KW_FALSE},
        {"null", TokenType::KW_NULL},
        {"and", TokenType::KW_AND},
        {"or", TokenType::KW_OR},
        {"not", TokenType::KW_NOT}
    };

    if (keywords.count(text)) {
        return {keywords[text], text, line, startCol};
    }
    return {TokenType::IDENTIFIER, text, line, startCol};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (pos < source.length()) {
        skipWhitespace();
        if (pos >= source.length()) break;

        char c = peek();
        int startCol = column;

        if (isalpha(c) || c == '_') {
            tokens.push_back(identifierOrKeyword());
        } else if (isdigit(c)) {
            tokens.push_back(numberLiteral());
        } else if (c == '"') {
            tokens.push_back(stringLiteral());
        } else {
            TokenType type = TokenType::UNKNOWN;
            std::string text(1, c);
            advance();

            switch (c) {
                case '{': type = TokenType::LBRACE; break;
                case '}': type = TokenType::RBRACE; break;
                case '(': type = TokenType::LPAREN; break;
                case ')': type = TokenType::RPAREN; break;
                case '[': type = TokenType::LBRACKET; break;
                case ']': type = TokenType::RBRACKET; break;
                case ':': type = TokenType::COLON; break;
                case ',': type = TokenType::COMMA; break;
                case '.': type = TokenType::DOT; break;
                case '+': type = TokenType::PLUS; break;
                case '-': type = TokenType::MINUS; break;
                case '*': type = TokenType::STAR; break;
                case '/': type = TokenType::SLASH; break;
                case '=': 
                    if (match('=')) { type = TokenType::EQ; text = "=="; }
                    else type = TokenType::ASSIGN;
                    break;
                case '!':
                    if (match('=')) { type = TokenType::NEQ; text = "!="; }
                    break;
                case '<':
                    if (match('=')) { type = TokenType::LE; text = "<="; }
                    else type = TokenType::LT;
                    break;
                case '>':
                    if (match('=')) { type = TokenType::GE; text = ">="; }
                    else type = TokenType::GT;
                    break;
            }
            tokens.push_back({type, text, line, startCol});
        }
    }
    tokens.push_back({TokenType::EOF_TOKEN, "", line, column});
    return tokens;
}
