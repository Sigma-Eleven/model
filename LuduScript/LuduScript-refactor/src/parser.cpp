#include "parser.h"
#include <stdexcept>

Parser::Parser(const std::vector<Token> &tokens) : tokens(tokens) {}

std::unique_ptr<GameDecl> Parser::parse()
{
    return parseGame();
}

Token Parser::peek() const
{
    return tokens[current];
}

Token Parser::previous() const
{
    return tokens[current - 1];
}

bool Parser::isAtEnd() const
{
    return peek().type == TokenType::EOF_TOKEN;
}

Token Parser::advance()
{
    if (!isAtEnd())
        current++;
    return previous();
}

bool Parser::check(TokenType type) const
{
    if (isAtEnd())
        return false;
    return peek().type == type;
}

bool Parser::match(TokenType type)
{
    if (check(type))
    {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string &message)
{
    if (check(type))
        return advance();
    error(peek(), message);
    throw std::runtime_error("Parse Error");
}

void Parser::error(const Token &token, const std::string &message)
{
    std::cerr << "Error at line " << token.line << " [" << token.text << "]: " << message << std::endl;
}


std::unique_ptr<GameDecl> Parser::parseGame()
{
    consume(TokenType::KW_GAME, "Expect 'game' keyword.");
    Token nameToken = consume(TokenType::IDENTIFIER, "Expect game name.");
    consume(TokenType::LBRACE, "Expect '{' after game name.");

    auto game = std::make_unique<GameDecl>(nameToken.text);

    while (!check(TokenType::RBRACE) && !isAtEnd())
    {
        if (check(TokenType::KW_CONFIG))
        {
            game->config = parseConfig();
        }
        else if (check(TokenType::KW_ROLE))
        {
            game->roles.push_back(parseRole());
        }
        else if (check(TokenType::KW_VAR))
        {
            game->vars.push_back(parseVar());
        }
        else if (match(TokenType::KW_SETUP))
        {
            game->setup = parseBlock();
        }
        else if (check(TokenType::KW_ACTION))
        {
            game->actions.push_back(parseAction());
        }
        else if (check(TokenType::KW_PHASE))
        {
            game->phases.push_back(parsePhase());
        }
        else
        {
            error(peek(), "Unexpected token in game declaration.");
            advance();
        }
    }

    consume(TokenType::RBRACE, "Expect '}' after game body.");
    return game;
}

std::unique_ptr<ConfigDecl> Parser::parseConfig()
{
    consume(TokenType::KW_CONFIG, "Expect 'config' keyword.");
    consume(TokenType::LBRACE, "Expect '{'.");

    auto config = std::make_unique<ConfigDecl>();

    while (!check(TokenType::RBRACE) && !isAtEnd())
    {
        Token key = consume(TokenType::IDENTIFIER, "Expect config key.");
        consume(TokenType::COLON, "Expect ':'.");
        Token value = consume(TokenType::NUMBER, "Expect number value.");

        if (key.text == "min_players")
            config->minPlayers = std::stoi(value.text);
        else if (key.text == "max_players")
            config->maxPlayers = std::stoi(value.text);
    }

    consume(TokenType::RBRACE, "Expect '}'.");
    return config;
}

std::unique_ptr<RoleDecl> Parser::parseRole()
{
    consume(TokenType::KW_ROLE, "Expect 'role'.");
    Token name = consume(TokenType::IDENTIFIER, "Expect role name.");
    std::string display = name.text;
    if (check(TokenType::STRING))
    {
        display = consume(TokenType::STRING, "Expect display name.").text;
    }
    return std::make_unique<RoleDecl>(name.text, display);
}

std::unique_ptr<VarDecl> Parser::parseVar()
{
    consume(TokenType::KW_VAR, "Expect 'var'.");
    Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");
    consume(TokenType::COLON, "Expect ':'.");
    Token type = consume(TokenType::IDENTIFIER, "Expect type.");

    std::unique_ptr<Expression> init = nullptr;
    if (match(TokenType::ASSIGN))
    {
        init = parseExpression();
    }

    return std::make_unique<VarDecl>(name.text, type.text, std::move(init));
}

std::unique_ptr<ActionDecl> Parser::parseAction()
{
    consume(TokenType::KW_ACTION, "Expect 'action'.");
    Token name = consume(TokenType::IDENTIFIER, "Expect action name.");
    std::string display = name.text;
    if (check(TokenType::STRING))
    {
        display = consume(TokenType::STRING, "Expect display name.").text;
    }

    auto action = std::make_unique<ActionDecl>(name.text, display);
    consume(TokenType::LBRACE, "Expect '{'.");

    while (!check(TokenType::RBRACE) && !isAtEnd())
    {
        if (check(TokenType::KW_EXECUTE))
        {
            consume(TokenType::KW_EXECUTE, "Expect 'execute'.");
            action->body = parseBlock();
        }
        else if (check(TokenType::IDENTIFIER))
        {
            Token key = consume(TokenType::IDENTIFIER, "Expect key.");
            if (key.text == "description")
            {
                consume(TokenType::COLON, "Expect ':'.");
                action->description = consume(TokenType::STRING, "Expect string.").text;
            }
        }
        else
        {
            advance(); 
        }
    }
    consume(TokenType::RBRACE, "Expect '}'.");
    return action;
}

std::unique_ptr<PhaseDecl> Parser::parsePhase()
{
    consume(TokenType::KW_PHASE, "Expect 'phase'.");
    Token name = consume(TokenType::IDENTIFIER, "Expect phase name.");
    std::string display = name.text;
    if (check(TokenType::STRING))
    {
        display = consume(TokenType::STRING, "Expect display name.").text;
    }

    auto phase = std::make_unique<PhaseDecl>(name.text, display);
    consume(TokenType::LBRACE, "Expect '{'.");

    while (!check(TokenType::RBRACE) && !isAtEnd())
    {
        if (check(TokenType::KW_STEP))
        {
            phase->steps.push_back(parseStep());
        }
        else
        {
            advance();
        }
    }
    consume(TokenType::RBRACE, "Expect '}'.");
    return phase;
}

std::unique_ptr<StepDecl> Parser::parseStep()
{
    consume(TokenType::KW_STEP, "Expect 'step'.");
    Token nameToken;
    if (check(TokenType::STRING))
    {
        nameToken = consume(TokenType::STRING, "Expect step name string.");
    }
    else
    {
        nameToken = consume(TokenType::IDENTIFIER, "Expect step name identifier.");
    }

    auto step = std::make_unique<StepDecl>(nameToken.text);
    consume(TokenType::LBRACE, "Expect '{'.");

    while (!check(TokenType::RBRACE) && !isAtEnd())
    {
        std::string keyText;
        if (match(TokenType::KW_ACTION))
        {
            keyText = "action";
        }
        else
        {
            keyText = consume(TokenType::IDENTIFIER, "Expect key.").text;
        }
        consume(TokenType::COLON, "Expect ':'.");

        if (keyText == "roles")
        {
            consume(TokenType::LBRACKET, "Expect '['.");
            while (!check(TokenType::RBRACKET) && !isAtEnd())
            {
                step->roles.push_back(consume(TokenType::IDENTIFIER, "Expect role name.").text);
                if (!check(TokenType::RBRACKET))
                    consume(TokenType::COMMA, "Expect ','.");
            }
            consume(TokenType::RBRACKET, "Expect ']'.");
        }
        else if (keyText == "action")
        {
            step->actionName = consume(TokenType::IDENTIFIER, "Expect action name.").text;
        }
    }
    consume(TokenType::RBRACE, "Expect '}'.");
    return step;
}


std::unique_ptr<Statement> Parser::parseStatement()
{
    if (match(TokenType::KW_LET))
        return parseLet();
    if (match(TokenType::KW_IF))
        return parseIf();
    if (match(TokenType::KW_FOR))
        return parseFor();
    if (match(TokenType::KW_RETURN))
        return parseReturn();
    if (check(TokenType::LBRACE))
        return parseBlock();

    return parseExpressionStatement();
}

std::unique_ptr<Statement> Parser::parseFor()
{
    bool hasParen = match(TokenType::LPAREN);
    Token iterator = consume(TokenType::IDENTIFIER, "Expect iterator name.");
    consume(TokenType::KW_IN, "Expect 'in'.");
    auto iterable = parseExpression();
    if (hasParen)
    {
        consume(TokenType::RPAREN, "Expect ')'.");
    }
    auto body = parseBlock();
    return std::make_unique<ForStmt>(iterator.text, std::move(iterable), std::move(body));
}

std::unique_ptr<BlockStmt> Parser::parseBlock()
{
    consume(TokenType::LBRACE, "Expect '{'.");
    auto block = std::make_unique<BlockStmt>();
    while (!check(TokenType::RBRACE) && !isAtEnd())
    {
        block->statements.push_back(parseStatement());
    }
    consume(TokenType::RBRACE, "Expect '}'.");
    return block;
}

std::unique_ptr<LetStmt> Parser::parseLet()
{
    Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");
    consume(TokenType::ASSIGN, "Expect '='.");
    auto init = parseExpression();
    return std::make_unique<LetStmt>(name.text, std::move(init));
}

std::unique_ptr<IfStmt> Parser::parseIf()
{
    bool hasParen = match(TokenType::LPAREN);
    auto condition = parseExpression();
    if (hasParen)
    {
        consume(TokenType::RPAREN, "Expect ')'.");
    }

    auto thenBranch = parseBlock();
    std::unique_ptr<Statement> elseBranch = nullptr;

    if (match(TokenType::KW_ELSE))
    {
        if (match(TokenType::KW_IF))
        {
            elseBranch = parseIf(); 
        }
        else
        {
            elseBranch = parseBlock();
        }
    }
    return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<ReturnStmt> Parser::parseReturn()
{
    std::unique_ptr<Expression> value = nullptr;
    if (!check(TokenType::RBRACE))
    { 
        value = parseExpression();
    }
    return std::make_unique<ReturnStmt>(std::move(value));
}

std::unique_ptr<Statement> Parser::parseExpressionStatement()
{
    auto expr = parseExpression();

    if (match(TokenType::ASSIGN))
    {
        auto value = parseExpression();
        return std::make_unique<AssignStmt>(std::move(expr), std::move(value));
    }

    return std::make_unique<ExpressionStmt>(std::move(expr));
}


std::unique_ptr<Expression> Parser::parseExpression()
{
    return parseEquality();
}

std::unique_ptr<Expression> Parser::parseEquality()
{
    auto expr = parseComparison();
    while (match(TokenType::EQ) || match(TokenType::NEQ))
    {
        std::string op = previous().text;
        auto right = parseComparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expression> Parser::parseComparison()
{
    auto expr = parseTerm();
    while (match(TokenType::GT) || match(TokenType::GE) || match(TokenType::LT) || match(TokenType::LE))
    {
        std::string op = previous().text;
        auto right = parseTerm();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expression> Parser::parseTerm()
{
    auto expr = parseFactor();
    while (match(TokenType::PLUS) || match(TokenType::MINUS))
    {
        std::string op = previous().text;
        auto right = parseFactor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expression> Parser::parseFactor()
{
    auto expr = parseUnary();
    while (match(TokenType::SLASH) || match(TokenType::STAR))
    {
        std::string op = previous().text;
        auto right = parseUnary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expression> Parser::parseUnary()
{
    if (match(TokenType::KW_NOT) || match(TokenType::MINUS))
    {
        std::string op = previous().text;
        auto right = parseUnary();
        return std::make_unique<UnaryExpr>(op, std::move(right));
    }
    return parsePrimary();
}

std::unique_ptr<Expression> Parser::parsePrimary()
{
    if (match(TokenType::KW_FALSE))
        return std::make_unique<LiteralExpr>("False", "bool");
    if (match(TokenType::KW_TRUE))
        return std::make_unique<LiteralExpr>("True", "bool");
    if (match(TokenType::KW_NULL))
        return std::make_unique<LiteralExpr>("None", "null");

    if (match(TokenType::NUMBER))
    {
        return std::make_unique<LiteralExpr>(previous().text, "number");
    }
    if (match(TokenType::STRING))
    {
        return std::make_unique<LiteralExpr>(previous().text, "string");
    }

    if (match(TokenType::IDENTIFIER) || match(TokenType::KW_GAME))
    {
        std::string name = previous().text;
        std::unique_ptr<Expression> expr = std::make_unique<VariableExpr>(name);

        while (true)
        {
            if (match(TokenType::DOT))
            {
                Token member = consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
                expr = std::make_unique<MemberExpr>(std::move(expr), member.text);
            }
            else if (match(TokenType::LBRACKET))
            {
                auto index = parseExpression();
                consume(TokenType::RBRACKET, "Expect ']' after index.");
                expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index));
            }
            else if (match(TokenType::LPAREN))
            {
                std::vector<std::unique_ptr<Expression>> args;
                if (!check(TokenType::RPAREN))
                {
                    do
                    {
                        args.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expect ')' after arguments.");
        
                if (auto *varExpr = dynamic_cast<VariableExpr *>(expr.get()))
                {
                    auto call = std::make_unique<CallExpr>(varExpr->name, std::move(args));
                    expr = std::move(call);
                }
                else
                {
                
                }
            }
            else
            {
                break;
            }
        }
        return expr;
    }

    if (match(TokenType::LBRACKET))
    {
        std::vector<std::unique_ptr<Expression>> elements;
        if (!check(TokenType::RBRACKET))
        {
            do
            {
                elements.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RBRACKET, "Expect ']'.");
        return std::make_unique<ListExpr>(std::move(elements));
    }

    if (match(TokenType::LPAREN))
    {
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "Expect ')' after expression.");
        return expr;
    }

    error(peek(), "Expect expression.");
    throw std::runtime_error("Parse Error");
}
