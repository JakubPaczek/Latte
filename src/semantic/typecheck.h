#pragma once

#include <optional>
#include <string>

#include "env.h"
#include "latte_error.h"
#include "Absyn.H"

class TypeChecker {
public:
    TypeChecker();

    void checkProgram(Program* program);

private:
    Env env_;

    std::optional<std::string> currentClass_;

    void collectPredefinedFunctions();
    void collectClassHeaders(Program* program);
    void collectSignatures(Program* program);

    void checkTopLevelFunction(FnDef* fn);
    void checkClassBodies(Program* program);
    void checkMethodBody(const std::string& className, Method* m);

    bool checkBlock(Block* block, const LatteType& expectedReturn);
    bool checkStmt(Stmt* stmt, const LatteType& expectedReturn);

    LatteType checkExpr(Expr* expr);
    LatteType typeFromAst(Type* ty);
    LatteType baseTypeFromAst(BaseType* bt);

    LatteType checkLValueExpr(Expr* e);

    LatteType dtypeFromAst(DType* ty);

    bool isAssignable(const LatteType& dst, const LatteType& src) const;
    bool isSubClassOf(const std::string& sub, const std::string& base) const;

    std::optional<LatteType> lookupVarOrFieldType(const std::string& name) const;

    [[noreturn]] void fail(const std::string& msg, int line = 0) const
    {
        throw LatteError(msg, line);
    }
};