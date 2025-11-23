#pragma once

#include <memory>
#include <vector>
#include <string>
#include <optional>

using namespace std;
using ll = long long;

//根
struct ASTNode{
    int line;
    ASTNode(int l) : line(l) {}
    virtual ~ASTNode() = default;
};

//表达式节点
struct Expr : ASTNode
{
    Expr(int l) : ASTNode(l) {}
};
using ExprPtr = unique_ptr<Expr>;

//语句节点
struct Stmt : ASTNode
{
    Stmt(int l) : ASTNode(l) {}
};
using StmtPtr = unique_ptr<Stmt>;

//字面量
struct LiteralExpr : Expr
{
    enum class Kind
    {
        INTEGER,
        FLOAT,
        STRING,
        BOOL
    } kind;

    union
    {
        ll ival;
        double dval;
    };
    string sval;
    bool bval;

    LiteralExpr(ll v, int l);
    LiteralExpr(double d, int l);
    LiteralExpr(string s, int l);
    LiteralExpr(bool b, int l);
};

//标识符
struct IdentExpr : Expr
{
    string name;
    IdentExpr(string n, int l);
};

//一元表达式(操作符 + 操作数)
struct UnaryExpr : Expr
{
    string op;
    ExprPtr rhs;
    UnaryExpr(string o, ExprPtr r, int l);
};

//二元表达式(左操作数 + 操作符 + 右操作数)
struct BinaryExpr : Expr
{
    string op;
    ExprPtr lhs, rhs;
    BinaryExpr(ExprPtr lh, string o, ExprPtr rh, int l);
};

//函数调用表达式(函数名 + 实参列表)
struct CallExpr : Expr
{
    ExprPtr name;
    vector<ExprPtr> args;
    CallExpr(ExprPtr n, vector<ExprPtr> a, int l);
};

//成员访问表达式(目标对象 + 成员名)
struct MemberExpr : Expr
{
    ExprPtr target;
    string member;
    MemberExpr(ExprPtr t, string m, int l);
};

//表达式语句
struct ExprStmt : Stmt
{
    ExprPtr expr;
    ExprStmt(ExprPtr e, int l);
};

//赋值语句(变量名 + 表达式)
struct AssignStmt : Stmt
{
    string name;
    ExprPtr expr;
    AssignStmt(string n, ExprPtr e, int l);
};

// 声明语句(类型 + 变量名 + 初始化表达式)
// 声明语句(类型 + 变量名 + 初始化语句块)
struct DeclStmt : Stmt
{
    string type; 
    string name;
    optional<ExprPtr> init;
    vector<StmtPtr> initBlock; 
    DeclStmt(string t, string n, optional<ExprPtr> i, int l);
    DeclStmt(string t, string n, vector<StmtPtr> block, int l);
};

//if语句
struct IfStmt : Stmt
{
    ExprPtr condition;
    StmtPtr thenBranch;
    optional<StmtPtr> elseBranch;
    IfStmt(ExprPtr cond, StmtPtr thenBranch, optional<StmtPtr> elseBranch, int l);
};

//for语句
struct ForStmt : Stmt
{
    DeclStmt decl; 
    ExprPtr condition; 
    StmtPtr body; 
    ForStmt(DeclStmt d, ExprPtr cond, StmtPtr b, int l);
};

//while语句
struct WhileStmt : Stmt
{
    ExprPtr condition;
    StmtPtr body;
    WhileStmt(ExprPtr cond, StmtPtr b, int l);
};

//continue语句
struct ContinueStmt : Stmt
{
    ContinueStmt(int l);
};

//break语句
struct BreakStmt : Stmt
{
    vector<StmtPtr> body; 
    BreakStmt(vector<StmtPtr> b, int l);
};

//return语句
struct ReturnStmt : Stmt
{
    optional<ExprPtr> value; 
    ReturnStmt(optional<ExprPtr> v, int l);
};

// 对象语句
struct ObjStmt : Stmt
{
    string className;
    ExprPtr idExpr;
    vector<StmtPtr> body;
    ObjStmt(string c, ExprPtr id, vector<StmtPtr> b, int l);
};

//程序(根节点)
struct Program : ASTNode
{
    std::vector<StmtPtr> stmts;
    Program();
};
