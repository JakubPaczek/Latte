#include "typecheck.hpp"

// BNFC AST
#include "frontend/Absyn.H"

TypeChecker::TypeChecker()
{
    collectPredefinedFunctions();
}

void TypeChecker::checkProgram(Program* program)
{
    if (!program)
    {
        throw LatteError("Empty program", 0);
    }

    Prog* prog = dynamic_cast<Prog*>(program);
    if (!prog)
    {
        throw LatteError("Unexpected Program node", 0);
    }

    // 1. collect function signatures (including main)
    collectFunctionSignatures(program);

    // 2. check for correct main
    auto mainFun = env_.lookupFunction("main");
    if (!mainFun.has_value())
    {
        throw LatteError("No function 'main' defined", 0);
    }
    if (mainFun->result != LatteType::Int() || !mainFun->args.empty())
    {
        throw LatteError("Function 'main' must have type 'int' and no parameters", 0);
    }

    // 3. check every function correctness
    ListTopDef* defs = prog->listtopdef_;
    for (TopDef* td : *defs)
    {
        FnDef* fn = dynamic_cast<FnDef*>(td);
        if (!fn)
        {
            continue;
        }
        checkFunction(fn);
    }
}

void TypeChecker::collectPredefinedFunctions()
{
    {
        FunInfo f;
        f.result = LatteType::Void();
        f.args = { LatteType::Int() };
        env_.enterFunction("printInt", f);
    }

    {
        FunInfo f;
        f.result = LatteType::Void();
        f.args = { LatteType::String() };
        env_.enterFunction("printString", f);
    }

    {
        FunInfo f;
        f.result = LatteType::Void();
        f.args = {};
        env_.enterFunction("error", f);
    }

    {
        FunInfo f;
        f.result = LatteType::Int();
        f.args = {};
        env_.enterFunction("readInt", f);
    }

    {
        FunInfo f;
        f.result = LatteType::String();
        f.args = {};
        env_.enterFunction("readString", f);
    }
}

void TypeChecker::collectFunctionSignatures(Program* program)
{
    Prog* prog = dynamic_cast<Prog*>(program);
    if (!prog)
    {
        throw LatteError("Unexpected Program node in collectFunctionSignatures", 0);
    }

    ListTopDef* defs = prog->listtopdef_;
    for (TopDef* td : *defs)
    {
        FnDef* fn = dynamic_cast<FnDef*>(td);
        if (!fn)
        {
            continue;
        }

        std::string name = fn->ident_;

        LatteType retType = typeFromAst(fn->type_);

        std::vector<LatteType> argTypes;
        ListArg* args = fn->listarg_;
        if (args)
        {
            for (Arg* a : *args)
            {
                Ar* ar = dynamic_cast<Ar*>(a);
                if (!ar) continue;
                LatteType t = typeFromAst(ar->type_);
                argTypes.push_back(t);
            }
        }

        if (env_.lookupFunction(name).has_value())
        {
            throw LatteError("Duplicate definition of function '" + name + "'", 0);
        }

        FunInfo info;
        info.result = retType;
        info.args = argTypes;
        env_.enterFunction(name, info);
    }
}

void TypeChecker::checkFunction(FnDef* fn)
{
    std::string name = fn->ident_;
    LatteType retType = typeFromAst(fn->type_);

    env_.pushScope();

    // function args as variables in highest scope
    ListArg* args = fn->listarg_;
    if (args)
    {
        for (Arg* a : *args)
        {
            Ar* ar = dynamic_cast<Ar*>(a);
            if (!ar) continue;

            std::string argName = ar->ident_;
            LatteType argType = typeFromAst(ar->type_);

            if (env_.isVarDeclaredInCurrentScope(argName))
            {
                throw LatteError("Duplicate parameter '" + argName + "' in function '" + name + "'", 0);
            }
            env_.declareVar(argName, VarInfo{ argType });
        }
    }

    // function body + check if every path has return
    bool alwaysReturns = checkBlock(fn->block_, retType);

    env_.popScope();

    if (retType != LatteType::Void() && !alwaysReturns)
    {
        throw LatteError("Function '" + name + "' may exit without returning a value", 0);
    }
}

// check if this block gurantess return
bool TypeChecker::checkBlock(Block* block, LatteType expectedReturn)
{
    Blk* blk = dynamic_cast<Blk*>(block);
    if (!blk)
    {
        throw LatteError("Unexpected Block node", 0);
    }

    env_.pushScope();

    bool alwaysReturns = false;

    ListStmt* stmts = blk->liststmt_;
    if (stmts)
    {
        for (Stmt* s : *stmts)
        {
            bool r = checkStmt(s, expectedReturn);

            // only first return matters for always return
            if (!alwaysReturns && r)
            {
                alwaysReturns = true;
            }
        }
    }

    env_.popScope();
    return alwaysReturns;
}

// check if this statement gurantess return
bool TypeChecker::checkStmt(Stmt* stmt, LatteType expectedReturn)
{
    if (dynamic_cast<Empty*>(stmt))
    {
        return false;
    }

    if (BStmt* s = dynamic_cast<BStmt*>(stmt))
    {
        return checkBlock(s->block_, expectedReturn);
    }

    if (Decl* s = dynamic_cast<Decl*>(stmt))
    {
        LatteType t = typeFromAst(s->type_);
        ListItem* items = s->listitem_;

        if (items)
        {
            for (Item* it : *items)
            {
                if (NoInit* ni = dynamic_cast<NoInit*>(it))
                {
                    std::string name = ni->ident_;
                    // only in current scope
                    if (env_.isVarDeclaredInCurrentScope(name))
                    {
                        throw LatteError("Variable '" + name + "' already declared in this scope", 0);
                    }
                    env_.declareVar(name, VarInfo{ t });
                }
                else if (Init* ii = dynamic_cast<Init*>(it))
                {
                    std::string name = ii->ident_;
                    if (env_.isVarDeclaredInCurrentScope(name))
                    {
                        throw LatteError("Variable '" + name + "' already declared in this scope", 0);
                    }

                    LatteType eType = checkExpr(ii->expr_);
                    if (eType != t)
                    {
                        throw LatteError("Type mismatch in initialization of '" + name + "'", 0);
                    }
                    env_.declareVar(name, VarInfo{ t });
                }
            }
        }
        return false;
    }


    if (Ass* s = dynamic_cast<Ass*>(stmt))
    {
        std::string name = s->ident_;
        auto varInfo = env_.lookupVar(name);
        if (!varInfo.has_value())
        {
            throw LatteError("Assignment to undeclared variable '" + name + "'", 0);
        }
        LatteType eType = checkExpr(s->expr_);
        if (eType != varInfo->type)
        {
            throw LatteError("Type mismatch in assignment to '" + name + "'", 0);
        }
        return false;
    }

    if (Incr* s = dynamic_cast<Incr*>(stmt))
    {
        std::string name = s->ident_;
        auto varInfo = env_.lookupVar(name);
        if (!varInfo.has_value())
        {
            throw LatteError("Increment of undeclared variable '" + name + "'", 0);
        }
        if (varInfo->type != LatteType::Int())
        {
            throw LatteError("Increment '++' requires int variable", 0);
        }
        return false;
    }

    if (Decr* s = dynamic_cast<Decr*>(stmt))
    {
        std::string name = s->ident_;
        auto varInfo = env_.lookupVar(name);
        if (!varInfo.has_value())
        {
            throw LatteError("Decrement of undeclared variable '" + name + "'", 0);
        }
        if (varInfo->type != LatteType::Int())
        {
            throw LatteError("Decrement '--' requires int variable", 0);
        }
        return false;
    }

    if (VRet* s = dynamic_cast<VRet*>(stmt))
    {
        (void)s;
        if (expectedReturn != LatteType::Void())
        {
            throw LatteError("Missing return value in non-void function", 0);
        }
        return true;
    }

    if (Ret* s = dynamic_cast<Ret*>(stmt))
    {
        if (expectedReturn == LatteType::Void())
        {
            throw LatteError("Cannot return value from void function", 0);
        }
        LatteType eType = checkExpr(s->expr_);
        if (eType != expectedReturn)
        {
            throw LatteError("Return expression has wrong type", 0);
        }
        return true;
    }

    if (Cond* s = dynamic_cast<Cond*>(stmt))
    {
        LatteType condType = checkExpr(s->expr_);
        if (condType != LatteType::Bool())
        {
            throw LatteError("Condition in 'if' must be boolean", 0);
        }

        Expr* cond = s->expr_;

        // if (true) S;
        if (dynamic_cast<ELitTrue*>(cond))
        {
            return checkStmt(s->stmt_, expectedReturn);
        }

        // if (false) S;
        if (dynamic_cast<ELitFalse*>(cond))
        {
            // block will never exectue but it's still typechecked
            (void)checkStmt(s->stmt_, expectedReturn);
            return false;
        }

        (void)checkStmt(s->stmt_, expectedReturn);
        return false;
    }

    if (CondElse* s = dynamic_cast<CondElse*>(stmt))
    {
        LatteType condType = checkExpr(s->expr_);
        if (condType != LatteType::Bool())
        {
            throw LatteError("Condition in 'if-else' must be boolean", 0);
        }

        Expr* cond = s->expr_;

        // if (true) S1 else S2;
        if (dynamic_cast<ELitTrue*>(cond))
        {
            bool thenRet = checkStmt(s->stmt_1, expectedReturn);
            (void)checkStmt(s->stmt_2, expectedReturn);
            return thenRet;
        }

        // if (false) S1 else S2;
        if (dynamic_cast<ELitFalse*>(cond))
        {
            (void)checkStmt(s->stmt_1, expectedReturn);
            bool elseRet = checkStmt(s->stmt_2, expectedReturn);
            return elseRet;
        }

        bool thenRet = checkStmt(s->stmt_1, expectedReturn);
        bool elseRet = checkStmt(s->stmt_2, expectedReturn);
        return thenRet && elseRet;
    }

    if (While* s = dynamic_cast<While*>(stmt))
    {
        LatteType condType = checkExpr(s->expr_);
        if (condType != LatteType::Bool())
        {
            throw LatteError("Condition in 'while' must be boolean", 0);
        }

        (void)checkStmt(s->stmt_, expectedReturn);
        return false;
    }

    if (SExp* s = dynamic_cast<SExp*>(stmt))
    {
        (void)checkExpr(s->expr_);
        return false;
    }

    throw LatteError("Unknown statement kind (not handled in typechecker)", 0);
}


LatteType TypeChecker::checkExpr(Expr* expr)
{
    if (!expr)
    {
        return LatteType::Unknown();
    }

    // EOr
    if (EOr* e = dynamic_cast<EOr*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);
        if (t1 != LatteType::Bool() || t2 != LatteType::Bool())
        {
            throw LatteError("Operator '||' expects boolean operands", 0);
        }
        return LatteType::Bool();
    }

    // EAnd
    if (EAnd* e = dynamic_cast<EAnd*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);
        if (t1 != LatteType::Bool() || t2 != LatteType::Bool())
        {
            throw LatteError("Operator '&&' expects boolean operands", 0);
        }
        return LatteType::Bool();
    }

    // ERel
    if (ERel* e = dynamic_cast<ERel*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);

        RelOp* op = e->relop_;

        if (dynamic_cast<EQU*>(op) || dynamic_cast<NE*>(op))
        {
            if (t1 != t2)
            {
                throw LatteError("Operator '=='/'!=' requires same types on both sides", 0);
            }
        }
        else
        {
            if (t1 != LatteType::Int() || t2 != LatteType::Int())
            {
                throw LatteError("Relational operator requires integer operands", 0);
            }
        }
        return LatteType::Bool();
    }

    // EAdd
    if (EAdd* e = dynamic_cast<EAdd*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);
        AddOp* op = e->addop_;

        if (dynamic_cast<Plus*>(op))
        {
            if (t1 == LatteType::Int() && t2 == LatteType::Int())
            {
                return LatteType::Int();
            }
            if (t1 == LatteType::String() && t2 == LatteType::String())
            {
                return LatteType::String();
            }
            throw LatteError("Operator '+' supports int+int or string+string only", 0);
        }
        if (dynamic_cast<Minus*>(op))
        {
            if (t1 == LatteType::Int() && t2 == LatteType::Int())
            {
                return LatteType::Int();
            }
            throw LatteError("Operator '-' supports int-int only", 0);
        }
    }

    // EMul
    if (EMul* e = dynamic_cast<EMul*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);
        if (t1 != LatteType::Int() || t2 != LatteType::Int())
        {
            throw LatteError("Operator '*', '/' or '%' requires int operands", 0);
        }
        return LatteType::Int();
    }

    // Neg
    if (Neg* e = dynamic_cast<Neg*>(expr))
    {
        LatteType t = checkExpr(e->expr_);
        if (t != LatteType::Int())
        {
            throw LatteError("Unary '-' expects int operand", 0);
        }
        return LatteType::Int();
    }

    // Not
    if (Not* e = dynamic_cast<Not*>(expr))
    {
        LatteType t = checkExpr(e->expr_);
        if (t != LatteType::Bool())
        {
            throw LatteError("Logical '!' expects boolean operand", 0);
        }
        return LatteType::Bool();
    }

    // Zmienne / literały / wywołania / string

    if (EVar* e = dynamic_cast<EVar*>(expr))
    {
        std::string name = e->ident_;
        auto varInfo = env_.lookupVar(name);
        if (!varInfo.has_value())
        {
            throw LatteError("Use of undeclared variable '" + name + "'", 0);
        }
        return varInfo->type;
    }

    if (ELitInt* e = dynamic_cast<ELitInt*>(expr))
    {
        (void)e;
        return LatteType::Int();
    }

    if (ELitTrue* e = dynamic_cast<ELitTrue*>(expr))
    {
        (void)e;
        return LatteType::Bool();
    }

    if (ELitFalse* e = dynamic_cast<ELitFalse*>(expr))
    {
        (void)e;
        return LatteType::Bool();
    }

    if (EString* e = dynamic_cast<EString*>(expr))
    {
        (void)e;
        return LatteType::String();
    }

    if (EApp* e = dynamic_cast<EApp*>(expr))
    {
        std::string fname = e->ident_;
        auto finfo = env_.lookupFunction(fname);
        if (!finfo.has_value())
        {
            throw LatteError("Call to undefined function '" + fname + "'", 0);
        }

        ListExpr* args = e->listexpr_;
        std::vector<LatteType> callArgTypes;
        if (args)
        {
            for (Expr* a : *args)
            {
                callArgTypes.push_back(checkExpr(a));
            }
        }

        if (callArgTypes.size() != finfo->args.size())
        {
            throw LatteError("Function '" + fname + "' called with wrong number of arguments", 0);
        }

        for (std::size_t i = 0; i < callArgTypes.size(); ++i)
        {
            if (callArgTypes[i] != finfo->args[i])
            {
                throw LatteError(
                    "Function '" + fname + "': argument " +
                    std::to_string(i + 1) + " has wrong type", 0);
            }
        }

        return finfo->result;
    }

    throw LatteError("Unknown expression kind (not handled in typechecker)", 0);
}

LatteType TypeChecker::typeFromAst(Type* ty)
{
    if (!ty)
    {
        return LatteType::Unknown();
    }

    if (dynamic_cast<Int*>(ty))
    {
        return LatteType::Int();
    }
    if (dynamic_cast<Bool*>(ty))
    {
        return LatteType::Bool();
    }
    if (dynamic_cast<Str*>(ty))
    {
        return LatteType::String();
    }
    if (dynamic_cast<Void*>(ty))
    {
        return LatteType::Void();
    }

    if (dynamic_cast<Fun*>(ty))
    {
        throw LatteError("Function types are not supported in this frontend", 0);
    }

    return LatteType::Unknown();
}
