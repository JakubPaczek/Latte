#pragma once

#include <optional>
#include <string>

#include "env.h"
#include "latte_error.h"
#include "Absyn.H"

class TypeChecker {
public:
    TypeChecker();

    void checkProgram(Program* program);        // main entry

private:
    Env env_; // all collected symbols + scope stack

    std::optional<std::string> currentClass_;   // only when checking a method body

    void collectPredefinedFunctions();
    void collectClassHeaders(Program* program); // pass 1: register classes + bases
    void collectSignatures(Program* program);   // pass 2: register function + fields + methods

    void checkTopLevelFunction(FnDef* fn);      // pass 3: check body of free function
    void checkClassBodies(Program* program);    // pass 4: check all method bodies
    void checkMethodBody(const std::string& className, Method* m);

    bool checkBlock(Block* block, const LatteType& expectedReturn);
    bool checkStmt(Stmt* stmt, const LatteType& expectedReturn);

    LatteType checkExpr(Expr* expr);
    LatteType typeFromAst(Type* ty);        // convert bnfc Type to LatteType
    LatteType typeFromAst(BaseType* t);     // convert bnfc BaseType to LatteType
    LatteType baseTypeFromAst(BaseType* bt);// up + validates class existence
    LatteType dtypeFromAst(DType* ty);      // convert bnfc DType (decl types) to LatteType

    LatteType checkLValueExpr(Expr* e);     // validate "assignable place" (var / field / index)

    int lineOf(Expr* e) const;
    int lineOf(Stmt* s) const;

    bool isReferenceType(const LatteType& t) const;                             // class or array
    bool isAssignable(const LatteType& dst, const LatteType& src) const;        // assignment rules
    bool isSubClassOf(const std::string& sub, const std::string& base) const;   // inheritance check

    std::optional<LatteType> lookupVarOrFieldType(const std::string& name) const;

    [[noreturn]] void fail(const std::string& msg, int line = 0) const
    {
        throw LatteError(msg, line); // central way to throw semantic errors
    }
};