#pragma once

#include "ast.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class PythonGenerator : public ASTVisitor
{
public:
    PythonGenerator(std::ostream &out);
    void generate(GameDecl &game);

    void visit(LiteralExpr &node) override;
    void visit(VariableExpr &node) override;
    void visit(BinaryExpr &node) override;
    void visit(UnaryExpr &node) override;
    void visit(CallExpr &node) override;
    void visit(ListExpr &node) override;
    void visit(MemberExpr &node) override;
    void visit(IndexExpr &node) override;

    void visit(BlockStmt &node) override;
    void visit(LetStmt &node) override;
    void visit(AssignStmt &node) override;
    void visit(IfStmt &node) override;
    void visit(ForStmt &node) override;
    void visit(ReturnStmt &node) override;
    void visit(ExpressionStmt &node) override;

    void visit(RoleDecl &node) override;
    void visit(VarDecl &node) override;
    void visit(ActionDecl &node) override;
    void visit(StepDecl &node) override;
    void visit(PhaseDecl &node) override;
    void visit(ConfigDecl &node) override;
    void visit(GameDecl &node) override;

private:
    std::ostream &out;
    int indentLevel = 0;
    std::string className;
    std::vector<std::string> globalVars;

    void indent();
    void emitImports();
    void emitHelpers();
};
