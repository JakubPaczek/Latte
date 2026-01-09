#include "backend/x86_emit.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>

static constexpr const char* kArgRegs64[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8",  "%r9" };
static constexpr const char* kArgRegs32[6] = { "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d" };

static inline bool isRegOp(const std::string& s) { return !s.empty() && s[0] == '%'; }

static inline VType vtypeOf(const FunctionIR& f, int vregId)
{
    if (vregId < 0) return VType::I32;
    if ((size_t)vregId >= f.vtypes.size()) return VType::I32; // default
    return f.vtypes[(size_t)vregId];
}

const char* X86Emitter::r64(PhysReg pr)
{
    switch (pr)
    {
    case PhysReg::EAX: return "%rax";
    case PhysReg::ECX: return "%rcx";
    case PhysReg::EDX: return "%rdx";
    case PhysReg::EBX: return "%rbx";
    case PhysReg::ESI: return "%rsi";
    case PhysReg::EDI: return "%rdi";
    default: return "<?>"; // unknown reg
    }
}

const char* X86Emitter::r32(PhysReg pr)
{
    switch (pr)
    {
    case PhysReg::EAX: return "%eax";
    case PhysReg::ECX: return "%ecx";
    case PhysReg::EDX: return "%edx";
    case PhysReg::EBX: return "%ebx";
    case PhysReg::ESI: return "%esi";
    case PhysReg::EDI: return "%edi";
    default: return "<?>"; // unknown reg
    }
}

bool X86Emitter::isCalleeSaved(PhysReg pr)
{
    // SysV AMD64 callee-saved: RBX, RBP, R12-R15.
    return pr == PhysReg::EBX;
}

int X86Emitter::spillOffsetBytes(int spillSlot, int savedCount)
{
    const int savedBytes = 8 * savedCount;
    return -(savedBytes + 8 + 8 * spillSlot);
}

PhysReg X86Emitter::pickScratch(const std::unordered_set<PhysReg>& exclude)
{
    const PhysReg order[] = { PhysReg::EDX, PhysReg::ECX, PhysReg::EAX, PhysReg::ESI, PhysReg::EDI, PhysReg::EBX };
    for (auto pr : order) if (!exclude.count(pr)) return pr;
    throw std::runtime_error("no scratch register available");
}

X86Emitter::Temp X86Emitter::loadVRegToReg(std::ostream& out,
    const FunctionIR& f,
    const AllocResult& a,
    int vregId,
    const std::unordered_set<PhysReg>& exclude,
    int savedCount)
{
    const auto& loc = a.loc[(size_t)vregId];
    if (loc.isReg)
        return Temp{ loc.reg, false };

    PhysReg scratch = pickScratch(exclude);
    out << "  pushq " << r64(scratch) << "\n"; // temporary save

    int off = spillOffsetBytes(loc.spillSlot, savedCount);
    if (vtypeOf(f, vregId) == VType::PTR)
        out << "  movq " << off << "(%rbp), " << r64(scratch) << "\n";
    else
        out << "  movl " << off << "(%rbp), " << r32(scratch) << "\n"; // i32 load

    return Temp{ scratch, true };
}

void X86Emitter::storeRegToVReg(std::ostream& out,
    const FunctionIR& f,
    const AllocResult& a,
    PhysReg valueReg,
    int vregId,
    int savedCount)
{
    const auto& loc = a.loc[(size_t)vregId];
    const bool isPtr = (vtypeOf(f, vregId) == VType::PTR);

    if (loc.isReg)
    {
        if (loc.reg != valueReg)
        {
            if (isPtr) out << "  movq " << r64(valueReg) << ", " << r64(loc.reg) << "\n";
            else       out << "  movl " << r32(valueReg) << ", " << r32(loc.reg) << "\n";
        }
        return;
    }

    int off = spillOffsetBytes(loc.spillSlot, savedCount);
    if (isPtr) out << "  movq " << r64(valueReg) << ", " << off << "(%rbp)\n";
    else       out << "  movl " << r32(valueReg) << ", " << off << "(%rbp)\n"; // i32 store
}

void X86Emitter::emitFunction(std::ostream& out, const FunctionIR& f, const AllocResult& a)
{
    std::vector<PhysReg> saved;
    for (PhysReg pr : { PhysReg::EBX })
        if (a.usedCalleeSaved.count(pr) && isCalleeSaved(pr)) saved.push_back(pr);

    const int savedCount = (int)saved.size();

    const int localsBytes = 8 * a.spillSlots; // 8B spill slots
    int frameBytes = localsBytes;
    int padBytes = (16 - ((frameBytes + 8 * savedCount) % 16)) % 16;
    frameBytes += padBytes;

    out << ".globl " << f.name << "\n";
    out << f.name << ":\n";
    out << "  pushq %rbp\n";
    out << "  movq %rsp, %rbp\n";
    for (auto pr : saved) out << "  pushq " << r64(pr) << "\n";
    if (frameBytes > 0) out << "  subq $" << frameBytes << ", %rsp\n";

    auto spillAddr = [&](int vregId) -> std::string {
        const auto& loc = a.loc[(size_t)vregId];
        int off = spillOffsetBytes(loc.spillSlot, savedCount);
        return std::to_string(off) + "(%rbp)";
        };

    auto opnd64 = [&](int vregId) -> std::string {
        const auto& loc = a.loc[(size_t)vregId];
        if (loc.isReg) return std::string(r64(loc.reg));
        return spillAddr(vregId);
        };

    auto opnd32 = [&](int vregId) -> std::string {
        const auto& loc = a.loc[(size_t)vregId];
        if (loc.isReg) return std::string(r32(loc.reg));
        return spillAddr(vregId);
        };

    auto moveToHome = [&](const std::string& srcOp, int vregId, bool isPtr) {
        const auto& loc = a.loc[(size_t)vregId];

        if (loc.isReg)
        {
            std::string dst = isPtr ? r64(loc.reg) : r32(loc.reg);
            if (dst != srcOp)
                out << "  " << (isPtr ? "movq " : "movl ") << srcOp << ", " << dst << "\n";
        }
        else
        {
            std::string dstMem = spillAddr(vregId);
            if (isRegOp(srcOp))
            {
                out << "  " << (isPtr ? "movq " : "movl ") << srcOp << ", " << dstMem << "\n";
            }
            else
            {
                // mem -> mem via r11 // fixed scratch
                if (isPtr)
                {
                    out << "  movq " << srcOp << ", %r11\n";
                    out << "  movq %r11, " << dstMem << "\n";
                }
                else
                {
                    out << "  movl " << srcOp << ", %r11d\n";
                    out << "  movl %r11d, " << dstMem << "\n";
                }
            }
        }
        };

    // params: register + stack (>=7th)
    const int pcount = (int)f.params.size();
    const int regCount = std::min(pcount, 6);

    for (int i = regCount - 1; i >= 0; --i)
    {
        int v = f.params[(size_t)i].id;
        bool isPtr = (vtypeOf(f, v) == VType::PTR);
        moveToHome(isPtr ? kArgRegs64[i] : kArgRegs32[i], v, isPtr);
    }

    for (int i = 6; i < pcount; ++i)
    {
        int v = f.params[(size_t)i].id;
        bool isPtr = (vtypeOf(f, v) == VType::PTR);
        int off = 16 + 8 * (i - 6);
        moveToHome(std::to_string(off) + "(%rbp)", v, isPtr);
    }

    auto loadAcc = [&](int vregId) {
        if (vtypeOf(f, vregId) == VType::PTR)
            out << "  movq " << opnd64(vregId) << ", %rax\n";
        else
            out << "  movl " << opnd32(vregId) << ", %eax\n";
        };

    for (const auto& bb : f.blocks)
    {
        out << ".L" << bb.label.id << ":\n";

        for (const auto& ins : bb.ins)
        {
            switch (ins.k)
            {
            case Instr::Kind::LoadImm:
            {
                int d = ins.dst->id;
                bool isPtr = (vtypeOf(f, d) == VType::PTR);
                const auto& dl = a.loc[(size_t)d];

                if (dl.isReg)
                {
                    out << "  " << (isPtr ? "movq $" : "movl $") << ins.imm << ", "
                        << (isPtr ? r64(dl.reg) : r32(dl.reg)) << "\n";
                }
                else
                {
                    out << "  " << (isPtr ? "movq $" : "movl $") << ins.imm << ", "
                        << (isPtr ? "%rax\n" : "%eax\n");
                    storeRegToVReg(out, f, a, PhysReg::EAX, d, savedCount);
                }
            } break;

            case Instr::Kind::Mov:
            {
                int d = ins.dst->id;
                int s = ins.a->id;
                std::unordered_set<PhysReg> ex;
                if (a.loc[(size_t)d].isReg) ex.insert(a.loc[(size_t)d].reg);
                auto ts = loadVRegToReg(out, f, a, s, ex, savedCount);
                storeRegToVReg(out, f, a, ts.pr, d, savedCount);
                if (ts.saved) out << "  popq " << r64(ts.pr) << "\n";
            } break;

            case Instr::Kind::Un:
            {
                int d = ins.dst->id;
                int x = ins.a->id;

                loadAcc(x);

                if (ins.unOp == UnOp::Neg)
                {
                    out << "  negl %eax\n"; // i32 neg
                }
                else
                {
                    out << "  testl %eax, %eax\n";
                    out << "  sete %al\n";
                    out << "  movzbl %al, %eax\n";
                }

                storeRegToVReg(out, f, a, PhysReg::EAX, d, savedCount);
            } break;

            case Instr::Kind::Bin:
            {
                int d = ins.dst->id;
                int x = ins.a->id;
                int y = ins.b->id;

                loadAcc(x);

                switch (ins.binOp)
                {
                case BinOp::Add: out << "  addl " << opnd32(y) << ", %eax\n"; break;
                case BinOp::Sub: out << "  subl " << opnd32(y) << ", %eax\n"; break;
                case BinOp::Mul: out << "  imull " << opnd32(y) << ", %eax\n"; break;
                case BinOp::And: out << "  andl " << opnd32(y) << ", %eax\n"; break;
                case BinOp::Or:  out << "  orl  " << opnd32(y) << ", %eax\n"; break;
                case BinOp::Xor: out << "  xorl " << opnd32(y) << ", %eax\n"; break;
                case BinOp::Div:
                case BinOp::Mod:
                {
                    // eax = x
                    // edx:eax = sign-extend
                    // idivl y
                    // quotient in eax, remainder in edx
                    out << "  movl " << opnd32(x) << ", %eax\n";
                    out << "  cdq\n"; // sign-extend eax into edx
                    // idiv doesn't accept mem+mem; operand can be reg or mem => OK
                    out << "  idivl " << opnd32(y) << "\n";
                    if (ins.binOp == BinOp::Mod)
                        out << "  movl %edx, %eax\n";
                    storeRegToVReg(out, f, a, PhysReg::EAX, d, savedCount);
                    break;
                }

                }

                storeRegToVReg(out, f, a, PhysReg::EAX, d, savedCount);
            } break;

            case Instr::Kind::Cmp:
            {
                int d = ins.dst->id;
                int x = ins.a->id;
                int y = ins.b->id;

                // comparisons are i32 in Latte (incl strcmp result)
                out << "  movl " << opnd32(x) << ", %eax\n";
                out << "  cmpl " << opnd32(y) << ", %eax\n";

                switch (ins.cmpOp)
                {
                case CmpOp::EQ: out << "  sete %al\n"; break;
                case CmpOp::NE: out << "  setne %al\n"; break;
                case CmpOp::LT: out << "  setl %al\n"; break;
                case CmpOp::LE: out << "  setle %al\n"; break;
                case CmpOp::GT: out << "  setg %al\n"; break;
                case CmpOp::GE: out << "  setge %al\n"; break;
                }
                out << "  movzbl %al, %eax\n";

                storeRegToVReg(out, f, a, PhysReg::EAX, d, savedCount);
            } break;

            case Instr::Kind::Call:
            {
                const int n = (int)ins.args.size();
                const int regN = std::min(n, 6);
                const int stackN = std::max(0, n - 6);

                // Stage reg args safely // parallel-move safe
                const int tempRaw = 8 * regN;
                const int tempBytes = (tempRaw == 0) ? 0 : ((tempRaw + 15) / 16) * 16;
                if (tempBytes) out << "  subq $" << tempBytes << ", %rsp\n";

                for (int i = 0; i < regN; ++i)
                {
                    int v = ins.args[(size_t)i].id;
                    bool isPtr = (vtypeOf(f, v) == VType::PTR);
                    std::string src = isPtr ? opnd64(v) : opnd32(v);
                    int off = 8 * i;
                    std::string dstMem = std::to_string(off) + "(%rsp)";

                    if (isRegOp(src))
                    {
                        out << "  " << (isPtr ? "movq " : "movl ") << src << ", " << dstMem << "\n";
                    }
                    else
                    {
                        out << "  " << (isPtr ? "movq " : "movl ") << src << ", " << (isPtr ? "%r11\n" : "%r11d\n");
                        out << "  " << (isPtr ? "movq " : "movl ") << (isPtr ? "%r11" : "%r11d") << ", " << dstMem << "\n";
                    }
                }

                for (int i = 0; i < regN; ++i)
                {
                    int v = ins.args[(size_t)i].id;
                    bool isPtr = (vtypeOf(f, v) == VType::PTR);
                    int off = 8 * i;
                    out << "  " << (isPtr ? "movq " : "movl ")
                        << off << "(%rsp), " << (isPtr ? kArgRegs64[i] : kArgRegs32[i]) << "\n";
                }

                if (tempBytes) out << "  addq $" << tempBytes << ", %rsp\n";

                // Stack args right-to-left; keep 16B align at call-site
                const int pad = (stackN % 2) ? 8 : 0;
                if (pad) out << "  subq $" << pad << ", %rsp\n";

                for (int i = n - 1; i >= 6; --i)
                {
                    int v = ins.args[(size_t)i].id;
                    bool isPtr = (vtypeOf(f, v) == VType::PTR);

                    if (isPtr)
                    {
                        out << "  pushq " << opnd64(v) << "\n";
                    }
                    else
                    {
                        // avoid pushing garbage high dword from spill // clean upper bits
                        const auto& loc = a.loc[(size_t)v];
                        if (loc.isReg)
                        {
                            out << "  pushq " << r64(loc.reg) << "\n";
                        }
                        else
                        {
                            out << "  movl " << opnd32(v) << ", %r11d\n";
                            out << "  pushq %r11\n";
                        }
                    }
                }

                out << "  call " << ins.callee << "\n";

                const int cleanup = 8 * stackN + pad;
                if (cleanup) out << "  addq $" << cleanup << ", %rsp\n";

                if (ins.dst)
                {
                    int d = ins.dst->id;
                    if (vtypeOf(f, d) == VType::PTR)
                        storeRegToVReg(out, f, a, PhysReg::EAX, d, savedCount); // uses %rax via type
                    else
                        storeRegToVReg(out, f, a, PhysReg::EAX, d, savedCount); // uses %eax via type
                }
            } break;

            case Instr::Kind::Ret:
            {
                if (ins.a)
                {
                    int v = ins.a->id;
                    bool isPtr = (vtypeOf(f, v) == VType::PTR);
                    const auto& loc = a.loc[(size_t)v];

                    if (loc.isReg)
                    {
                        if (isPtr)
                        {
                            if (loc.reg != PhysReg::EAX) out << "  movq " << r64(loc.reg) << ", %rax\n";
                        }
                        else
                        {
                            if (loc.reg != PhysReg::EAX) out << "  movl " << r32(loc.reg) << ", %eax\n";
                        }
                    }
                    else
                    {
                        int off = spillOffsetBytes(loc.spillSlot, savedCount);
                        out << "  " << (isPtr ? "movq " : "movl ") << off << "(%rbp), " << (isPtr ? "%rax\n" : "%eax\n");
                    }
                }

                if (frameBytes > 0) out << "  addq $" << frameBytes << ", %rsp\n";
                for (int i = (int)saved.size() - 1; i >= 0; --i) out << "  popq " << r64(saved[(size_t)i]) << "\n";
                out << "  popq %rbp\n";
                out << "  ret\n";
            } break;

            case Instr::Kind::Jmp:
                out << "  jmp .L" << ins.target.id << "\n";
                break;

            case Instr::Kind::JmpIfZero:
            {
                int v = ins.a->id;
                std::unordered_set<PhysReg> ex;
                auto tv = loadVRegToReg(out, f, a, v, ex, savedCount);
                out << "  testl " << r32(tv.pr) << ", " << r32(tv.pr) << "\n";
                out << "  je .L" << ins.target.id << "\n";
                if (tv.saved) out << "  popq " << r64(tv.pr) << "\n";
            } break;

            case Instr::Kind::JmpIfNonZero:
            {
                int v = ins.a->id;
                std::unordered_set<PhysReg> ex;
                auto tv = loadVRegToReg(out, f, a, v, ex, savedCount);
                out << "  testl " << r32(tv.pr) << ", " << r32(tv.pr) << "\n";
                out << "  jne .L" << ins.target.id << "\n";
                if (tv.saved) out << "  popq " << r64(tv.pr) << "\n";
            } break;
            }
        }
    }
}

void X86Emitter::emitFunction(FILE* outF, const FunctionIR& f, const AllocResult& a)
{
    if (!outF) throw std::runtime_error("emitFunction(FILE*): null FILE*");

    std::ostringstream oss;
    emitFunction(oss, f, a);

    const std::string s = oss.str();
    if (std::fwrite(s.data(), 1, s.size(), outF) != s.size())
        throw std::runtime_error("emitFunction(FILE*): fwrite failed");
}
