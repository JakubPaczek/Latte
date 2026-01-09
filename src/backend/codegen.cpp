#include "backend/codegen.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <utility>
#include <cctype>

// --- UWAGA praktyczna ---
// BNFC zwykle generuje klasy o nazwach zgodnych z etykietami z gramatyki:
// Prog, FnDef, Blk, Empty, BStmt, Decl, Ass, Incr, Decr, Ret, VRet, Cond, CondElse, While, SExp
// EVar, ELitInt, ELitTrue, ELitFalse, EApp, EString, Neg, Not, EMul, EAdd, ERel, EAnd, EOr
// Plus/Minus, Times/Div/Mod, LTH/LE/GTH/GE/EQU/NE
//
// Jeśli u Ciebie BNFC nazwał pola minimalnie inaczej (np. stmt_1 vs stmt_2),
// to poprawisz 2-3 identyfikatory – logika zostaje.

namespace {

    enum class Ty { Int, Bool, Str, Void };

    struct FuncSig {
        Ty ret;
        std::vector<Ty> args;
    };

    struct Val {
        VReg v;
        Ty  t;
    };

    struct VarInfo {
        VReg v;
        Ty  t;
    };

    static Ty tyFromAst(Type* t)
    {
        if (dynamic_cast<Int*>(t))  return Ty::Int;
        if (dynamic_cast<Bool*>(t)) return Ty::Bool;
        if (dynamic_cast<Str*>(t))  return Ty::Str;
        if (dynamic_cast<Void*>(t)) return Ty::Void;
        throw std::runtime_error("Unknown Type node");
    }

    static Ar* asAr(Arg* a)
    {
        auto* ar = dynamic_cast<Ar*>(a);
        if (!ar) throw std::runtime_error("Unexpected Arg node (expected Ar)");
        return ar;
    }

    static std::string unescapeBnfcString(std::string s)
    {
        // obsłuż dwa warianty:
        // 1) BNFC daje z cudzysłowami: "abc\n"
        // 2) BNFC daje już bez cudzysłowów: abc\n
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        {
            s = s.substr(1, s.size() - 2);
        }

        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i)
        {
            char c = s[i];
            if (c != '\\') { out.push_back(c); continue; }
            if (i + 1 >= s.size()) break;
            char n = s[++i];
            switch (n)
            {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case '\\': out.push_back('\\'); break;
            case '"': out.push_back('"'); break;
            default: out.push_back(n); break;
            }
        }
        return out;
    }

    struct CGCtx {
        ModuleIR mod;
        std::unordered_map<std::string, FuncSig> sigs;

        // deduplikacja stringów
        std::unordered_map<std::string, int> strId;

        // globalnie unikalne label-id (żeby .L<num> nie kolidowały między funkcjami)
        int nextLabelId = 0;

        Label newLabel() { return Label(nextLabelId++); }

        int internString(const std::string& raw)
        {
            auto it = strId.find(raw);
            if (it != strId.end()) return it->second;
            int id = (int)mod.stringLits.size();
            mod.stringLits.push_back(raw);
            strId.emplace(raw, id);
            return id;
        }

        std::string strGetterName(int id) const
        {
            return "__latte_str_" + std::to_string(id);
        }
    };

    struct FnCG {
        CGCtx& g;
        FunctionIR f;

        // scope stack
        std::vector<std::unordered_map<std::string, VarInfo>> scopes;

        // blocks builder
        int curBlock = -1;
        bool curTerminated = false;

        explicit FnCG(CGCtx& gg) : g(gg) {}

        void pushScope() { scopes.emplace_back(); }
        void popScope() { scopes.pop_back(); }

        VType vtypeFromTy(Ty t) const
        {
            switch (t)
            {
            case Ty::Str:  return VType::PTR;
            case Ty::Int:  return VType::I32;
            case Ty::Bool: return VType::I32;
            case Ty::Void: return VType::I32; // dummy, shouldn't really be used
            }
            return VType::I32;
        }

        VReg newTmp(Ty t) { return f.newVReg(vtypeFromTy(t)); }

        void defineVar(const std::string& name, VarInfo vi)
        {
            if (scopes.empty()) pushScope();
            auto& top = scopes.back();
            top.emplace(name, vi);
        }

        VarInfo lookupVar(const std::string& name) const
        {
            for (int i = (int)scopes.size() - 1; i >= 0; --i)
            {
                auto it = scopes[i].find(name);
                if (it != scopes[i].end()) return it->second;
            }
            throw std::runtime_error("Undefined variable in codegen: " + name);
        }

        void startNewBlock(Label L)
        {
            BasicBlock b;
            b.label = L;
            b.ins.clear();
            b.succ.clear();
            f.blocks.push_back(std::move(b));
            curBlock = (int)f.blocks.size() - 1;
            curTerminated = false;
        }

        void emit(const Instr& i)
        {
            if (curBlock < 0) throw std::runtime_error("emit(): no current block");
            if (curTerminated) return; // ignoruj unreachable
            f.blocks[curBlock].ins.push_back(i);
            auto k = i.k;
            if (k == Instr::Kind::Ret || k == Instr::Kind::Jmp ||
                k == Instr::Kind::JmpIfZero || k == Instr::Kind::JmpIfNonZero)
            {
                // UWAGA: warunkowe skoki traktujemy jako terminatory bloków,
                // a "prawdziwy" CFG robimy jako (cond + jmp) w 2 instrukcjach.
                curTerminated = (k != Instr::Kind::JmpIfZero && k != Instr::Kind::JmpIfNonZero);
            }
        }

        void emitJmp(Label L) { emit(Instr::jmp(L)); curTerminated = true; }
        void emitJz(VReg c, Label L) { emit(Instr::jz(c, L)); /* nie ustawiamy terminated, bo zaraz dodamy jmp */ }
        void emitJnz(VReg c, Label L) { emit(Instr::jnz(c, L)); }

        VReg makeConst(long v)
        {
            VReg r = f.newVReg(VType::I32);
            emit(Instr::loadImm(r, v));
            return r;
        }

        Val genExpr(Expr* e)
        {
            if (!e) throw std::runtime_error("genExpr: nullptr");

            if (auto* n = dynamic_cast<ELitInt*>(e))
            {
                VReg r = f.newVReg();
                emit(Instr::loadImm(r, (long)n->integer_));
                return { r, Ty::Int };
            }
            if (dynamic_cast<ELitTrue*>(e))
            {
                return { makeConst(1), Ty::Bool };
            }
            if (dynamic_cast<ELitFalse*>(e))
            {
                return { makeConst(0), Ty::Bool };
            }
            if (auto* v = dynamic_cast<EVar*>(e))
            {
                VarInfo vi = lookupVar(v->ident_);
                return { vi.v, vi.t };
            }
            if (auto* s = dynamic_cast<EString*>(e))
            {
                std::string raw = unescapeBnfcString(s->string_);
                int id = g.internString(raw);
                VReg dst = f.newVReg();
                // string literal jako: call __latte_str_ID()
                emit(Instr::call(dst, g.strGetterName(id), {}));
                return { dst, Ty::Str };
            }
            if (auto* app = dynamic_cast<EApp*>(e))
            {
                std::string name = app->ident_;
                auto it = g.sigs.find(name);
                if (it == g.sigs.end()) throw std::runtime_error("Unknown function in codegen: " + name);

                std::vector<VReg> args;
                args.reserve(app->listexpr_ ? app->listexpr_->size() : 0);

                if (app->listexpr_)
                {
                    for (Expr* a : *app->listexpr_)
                    {
                        Val av = genExpr(a);
                        args.push_back(av.v);
                    }
                }

                Ty rt = it->second.ret;
                if (rt == Ty::Void)
                {
                    emit(Instr::call(std::nullopt, name, std::move(args)));
                    // void expression w Latte występuje tylko jako SExp, więc tu zwracamy dummy
                    return { makeConst(0), Ty::Int };
                }
                else
                {
                    VReg dst = f.newVReg();
                    emit(Instr::call(dst, name, std::move(args)));
                    return { dst, rt };
                }
            }
            if (auto* neg = dynamic_cast<Neg*>(e))
            {
                Val x = genExpr((Expr*)neg->expr_);
                VReg dst = f.newVReg();
                emit(Instr::un(dst, UnOp::Neg, x.v));
                return { dst, Ty::Int };
            }
            if (auto* nt = dynamic_cast<Not*>(e))
            {
                Val x = genExpr((Expr*)nt->expr_);
                VReg dst = f.newVReg();
                emit(Instr::un(dst, UnOp::Not, x.v));
                return { dst, Ty::Bool };
            }
            if (auto* mul = dynamic_cast<EMul*>(e))
            {
                Val a = genExpr((Expr*)mul->expr_1);
                Val b = genExpr((Expr*)mul->expr_2);

                // Times/Div/Mod
                if (dynamic_cast<Times*>(mul->mulop_))
                {
                    VReg dst = f.newVReg();
                    emit(Instr::bin(dst, a.v, BinOp::Mul, b.v));
                    return { dst, Ty::Int };
                }
                if (dynamic_cast<Div*>(mul->mulop_))
                {
                    VReg dst = f.newVReg(VType::I32);
                    emit(Instr::bin(dst, a.v, BinOp::Div, b.v));
                    return { dst, Ty::Int };
                }
                if (dynamic_cast<Mod*>(mul->mulop_))
                {
                    VReg dst = f.newVReg(VType::I32);
                    emit(Instr::bin(dst, a.v, BinOp::Mod, b.v));
                    return { dst, Ty::Int };
                }
                throw std::runtime_error("Unknown MulOp in codegen");
            }
            if (auto* add = dynamic_cast<EAdd*>(e))
            {
                Val a = genExpr((Expr*)add->expr_1);
                Val b = genExpr((Expr*)add->expr_2);

                if (dynamic_cast<Minus*>(add->addop_))
                {
                    VReg dst = f.newVReg();
                    emit(Instr::bin(dst, a.v, BinOp::Sub, b.v));
                    return { dst, Ty::Int };
                }
                if (dynamic_cast<Plus*>(add->addop_))
                {
                    // int+int albo string+string
                    if (a.t == Ty::Str || b.t == Ty::Str)
                    {
                        // Latte: konkatenacja stringów operatorem +
                        VReg dst = f.newVReg();
                        emit(Instr::call(dst, "__latte_concat", { a.v, b.v }));
                        return { dst, Ty::Str };
                    }
                    else
                    {
                        VReg dst = f.newVReg();
                        emit(Instr::bin(dst, a.v, BinOp::Add, b.v));
                        return { dst, Ty::Int };
                    }
                }
                throw std::runtime_error("Unknown AddOp in codegen");
            }
            if (auto* rel = dynamic_cast<ERel*>(e))
            {
                Val a = genExpr((Expr*)rel->expr_1);
                Val b = genExpr((Expr*)rel->expr_2);

                // EQU/NE dla stringów robimy przez strcmp == 0
                if ((dynamic_cast<EQU*>(rel->relop_) || dynamic_cast<NE*>(rel->relop_)) && (a.t == Ty::Str || b.t == Ty::Str))
                {
                    VReg cmpRes = f.newVReg();
                    emit(Instr::call(cmpRes, "strcmp", { a.v, b.v }));
                    VReg zero = makeConst(0);
                    VReg dst = f.newVReg();
                    if (dynamic_cast<EQU*>(rel->relop_))
                        emit(Instr::cmp(dst, cmpRes, CmpOp::EQ, zero));
                    else
                        emit(Instr::cmp(dst, cmpRes, CmpOp::NE, zero));
                    return { dst, Ty::Bool };
                }

                CmpOp op;
                if (dynamic_cast<LTH*>(rel->relop_)) op = CmpOp::LT;
                else if (dynamic_cast<LE*>(rel->relop_)) op = CmpOp::LE;
                else if (dynamic_cast<GTH*>(rel->relop_)) op = CmpOp::GT;
                else if (dynamic_cast<GE*>(rel->relop_)) op = CmpOp::GE;
                else if (dynamic_cast<EQU*>(rel->relop_)) op = CmpOp::EQ;
                else if (dynamic_cast<NE*>(rel->relop_)) op = CmpOp::NE;
                else throw std::runtime_error("Unknown RelOp in codegen");

                VReg dst = f.newVReg();
                emit(Instr::cmp(dst, a.v, op, b.v));
                return { dst, Ty::Bool };
            }
            if (auto* a = dynamic_cast<EAnd*>(e))
            {
                // lazy: (x && y)
                Val left = genExpr((Expr*)a->expr_1);
                VReg res = f.newVReg();

                Label L_rhs = g.newLabel();
                Label L_false = g.newLabel();
                Label L_done = g.newLabel();

                emitJz(left.v, L_false);
                emitJmp(L_rhs);

                startNewBlock(L_rhs);
                Val right = genExpr((Expr*)a->expr_2);
                emit(Instr::mov(res, right.v));
                emitJmp(L_done);

                startNewBlock(L_false);
                emit(Instr::loadImm(res, 0));
                emitJmp(L_done);

                startNewBlock(L_done);
                return { res, Ty::Bool };
            }
            if (auto* o = dynamic_cast<EOr*>(e))
            {
                // lazy: (x || y)
                Val left = genExpr((Expr*)o->expr_1);
                VReg res = f.newVReg();

                Label L_rhs = g.newLabel();
                Label L_true = g.newLabel();
                Label L_done = g.newLabel();

                emitJnz(left.v, L_true);
                emitJmp(L_rhs);

                startNewBlock(L_rhs);
                Val right = genExpr((Expr*)o->expr_2);
                emit(Instr::mov(res, right.v));
                emitJmp(L_done);

                startNewBlock(L_true);
                emit(Instr::loadImm(res, 1));
                emitJmp(L_done);

                startNewBlock(L_done);
                return { res, Ty::Bool };
            }

            throw std::runtime_error("Unsupported Expr node in codegen");
        }

        void genStmt(Stmt* s)
        {
            if (!s) return;
            if (curTerminated) return;

            if (dynamic_cast<Empty*>(s)) return;

            if (auto* bs = dynamic_cast<BStmt*>(s))
            {
                genBlock(bs->block_);
                return;
            }
            if (auto* d = dynamic_cast<Decl*>(s))
            {
                Ty t = tyFromAst(d->type_);
                if (!d->listitem_) return;
                for (Item* it : *d->listitem_)
                {
                    if (auto* ni = dynamic_cast<NoInit*>(it))
                    {
                        VReg v = f.newVReg();
                        defineVar(ni->ident_, { v, t });
                        if (t == Ty::Int || t == Ty::Bool)
                        {
                            emit(Instr::loadImm(v, 0));
                        }
                        else if (t == Ty::Str)
                        {
                            int id = g.internString("");
                            emit(Instr::call(v, g.strGetterName(id), {}));
                        }
                        else
                        {
                            // void zmiennych nie ma
                            emit(Instr::loadImm(v, 0));
                        }
                    }
                    else if (auto* ini = dynamic_cast<Init*>(it))
                    {
                        VReg v = f.newVReg();
                        defineVar(ini->ident_, { v, t });
                        Val rhs = genExpr(ini->expr_);
                        emit(Instr::mov(v, rhs.v));
                    }
                    else
                    {
                        throw std::runtime_error("Unknown Item in Decl");
                    }
                }
                return;
            }
            if (auto* as = dynamic_cast<Ass*>(s))
            {
                VarInfo vi = lookupVar(as->ident_);
                Val rhs = genExpr(as->expr_);
                emit(Instr::mov(vi.v, rhs.v));
                return;
            }
            if (auto* in = dynamic_cast<Incr*>(s))
            {
                VarInfo vi = lookupVar(in->ident_);
                VReg one = makeConst(1);
                VReg dst = f.newVReg();
                emit(Instr::bin(dst, vi.v, BinOp::Add, one));
                emit(Instr::mov(vi.v, dst));
                return;
            }
            if (auto* de = dynamic_cast<Decr*>(s))
            {
                VarInfo vi = lookupVar(de->ident_);
                VReg one = makeConst(1);
                VReg dst = f.newVReg();
                emit(Instr::bin(dst, vi.v, BinOp::Sub, one));
                emit(Instr::mov(vi.v, dst));
                return;
            }
            if (auto* r = dynamic_cast<Ret*>(s))
            {
                Val v = genExpr(r->expr_);
                emit(Instr::ret(v.v));
                curTerminated = true;
                return;
            }
            if (dynamic_cast<VRet*>(s))
            {
                emit(Instr::ret(std::nullopt));
                curTerminated = true;
                return;
            }
            if (auto* c = dynamic_cast<Cond*>(s))
            {
                Val cond = genExpr(c->expr_);

                Label L_then = g.newLabel();
                Label L_end = g.newLabel();

                emitJz(cond.v, L_end);
                emitJmp(L_then);

                startNewBlock(L_then);
                genStmt(c->stmt_);
                if (!curTerminated) emitJmp(L_end);

                startNewBlock(L_end);
                return;
            }
            if (auto* ce = dynamic_cast<CondElse*>(s))
            {
                Val cond = genExpr(ce->expr_);

                Label L_then = g.newLabel();
                Label L_else = g.newLabel();
                Label L_end = g.newLabel();

                emitJz(cond.v, L_else);
                emitJmp(L_then);

                startNewBlock(L_then);
                genStmt(ce->stmt_1);
                if (!curTerminated) emitJmp(L_end);

                startNewBlock(L_else);
                genStmt(ce->stmt_2);
                if (!curTerminated) emitJmp(L_end);

                startNewBlock(L_end);
                return;
            }
            if (auto* w = dynamic_cast<While*>(s))
            {
                Label L_cond = g.newLabel();
                Label L_body = g.newLabel();
                Label L_end = g.newLabel();

                emitJmp(L_cond);

                startNewBlock(L_cond);
                Val cond = genExpr(w->expr_);
                emitJz(cond.v, L_end);
                emitJmp(L_body);

                startNewBlock(L_body);
                genStmt(w->stmt_);
                if (!curTerminated) emitJmp(L_cond);

                startNewBlock(L_end);
                return;
            }
            if (auto* se = dynamic_cast<SExp*>(s))
            {
                // Jeżeli to jest wywołanie funkcji zwracającej void, emituj je jako "statement call"
                if (auto* app = dynamic_cast<EApp*>(se->expr_))
                {
                    auto it = g.sigs.find(app->ident_);
                    if (it != g.sigs.end() && it->second.ret == Ty::Void)
                    {
                        std::vector<VReg> args;
                        if (app->listexpr_)
                        {
                            args.reserve(app->listexpr_->size());
                            for (Expr* ae : *app->listexpr_)
                            {
                                Val av = genExpr(ae);
                                args.push_back(av.v);
                            }
                        }
                        emit(Instr::call(std::nullopt, app->ident_, std::move(args)));
                        return;
                    }
                }

                // inne expr jako statement: policz i zignoruj wynik
                (void)genExpr(se->expr_);
                return;
            }


            throw std::runtime_error("Unsupported Stmt node in codegen");
        }

        void genBlock(Block* b)
        {
            if (!b) return;
            // Blk
            auto* blk = dynamic_cast<Blk*>(b);
            if (!blk) throw std::runtime_error("Unknown Block node");

            pushScope();
            if (blk->liststmt_)
            {
                for (Stmt* s : *blk->liststmt_)
                {
                    genStmt(s);
                    if (curTerminated) break;
                }
            }
            popScope();
        }

        void finalizeCFG()
        {
            // label.id -> block index
            std::unordered_map<int, int> idx;
            idx.reserve(f.blocks.size() * 2);
            for (int i = 0; i < (int)f.blocks.size(); ++i) idx[f.blocks[i].label.id] = i;

            for (int i = 0; i < (int)f.blocks.size(); ++i)
            {
                auto& bb = f.blocks[i];
                bb.succ.clear();
                auto& ins = bb.ins;
                if (ins.empty())
                {
                    if (i + 1 < (int)f.blocks.size()) bb.succ.push_back(i + 1);
                    continue;
                }

                auto lastK = ins.back().k;
                if (lastK == Instr::Kind::Ret)
                {
                    continue;
                }
                if (lastK == Instr::Kind::Jmp)
                {
                    int t = idx.at(ins.back().target.id);
                    // czy przedostatnia to branch?
                    if (ins.size() >= 2)
                    {
                        auto preK = ins[ins.size() - 2].k;
                        if (preK == Instr::Kind::JmpIfZero || preK == Instr::Kind::JmpIfNonZero)
                        {
                            int c = idx.at(ins[ins.size() - 2].target.id);
                            bb.succ.push_back(c);
                        }
                    }
                    bb.succ.push_back(t);
                    continue;
                }
                if (lastK == Instr::Kind::JmpIfZero || lastK == Instr::Kind::JmpIfNonZero)
                {
                    int t = idx.at(ins.back().target.id);
                    bb.succ.push_back(t);
                    if (i + 1 < (int)f.blocks.size()) bb.succ.push_back(i + 1);
                    continue;
                }

                // fallthrough
                if (i + 1 < (int)f.blocks.size()) bb.succ.push_back(i + 1);
            }
        }
    };

    static void addBuiltins(CGCtx& g)
    {
        g.sigs["printInt"] = { Ty::Void, { Ty::Int } };
        g.sigs["printString"] = { Ty::Void, { Ty::Str } };
        g.sigs["error"] = { Ty::Void, { } };
        g.sigs["readInt"] = { Ty::Int,  { } };
        g.sigs["readString"] = { Ty::Str,  { } };

        // nasze helpery
        g.sigs["__latte_concat"] = { Ty::Str, { Ty::Str, Ty::Str } };
        // libgcc:
        g.sigs["__divsi3"] = { Ty::Int, { Ty::Int, Ty::Int } };
        g.sigs["__modsi3"] = { Ty::Int, { Ty::Int, Ty::Int } };
        // libc:
        g.sigs["strcmp"] = { Ty::Int, { Ty::Str, Ty::Str } };
    }

} // namespace

ModuleIR buildModuleIR(Program* program)
{
    if (!program) throw std::runtime_error("buildModuleIR: program == nullptr");

    CGCtx g;
    addBuiltins(g);

    auto* p = dynamic_cast<Prog*>(program);
    if (!p) throw std::runtime_error("Program is not Prog");

    // 1) collect signatures (żeby wspierać wzajemną rekurencję)
    if (p->listtopdef_)
    {
        for (TopDef* td : *p->listtopdef_)
        {
            auto* fn = dynamic_cast<FnDef*>(td);
            if (!fn) throw std::runtime_error("Unknown TopDef (expected FnDef)");

            std::string name = fn->ident_;
            Ty rt = tyFromAst(fn->type_);

            FuncSig sig;
            sig.ret = rt;

            if (fn->listarg_)
            {
                for (Arg* a : *fn->listarg_)
                {
                    Ar* ar = asAr(a);
                    sig.args.push_back(tyFromAst(ar->type_));
                }
            }

            // unikalność nazw i konflikt z builtinami powinien łapać TypeChecker,
            // ale tu też zabezpieczmy:
            if (g.sigs.count(name))
                throw std::runtime_error("Function name conflicts/duplicate in codegen: " + name);

            g.sigs[name] = std::move(sig);
        }
    }

    // zapewnij empty-string w puli (ułatwia default init)
    g.internString("");

    // 2) emit IR per function
    if (p->listtopdef_)
    {
        for (TopDef* td : *p->listtopdef_)
        {
            auto* fn = dynamic_cast<FnDef*>(td);
            if (!fn) continue;

            FnCG cg(g);

            cg.f.name = fn->ident_;
            auto itSig = g.sigs.find(cg.f.name);
            if (itSig == g.sigs.end()) throw std::runtime_error("Missing signature for: " + cg.f.name);

            cg.f.argc = (int)itSig->second.args.size();

            // params -> vregs
            cg.f.params.clear();
            cg.f.params.reserve(cg.f.argc);

            cg.pushScope(); // function-scope
            if (fn->listarg_)
            {
                for (Arg* a : *fn->listarg_)
                {
                    Ar* ar = asAr(a);
                    VReg pv = cg.f.newVReg();
                    cg.f.params.push_back(pv);
                    cg.defineVar(ar->ident_, { pv, tyFromAst(ar->type_) });
                }
            }


            // entry block
            cg.startNewBlock(g.newLabel());

            // body block scope
            cg.genBlock(fn->block_);

            // jeśli void i nie ma return na końcu:
            if (itSig->second.ret == Ty::Void && !cg.curTerminated)
            {
                cg.emit(Instr::ret(std::nullopt));
            }

            cg.popScope(); // function-scope

            cg.finalizeCFG();

            g.mod.funs.push_back(std::move(cg.f));
        }
    }

    return g.mod;
}
