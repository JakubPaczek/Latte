#pragma once
#include "ir.hpp"
#include "regalloc.hpp"
#include <ostream>
#include <unordered_set>

class X86Emitter {
public:
    // Emits one function in AT&T syntax for i386 System V cdecl.
    void emitFunction(std::ostream& out, const FunctionIR& f, const AllocResult& a);
    void emitFunction(FILE* out, const FunctionIR& f, const AllocResult& a);

private:
    static const char* r(PhysReg pr);
    static bool isCalleeSaved(PhysReg pr);

    // stack layout:
    //  push %ebp
    //  mov  %esp, %ebp
    //  push used callee-saved regs (subset of EBX/ESI/EDI)
    //  sub  $localsBytes, %esp
    //
    // spill slot i is at: -(savedBytes + 4 + 4*i)(%ebp)
    // where savedBytes = 4 * savedCount
    static int spillOffsetBytes(int spillSlot, int savedCount);

    struct Temp {
        PhysReg pr = PhysReg::NONE;
        bool saved = false; // whether we pushed it and must pop later
    };

    Temp loadVRegToReg(std::ostream& out,
        const AllocResult& a,
        int vregId,
        const std::unordered_set<PhysReg>& exclude,
        int savedCount);

    void storeRegToVReg(std::ostream& out,
        const AllocResult& a,
        PhysReg valueReg,
        int vregId,
        int savedCount);

    PhysReg pickScratch(const std::unordered_set<PhysReg>& exclude);
};
