#include "codegen.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <utility>
#include <algorithm>
#include <memory>
#include <functional>

// --------------------
// Types for codegen
// --------------------
namespace
{

    struct Ty
    {
        enum class K
        {
            I32,
            STR,
            CLASS,
            ARR,
            VOID,
            PTR /* null cast etc */
        } k;
        std::string name;         // CLASS
        std::shared_ptr<Ty> elem; // ARR

        static Ty I32() { return Ty{K::I32}; }
        static Ty Str() { return Ty{K::STR}; }
        static Ty Void() { return Ty{K::VOID}; }
        static Ty Ptr() { return Ty{K::PTR}; }
        static Ty Class(std::string n)
        {
            Ty t{K::CLASS};
            t.name = std::move(n);
            return t;
        }
        static Ty Arr(Ty e)
        {
            Ty t{K::ARR};
            t.elem = std::make_shared<Ty>(std::move(e));
            return t;
        }
    };

    static bool isPtrLike(const Ty &t)
    {
        return t.k == Ty::K::STR || t.k == Ty::K::CLASS || t.k == Ty::K::ARR || t.k == Ty::K::PTR;
    }

    static VType vtypeFromTy(const Ty &t)
    {
        return isPtrLike(t) ? VType::PTR : VType::I32;
    }

    static int elemSizeBytes(const Ty &elem)
    {
        return isPtrLike(elem) ? 8 : 4;
    }

    // --------------------
    // Signatures
    // --------------------
    struct FuncSig
    {
        Ty ret;
        std::vector<Ty> args; // for methods
    };

    // --------------------
    // Class layout
    // --------------------
    struct FieldInfo
    {
        std::string name;
        Ty type;
        int offset = 0;
    };

    struct ClassInfo
    {
        std::string name;
        std::optional<std::string> base;
        std::vector<FieldInfo> fields; // after offset
        int size = 0;
    };

    static std::string mangleMethod(const std::string &cls, const std::string &m)
    {
        return cls + "__" + m;
    }

    // --------------------
    // String intern
    // --------------------
    static std::string unescapeBnfcString(std::string s)
    {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            s = s.substr(1, s.size() - 2);

        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i)
        {
            char c = s[i];
            if (c != '\\')
            {
                out.push_back(c);
                continue;
            }
            if (i + 1 >= s.size())
                break;
            char n = s[++i];
            switch (n)
            {
            case 'n':
                out.push_back('\n');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '"':
                out.push_back('"');
                break;
            default:
                out.push_back(n);
                break;
            }
        }
        return out;
    }

    // --------------------
    // AST type -> Ty
    // --------------------
    static Ty tyFromBaseType(BaseType *bt)
    {
        if (dynamic_cast<Int *>(bt))
            return Ty::I32();
        if (dynamic_cast<Bool *>(bt))
            return Ty::I32();
        if (dynamic_cast<Str *>(bt))
            return Ty::Str();
        if (auto *ct = dynamic_cast<ClassT *>(bt))
            return Ty::Class(ct->ident_);
        throw std::runtime_error("Unknown BaseType");
    }

    static Ty tyFromDType(DType *dt)
    {
        if (dynamic_cast<DVoid *>(dt))
            return Ty::Void();
        if (auto *b = dynamic_cast<DTypeBase *>(dt))
            return tyFromBaseType(b->basetype_);
        if (auto *a = dynamic_cast<DTypeArr *>(dt))
            return Ty::Arr(tyFromBaseType(a->basetype_));
        throw std::runtime_error("Unknown DType");
    }

    static Ty tyFromTypeNode(Type *t)
    {
        if (dynamic_cast<Void *>(t))
            return Ty::Void();
        if (auto *b = dynamic_cast<TBase *>(t))
            return tyFromBaseType(b->basetype_);
        if (auto *a = dynamic_cast<TArr *>(t))
            return Ty::Arr(tyFromBaseType(a->basetype_));
        if (dynamic_cast<Fun *>(t))
            return Ty::Ptr();
        throw std::runtime_error("Unknown Type node");
    }

    static Ar *asAr(Arg *a)
    {
        auto *ar = dynamic_cast<Ar *>(a);
        if (!ar)
            throw std::runtime_error("Unexpected Arg node (expected Ar)");
        return ar;
    }

    // --------------------
    // Global context
    // --------------------
    struct CGCtx
    {
        ModuleIR mod;

        std::unordered_map<std::string, FuncSig> sigs;      // name (mangled) -> sig
        std::unordered_map<std::string, int> strId;         // literal -> id
        std::unordered_map<std::string, ClassInfo> classes; // class name -> info

        int nextLabelId = 0;
        Label newLabel() { return Label(nextLabelId++); }

        bool hasField(const std::string &cls, const std::string &field) const
        {
            const auto &ci = getClass(cls);
            for (const auto &f : ci.fields)
                if (f.name == field)
                    return true;
            return false;
        }

        std::string resolveMethodClass(std::string cls, const std::string &method) const
        {
            while (true)
            {
                std::string mangled = mangleMethod(cls, method);
                if (sigs.find(mangled) != sigs.end())
                    return cls;
                const auto &ci = getClass(cls);
                if (!ci.base)
                    break;
                cls = *ci.base;
            }
            throw std::runtime_error("Unknown method: " + mangleMethod(cls, method));
        }

        int internString(const std::string &raw)
        {
            auto it = strId.find(raw);
            if (it != strId.end())
                return it->second;
            int id = (int)mod.stringLits.size();
            mod.stringLits.push_back(raw);
            strId.emplace(raw, id);
            return id;
        }

        std::string strGetterName(int id) const
        {
            return "__latte_str_" + std::to_string(id);
        }

        const ClassInfo &getClass(const std::string &n) const
        {
            auto it = classes.find(n);
            if (it == classes.end())
                throw std::runtime_error("Unknown class: " + n);
            return it->second;
        }

        int fieldOffset(const std::string &cls, const std::string &field) const
        {
            const auto &ci = getClass(cls);
            for (const auto &f : ci.fields)
                if (f.name == field)
                    return f.offset;
            throw std::runtime_error("Unknown field " + field + " in class " + cls);
        }

        Ty fieldType(const std::string &cls, const std::string &field) const
        {
            const auto &ci = getClass(cls);
            for (const auto &f : ci.fields)
                if (f.name == field)
                    return f.type;
            throw std::runtime_error("Unknown field " + field + " in class " + cls);
        }
    };

    // --------------------
    // Function codegen context
    // --------------------
    struct VarInfo
    {
        VReg v;
        Ty t;
    };

    struct Val
    {
        VReg v;
        Ty t;
    };

    struct LValue
    {
        enum class K
        {
            Var,
            Mem
        } k;
        VarInfo var{};
        MemRef mem{};
        Ty t;

        static LValue fromVar(VarInfo vi)
        {
            LValue lv;
            lv.k = K::Var;
            lv.var = vi;
            lv.t = vi.t;
            return lv;
        }
        static LValue fromMem(MemRef m, Ty t)
        {
            LValue lv;
            lv.k = K::Mem;
            lv.mem = m;
            lv.t = std::move(t);
            return lv;
        }
    };

    struct FnCG
    {
        CGCtx &g;
        FunctionIR f;

        std::vector<std::unordered_map<std::string, VarInfo>> scopes;

        int curBlock = -1;
        bool curTerminated = false;

        std::optional<std::string> currentClass;

        explicit FnCG(CGCtx &gg) : g(gg) {}

        void pushScope() { scopes.emplace_back(); }
        void popScope() { scopes.pop_back(); }

        void defineVar(const std::string &name, VarInfo vi)
        {
            if (scopes.empty())
                pushScope();
            auto &top = scopes.back();
            top.emplace(name, vi);
        }

        VarInfo lookupVar(const std::string &name) const
        {
            for (int i = (int)scopes.size() - 1; i >= 0; --i)
            {
                auto it = scopes[i].find(name);
                if (it != scopes[i].end())
                    return it->second;
            }
            throw std::runtime_error("Undefined variable in codegen: " + name);
        }

        VReg newTmp(const Ty &t) { return f.newVReg(vtypeFromTy(t)); }

        void startNewBlock(Label L)
        {
            BasicBlock b;
            b.label = L;
            f.blocks.push_back(std::move(b));
            curBlock = (int)f.blocks.size() - 1;
            curTerminated = false;
        }

        void emit(const Instr &i)
        {
            if (curBlock < 0)
                throw std::runtime_error("emit(): no current block");
            if (curTerminated)
                return;
            f.blocks[curBlock].ins.push_back(i);

            if (i.k == Instr::Kind::Ret || i.k == Instr::Kind::Jmp)
                curTerminated = true;
        }

        void emitJmp(Label L)
        {
            emit(Instr::jmp(L));
            curTerminated = true;
        }
        void emitJz(VReg c, Label L) { emit(Instr::jz(c, L)); }
        void emitJnz(VReg c, Label L) { emit(Instr::jnz(c, L)); }

        VReg makeI32Const(long v)
        {
            VReg r = f.newVReg(VType::I32);
            emit(Instr::loadImm(r, v));
            return r;
        }

        // --------------------
        // genExpr: r-value
        // --------------------
        Val genExpr(Expr *e)
        {
            if (!e)
                throw std::runtime_error("genExpr: nullptr");

            if (auto *p = dynamic_cast<EParen *>(e))
                return genExpr(p->expr_);

            if (auto *a = dynamic_cast<EAtom *>(e))
                return genExpr(a->expr_);

            if (auto *n = dynamic_cast<ELitInt *>(e))
            {
                VReg r = f.newVReg(VType::I32);
                emit(Instr::loadImm(r, (long)n->integer_));
                return {r, Ty::I32()};
            }
            if (dynamic_cast<ELitTrue *>(e))
                return {makeI32Const(1), Ty::I32()};
            if (dynamic_cast<ELitFalse *>(e))
                return {makeI32Const(0), Ty::I32()};

            if (auto *v = dynamic_cast<EVar *>(e))
            {
                for (int i = (int)scopes.size() - 1; i >= 0; --i)
                {
                    auto it = scopes[i].find(v->ident_);
                    if (it != scopes[i].end())
                        return {it->second.v, it->second.t};
                }

                if (currentClass && g.hasField(*currentClass, v->ident_))
                {
                    auto self = lookupVar("self");
                    int off = g.fieldOffset(*currentClass, v->ident_);
                    Ty ft = g.fieldType(*currentClass, v->ident_);
                    VReg dst = newTmp(ft);
                    emit(Instr::load(dst, MemRef(self.v, off)));
                    return {dst, ft};
                }

                throw std::runtime_error("Undefined variable in codegen: " + v->ident_);
            }

            if (dynamic_cast<ESelf *>(e))
            {
                auto vi = lookupVar("self");
                return {vi.v, vi.t};
            }

            if (auto *nc = dynamic_cast<ENullCast *>(e))
            {
                (void)nc;
                VReg r = f.newVReg(VType::PTR);
                emit(Instr::loadImm(r, 0));
                return {r, Ty::Ptr()};
            }

            if (auto *s = dynamic_cast<EString *>(e))
            {
                std::string raw = unescapeBnfcString(s->string_);
                int id = g.internString(raw);
                VReg dst = f.newVReg(VType::PTR);
                emit(Instr::call(dst, g.strGetterName(id), {}));
                return {dst, Ty::Str()};
            }

            if (auto *app = dynamic_cast<EApp *>(e))
            {
                std::string name = app->ident_;
                auto it = g.sigs.find(name);
                if (it == g.sigs.end())
                    throw std::runtime_error("Unknown function: " + name);

                std::vector<VReg> args;
                if (app->listexpr_)
                {
                    args.reserve(app->listexpr_->size());
                    for (Expr *a : *app->listexpr_)
                        args.push_back(genExpr(a).v);
                }

                if (it->second.ret.k == Ty::K::VOID)
                {
                    emit(Instr::call(std::nullopt, name, std::move(args)));

                    return {makeI32Const(0), Ty::I32()};
                }

                VReg dst = newTmp(it->second.ret);
                emit(Instr::call(dst, name, std::move(args)));
                return {dst, it->second.ret};
            }

            if (auto *na = dynamic_cast<Neg *>(e))
            {
                Val x = genExpr(na->expr_);
                VReg dst = f.newVReg(VType::I32);
                emit(Instr::un(dst, UnOp::Neg, x.v));
                return {dst, Ty::I32()};
            }
            if (auto *nt = dynamic_cast<Not *>(e))
            {
                Val x = genExpr(nt->expr_);
                VReg dst = f.newVReg(VType::I32);
                emit(Instr::un(dst, UnOp::Not, x.v));
                return {dst, Ty::I32()};
            }

            if (auto *mul = dynamic_cast<EMul *>(e))
            {
                Val a = genExpr(mul->expr_1);
                Val b = genExpr(mul->expr_2);

                VReg dst = f.newVReg(VType::I32);
                if (dynamic_cast<Times *>(mul->mulop_))
                    emit(Instr::bin(dst, a.v, BinOp::Mul, b.v));
                else if (dynamic_cast<Div *>(mul->mulop_))
                    emit(Instr::bin(dst, a.v, BinOp::Div, b.v));
                else if (dynamic_cast<Mod *>(mul->mulop_))
                    emit(Instr::bin(dst, a.v, BinOp::Mod, b.v));
                else
                    throw std::runtime_error("Unknown MulOp");
                return {dst, Ty::I32()};
            }

            if (auto *add = dynamic_cast<EAdd *>(e))
            {
                Val a = genExpr(add->expr_1);
                Val b = genExpr(add->expr_2);

                if (dynamic_cast<Minus *>(add->addop_))
                {
                    VReg dst = f.newVReg(VType::I32);
                    emit(Instr::bin(dst, a.v, BinOp::Sub, b.v));
                    return {dst, Ty::I32()};
                }

                if (dynamic_cast<Plus *>(add->addop_))
                {
                    if (a.t.k == Ty::K::STR || b.t.k == Ty::K::STR)
                    {
                        VReg dst = f.newVReg(VType::PTR);
                        emit(Instr::call(dst, "__latte_concat", {a.v, b.v}));
                        return {dst, Ty::Str()};
                    }
                    VReg dst = f.newVReg(VType::I32);
                    emit(Instr::bin(dst, a.v, BinOp::Add, b.v));
                    return {dst, Ty::I32()};
                }

                throw std::runtime_error("Unknown AddOp");
            }

            if (auto *rel = dynamic_cast<ERel *>(e))
            {
                Val a = genExpr(rel->expr_1);
                Val b = genExpr(rel->expr_2);

                if ((dynamic_cast<EQU *>(rel->relop_) || dynamic_cast<NE *>(rel->relop_)) &&
                    (a.t.k == Ty::K::STR || b.t.k == Ty::K::STR))
                {
                    VReg cmpRes = f.newVReg(VType::I32);
                    emit(Instr::call(cmpRes, "strcmp", {a.v, b.v}));
                    VReg zero = makeI32Const(0);
                    VReg dst = f.newVReg(VType::I32);
                    emit(Instr::cmp(dst, cmpRes,
                                    dynamic_cast<EQU *>(rel->relop_) ? CmpOp::EQ : CmpOp::NE, zero));
                    return {dst, Ty::I32()};
                }

                CmpOp op;
                if (dynamic_cast<LTH *>(rel->relop_))
                    op = CmpOp::LT;
                else if (dynamic_cast<LE *>(rel->relop_))
                    op = CmpOp::LE;
                else if (dynamic_cast<GTH *>(rel->relop_))
                    op = CmpOp::GT;
                else if (dynamic_cast<GE *>(rel->relop_))
                    op = CmpOp::GE;
                else if (dynamic_cast<EQU *>(rel->relop_))
                    op = CmpOp::EQ;
                else if (dynamic_cast<NE *>(rel->relop_))
                    op = CmpOp::NE;
                else
                    throw std::runtime_error("Unknown RelOp");

                VReg dst = f.newVReg(VType::I32);
                emit(Instr::cmp(dst, a.v, op, b.v));
                return {dst, Ty::I32()};
            }

            if (auto *a = dynamic_cast<EAnd *>(e))
            {
                Val left = genExpr(a->expr_1);
                VReg res = f.newVReg(VType::I32);

                Label L_rhs = g.newLabel();
                Label L_false = g.newLabel();
                Label L_done = g.newLabel();

                emitJz(left.v, L_false);
                emitJmp(L_rhs);

                startNewBlock(L_rhs);
                Val right = genExpr(a->expr_2);
                emit(Instr::mov(res, right.v));
                emitJmp(L_done);

                startNewBlock(L_false);
                emit(Instr::loadImm(res, 0));
                emitJmp(L_done);

                startNewBlock(L_done);
                return {res, Ty::I32()};
            }

            if (auto *o = dynamic_cast<EOr *>(e))
            {
                Val left = genExpr(o->expr_1);
                VReg res = f.newVReg(VType::I32);

                Label L_rhs = g.newLabel();
                Label L_true = g.newLabel();
                Label L_done = g.newLabel();

                emitJnz(left.v, L_true);
                emitJmp(L_rhs);

                startNewBlock(L_rhs);
                Val right = genExpr(o->expr_2);
                emit(Instr::mov(res, right.v));
                emitJmp(L_done);

                startNewBlock(L_true);
                emit(Instr::loadImm(res, 1));
                emitJmp(L_done);

                startNewBlock(L_done);
                return {res, Ty::I32()};
            }

            if (auto *na = dynamic_cast<ENewArr *>(e))
            {
                Ty elemT = tyFromBaseType(na->basetype_);
                Val len = genExpr(na->expr_);

                VReg elemSize = makeI32Const(elemSizeBytes(elemT));
                VReg dst = f.newVReg(VType::PTR);

                emit(Instr::call(dst, "__latte_new_array", {len.v, elemSize}));
                return {dst, Ty::Arr(elemT)};
            }

            if (auto *no = dynamic_cast<ENewObj *>(e))
            {
                const std::string cls = no->ident_;
                const auto &ci = g.getClass(cls);
                VReg size = makeI32Const(ci.size);
                VReg dst = f.newVReg(VType::PTR);
                emit(Instr::call(dst, "__latte_new_obj", {size}));
                return {dst, Ty::Class(cls)};
            }

            if (auto *ix = dynamic_cast<EIndex *>(e))
            {
                Val arr = genExpr(ix->expr_1);
                Val idx = genExpr(ix->expr_2);

                if (arr.t.k != Ty::K::ARR)
                    throw std::runtime_error("EIndex on non-array");

                Ty elemT = *arr.t.elem;
                int sc = elemSizeBytes(elemT);
                int disp = g.mod.arrayHeaderBytes;

                VReg dst = newTmp(elemT);
                emit(Instr::load(dst, MemRef(arr.v, idx.v, sc, disp)));
                return {dst, elemT};
            }

            if (auto *fe = dynamic_cast<EField *>(e))
            {
                Val base = genExpr(fe->expr_);

                // array.length
                if (base.t.k == Ty::K::ARR && fe->ident_ == "length")
                {
                    VReg dst = f.newVReg(VType::I32);
                    // header: length at offset 0
                    emit(Instr::load(dst, MemRef(base.v, 0)));
                    return {dst, Ty::I32()};
                }

                // normal object field
                if (base.t.k != Ty::K::CLASS)
                    throw std::runtime_error("Field access on non-object");

                int off = g.fieldOffset(base.t.name, fe->ident_);
                Ty ft = g.fieldType(base.t.name, fe->ident_);

                VReg dst = newTmp(ft);
                emit(Instr::load(dst, MemRef(base.v, off)));
                return {dst, ft};
            }

            if (auto *me = dynamic_cast<EMethod *>(e))
            {
                Val obj = genExpr(me->expr_);
                if (obj.t.k != Ty::K::CLASS)
                    throw std::runtime_error("Method call on non-object");

                std::string defCls = g.resolveMethodClass(obj.t.name, me->ident_);
                std::string mangled = mangleMethod(defCls, me->ident_);

                auto it = g.sigs.find(mangled);
                if (it == g.sigs.end())
                    throw std::runtime_error("Unknown method: " + mangled);

                std::vector<VReg> args;
                args.push_back(obj.v); // self
                if (me->listexpr_)
                {
                    args.reserve(1 + me->listexpr_->size());
                    for (Expr *ae : *me->listexpr_)
                        args.push_back(genExpr(ae).v);
                }

                if (it->second.ret.k == Ty::K::VOID)
                {
                    emit(Instr::call(std::nullopt, mangled, std::move(args)));
                    return {makeI32Const(0), Ty::I32()};
                }

                VReg dst = newTmp(it->second.ret);
                emit(Instr::call(dst, mangled, std::move(args)));
                return {dst, it->second.ret};
            }

            throw std::runtime_error("Unsupported Expr node in codegen");
        }

        // --------------------
        // genLValue
        // --------------------
        LValue genLValue(Expr *e6)
        {
            if (auto *at = dynamic_cast<EAtom *>(e6))
                return genLValue(at->expr_);

            if (auto *v = dynamic_cast<EVar *>(e6))
            {
                // locals/args
                for (int i = (int)scopes.size() - 1; i >= 0; --i)
                {
                    auto it = scopes[i].find(v->ident_);
                    if (it != scopes[i].end())
                        return LValue::fromVar(it->second);
                }

                if (currentClass && g.hasField(*currentClass, v->ident_))
                {
                    auto self = lookupVar("self");
                    int off = g.fieldOffset(*currentClass, v->ident_);
                    Ty ft = g.fieldType(*currentClass, v->ident_);
                    return LValue::fromMem(MemRef(self.v, off), ft);
                }

                throw std::runtime_error("Undefined variable in codegen: " + v->ident_);
            }

            if (auto *ix = dynamic_cast<EIndex *>(e6))
            {
                Val arr = genExpr(ix->expr_1);
                Val idx = genExpr(ix->expr_2);
                if (arr.t.k != Ty::K::ARR)
                    throw std::runtime_error("Assign to index of non-array");

                Ty elemT = *arr.t.elem;
                int sc = elemSizeBytes(elemT);
                int disp = g.mod.arrayHeaderBytes;

                return LValue::fromMem(MemRef(arr.v, idx.v, sc, disp), elemT);
            }

            if (auto *fe = dynamic_cast<EField *>(e6))
            {
                Val obj = genExpr(fe->expr_);
                if (obj.t.k != Ty::K::CLASS)
                    throw std::runtime_error("Assign to field of non-object");

                int off = g.fieldOffset(obj.t.name, fe->ident_);
                Ty ft = g.fieldType(obj.t.name, fe->ident_);
                return LValue::fromMem(MemRef(obj.v, off), ft);
            }

            throw std::runtime_error("Unsupported lvalue Expr6 in codegen");
        }

        // --------------------
        // Statements
        // --------------------
        std::string asLValueIdent(Expr *e)
        {
            // unwrap
            while (true)
            {
                if (auto *p = dynamic_cast<EParen *>(e))
                {
                    e = p->expr_;
                    continue;
                }
                if (auto *a = dynamic_cast<EAtom *>(e))
                {
                    e = a->expr_;
                    continue;
                }
                break;
            }

            if (auto *v = dynamic_cast<EVar *>(e))
                return v->ident_;
            throw std::runtime_error("Unsupported lvalue (expected variable) in assignment/incr/decr");
        }

        void genStmt(Stmt *s)
        {
            if (!s || curTerminated)
                return;

            if (dynamic_cast<Empty *>(s))
                return;

            if (auto *bs = dynamic_cast<BStmt *>(s))
            {
                genBlock(bs->block_);
                return;
            }

            if (auto *d = dynamic_cast<Decl *>(s))
            {
                Ty t = tyFromDType(d->dtype_);
                if (!d->listitem_)
                    return;

                for (Item *it : *d->listitem_)
                {
                    if (auto *ni = dynamic_cast<NoInit *>(it))
                    {
                        VReg v = newTmp(t);
                        defineVar(ni->ident_, {v, t});

                        // default init
                        if (!isPtrLike(t))
                            emit(Instr::loadImm(v, 0));
                        else
                            emit(Instr::loadImm(v, 0));
                    }
                    else if (auto *ini = dynamic_cast<Init *>(it))
                    {
                        Val rhs = genExpr(ini->expr_);
                        VReg v = newTmp(t);
                        defineVar(ini->ident_, {v, t});
                        emit(Instr::mov(v, rhs.v));
                    }
                    else
                        throw std::runtime_error("Unknown Item in Decl");
                }
                return;
            }

            if (auto *as = dynamic_cast<Ass *>(s))
            {
                LValue lv = genLValue(as->expr_1);
                Val rhs = genExpr(as->expr_2);

                if (lv.k == LValue::K::Var)
                {
                    emit(Instr::mov(lv.var.v, rhs.v));
                }
                else
                {
                    // store rhs -> [mem]
                    emit(Instr::store(lv.mem, rhs.v));
                }
                return;
            }

            if (auto *in = dynamic_cast<Incr *>(s))
            {
                LValue lv = genLValue(in->expr_);

                // load current value
                VReg cur = f.newVReg(VType::I32);
                if (lv.k == LValue::K::Var)
                {
                    emit(Instr::mov(cur, lv.var.v));
                }
                else
                {
                    emit(Instr::load(cur, lv.mem));
                }

                VReg one = makeI32Const(1);
                VReg tmp = f.newVReg(VType::I32);
                emit(Instr::bin(tmp, cur, BinOp::Add, one));

                // store back
                if (lv.k == LValue::K::Var)
                {
                    emit(Instr::mov(lv.var.v, tmp));
                }
                else
                {
                    emit(Instr::store(lv.mem, tmp));
                }
                return;
            }

            if (auto *de = dynamic_cast<Decr *>(s))
            {
                LValue lv = genLValue(de->expr_);

                VReg cur = f.newVReg(VType::I32);
                if (lv.k == LValue::K::Var)
                {
                    emit(Instr::mov(cur, lv.var.v));
                }
                else
                {
                    emit(Instr::load(cur, lv.mem));
                }

                VReg one = makeI32Const(1);
                VReg tmp = f.newVReg(VType::I32);
                emit(Instr::bin(tmp, cur, BinOp::Sub, one));

                if (lv.k == LValue::K::Var)
                {
                    emit(Instr::mov(lv.var.v, tmp));
                }
                else
                {
                    emit(Instr::store(lv.mem, tmp));
                }
                return;
            }

            if (auto *r = dynamic_cast<Ret *>(s))
            {
                Val v = genExpr(r->expr_);
                emit(Instr::ret(v.v));
                curTerminated = true;
                return;
            }

            if (dynamic_cast<VRet *>(s))
            {
                emit(Instr::ret(std::nullopt));
                curTerminated = true;
                return;
            }

            if (auto *c = dynamic_cast<Cond *>(s))
            {
                Val cond = genExpr(c->expr_);

                Label L_then = g.newLabel();
                Label L_end = g.newLabel();

                emitJz(cond.v, L_end);
                emitJmp(L_then);

                startNewBlock(L_then);
                genStmt(c->stmt_);
                if (!curTerminated)
                    emitJmp(L_end);

                startNewBlock(L_end);
                return;
            }

            if (auto *ce = dynamic_cast<CondElse *>(s))
            {
                Val cond = genExpr(ce->expr_);

                Label L_then = g.newLabel();
                Label L_else = g.newLabel();
                Label L_end = g.newLabel();

                emitJz(cond.v, L_else);
                emitJmp(L_then);

                startNewBlock(L_then);
                genStmt(ce->stmt_1);
                if (!curTerminated)
                    emitJmp(L_end);

                startNewBlock(L_else);
                genStmt(ce->stmt_2);
                if (!curTerminated)
                    emitJmp(L_end);

                startNewBlock(L_end);
                return;
            }

            if (auto *w = dynamic_cast<While *>(s))
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
                if (!curTerminated)
                    emitJmp(L_cond);

                startNewBlock(L_end);
                return;
            }

            // ForEach: for (DType x : expr) stmt
            if (auto *fe = dynamic_cast<ForEach *>(s))
            {
                Ty itTy = tyFromDType(fe->dtype_);
                Val arr = genExpr(fe->expr_);
                if (arr.t.k != Ty::K::ARR)
                    throw std::runtime_error("foreach on non-array");

                // i = 0
                VReg ireg = f.newVReg(VType::I32);
                emit(Instr::loadImm(ireg, 0));

                Label L_cond = g.newLabel();
                Label L_body = g.newLabel();
                Label L_end = g.newLabel();

                emitJmp(L_cond);

                startNewBlock(L_cond);
                // len = *(i32*)arr  (header at 0)
                VReg len = f.newVReg(VType::I32);
                emit(Instr::load(len, MemRef(arr.v, 0)));

                VReg ok = f.newVReg(VType::I32);
                emit(Instr::cmp(ok, ireg, CmpOp::LT, len));
                emitJz(ok, L_end);
                emitJmp(L_body);

                startNewBlock(L_body);
                pushScope();
                {
                    // x = arr[i]
                    Ty elemT = *arr.t.elem;
                    int sc = elemSizeBytes(elemT);
                    int disp = g.mod.arrayHeaderBytes;

                    VReg elem = newTmp(elemT);
                    emit(Instr::load(elem, MemRef(arr.v, ireg, sc, disp)));

                    defineVar(fe->ident_, {elem, elemT});

                    genStmt(fe->stmt_);
                }
                popScope();

                if (!curTerminated)
                {
                    VReg one = makeI32Const(1);
                    VReg inext = f.newVReg(VType::I32);
                    emit(Instr::bin(inext, ireg, BinOp::Add, one));
                    emit(Instr::mov(ireg, inext));
                    emitJmp(L_cond);
                }

                startNewBlock(L_end);
                return;
            }

            if (auto *se = dynamic_cast<SExp *>(s))
            {
                (void)genExpr(se->expr_);
                return;
            }

            throw std::runtime_error("Unsupported Stmt node in codegen");
        }

        void genBlock(Block *b)
        {
            if (!b)
                return;
            auto *blk = dynamic_cast<Blk *>(b);
            if (!blk)
                throw std::runtime_error("Unknown Block node");

            pushScope();
            if (blk->liststmt_)
            {
                for (Stmt *s : *blk->liststmt_)
                {
                    genStmt(s);
                    if (curTerminated)
                        break;
                }
            }
            popScope();
        }

        void finalizeCFG()
        {
            std::unordered_map<int, int> idx;
            idx.reserve(f.blocks.size() * 2);
            for (int i = 0; i < (int)f.blocks.size(); ++i)
                idx[f.blocks[i].label.id] = i;

            for (int i = 0; i < (int)f.blocks.size(); ++i)
            {
                auto &bb = f.blocks[i];
                bb.succ.clear();
                auto &ins = bb.ins;
                if (ins.empty())
                {
                    if (i + 1 < (int)f.blocks.size())
                        bb.succ.push_back(i + 1);
                    continue;
                }

                auto lastK = ins.back().k;
                if (lastK == Instr::Kind::Ret)
                    continue;

                if (lastK == Instr::Kind::Jmp)
                {
                    int t = idx.at(ins.back().target.id);
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
                    if (i + 1 < (int)f.blocks.size())
                        bb.succ.push_back(i + 1);
                    continue;
                }

                if (i + 1 < (int)f.blocks.size())
                    bb.succ.push_back(i + 1);
            }
        }
    };

    // --------------------
    // Builtins + runtime helpers
    // --------------------
    static void addBuiltins(CGCtx &g)
    {
        g.sigs["printInt"] = {Ty::Void(), {Ty::I32()}};
        g.sigs["printString"] = {Ty::Void(), {Ty::Str()}};
        g.sigs["error"] = {Ty::Void(), {}};
        g.sigs["readInt"] = {Ty::I32(), {}};
        g.sigs["readString"] = {Ty::Str(), {}};

        g.sigs["__latte_concat"] = {Ty::Str(), {Ty::Str(), Ty::Str()}};
        g.sigs["strcmp"] = {Ty::I32(), {Ty::Str(), Ty::Str()}};

        g.sigs["__latte_new_obj"] = {Ty::Ptr(), {Ty::I32()}};              // size
        g.sigs["__latte_new_array"] = {Ty::Ptr(), {Ty::I32(), Ty::I32()}}; // len, elemSize
    }

    static void collectClasses(CGCtx &g, Prog *p)
    {
        for (TopDef *td : *p->listtopdef_)
        {
            if (auto *cd = dynamic_cast<ClassDef *>(td))
            {
                ClassInfo ci;
                ci.name = cd->ident_;
                ci.base = std::nullopt;
                g.classes[ci.name] = std::move(ci);
            }
            else if (auto *ce = dynamic_cast<ClassExt *>(td))
            {
                ClassInfo ci;
                ci.name = ce->ident_1;
                ci.base = ce->ident_2;
                g.classes[ci.name] = std::move(ci);
            }
        }

        for (TopDef *td : *p->listtopdef_)
        {
            if (auto *cd = dynamic_cast<ClassDef *>(td))
            {
                auto &ci = g.classes.at(cd->ident_);
                if (cd->listmember_)
                {
                    for (Member *m : *cd->listmember_)
                    {
                        if (auto *fld = dynamic_cast<Field *>(m))
                        {
                            FieldInfo fi;
                            fi.name = fld->ident_;
                            fi.type = tyFromDType(fld->dtype_);
                            ci.fields.push_back(std::move(fi));
                        }
                    }
                }
            }
            else if (auto *ce = dynamic_cast<ClassExt *>(td))
            {
                auto &ci = g.classes.at(ce->ident_1);
                if (ce->listmember_)
                {
                    for (Member *m : *ce->listmember_)
                    {
                        if (auto *fld = dynamic_cast<Field *>(m))
                        {
                            FieldInfo fi;
                            fi.name = fld->ident_;
                            fi.type = tyFromDType(fld->dtype_);
                            ci.fields.push_back(std::move(fi));
                        }
                    }
                }
            }
        }

        std::unordered_map<std::string, bool> done;

        std::function<void(const std::string &)> dfs = [&](const std::string &c)
        {
            if (done[c])
                return;
            auto &ci = g.classes.at(c);

            std::vector<FieldInfo> merged;
            int off = 0;

            if (ci.base)
            {
                dfs(*ci.base);
                const auto &b = g.classes.at(*ci.base);
                merged = b.fields;
                off = b.size;
            }

            for (auto &f : ci.fields)
            {
                f.offset = off;
                off += isPtrLike(f.type) ? 8 : 4;
                if (isPtrLike(f.type) && (off % 8 != 0))
                    off += (8 - (off % 8));
                merged.push_back(f);
            }

            ci.fields = std::move(merged);
            ci.size = std::max(off, 8);
            done[c] = true;
        };

        for (auto &[name, _] : g.classes)
            dfs(name);

        g.mod.classes.clear();
        g.mod.classes.reserve(g.classes.size());
        for (auto &[name, ci] : g.classes)
        {
            ClassLayout cl;
            cl.name = ci.name;
            cl.base = ci.base;
            cl.size = ci.size;
            for (auto &f : ci.fields)
            {
                FieldLayout fl;
                fl.name = f.name;
                fl.type = vtypeFromTy(f.type);
                fl.offset = f.offset;
                cl.fields.push_back(std::move(fl));
            }
            g.mod.classes.push_back(std::move(cl));
        }
    }

    static void collectSigs(CGCtx &g, Prog *p)
    {
        for (TopDef *td : *p->listtopdef_)
        {
            if (auto *fn = dynamic_cast<FnDef *>(td))
            {
                FuncSig sig;
                sig.ret = tyFromDType(fn->dtype_);
                if (fn->listarg_)
                    for (Arg *a : *fn->listarg_)
                        sig.args.push_back(tyFromDType(asAr(a)->dtype_));

                std::string name = fn->ident_;
                if (g.sigs.count(name))
                    throw std::runtime_error("Duplicate/conflict function: " + name);
                g.sigs[name] = std::move(sig);
            }
        }

        for (TopDef *td : *p->listtopdef_)
        {
            if (auto *cd = dynamic_cast<ClassDef *>(td))
            {
                const std::string cls = cd->ident_;
                if (!cd->listmember_)
                    continue;

                for (Member *m : *cd->listmember_)
                {
                    auto *md = dynamic_cast<Method *>(m);
                    if (!md)
                        continue;

                    FuncSig sig;
                    sig.ret = tyFromDType(md->dtype_);
                    sig.args.push_back(Ty::Class(cls)); // self
                    if (md->listarg_)
                        for (Arg *a : *md->listarg_)
                            sig.args.push_back(tyFromDType(asAr(a)->dtype_));

                    std::string mangled = mangleMethod(cls, md->ident_);
                    if (g.sigs.count(mangled))
                        throw std::runtime_error("Duplicate method: " + mangled);
                    g.sigs[mangled] = std::move(sig);
                }
            }
            else if (auto *ce = dynamic_cast<ClassExt *>(td))
            {
                const std::string cls = ce->ident_1;
                if (!ce->listmember_)
                    continue;

                for (Member *m : *ce->listmember_)
                {
                    auto *md = dynamic_cast<Method *>(m);
                    if (!md)
                        continue;

                    FuncSig sig;
                    sig.ret = tyFromDType(md->dtype_);
                    sig.args.push_back(Ty::Class(cls)); // self
                    if (md->listarg_)
                        for (Arg *a : *md->listarg_)
                            sig.args.push_back(tyFromDType(asAr(a)->dtype_));

                    std::string mangled = mangleMethod(cls, md->ident_);
                    if (g.sigs.count(mangled))
                        throw std::runtime_error("Duplicate method: " + mangled);
                    g.sigs[mangled] = std::move(sig);
                }
            }
        }
    }

} // namespace

// --------------------
// buildModuleIR
// --------------------
ModuleIR buildModuleIR(Program *program)
{
    if (!program)
        throw std::runtime_error("buildModuleIR: program == nullptr");

    auto *p = dynamic_cast<Prog *>(program);
    if (!p)
        throw std::runtime_error("Program is not Prog");

    CGCtx g;
    addBuiltins(g);

    // string pool: ""
    g.internString("");

    if (p->listtopdef_)
        collectClasses(g, p);

    if (p->listtopdef_)
        collectSigs(g, p);

    if (p->listtopdef_)
    {
        for (TopDef *td : *p->listtopdef_)
        {
            auto *fn = dynamic_cast<FnDef *>(td);
            if (!fn)
                continue;

            FnCG cg(g);
            cg.f.name = fn->ident_;

            // params
            cg.pushScope();
            if (fn->listarg_)
            {
                for (Arg *a : *fn->listarg_)
                {
                    Ar *ar = asAr(a);
                    Ty pt = tyFromDType(ar->dtype_);
                    VReg pv = cg.f.newVReg(vtypeFromTy(pt));
                    cg.f.params.push_back(pv);
                    cg.defineVar(ar->ident_, {pv, pt});
                }
            }

            cg.startNewBlock(g.newLabel());
            cg.genBlock(fn->block_);

            if (!cg.curTerminated && g.sigs.at(cg.f.name).ret.k == Ty::K::VOID)
                cg.emit(Instr::ret(std::nullopt));

            cg.popScope();
            cg.finalizeCFG();

            g.mod.funs.push_back(std::move(cg.f));
        }
    }

    if (p->listtopdef_)
    {
        for (TopDef *td : *p->listtopdef_)
        {
            std::string cls;
            ListMember *members = nullptr;

            if (auto *cd = dynamic_cast<ClassDef *>(td))
            {
                cls = cd->ident_;
                members = cd->listmember_;
            }
            else if (auto *ce = dynamic_cast<ClassExt *>(td))
            {
                cls = ce->ident_1;
                members = ce->listmember_;
            }
            else
                continue;

            if (!members)
                continue;

            for (Member *m : *members)
            {
                auto *md = dynamic_cast<Method *>(m);
                if (!md)
                    continue;

                FnCG cg(g);
                cg.currentClass = cls;

                cg.f.name = mangleMethod(cls, md->ident_);

                // param0 = self
                cg.pushScope();

                Ty selfTy = Ty::Class(cls);
                VReg selfV = cg.f.newVReg(VType::PTR);
                cg.f.params.push_back(selfV);
                cg.defineVar("self", {selfV, selfTy});

                if (md->listarg_)
                {
                    for (Arg *a : *md->listarg_)
                    {
                        Ar *ar = asAr(a);
                        Ty pt = tyFromDType(ar->dtype_);
                        VReg pv = cg.f.newVReg(vtypeFromTy(pt));
                        cg.f.params.push_back(pv);
                        cg.defineVar(ar->ident_, {pv, pt});
                    }
                }

                cg.startNewBlock(g.newLabel());
                cg.genBlock(md->block_);

                if (!cg.curTerminated && g.sigs.at(cg.f.name).ret.k == Ty::K::VOID)
                    cg.emit(Instr::ret(std::nullopt));

                cg.popScope();
                cg.finalizeCFG();

                g.mod.funs.push_back(std::move(cg.f));
            }
        }
    }

    return g.mod;
}
