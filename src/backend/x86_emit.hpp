#pragma once
#include "ir.hpp"
#include "regalloc.hpp"
#include <ostream>
#include <unordered_set>

class X86Emitter {
public:
    void emitFunction(std::ostream& out, const FunctionIR& f, const AllocResult& a);
    void emitFunction(FILE* out, const FunctionIR& f, const AllocResult& a);

private:
    static const char* r64(PhysReg pr);
    static const char* r32(PhysReg pr);
    static bool isCalleeSaved(PhysReg pr);

    static int spillOffsetBytes(int spillSlot, int savedCount);

    struct Temp {
        PhysReg pr = PhysReg::NONE;
        bool saved = false;
    };

    Temp loadVRegToReg(std::ostream& out,
        const FunctionIR& f,
        const AllocResult& a,
        int vregId,
        const std::unordered_set<PhysReg>& exclude,
        int savedCount);

    void storeRegToVReg(std::ostream& out,
        const FunctionIR& f,
        const AllocResult& a,
        PhysReg valueReg,
        int vregId,
        int savedCount);

    PhysReg pickScratch(const std::unordered_set<PhysReg>& exclude);
};
