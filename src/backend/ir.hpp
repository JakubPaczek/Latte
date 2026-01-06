#pragma once
#include <string>
#include <vector>
#include <optional>

struct VReg {
    int id = -1;
    explicit VReg(int i = -1) : id(i) {}
    friend bool operator==(VReg a, VReg b) { return a.id == b.id; }
};

struct Label {
    int id = -1;
    explicit Label(int i = -1) : id(i) {}
};

enum class BinOp { Add, Sub, Mul, And, Or, Xor };
enum class UnOp { Neg, Not };
enum class CmpOp { EQ, NE, LT, LE, GT, GE };

struct Instr {
    enum class Kind {
        Mov,        // dst = src
        LoadImm,    // dst = imm
        Bin,        // dst = a (op) b
        Un,         // dst = (op) a
        Cmp,        // dst = (a cmp b) -> 0/1
        Call,       // dst? = call name(args)
        Ret,        // return a? (if none => void)
        Jmp,        // jump label
        JmpIfZero,  // if (a==0) jump label
        JmpIfNonZero// if (a!=0) jump label
    } k;

    std::optional<VReg> dst{};
    std::optional<VReg> a{};
    std::optional<VReg> b{};

    long imm = 0;
    BinOp binOp = BinOp::Add;
    UnOp  unOp = UnOp::Neg;
    CmpOp cmpOp = CmpOp::EQ;

    std::string callee{};
    std::vector<VReg> args{};

    Label target{ -1 };

    explicit Instr(Kind kk) : k(kk), target(-1) {}

    // Helpers:
    static Instr mov(VReg d, VReg src)
    {
        Instr i(Kind::Mov); i.dst = d; i.a = src; return i;
    }
    static Instr loadImm(VReg d, long v)
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
    static Instr jmp(Label L) { Instr i(Kind::Jmp); i.target = L; return i; }
    static Instr jz(VReg x, Label L) { Instr i(Kind::JmpIfZero); i.a = x; i.target = L; return i; }
    static Instr jnz(VReg x, Label L) { Instr i(Kind::JmpIfNonZero); i.a = x; i.target = L; return i; }
};

struct BasicBlock {
    Label label;
    std::vector<Instr> ins;
    std::vector<int> succ; // indices of successor blocks
};

struct FunctionIR {
    std::string name;
    int argc = 0;
    std::vector<VReg> params;
    std::vector<BasicBlock> blocks;

    int nextVRegId = 0;
    VReg newVReg() { return VReg(nextVRegId++); }
};

struct ModuleIR {
    std::vector<FunctionIR> funs;

    std::vector<std::string> stringLits;
};
