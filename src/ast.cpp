#include "ast.h"

//字面量构造函数
LiteralExpr::LiteralExpr(ll v, int l) : Expr(l), kind(Kind::INTEGER), ival(v) {}
LiteralExpr::LiteralExpr(double d, int l) : Expr(l), kind(Kind::FLOAT), dval(d) {}
LiteralExpr::LiteralExpr(string s, int l) : Expr(l), kind(Kind::STRING), sval(std::move(s)) {}
LiteralExpr::LiteralExpr(bool b, int l) : Expr(l), kind(Kind::BOOL), bval(b) {}

//标识符构造函数
IdentExpr::IdentExpr(string n, int l) : Expr(l), name(std::move(n)) {}

//一元表达式构造函数
UnaryExpr::UnaryExpr(string o, ExprPtr r, int l) : Expr(l), op(std::move(o)), rhs(std::move(r)) {}

//二元表达式构造函数
BinaryExpr::BinaryExpr(ExprPtr lh, string o, ExprPtr rh, int l): Expr(l), lhs(std::move(lh)), op(std::move(o)), rhs(std::move(rh)) {} 

//函数调用表达式构造函数
CallExpr::CallExpr(ExprPtr n, vector<ExprPtr> a, int l):Expr(l), name(std::move(n)), args(std::move(a)) {}

//成员访问表达式构造函数
MemberExpr::MemberExpr(ExprPtr t, string m, int l): Expr(l), target(std::move(t)), member(std::move(m)) {}

//语句构造函数
ExprStmt::ExprStmt(ExprPtr e, int l) : Stmt(l), expr(std::move(e)) {}

//赋值语句构造函数
AssignStmt::AssignStmt(string n, ExprPtr e, int l) : Stmt(l), name(std::move(n)),  expr(std::move(e)) {}

//声明语句构造函数(初始化表达式)
DeclStmt::DeclStmt(string t, string n, optional<ExprPtr> i, int l) : Stmt(l), type(std::move(t)), name(std::move(n)), init(std::move(i)) {}

//if语句构造函数
IfStmt::IfStmt(ExprPtr cond, StmtPtr thenBr, optional<StmtPtr> elseBr, int l) : Stmt(l), condition(std::move(cond)), thenBranch(std::move(thenBr)), elseBranch(std::move(elseBr)) {}

// for语句构造函数
ForStmt::ForStmt(DeclStmt d, ExprPtr cond, StmtPtr b, int l) : Stmt(l), decl(std::move(d)), condition(std::move(cond)), body(std::move(b)) {}

//while语句构造函数
WhileStmt::WhileStmt(ExprPtr cond, StmtPtr b, int l) : Stmt(l), condition(std::move(cond)), body(std::move(b)) {}

//continue语句构造函数
ContinueStmt::ContinueStmt(int l) : Stmt(l) {}

//break语句构造函数
BreakStmt::BreakStmt(vector<StmtPtr> b, int l) : Stmt(l), body(std::move(b)) {}

//return语句构造函数
ReturnStmt::ReturnStmt(optional<ExprPtr> e, int l) : Stmt(l), value(std::move(e)) {}

//对象构造函数
ObjStmt::ObjStmt(string c, ExprPtr id, vector<StmtPtr> b, int l) : Stmt(l), className(std::move(c)), idExpr(std::move(id)), body(std::move(b)) {}

//program构造函数
Program::Program() : ASTNode(0){}