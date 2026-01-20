#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

// -----------------------------
// Types
// -----------------------------
// I32 covers int + boolean (as 0/1).
// PTR covers: string ptr, object ptr, array ptr, nullptr.
// Void used for function return type metadata only.
enum class VType { Void, I32, PTR };

// Size helpers (for x86_64 SysV)
inline constexpr int vtypeSizeBytes(VType t)
{
    switch (t)
    {
    case VType::I32:  return 4;
    case VType::PTR:  return 8;
    case VType::Void: return 0;
    }
    return 0;
}

// -----------------------------
// Virtual registers / labels
// -----------------------------
struct VReg {
    int id = -1;
    explicit VReg(int i = -1) : id(i) {}
    friend bool operator==(VReg a, VReg b) { return a.id == b.id; }
    friend bool operator!=(VReg a, VReg b) { return a.id != b.id; }
};

struct Label {
    int id = -1;
    explicit Label(int i = -1) : id(i) {}
};

// -----------------------------
// ALU ops
// -----------------------------
enum class BinOp {
    Add, Sub, Mul, Div, Mod,
    And, Or, Xor,
    Shl, Shr, Sar // optional, useful for strength reduction later
};

enum class UnOp  { Neg, Not };
enum class CmpOp { EQ, NE, LT, LE, GT, GE };

// -----------------------------
// Memory reference: [base + index*scale + disp]
// base is always required.
// index is optional (can be absent).
// scale must be 1,2,4,8 (x86 rule).
// disp is signed bytes.
// -----------------------------
struct MemRef {
    VReg base;
    std::optional<VReg> index{};
    int scale = 1;
    int disp = 0;

    MemRef() = default;
    explicit MemRef(VReg b, int d = 0) : base(b), disp(d) {}
    MemRef(VReg b, std::optional<VReg> idx, int sc, int d)
        : base(b), index(idx), scale(sc), disp(d) {}
};

// -----------------------------
// Instruction
// -----------------------------
struct Instr {
    enum class Kind {
        // value ops
        Mov,
        LoadImm,    // dst = imm (type is dst's vtype)
        Bin,
        Un,
        Cmp,        // dst = (a cmp b) ? 1 : 0  (dst is I32)

        // calls / control flow
        Call,       // optional dst
        Ret,        // optional value
        Jmp,
        JmpIfZero,      // if (a==0) goto target
        JmpIfNonZero,   // if (a!=0) goto target

        // memory / address
        Lea,        // dst = &mem  (dst is PTR)
        Load,       // dst = *(mem)  (size from dst vtype)
        Store       // *(mem) = a    (size from a vtype)
    } k;

    // Common regs (meaning depends on instruction)
    // - Mov:      dst <- a
    // - Un/Bin:   dst <- op(a[,b])
    // - Cmp:      dst <- cmp(a,b)
    // - Jz/Jnz:   a is condition value
    std::optional<VReg> dst{};
    std::optional<VReg> a{};
    std::optional<VReg> b{};

    // Immediate (LoadImm)
    std::int64_t imm = 0;

    // Operator tags
    BinOp binOp = BinOp::Add;
    UnOp  unOp  = UnOp::Neg;
    CmpOp cmpOp = CmpOp::EQ;

    // Call
    std::string callee{};
    std::vector<VReg> args{};

    // Branch target
    Label target{ -1 };

    // Memory operand (Lea/Load/Store)
    std::optional<MemRef> mem{};

    explicit Instr(Kind kk) : k(kk), target(-1) {}

    // -----------------------------
    // Constructors (helpers)
    // -----------------------------
    static Instr mov(VReg d, VReg src)
    {
        Instr i(Kind::Mov); i.dst = d; i.a = src; return i;
    }

    static Instr loadImm(VReg d, std::int64_t v)
    {
        Instr i(Kind::LoadImm); i.dst = d; i.imm = v; return i;
    }

    static Instr bin(VReg d, VReg x, BinOp op, VReg y)
    {
        Instr i(Kind::Bin); i.dst = d; i.a = x; i.b = y; i.binOp = op; return i;
    }

    static Instr un(VReg d, UnOp op, VReg x)
    {
        Instr i(Kind::Un); i.dst = d; i.a = x; i.unOp = op; return i;
    }

    static Instr cmp(VReg d, VReg x, CmpOp op, VReg y)
    {
        Instr i(Kind::Cmp); i.dst = d; i.a = x; i.b = y; i.cmpOp = op; return i;
    }

    static Instr call(std::optional<VReg> d, std::string name, std::vector<VReg> as)
    {
        Instr i(Kind::Call); i.dst = d; i.callee = std::move(name); i.args = std::move(as); return i;
    }

    static Instr ret(std::optional<VReg> x = std::nullopt)
    {
        Instr i(Kind::Ret); i.a = x; return i;
    }

    static Instr jmp(Label L)
    {
        Instr i(Kind::Jmp); i.target = L; return i;
    }

    static Instr jz(VReg x, Label L)
    {
        Instr i(Kind::JmpIfZero); i.a = x; i.target = L; return i;
    }

    static Instr jnz(VReg x, Label L)
    {
        Instr i(Kind::JmpIfNonZero); i.a = x; i.target = L; return i;
    }

    // Address/memory:
    // lea dst, [base + index*scale + disp]
    static Instr lea(VReg d, MemRef m)
    {
        Instr i(Kind::Lea); i.dst = d; i.mem = m; return i;
    }

    // load dst, [..]
    static Instr load(VReg d, MemRef m)
    {
        Instr i(Kind::Load); i.dst = d; i.mem = m; return i;
    }

    // store [..], src
    static Instr store(MemRef m, VReg src)
    {
        Instr i(Kind::Store); i.a = src; i.mem = m; return i;
    }
};

// -----------------------------
// Basic block
// -----------------------------
struct BasicBlock {
    Label label;
    std::vector<Instr> ins;

    // Successors as indices into FunctionIR::blocks (kept for your existing code).
    // If you prefer labels, you can later switch to vector<Label>.
    std::vector<int> succ;
};

// -----------------------------
// Function IR
// -----------------------------
struct FunctionIR {
    std::string name;

    VType retType = VType::Void;

    int argc = 0;
    std::vector<VReg> params; // vregs holding incoming args (assigned by codegen)
    std::vector<BasicBlock> blocks;

    int nextVRegId = 0;

    // vregId -> type (default I32)
    std::vector<VType> vtypes;

    VReg newVReg(VType t)
    {
        const int id = nextVRegId++;
        if ((int)vtypes.size() <= id) vtypes.resize((size_t)id + 1, VType::I32);
        vtypes[(size_t)id] = t;
        return VReg(id);
    }

    // backward-compatible default
    VReg newVReg() { return newVReg(VType::I32); }

    VType typeOf(VReg r) const
    {
        if (r.id < 0 || r.id >= (int)vtypes.size()) return VType::I32;
        return vtypes[(size_t)r.id];
    }
};

// -----------------------------
// Layout metadata for classes (struct/objects)
// -----------------------------
// Minimal contract:
// - codegen computes field offsets and object size
// - emitter just uses offsets in MemRef.disp
struct FieldLayout {
    std::string name;
    VType type = VType::I32; // for codegen; in practice Class pointers are PTR
    int offset = 0;          // bytes from object base
};

struct MethodSig {
    std::string name;        // source-level name
    std::string mangled;     // emitted symbol, e.g. "C__m" or "Base__m"
    VType retType = VType::Void;
    std::vector<VType> argTypes; // includes implicit self as arg0 if you choose
};

struct ClassLayout {
    std::string name;
    std::optional<std::string> base; // if extends
    int size = 0;                    // bytes
    std::vector<FieldLayout> fields;
    std::vector<MethodSig> methods;
};

// -----------------------------
// Module IR
// -----------------------------
struct ModuleIR {
    std::vector<FunctionIR> funs;
    std::vector<std::string> stringLits;

    // For structs/objects
    std::vector<ClassLayout> classes;

    // Optional: ABI/layout knobs for arrays
    // You can keep it fixed in runtime and just document it.
    // Example convention:
    //   array points to header; header[0]=len (i64), data starts at +8
    int arrayHeaderBytes = 8;
};
