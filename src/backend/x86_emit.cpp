#include "backend/x86_emit.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>


const char* X86Emitter::r(PhysReg pr)
{
    switch (pr)
    {
    case PhysReg::EAX: return "%eax";
    case PhysReg::ECX: return "%ecx";
    case PhysReg::EDX: return "%edx";
    case PhysReg::EBX: return "%ebx";
    case PhysReg::ESI: return "%esi";
    case PhysReg::EDI: return "%edi";
    default: return "<?>";
    }
}

bool X86Emitter::isCalleeSaved(PhysReg pr)
{
    return pr == PhysReg::EBX || pr == PhysReg::ESI || pr == PhysReg::EDI;
}

int X86Emitter::spillOffsetBytes(int spillSlot, int savedCount)
{
    const int savedBytes = 4 * savedCount;
    return -(savedBytes + 4 + 4 * spillSlot);
}

PhysReg X86Emitter::pickScratch(const std::unordered_set<PhysReg>& exclude)
{
    const PhysReg order[] = { PhysReg::EDX, PhysReg::ECX, PhysReg::EAX, PhysReg::ESI, PhysReg::EDI, PhysReg::EBX };
    for (auto pr : order) if (!exclude.count(pr)) return pr;
    throw std::runtime_error("no scratch register available");
}

X86Emitter::Temp X86Emitter::loadVRegToReg(std::ostream& out,
    const AllocResult& a,
    int vregId,
    const std::unordered_set<PhysReg>& exclude,
    int savedCount)
{
    const auto& loc = a.loc[(size_t)vregId];
    if (loc.isReg)
    {
        return Temp{ loc.reg, false };
    }

    PhysReg scratch = pickScratch(exclude);
    out << "  pushl " << r(scratch) << "\n";
    int off = spillOffsetBytes(loc.spillSlot, savedCount);
    out << "  movl " << off << "(%ebp), " << r(scratch) << "\n";
    return Temp{ scratch, true };
}

void X86Emitter::storeRegToVReg(std::ostream& out,
    const AllocResult& a,
    PhysReg valueReg,
    int vregId,
    int savedCount)
{
    const auto& loc = a.loc[(size_t)vregId];
    if (loc.isReg)
    {
        if (loc.reg != valueReg)
        {
            out << "  movl " << r(valueReg) << ", " << r(loc.reg) << "\n";
        }
        return;
    }
    int off = spillOffsetBytes(loc.spillSlot, savedCount);
    out << "  movl " << r(valueReg) << ", " << off << "(%ebp)\n";
}


void X86Emitter::emitFunction(std::ostream& out, const FunctionIR& f, const AllocResult& a)
{
    std::vector<PhysReg> saved;
    for (PhysReg pr : {PhysReg::EBX, PhysReg::ESI, PhysReg::EDI})
    {
        if (a.usedCalleeSaved.count(pr)) saved.push_back(pr);
    }
    const int savedCount = (int)saved.size();
    const int localsBytes = 4 * a.spillSlots;

    // Chcemy: %esp 16-byte aligned po prologu (i między instrukcjami),
    // żeby łatwo doalignować call’e.
    int frameBytes = localsBytes;

    // Po wejściu do funkcji (po call) stos jest przesunięty o 4 bajty (return address).
    // Po: push %ebp (4) -> +4, po pushach callee-saved -> +4*savedCount,
    // po sub frameBytes -> +frameBytes.
    // Dobieramy alignBytes tak, by finalnie esp % 16 == 0.
    int alignBytes = (16 - ((8 + 4 * savedCount + frameBytes) % 16)) % 16;
    frameBytes += alignBytes;

    out << ".globl " << f.name << "\n";
    out << f.name << ":\n";
    out << "  pushl %ebp\n";
    out << "  movl %esp, %ebp\n";
    for (auto pr : saved) out << "  pushl " << r(pr) << "\n";
    if (frameBytes > 0) out << "  subl $" << frameBytes << ", %esp\n";

    // params: [ebp+8], [ebp+12], ...
    for (int i = 0; i < (int)f.params.size(); ++i)
    {
        int argOff = 8 + 4 * i;
        int v = f.params[(size_t)i].id;
        const auto& loc = a.loc[(size_t)v];
        if (loc.isReg)
        {
            out << "  movl " << argOff << "(%ebp), " << r(loc.reg) << "\n";
        }
        else
        {
            out << "  movl " << argOff << "(%ebp), %eax\n";
            int off = spillOffsetBytes(loc.spillSlot, savedCount);
            out << "  movl %eax, " << off << "(%ebp)\n";
        }
    }

    auto opnd = [&](int vregId) -> std::string {
        const auto& loc = a.loc[(size_t)vregId];
        if (loc.isReg) return std::string(r(loc.reg));
        int off = spillOffsetBytes(loc.spillSlot, savedCount);
        return std::to_string(off) + "(%ebp)";
        };

    auto loadEAX = [&](int vregId) {
        out << "  movl " << opnd(vregId) << ", %eax\n";
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
                const auto& dl = a.loc[(size_t)d];
                if (dl.isReg)
                {
                    out << "  movl $" << ins.imm << ", " << r(dl.reg) << "\n";
                }
                else
                {
                    out << "  movl $" << ins.imm << ", %eax\n";
                    storeRegToVReg(out, a, PhysReg::EAX, d, savedCount);
                }
            } break;

            case Instr::Kind::Mov:
            {
                int d = ins.dst->id;
                int s = ins.a->id;
                std::unordered_set<PhysReg> ex;
                if (a.loc[(size_t)d].isReg) ex.insert(a.loc[(size_t)d].reg);
                auto ts = loadVRegToReg(out, a, s, ex, savedCount);
                storeRegToVReg(out, a, ts.pr, d, savedCount);
                if (ts.saved) out << "  popl " << r(ts.pr) << "\n";
            } break;

            case Instr::Kind::Un:
            {
                int d = ins.dst->id;
                int x = ins.a->id;

                loadEAX(x);

                if (ins.unOp == UnOp::Neg)
                {
                    out << "  negl %eax\n";
                }
                else
                {
                    out << "  testl %eax, %eax\n";
                    out << "  sete %al\n";
                    out << "  movzbl %al, %eax\n";
                }

                storeRegToVReg(out, a, PhysReg::EAX, d, savedCount);
            }
            break;


            case Instr::Kind::Bin:
            {
                int d = ins.dst->id;
                int x = ins.a->id;
                int y = ins.b->id;

                loadEAX(x);

                switch (ins.binOp)
                {
                case BinOp::Add: out << "  addl " << opnd(y) << ", %eax\n"; break;
                case BinOp::Sub: out << "  subl " << opnd(y) << ", %eax\n"; break;
                case BinOp::Mul: out << "  imull " << opnd(y) << ", %eax\n"; break;
                case BinOp::And: out << "  andl " << opnd(y) << ", %eax\n"; break;
                case BinOp::Or:  out << "  orl  " << opnd(y) << ", %eax\n"; break;
                case BinOp::Xor: out << "  xorl " << opnd(y) << ", %eax\n"; break;
                }

                storeRegToVReg(out, a, PhysReg::EAX, d, savedCount);
            }
            break;


            case Instr::Kind::Cmp:
            {
                int d = ins.dst->id;
                int x = ins.a->id;
                int y = ins.b->id;

                loadEAX(x);
                out << "  cmpl " << opnd(y) << ", %eax\n";

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

                storeRegToVReg(out, a, PhysReg::EAX, d, savedCount);
            }
            break;


            case Instr::Kind::Call:
            {
                const int n = (int)ins.args.size();
                const int argBytes = 4 * n;

                // 16-byte alignment przed call
                const int pad = (16 - (argBytes % 16)) % 16;
                if (pad) out << "  subl $" << pad << ", %esp\n";

                // push args (cdecl): od końca
                for (int i = n - 1; i >= 0; --i)
                {
                    int v = ins.args[(size_t)i].id;
                    const auto& loc = a.loc[(size_t)v];

                    if (loc.isReg)
                    {
                        out << "  pushl " << r(loc.reg) << "\n";
                    }
                    else
                    {
                        int off = spillOffsetBytes(loc.spillSlot, savedCount);
                        out << "  pushl " << off << "(%ebp)\n";
                    }
                }

                out << "  call " << ins.callee << "\n";

                // caller cleans args + pad
                const int cleanup = argBytes + pad;
                if (cleanup) out << "  addl $" << cleanup << ", %esp\n";

                if (ins.dst)
                    storeRegToVReg(out, a, PhysReg::EAX, ins.dst->id, savedCount);
            }
            break;






            case Instr::Kind::Ret:
            {
                if (ins.a)
                {
                    int v = ins.a->id;
                    const auto& loc = a.loc[(size_t)v];
                    if (loc.isReg)
                    {
                        if (loc.reg != PhysReg::EAX) out << "  movl " << r(loc.reg) << ", %eax\n";
                    }
                    else
                    {
                        int off = spillOffsetBytes(loc.spillSlot, savedCount);
                        out << "  movl " << off << "(%ebp), %eax\n";
                    }
                }

                if (localsBytes > 0) out << "  addl $" << localsBytes << ", %esp\n";
                for (int i = (int)saved.size() - 1; i >= 0; --i) out << "  popl " << r(saved[(size_t)i]) << "\n";
                out << "  popl %ebp\n";
                out << "  ret\n";
            } break;

            case Instr::Kind::Jmp:
                out << "  jmp .L" << ins.target.id << "\n";
                break;

            case Instr::Kind::JmpIfZero:
            {
                int v = ins.a->id;
                std::unordered_set<PhysReg> ex;
                auto tv = loadVRegToReg(out, a, v, ex, savedCount);
                out << "  testl " << r(tv.pr) << ", " << r(tv.pr) << "\n";
                out << "  je .L" << ins.target.id << "\n";
                if (tv.saved) out << "  popl " << r(tv.pr) << "\n";
            } break;

            case Instr::Kind::JmpIfNonZero:
            {
                int v = ins.a->id;
                std::unordered_set<PhysReg> ex;
                auto tv = loadVRegToReg(out, a, v, ex, savedCount);
                out << "  testl " << r(tv.pr) << ", " << r(tv.pr) << "\n";
                out << "  jne .L" << ins.target.id << "\n";
                if (tv.saved) out << "  popl " << r(tv.pr) << "\n";
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
    {
        throw std::runtime_error("emitFunction(FILE*): fwrite failed");
    }
}
