#pragma once

#include "env.h"
#include "latte_error.h"
#include "frontend/Absyn.H"

class TypeChecker {
public:
    TypeChecker();

    void checkProgram(Program* program);

private:
    Env env_;

    void collectPredefinedFunctions();
    void collectFunctionSignatures(Program* program);
    void checkFunction(FnDef* fn);

    bool checkBlock(Block* block, LatteType expectedReturn);
    bool checkStmt(Stmt* stmt, LatteType expectedReturn);

    LatteType checkExpr(Expr* expr);
    LatteType typeFromAst(Type* ty);
};
