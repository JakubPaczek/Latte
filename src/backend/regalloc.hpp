#pragma once
#include "ir.hpp"
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class PhysReg { EAX, ECX, EDX, EBX, ESI, EDI, NONE };

struct Location {
    bool isReg = true;
    PhysReg reg = PhysReg::NONE;
    int spillSlot = -1; // if !isReg
};

struct AllocResult {
    // vreg.id -> location
    std::vector<Location> loc;

    // how many spill slots (each 8 bytes on x86_64) // 64-bit spills
    int spillSlots = 0;

    // which callee-saved regs are used (SysV x86_64: only EBX among current set)
    std::unordered_set<PhysReg> usedCalleeSaved;
};

class RegAllocator {
public:
    AllocResult allocate(const FunctionIR& f);

private:
    struct Interval {
        int v = -1;
        int start = 0;
        int end = 0;
        bool spansCall = false;

        bool isReg = false;
        PhysReg pr = PhysReg::NONE;
        int spillSlot = -1;
    };

    // Liveness
    struct BlockSets {
        std::unordered_set<int> use, def;
    };

    static std::unordered_set<int> setUnion(const std::unordered_set<int>& a,
        const std::unordered_set<int>& b);
    static std::unordered_set<int> setDiff(const std::unordered_set<int>& a,
        const std::unordered_set<int>& b);

    static void computeUseDef(const FunctionIR& f, std::vector<BlockSets>& out);
    static void computeLiveInOut(const FunctionIR& f,
        const std::vector<BlockSets>& sets,
        std::vector<std::unordered_set<int>>& liveIn,
        std::vector<std::unordered_set<int>>& liveOut);

    static std::unordered_set<int> computeSpansCallRegs(const FunctionIR& f,
        const std::vector<BlockSets>& sets,
        const std::vector<std::unordered_set<int>>& liveOut);

    static std::vector<Interval> buildIntervals(const FunctionIR& f,
        const std::vector<std::unordered_set<int>>& liveOut,
        const std::unordered_set<int>& spansCallRegs);

    static bool isCalleeSaved(PhysReg r);
    static bool isAllocable(PhysReg r);

    static std::vector<PhysReg> allRegs();         // may include caller-saved
    static std::vector<PhysReg> calleeSavedRegs(); // safe across call

    // Linear scan
    static void expireOld(std::vector<Interval*>& active, int curStart, std::array<bool, 6>& free);
    static int  regIndex(PhysReg r);
    static PhysReg idxReg(int i);
};
