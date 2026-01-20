#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

// ------------------------------------
// Value types
// ------------------------------------
// I32: int/bool
// PTR: pointers (string, object, array, nullptr)
// Void: (retType)
enum class VType
{
    Void,
    I32,
    PTR
};

struct VReg
{
    int id = -1;
    explicit VReg(int i = -1) : id(i) {}
    friend bool operator==(VReg a, VReg b) { return a.id == b.id; }
    friend bool operator!=(VReg a, VReg b) { return a.id != b.id; }
};

struct Label
{
    int id = -1;
    explicit Label(int i = -1) : id(i) {}
};

enum class BinOp
{
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    And,
    Or,
    Xor,
    Shl,
    Shr,
    Sar // for optimalization...
};

enum class UnOp
{
    Neg,
    Not
};
enum class CmpOp
{
    EQ,
    NE,
    LT,
    LE,
    GT,
    GE
};

// ------------------------------------
// Memory reference: [base + index*scale + disp]
// base
// index
// scale: 1/2/4/8 (x86)
// disp
// ------------------------------------
struct MemRef
{
    VReg base;
    std::optional<VReg> index{};
    int scale = 1;
    int disp = 0;

    MemRef() = default;
    explicit MemRef(VReg b, int d = 0) : base(b), disp(d) {}
    MemRef(VReg b, std::optional<VReg> idx, int sc, int d)
        : base(b), index(idx), scale(sc), disp(d) {}
};

// ------------------------------------
// Instruction
// ------------------------------------
struct Instr
{
    enum class Kind
    {
        // value ops
        Mov,
        LoadImm, // dst = imm
        Bin,
        Un,
        Cmp, // dst = (a cmp b) ? 1 : 0   (dst jest I32)

        // call/cfg
        Call,
        Ret,
        Jmp,
        JmpIfZero,
        JmpIfNonZero,

        // memory/address
        Lea,  // dst = &mem   (dst typ PTR)
        Load, // dst = *mem
        Store // *mem = a
    } k;

    std::optional<VReg> dst{};
    std::optional<VReg> a{};
    std::optional<VReg> b{};

    std::int64_t imm = 0;
    BinOp binOp = BinOp::Add;
    UnOp unOp = UnOp::Neg;
    CmpOp cmpOp = CmpOp::EQ;

    std::string callee{};
    std::vector<VReg> args{};

    Label target{-1};

    std::optional<MemRef> mem{};

    explicit Instr(Kind kk) : k(kk), target(-1) {}

    static Instr mov(VReg d, VReg src)
    {
        Instr i(Kind::Mov);
        i.dst = d;
        i.a = src;
        return i;
    }
    static Instr loadImm(VReg d, std::int64_t v)
    {
        Instr i(Kind::LoadImm);
        i.dst = d;
        i.imm = v;
        return i;
    }
    static Instr bin(VReg d, VReg x, BinOp op, VReg y)
    {
        Instr i(Kind::Bin);
        i.dst = d;
        i.a = x;
        i.b = y;
        i.binOp = op;
        return i;
    }
    static Instr un(VReg d, UnOp op, VReg x)
    {
        Instr i(Kind::Un);
        i.dst = d;
        i.a = x;
        i.unOp = op;
        return i;
    }
    static Instr cmp(VReg d, VReg x, CmpOp op, VReg y)
    {
        Instr i(Kind::Cmp);
        i.dst = d;
        i.a = x;
        i.b = y;
        i.cmpOp = op;
        return i;
    }
    static Instr call(std::optional<VReg> d, std::string name, std::vector<VReg> as)
    {
        Instr i(Kind::Call);
        i.dst = d;
        i.callee = std::move(name);
        i.args = std::move(as);
        return i;
    }
    static Instr ret(std::optional<VReg> x = std::nullopt)
    {
        Instr i(Kind::Ret);
        i.a = x;
        return i;
    }
    static Instr jmp(Label L)
    {
        Instr i(Kind::Jmp);
        i.target = L;
        return i;
    }
    static Instr jz(VReg x, Label L)
    {
        Instr i(Kind::JmpIfZero);
        i.a = x;
        i.target = L;
        return i;
    }
    static Instr jnz(VReg x, Label L)
    {
        Instr i(Kind::JmpIfNonZero);
        i.a = x;
        i.target = L;
        return i;
    }

    static Instr lea(VReg d, MemRef m)
    {
        Instr i(Kind::Lea);
        i.dst = d;
        i.mem = m;
        return i;
    }
    static Instr load(VReg d, MemRef m)
    {
        Instr i(Kind::Load);
        i.dst = d;
        i.mem = m;
        return i;
    }
    static Instr store(MemRef m, VReg src)
    {
        Instr i(Kind::Store);
        i.a = src;
        i.mem = m;
        return i;
    }
};

struct BasicBlock
{
    Label label;
    std::vector<Instr> ins;
    std::vector<int> succ;
};

struct FunctionIR
{
    std::string name;
    VType retType = VType::Void;

    int argc = 0;
    std::vector<VReg> params;
    std::vector<BasicBlock> blocks;

    int nextVRegId = 0;

    // vregId -> type
    std::vector<VType> vtypes;

    VReg newVReg(VType t)
    {
        int id = nextVRegId++;
        if ((int)vtypes.size() <= id)
            vtypes.resize((size_t)id + 1, VType::I32);
        vtypes[(size_t)id] = t;
        return VReg(id);
    }
    VReg newVReg() { return newVReg(VType::I32); }

    VType typeOf(VReg r) const
    {
        if (r.id < 0 || r.id >= (int)vtypes.size())
            return VType::I32;
        return vtypes[(size_t)r.id];
    }
};

// ------------------------------------
// Class/object layout metadata
// ------------------------------------
struct FieldLayout
{
    std::string name;
    VType type = VType::I32; // Class ptr / array ptr / string ptr => PTR
    int offset = 0;          // bytes from object base
};

struct MethodSig
{
    std::string name;    // source-level
    std::string mangled; // symbol emitted, np. "C__m"
    VType retType = VType::Void;
    std::vector<VType> argTypes;
};

struct ClassLayout
{
    std::string name;
    std::optional<std::string> base; // extends
    int size = 0;                    // bytes
    std::vector<FieldLayout> fields;
    std::vector<MethodSig> methods;
};

struct ModuleIR
{
    std::vector<FunctionIR> funs;
    std::vector<std::string> stringLits;

    // structs/objects
    std::vector<ClassLayout> classes;

    int arrayHeaderBytes = 8;
};
