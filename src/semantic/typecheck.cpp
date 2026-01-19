#include "typecheck.h"

#include <vector>
#include <string>
#include <cstddef>

// ---------------- ctor / entry ----------------

TypeChecker::TypeChecker()
{
    collectPredefinedFunctions();
}

void TypeChecker::checkProgram(Program* program)
{
    if (!program) fail("Empty program", 0);

    auto* prog = dynamic_cast<Prog*>(program);
    if (!prog) fail("Unexpected Program node", 0);

    // Pass 1: klasy (nagłówki)
    collectClassHeaders(program);

    // Pass 2: sygnatury funkcji + pól/metod w klasach
    collectSignatures(program);

    // Sprawdź main
    auto mainFun = env_.lookupFunction("main");
    if (!mainFun.has_value())
        fail("No function 'main' defined", 0);

    if (mainFun->result != LatteType::Int() || !mainFun->args.empty())
        fail("Function 'main' must have type 'int' and no parameters", 0);

    // Pass 3: typecheck ciał funkcji top-level
    for (TopDef* td : *prog->listtopdef_)
    {
        if (auto* fn = dynamic_cast<FnDef*>(td))
            checkTopLevelFunction(fn);
    }

    // Pass 4: typecheck ciał metod w klasach
    checkClassBodies(program);
}

// ---------------- builtins ----------------

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

// ---------------- passes: classes & signatures ----------------

void TypeChecker::collectClassHeaders(Program* program)
{
    auto* prog = dynamic_cast<Prog*>(program);
    if (!prog) fail("Unexpected Program node in collectClassHeaders", 0);

    for (TopDef* td : *prog->listtopdef_)
    {
        if (auto* c = dynamic_cast<ClassDef*>(td))
        {
            ClassInfo ci;
            ci.name = c->ident_;
            ci.base = std::nullopt;

            if (!env_.tryEnterClass(ci))
                fail("Duplicate definition of class '" + ci.name + "'", 0);
        }
        else if (auto* ce = dynamic_cast<ClassExt*>(td))
        {
            ClassInfo ci;
            ci.name = ce->ident_1;      // class X extends Y
            ci.base = ce->ident_2;

            if (!env_.tryEnterClass(ci))
                fail("Duplicate definition of class '" + ci.name + "'", 0);
        }
    }

    // weryfikacja "extends": baza musi istnieć (jeśli jest)
    for (TopDef* td : *prog->listtopdef_)
    {
        if (auto* ce = dynamic_cast<ClassExt*>(td))
        {
            auto baseName = ce->ident_2;
            if (!env_.lookupClass(baseName).has_value())
                fail("Class '" + std::string(ce->ident_1) + "' extends unknown class '" + baseName + "'", 0);
        }
    }
}

void TypeChecker::collectSignatures(Program* program)
{
    auto* prog = dynamic_cast<Prog*>(program);
    if (!prog) fail("Unexpected Program node in collectSignatures", 0);

    // 1) top-level function signatures
    for (TopDef* td : *prog->listtopdef_)
    {
        if (auto* fn = dynamic_cast<FnDef*>(td))
        {
            std::string name = fn->ident_;
            LatteType retType = typeFromAst(fn->type_);

            std::vector<LatteType> argTypes;
            if (fn->listarg_)
            {
                for (Arg* a : *fn->listarg_)
                {
                    auto* ar = dynamic_cast<Ar*>(a);
                    if (!ar) continue;
                    argTypes.push_back(typeFromAst(ar->type_));
                }
            }

            if (env_.lookupFunction(name).has_value())
                fail("Duplicate definition of function '" + name + "'", 0);

            FunInfo info;
            info.result = retType;
            info.args = std::move(argTypes);
            env_.enterFunction(name, info);
        }
    }

    // 2) class fields/method signatures
    // UWAGA: env_.lookupClass zwraca kopię, więc musimy pobrać, zmodyfikować, a potem "nadpisać" w env
    // Najprościej: użyj tryEnterClass na nagłówkach, a tu: odczyt, modyfikacja i ponowny insert nie zadziała.
    // Dlatego zakładam, że w Env::classes_ trzymasz wartości i lookupClass zwraca kopię.
    // W takim wypadku dodaj w Env metodę "updateClass" albo zwracaj referencję.
    //
    // Jeżeli NIE masz update/ref, to i tak możesz użyć env_.tryEnterClass ponownie nie zmieniając, ale to nie wprowadzi pól.
    //
    // -> Żeby Cię nie blokować, zrobię minimalną wersję:
    //    - signature-checking robię "na żywo" poprzez env_.lookupField/lookupMethod w hierarchii
    //    - a właściwe mapy fields/methods przechowuję w Env (wymaga, by Env miał możliwość modyfikacji).
    //
    // Jeśli masz Env jak w mojej propozycji, dopisz w nim:
    //   ClassInfo& getClassRef(const std::string& name);
    // i wtedy poniższy kod zadziała.
    //
    // Jeśli nie masz - wklej później, a teraz przejdź do typecheckowania bazowych funkcji.

    // --- wersja z getClassRef (zalecana) ---
    // Zakładam: Env::getClassRef(name) -> ClassInfo& (referencja do wpisu w mapie).
    // Jeśli nie masz, powiedz — dam Ci minimalny patch do Env.

    for (TopDef* td : *prog->listtopdef_)
    {
        if (auto* c = dynamic_cast<ClassDef*>(td))
        {
            auto& ci = env_.getClassRef(c->ident_);

            int fieldIdx = 0;
            int methodIdx = 0;

            for (Member* m : *c->listmember_)
            {
                if (auto* f = dynamic_cast<Field*>(m))
                {
                    std::string fname = f->ident_;
                    if (ci.fields.count(fname))
                        fail("Duplicate field '" + fname + "' in class '" + ci.name + "'", 0);

                    FieldInfo fi;
                    fi.type = typeFromAst(f->type_);
                    fi.index = fieldIdx++;
                    ci.fields.emplace(fname, fi);
                }
                else if (auto* mm = dynamic_cast<Method*>(m))
                {
                    std::string mname = mm->ident_;
                    if (ci.methods.count(mname))
                        fail("Duplicate method '" + mname + "' in class '" + ci.name + "'", 0);

                    MethodInfo mi;
                    mi.index = methodIdx++;

                    mi.sig.result = typeFromAst(mm->type_);
                    mi.sig.args.clear();
                    if (mm->listarg_)
                    {
                        for (Arg* a : *mm->listarg_)
                        {
                            auto* ar = dynamic_cast<Ar*>(a);
                            mi.sig.args.push_back(typeFromAst(ar->type_));
                        }
                    }

                    // objects1: bez override => jeśli baza ma metodę o tej samej nazwie -> błąd
                    if (ci.base && env_.lookupMethod(*ci.base, mname).has_value())
                        fail("Method '" + mname + "' in class '" + ci.name + "' overrides base method (not allowed in objects1)", 0);

                    ci.methods.emplace(mname, mi);
                }
            }
        }
        else if (auto* ce = dynamic_cast<ClassExt*>(td))
        {
            auto& ci = env_.getClassRef(ce->ident_1);
            ci.base = ce->ident_2;

            int fieldIdx = 0;
            int methodIdx = 0;

            for (Member* m : *ce->listmember_)
            {
                if (auto* f = dynamic_cast<Field*>(m))
                {
                    std::string fname = f->ident_;
                    if (ci.fields.count(fname))
                        fail("Duplicate field '" + fname + "' in class '" + ci.name + "'", 0);

                    FieldInfo fi;
                    fi.type = typeFromAst(f->type_);
                    fi.index = fieldIdx++;
                    ci.fields.emplace(fname, fi);
                }
                else if (auto* mm = dynamic_cast<Method*>(m))
                {
                    std::string mname = mm->ident_;
                    if (ci.methods.count(mname))
                        fail("Duplicate method '" + mname + "' in class '" + ci.name + "'", 0);

                    MethodInfo mi;
                    mi.index = methodIdx++;

                    mi.sig.result = typeFromAst(mm->type_);
                    mi.sig.args.clear();
                    if (mm->listarg_)
                    {
                        for (Arg* a : *mm->listarg_)
                        {
                            auto* ar = dynamic_cast<Ar*>(a);
                            mi.sig.args.push_back(typeFromAst(ar->type_));
                        }
                    }

                    if (ci.base && env_.lookupMethod(*ci.base, mname).has_value())
                        fail("Method '" + mname + "' in class '" + ci.name + "' overrides base method (not allowed in objects1)", 0);

                    ci.methods.emplace(mname, mi);
                }
            }
        }
    }
}

// ---------------- typecheck bodies ----------------

void TypeChecker::checkTopLevelFunction(FnDef* fn)
{
    std::string name = fn->ident_;
    LatteType retType = typeFromAst(fn->type_);

    currentClass_.reset();

    env_.pushScope();

    // args jako zmienne
    if (fn->listarg_)
    {
        for (Arg* a : *fn->listarg_)
        {
            auto* ar = dynamic_cast<Ar*>(a);
            if (!ar) continue;

            std::string argName = ar->ident_;
            LatteType argType = typeFromAst(ar->type_);

            if (env_.isVarDeclaredInCurrentScope(argName))
                fail("Duplicate parameter '" + argName + "' in function '" + name + "'", 0);

            env_.declareVar(argName, VarInfo{ argType });
        }
    }

    bool alwaysReturns = checkBlock(fn->block_, retType);

    env_.popScope();

    if (retType != LatteType::Void() && !alwaysReturns)
        fail("Function '" + name + "' may exit without returning a value", 0);
}

void TypeChecker::checkClassBodies(Program* program)
{
    auto* prog = dynamic_cast<Prog*>(program);
    if (!prog) fail("Unexpected Program node in checkClassBodies", 0);

    for (TopDef* td : *prog->listtopdef_)
    {
        if (auto* c = dynamic_cast<ClassDef*>(td))
        {
            std::string cname = c->ident_;
            for (Member* m : *c->listmember_)
            {
                if (auto* mm = dynamic_cast<Method*>(m))
                    checkMethodBody(cname, mm);
            }
        }
        else if (auto* ce = dynamic_cast<ClassExt*>(td))
        {
            std::string cname = ce->ident_1;
            for (Member* m : *ce->listmember_)
            {
                if (auto* mm = dynamic_cast<Method*>(m))
                    checkMethodBody(cname, mm);
            }
        }
    }
}

void TypeChecker::checkMethodBody(const std::string& className, Method* m)
{
    LatteType retType = typeFromAst(m->type_);
    currentClass_ = className;

    env_.pushScope();

    // implicit this
    env_.declareVar("this", VarInfo{ LatteType::Class(className) });

    // params
    if (m->listarg_)
    {
        for (Arg* a : *m->listarg_)
        {
            auto* ar = dynamic_cast<Ar*>(a);
            if (!ar) continue;

            std::string argName = ar->ident_;
            LatteType argType = typeFromAst(ar->type_);

            if (env_.isVarDeclaredInCurrentScope(argName))
                fail("Duplicate parameter '" + argName + "' in method '" + std::string(m->ident_) +
                     "' of class '" + className + "'", 0);

            env_.declareVar(argName, VarInfo{ argType });
        }
    }

    bool alwaysReturns = checkBlock(m->block_, retType);

    env_.popScope();

    if (retType != LatteType::Void() && !alwaysReturns)
        fail("Method '" + std::string(m->ident_) + "' of class '" + className + "' may exit without returning a value", 0);

    currentClass_.reset();
}

// ---------------- blocks / stmts ----------------

bool TypeChecker::checkBlock(Block* block, const LatteType& expectedReturn)
{
    auto* blk = dynamic_cast<Blk*>(block);
    if (!blk) fail("Unexpected Block node", 0);

    env_.pushScope();

    bool alwaysReturns = false;

    if (blk->liststmt_)
    {
        for (Stmt* s : *blk->liststmt_)
        {
            bool r = checkStmt(s, expectedReturn);
            if (!alwaysReturns && r) alwaysReturns = true;
        }
    }

    env_.popScope();
    return alwaysReturns;
}

static std::optional<bool> constBool(Expr* e)
{
    if (dynamic_cast<ELitTrue*>(e))  return true;
    if (dynamic_cast<ELitFalse*>(e)) return false;
    return std::nullopt;
}

bool TypeChecker::checkStmt(Stmt* stmt, const LatteType& expectedReturn)
{
    if (dynamic_cast<Empty*>(stmt)) return false;

    if (auto* s = dynamic_cast<BStmt*>(stmt))
        return checkBlock(s->block_, expectedReturn);

    if (auto* s = dynamic_cast<Decl*>(stmt))
    {
        LatteType t = typeFromAst(s->type_);
        if (t.kind == LatteTypeKind::Void)
            fail("Cannot declare variable of type void", 0);

        if (s->listitem_)
        {
            for (Item* it : *s->listitem_)
            {
                if (auto* ni = dynamic_cast<NoInit*>(it))
                {
                    std::string name = ni->ident_;
                    if (env_.isVarDeclaredInCurrentScope(name))
                        fail("Variable '" + name + "' already declared in this scope", 0);
                    env_.declareVar(name, VarInfo{ t });
                }
                else if (auto* ii = dynamic_cast<Init*>(it))
                {
                    std::string name = ii->ident_;
                    if (env_.isVarDeclaredInCurrentScope(name))
                        fail("Variable '" + name + "' already declared in this scope", 0);

                    LatteType eType = checkExpr(ii->expr_);
                    if (!isAssignable(t, eType))
                        fail("Type mismatch in initialization of '" + name + "'", 0);

                    env_.declareVar(name, VarInfo{ t });
                }
            }
        }
        return false;
    }

    if (auto* s = dynamic_cast<Ass*>(stmt))
    {
        LatteType lType = checkLVal(s->lval_);
        LatteType rType = checkExpr(s->expr_);
        if (!isAssignable(lType, rType))
            fail("Type mismatch in assignment", 0);
        return false;
    }

    if (auto* s = dynamic_cast<Incr*>(stmt))
    {
        LatteType lType = checkLVal(s->lval_);
        if (lType != LatteType::Int())
            fail("Increment '++' requires int l-value", 0);
        return false;
    }

    if (auto* s = dynamic_cast<Decr*>(stmt))
    {
        LatteType lType = checkLVal(s->lval_);
        if (lType != LatteType::Int())
            fail("Decrement '--' requires int l-value", 0);
        return false;
    }

    if (auto* s = dynamic_cast<VRet*>(stmt))
    {
        (void)s;
        if (expectedReturn != LatteType::Void())
            fail("Missing return value in non-void function", 0);
        return true;
    }

    if (auto* s = dynamic_cast<Ret*>(stmt))
    {
        if (expectedReturn == LatteType::Void())
            fail("Cannot return value from void function", 0);

        LatteType eType = checkExpr(s->expr_);
        if (!isAssignable(expectedReturn, eType))
            fail("Return expression has wrong type", 0);

        return true;
    }

    if (auto* s = dynamic_cast<Cond*>(stmt))
    {
        LatteType condType = checkExpr(s->expr_);
        if (condType != LatteType::Bool())
            fail("Condition in 'if' must be boolean", 0);
    
        if (auto cb = constBool(s->expr_))
        {
            if (*cb)  return checkStmt(s->stmt_, expectedReturn); // if(true)
            else { (void)checkStmt(s->stmt_, expectedReturn); return false; } // if(false)
        }
    
        (void)checkStmt(s->stmt_, expectedReturn);
        return false;
    }    

    if (auto* s = dynamic_cast<CondElse*>(stmt))
    {
        LatteType condType = checkExpr(s->expr_);
        if (condType != LatteType::Bool())
            fail("Condition in 'if-else' must be boolean", 0);
    
        if (auto cb = constBool(s->expr_))
        {
            if (*cb) { (void)checkStmt(s->stmt_2, expectedReturn); return checkStmt(s->stmt_1, expectedReturn); }
            else     { (void)checkStmt(s->stmt_1, expectedReturn); return checkStmt(s->stmt_2, expectedReturn); }
        }
    
        bool thenRet = checkStmt(s->stmt_1, expectedReturn);
        bool elseRet = checkStmt(s->stmt_2, expectedReturn);
        return thenRet && elseRet;
    }    

    if (auto* s = dynamic_cast<While*>(stmt))
    {
        LatteType condType = checkExpr(s->expr_);
        if (condType != LatteType::Bool())
            fail("Condition in 'while' must be boolean", 0);

        (void)checkStmt(s->stmt_, expectedReturn);
        return false;
    }

    if (auto* s = dynamic_cast<ForEach*>(stmt))
    {
        LatteType iterT = checkExpr(s->expr_);
        LatteType varT  = typeFromAst(s->type_);

        // expr musi byc tablica
        if (iterT.kind != LatteTypeKind::Array || !iterT.elem)
            fail("foreach expects array expression on the right side", 0);

        if (!(*iterT.elem == varT))
            fail("foreach variable type does not match array element type", 0);

        env_.pushScope();
        std::string varName = s->ident_;
        if (env_.isVarDeclaredInCurrentScope(varName))
            fail("Duplicate foreach variable '" + varName + "' in this scope", 0);
        env_.declareVar(varName, VarInfo{ varT });

        (void)checkStmt(s->stmt_, expectedReturn);

        env_.popScope();
        return false;
    }

    if (auto* s = dynamic_cast<SExp*>(stmt))
    {
        (void)checkExpr(s->expr_);
        return false;
    }

    fail("Unknown statement kind (not handled in typechecker)", 0);
}

// ---------------- expressions ----------------

LatteType TypeChecker::checkExpr(Expr* expr)
{
    if (!expr) return LatteType::Unknown();

    // (Expr)
    if (auto* e = dynamic_cast<EParen*>(expr))
        return checkExpr(e->expr_);

    // null
    if (dynamic_cast<ENull*>(expr))
        return LatteType::Null();

    // int / bool / string literals
    if (dynamic_cast<ELitInt*>(expr))
        return LatteType::Int();

    if (dynamic_cast<ELitTrue*>(expr) || dynamic_cast<ELitFalse*>(expr))
        return LatteType::Bool();

    if (dynamic_cast<EString*>(expr))
        return LatteType::String();

    // variable
    if (auto* e = dynamic_cast<EVar*>(expr))
    {
        auto t = lookupVarOrFieldType(e->ident_);
        if (!t.has_value())
            fail("Use of undeclared identifier '" + std::string(e->ident_) + "'", 0);
        return *t;
    }

    // function call: f(...)
    if (auto* e = dynamic_cast<EApp*>(expr))
    {
        std::string fname = e->ident_;
        auto finfo = env_.lookupFunction(fname);
        if (!finfo.has_value())
            fail("Call to unknown function '" + fname + "'", 0);

        std::vector<LatteType> actuals;
        if (e->listexpr_)
        {
            for (Expr* a : *e->listexpr_)
                actuals.push_back(checkExpr(a));
        }

        if (actuals.size() != finfo->args.size())
            fail("Wrong number of arguments in call to '" + fname + "'", 0);

        for (size_t i = 0; i < actuals.size(); ++i)
        {
            if (!isAssignable(finfo->args[i], actuals[i]))
                fail("Type mismatch in argument " + std::to_string(i + 1) +
                     " of call to '" + fname + "'", 0);
        }

        return finfo->result;
    }

    // --- Arrays ---

    // new BaseType[Expr]
    if (auto* e = dynamic_cast<ENewArr*>(expr))
    {
        LatteType sizeT = checkExpr(e->expr_);
        if (sizeT != LatteType::Int())
            fail("Array size must be int", 0);

        LatteType elemT = baseTypeFromAst(e->basetype_);
        if (elemT.kind == LatteTypeKind::Void)
            fail("void[] is not allowed", 0);

        return LatteType::Array(elemT);
    }

    // Expr6[Expr]
    if (auto* e = dynamic_cast<EIndex*>(expr))
    {
        LatteType arrT = checkExpr(e->expr_1);
        LatteType idxT = checkExpr(e->expr_2);

        if (idxT != LatteType::Int())
            fail("Array index must be int", 0);

        if (arrT.kind != LatteTypeKind::Array || !arrT.elem)
            fail("Indexing requires array type", 0);

        return *arrT.elem;
    }

    // --- Objects / Structs ---

    // new Ident
    if (auto* e = dynamic_cast<ENewObj*>(expr))
    {
        std::string cname = e->ident_;
        if (!env_.lookupClass(cname).has_value())
            fail("Unknown class type '" + cname + "'", 0);

        return LatteType::Class(cname);
    }

    // Expr6 . Ident  (field access)
    if (auto* e = dynamic_cast<EField*>(expr))
    {
        LatteType baseT = checkExpr(e->expr_);
        std::string field = e->ident_;
    
        // array.length
        if (baseT.kind == LatteTypeKind::Array)
        {
            if (field == "length")
                return LatteType::Int();
            fail("Unknown array field '" + field + "'", 0);
        }
    
        // class.field
        if (baseT.kind != LatteTypeKind::Class)
            fail("Field access requires class type", 0);
    
        auto fi = env_.lookupField(baseT.name, field);
        if (!fi.has_value())
            fail("Unknown field '" + field + "' in class '" + baseT.name + "'", 0);
    
        return fi->type;
    }    

    // Expr6 . Ident ( [Expr] )  (method call)
    if (auto* e = dynamic_cast<EMethod*>(expr))
    {
        LatteType objT = checkExpr(e->expr_);
        if (objT.kind != LatteTypeKind::Class)
            fail("Method call requires class type", 0);

        std::string mname = e->ident_;
        auto mi = env_.lookupMethod(objT.name, mname);
        if (!mi.has_value())
            fail("Unknown method '" + mname + "' in class '" + objT.name + "'", 0);

        std::vector<LatteType> actuals;
        if (e->listexpr_)
        {
            for (Expr* a : *e->listexpr_)
                actuals.push_back(checkExpr(a));
        }

        if (actuals.size() != mi->sig.args.size())
            fail("Wrong number of arguments in call to method '" + mname + "'", 0);

        for (size_t i = 0; i < actuals.size(); ++i)
        {
            if (!isAssignable(mi->sig.args[i], actuals[i]))
                fail("Type mismatch in argument " + std::to_string(i + 1) +
                     " of call to method '" + mname + "'", 0);
        }

        return mi->sig.result;
    }

    // --- Casts ( (Type) Expr6 ) ---
    // W Twojej gramatyce jest ECast, więc minimalnie obsłuż:
    // - cast do typu referencyjnego z null
    // - cast w dół/górę w hierarchii klas (jeśli chcesz dopuścić)
    // Jeśli nie chcesz wspierać castów teraz, lepiej dać czytelny błąd,
    // ale testy extensions mogą to wykorzystywać.
    if (auto* e = dynamic_cast<ECast*>(expr))
    {
        LatteType dst = typeFromAst(e->type_);
        LatteType src = checkExpr(e->expr_);

        // null -> dowolny ref
        if (src.kind == LatteTypeKind::Null && dst.isRef())
            return dst;

        // klasy: pozwalamy na cast w obrębie hierarchii (w górę i w dół),
        // ale tylko jeśli istnieje relacja dziedziczenia w którąś stronę.
        if (dst.kind == LatteTypeKind::Class && src.kind == LatteTypeKind::Class)
        {
            if (isSubClassOf(src.name, dst.name) || isSubClassOf(dst.name, src.name))
                return dst;
            fail("Invalid cast between unrelated class types", 0);
        }

        // tablice: dopuszczamy tylko identyczny typ (bez kowariancji)
        if (dst.kind == LatteTypeKind::Array && src.kind == LatteTypeKind::Array)
        {
            if (dst == src) return dst;
            fail("Invalid cast between different array types", 0);
        }

        // prymitywy: tylko identyczny typ
        if (dst == src) return dst;

        fail("Invalid cast", 0);
    }

    // --- Boolean / arithmetic / relational ops ---

    // EOr
    if (auto* e = dynamic_cast<EOr*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);
        if (t1 != LatteType::Bool() || t2 != LatteType::Bool())
            fail("Operator '||' expects boolean operands", 0);
        return LatteType::Bool();
    }

    // EAnd
    if (auto* e = dynamic_cast<EAnd*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);
        if (t1 != LatteType::Bool() || t2 != LatteType::Bool())
            fail("Operator '&&' expects boolean operands", 0);
        return LatteType::Bool();
    }

    // ERel
    if (auto* e = dynamic_cast<ERel*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);
        RelOp* op = e->relop_;

        if (dynamic_cast<EQU*>(op) || dynamic_cast<NE*>(op))
        {
            // == / != : identyczne typy, albo null vs ref
            if (t1 == t2) return LatteType::Bool();
            if ((t1.kind == LatteTypeKind::Null && t2.isRef()) ||
                (t2.kind == LatteTypeKind::Null && t1.isRef()))
                return LatteType::Bool();

            fail("Operator '=='/'!=' requires same types or null vs reference", 0);
        }
        else
        {
            if (t1 != LatteType::Int() || t2 != LatteType::Int())
                fail("Relational operator requires integer operands", 0);
            return LatteType::Bool();
        }
    }

    // EAdd
    if (auto* e = dynamic_cast<EAdd*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);
        AddOp* op = e->addop_;

        if (dynamic_cast<Plus*>(op))
        {
            if (t1 == LatteType::Int() && t2 == LatteType::Int())
                return LatteType::Int();
            if (t1 == LatteType::String() && t2 == LatteType::String())
                return LatteType::String();

            fail("Operator '+' supports int+int or string+string only", 0);
        }
        if (dynamic_cast<Minus*>(op))
        {
            if (t1 == LatteType::Int() && t2 == LatteType::Int())
                return LatteType::Int();
            fail("Operator '-' supports int-int only", 0);
        }
    }

    // EMul
    if (auto* e = dynamic_cast<EMul*>(expr))
    {
        LatteType t1 = checkExpr(e->expr_1);
        LatteType t2 = checkExpr(e->expr_2);
        if (t1 != LatteType::Int() || t2 != LatteType::Int())
            fail("Operator '*', '/' or '%' requires int operands", 0);
        return LatteType::Int();
    }

    // Neg / Not
    if (auto* e = dynamic_cast<Neg*>(expr))
    {
        LatteType t = checkExpr(e->expr_);
        if (t != LatteType::Int())
            fail("Unary '-' expects int operand", 0);
        return LatteType::Int();
    }

    if (auto* e = dynamic_cast<Not*>(expr))
    {
        LatteType t = checkExpr(e->expr_);
        if (t != LatteType::Bool())
            fail("Logical '!' expects boolean operand", 0);
        return LatteType::Bool();
    }

    fail("Unknown expression kind (not handled in typechecker)", 0);
}

// ---------------- LVal ----------------

LatteType TypeChecker::checkLVal(LVal* lv)
{
    if (auto* v = dynamic_cast<LVar*>(lv))
    {
        std::string name = v->ident_;

        // w metodzie: ident może być polem
        auto t = lookupVarOrFieldType(name);
        if (!t.has_value())
            fail("Assignment to undeclared identifier '" + name + "'", 0);

        return *t;
    }

    if (auto* f = dynamic_cast<LField*>(lv))
    {
        LatteType objT = checkLVal(f->lval_); // Expr6 "." Ident
        if (objT.kind != LatteTypeKind::Class)
            fail("Field l-value requires class type", 0);

        auto fi = env_.lookupField(objT.name, f->ident_);
        if (!fi.has_value())
            fail("Unknown field '" + std::string(f->ident_) + "' in class '" + objT.name + "'", 0);

        return fi->type;
    }

    if (auto* idx = dynamic_cast<LIndex*>(lv))
    {
        LatteType arrT = checkLVal(idx->lval_);
        LatteType iT   = checkExpr(idx->expr_);

        if (iT != LatteType::Int())
            fail("Array index must be int", 0);

        if (arrT.kind != LatteTypeKind::Array || !arrT.elem)
            fail("Indexing l-value requires array type", 0);

        return *arrT.elem;
    }

    fail("Unknown LVal kind", 0);
}

// ---------------- types ----------------

LatteType TypeChecker::typeFromAst(Type* ty)
{
    if (!ty) return LatteType::Unknown();

    if (auto* t = dynamic_cast<TBase*>(ty))
        return baseTypeFromAst(t->basetype_);

    if (auto* t = dynamic_cast<TArr*>(ty))
    {
        LatteType bt = baseTypeFromAst(t->basetype_);
        if (bt.kind == LatteTypeKind::Void)
            fail("void[] is not allowed", 0);
        return LatteType::Array(bt);
    }

    if (dynamic_cast<Fun*>(ty))
        fail("Function types are not supported in this frontend", 0);

    return LatteType::Unknown();
}

LatteType TypeChecker::baseTypeFromAst(BaseType* bt)
{
    if (!bt) return LatteType::Unknown();

    if (dynamic_cast<Int*>(bt))  return LatteType::Int();
    if (dynamic_cast<Bool*>(bt)) return LatteType::Bool();
    if (dynamic_cast<Str*>(bt))  return LatteType::String();
    if (dynamic_cast<Void*>(bt)) return LatteType::Void();

    if (auto* c = dynamic_cast<ClassT*>(bt))
    {
        std::string cname = c->ident_;
        if (!env_.lookupClass(cname).has_value())
            fail("Unknown class type '" + cname + "'", 0);
        return LatteType::Class(cname);
    }

    return LatteType::Unknown();
}

// ---------------- assignability / subtyping ----------------

bool TypeChecker::isSubClassOf(const std::string& sub, const std::string& base) const
{
    if (sub == base) return true;

    auto cur = env_.lookupClass(sub);
    while (cur.has_value() && cur->base.has_value())
    {
        if (*cur->base == base) return true;
        cur = env_.lookupClass(*cur->base);
    }
    return false;
}

bool TypeChecker::isAssignable(const LatteType& dst, const LatteType& src) const
{
    if (dst == src) return true;

    // null -> dowolny ref
    if (src.kind == LatteTypeKind::Null && dst.isRef())
        return true;

    // klasy: src może być podtypem dst
    if (dst.kind == LatteTypeKind::Class && src.kind == LatteTypeKind::Class)
        return isSubClassOf(src.name, dst.name);

    // tablice: na razie tylko dokładnie taki sam typ
    if (dst.kind == LatteTypeKind::Array && src.kind == LatteTypeKind::Array)
        return dst == src;

    return false;
}

// ---------------- method context name resolution ----------------

std::optional<LatteType> TypeChecker::lookupVarOrFieldType(const std::string& name) const
{
    if (auto vi = env_.lookupVar(name))
        return vi->type;

    if (currentClass_.has_value())
    {
        auto fi = env_.lookupField(*currentClass_, name);
        if (fi.has_value())
            return fi->type;
    }

    return std::nullopt;
}
