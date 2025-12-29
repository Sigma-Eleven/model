#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>

class ASTVisitor;

class Node
{
public:
    virtual ~Node() = default;
    virtual void accept(ASTVisitor &visitor) = 0;
};

class Expression : public Node
{
};

class LiteralExpr : public Expression
{
public:
    std::string value; 
    std::string type;  

    LiteralExpr(std::string v, std::string t) : value(v), type(t) {}
    void accept(ASTVisitor &visitor) override;
};

class VariableExpr : public Expression
{
public:
    std::string name;
    VariableExpr(std::string n) : name(n) {}
    void accept(ASTVisitor &visitor) override;
};

class BinaryExpr : public Expression
{
public:
    std::unique_ptr<Expression> left;
    std::string op;
    std::unique_ptr<Expression> right;

    BinaryExpr(std::unique_ptr<Expression> l, std::string o, std::unique_ptr<Expression> r)
        : left(std::move(l)), op(o), right(std::move(r)) {}
    void accept(ASTVisitor &visitor) override;
};

class UnaryExpr : public Expression
{
public:
    std::string op;
    std::unique_ptr<Expression> right;

    UnaryExpr(std::string o, std::unique_ptr<Expression> r) : op(o), right(std::move(r)) {}
    void accept(ASTVisitor &visitor) override;
};

class CallExpr : public Expression
{
public:
    std::string callee;
    std::vector<std::unique_ptr<Expression>> args;

    CallExpr(std::string c, std::vector<std::unique_ptr<Expression>> a)
        : callee(c), args(std::move(a)) {}
    void accept(ASTVisitor &visitor) override;
};

class ListExpr : public Expression
{
public:
    std::vector<std::unique_ptr<Expression>> elements;
    ListExpr(std::vector<std::unique_ptr<Expression>> e) : elements(std::move(e)) {}
    void accept(ASTVisitor &visitor) override;
};

class MemberExpr : public Expression
{
public:
    std::unique_ptr<Expression> object;
    std::string member;

    MemberExpr(std::unique_ptr<Expression> obj, std::string m)
        : object(std::move(obj)), member(m) {}
    void accept(ASTVisitor &visitor) override;
};

class IndexExpr : public Expression
{
public:
    std::unique_ptr<Expression> object;
    std::unique_ptr<Expression> index;

    IndexExpr(std::unique_ptr<Expression> obj, std::unique_ptr<Expression> idx)
        : object(std::move(obj)), index(std::move(idx)) {}
    void accept(ASTVisitor &visitor) override;
};

class Statement : public Node
{
};

class BlockStmt : public Statement
{
public:
    std::vector<std::unique_ptr<Statement>> statements;
    void accept(ASTVisitor &visitor) override;
};

class LetStmt : public Statement
{
public:
    std::string name;
    std::unique_ptr<Expression> initializer;

    LetStmt(std::string n, std::unique_ptr<Expression> init)
        : name(n), initializer(std::move(init)) {}
    void accept(ASTVisitor &visitor) override;
};

class AssignStmt : public Statement
{
public:
    std::unique_ptr<Expression> target; 
    std::unique_ptr<Expression> value;

    AssignStmt(std::unique_ptr<Expression> t, std::unique_ptr<Expression> v)
        : target(std::move(t)), value(std::move(v)) {}
    void accept(ASTVisitor &visitor) override;
};

class IfStmt : public Statement
{
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> thenBranch;
    std::unique_ptr<Statement> elseBranch; 

    IfStmt(std::unique_ptr<Expression> cond, std::unique_ptr<Statement> thenB, std::unique_ptr<Statement> elseB = nullptr)
        : condition(std::move(cond)), thenBranch(std::move(thenB)), elseBranch(std::move(elseB)) {}
    void accept(ASTVisitor &visitor) override;
};

class ForStmt : public Statement
{
public:
    std::string iterator;
    std::unique_ptr<Expression> iterable;
    std::unique_ptr<BlockStmt> body;

    ForStmt(std::string it, std::unique_ptr<Expression> list, std::unique_ptr<BlockStmt> b)
        : iterator(it), iterable(std::move(list)), body(std::move(b)) {}
    void accept(ASTVisitor &visitor) override;
};

class ReturnStmt : public Statement
{
public:
    std::unique_ptr<Expression> value; 
    ReturnStmt(std::unique_ptr<Expression> v = nullptr) : value(std::move(v)) {}
    void accept(ASTVisitor &visitor) override;
};

class ExpressionStmt : public Statement
{
public:
    std::unique_ptr<Expression> expression;
    ExpressionStmt(std::unique_ptr<Expression> expr) : expression(std::move(expr)) {}
    void accept(ASTVisitor &visitor) override;
};

class RoleDecl : public Node
{
public:
    std::string name;
    std::string displayName;
    
    RoleDecl(std::string n, std::string d) : name(n), displayName(d) {}
    void accept(ASTVisitor &visitor) override;
};

class VarDecl : public Node
{
public:
    std::string name;
    std::string type;
    std::unique_ptr<Expression> initializer;

    VarDecl(std::string n, std::string t, std::unique_ptr<Expression> init)
        : name(n), type(t), initializer(std::move(init)) {}
    void accept(ASTVisitor &visitor) override;
};

class ActionDecl : public Node
{
public:
    std::string name;
    std::string displayName;
    std::string description;
    std::unique_ptr<BlockStmt> body;

    ActionDecl(std::string n, std::string d) : name(n), displayName(d) {}
    void accept(ASTVisitor &visitor) override;
};

class StepDecl : public Node
{
public:
    std::string name;
    std::vector<std::string> roles;
    std::string actionName;

    StepDecl(std::string n) : name(n) {}
    void accept(ASTVisitor &visitor) override;
};

class PhaseDecl : public Node
{
public:
    std::string name;
    std::string displayName;
    std::vector<std::unique_ptr<StepDecl>> steps;

    PhaseDecl(std::string n, std::string d) : name(n), displayName(d) {}
    void accept(ASTVisitor &visitor) override;
};

class ConfigDecl : public Node
{
public:
    int minPlayers = 0;
    int maxPlayers = 0;
    void accept(ASTVisitor &visitor) override;
};

class GameDecl : public Node
{
public:
    std::string name;
    std::unique_ptr<ConfigDecl> config;
    std::vector<std::unique_ptr<RoleDecl>> roles;
    std::vector<std::unique_ptr<VarDecl>> vars;
    std::unique_ptr<BlockStmt> setup;
    std::vector<std::unique_ptr<ActionDecl>> actions;
    std::vector<std::unique_ptr<PhaseDecl>> phases;

    GameDecl(std::string n) : name(n) {}
    void accept(ASTVisitor &visitor) override;
};

class ASTVisitor
{
public:
    virtual void visit(LiteralExpr &node) = 0;
    virtual void visit(VariableExpr &node) = 0;
    virtual void visit(BinaryExpr &node) = 0;
    virtual void visit(UnaryExpr &node) = 0;
    virtual void visit(CallExpr &node) = 0;
    virtual void visit(ListExpr &node) = 0;
    virtual void visit(MemberExpr &node) = 0;
    virtual void visit(IndexExpr &node) = 0;

    virtual void visit(BlockStmt &node) = 0;
    virtual void visit(LetStmt &node) = 0;
    virtual void visit(AssignStmt &node) = 0;
    virtual void visit(IfStmt &node) = 0;
    virtual void visit(ForStmt &node) = 0;
    virtual void visit(ReturnStmt &node) = 0;
    virtual void visit(ExpressionStmt &node) = 0;

    virtual void visit(RoleDecl &node) = 0;
    virtual void visit(VarDecl &node) = 0;
    virtual void visit(ActionDecl &node) = 0;
    virtual void visit(StepDecl &node) = 0;
    virtual void visit(PhaseDecl &node) = 0;
    virtual void visit(ConfigDecl &node) = 0;
    virtual void visit(GameDecl &node) = 0;
};

inline void LiteralExpr::accept(ASTVisitor &v) { v.visit(*this); }
inline void VariableExpr::accept(ASTVisitor &v) { v.visit(*this); }
inline void BinaryExpr::accept(ASTVisitor &v) { v.visit(*this); }
inline void UnaryExpr::accept(ASTVisitor &v) { v.visit(*this); }
inline void CallExpr::accept(ASTVisitor &v) { v.visit(*this); }
inline void ListExpr::accept(ASTVisitor &v) { v.visit(*this); }
inline void MemberExpr::accept(ASTVisitor &v) { v.visit(*this); }
inline void IndexExpr::accept(ASTVisitor &v) { v.visit(*this); }
inline void BlockStmt::accept(ASTVisitor &v) { v.visit(*this); }
inline void LetStmt::accept(ASTVisitor &v) { v.visit(*this); }
inline void AssignStmt::accept(ASTVisitor &v) { v.visit(*this); }
inline void IfStmt::accept(ASTVisitor &v) { v.visit(*this); }
inline void ForStmt::accept(ASTVisitor &v) { v.visit(*this); }
inline void ReturnStmt::accept(ASTVisitor &v) { v.visit(*this); }
inline void ExpressionStmt::accept(ASTVisitor &v) { v.visit(*this); }
inline void RoleDecl::accept(ASTVisitor &v) { v.visit(*this); }
inline void VarDecl::accept(ASTVisitor &v) { v.visit(*this); }
inline void ActionDecl::accept(ASTVisitor &v) { v.visit(*this); }
inline void StepDecl::accept(ASTVisitor &v) { v.visit(*this); }
inline void PhaseDecl::accept(ASTVisitor &v) { v.visit(*this); }
inline void ConfigDecl::accept(ASTVisitor &v) { v.visit(*this); }
inline void GameDecl::accept(ASTVisitor &v) { v.visit(*this); }
