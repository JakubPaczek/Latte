#include "backend/regalloc.hpp"
#include <algorithm>
#include <stdexcept>

static int maxVRegId(const FunctionIR& f)
{
    return f.nextVRegId;
}

std::unordered_set<int> RegAllocator::setUnion(const std::unordered_set<int>& a,
    const std::unordered_set<int>& b)
{
    std::unordered_set<int> r = a;
    r.insert(b.begin(), b.end());
    return r;
}

std::unordered_set<int> RegAllocator::setDiff(const std::unordered_set<int>& a,
    const std::unordered_set<int>& b)
{
    std::unordered_set<int> r;
    for (int x : a) if (!b.count(x)) r.insert(x);
    return r;
}

void RegAllocator::computeUseDef(const FunctionIR& f, std::vector<BlockSets>& out)
{
    out.assign(f.blocks.size(), {});
    for (size_t bi = 0; bi < f.blocks.size(); ++bi)
    {
        auto& bs = out[bi];
        for (const auto& ins : f.blocks[bi].ins)
        {
            auto useReg = [&](const std::optional<VReg>& vr) {
                if (!vr) return;
                int v = vr->id;
                if (!bs.def.count(v)) bs.use.insert(v);
                };
            auto defReg = [&](const std::optional<VReg>& vr) {
                if (!vr) return;
                bs.def.insert(vr->id);
                };

            switch (ins.k)
            {
            case Instr::Kind::Mov:
                useReg(ins.a); defReg(ins.dst); break;
            case Instr::Kind::LoadImm:
                defReg(ins.dst); break;
            case Instr::Kind::Bin:
                useReg(ins.a); useReg(ins.b); defReg(ins.dst); break;
            case Instr::Kind::Un:
                useReg(ins.a); defReg(ins.dst); break;
            case Instr::Kind::Cmp:
                useReg(ins.a); useReg(ins.b); defReg(ins.dst); break;
            case Instr::Kind::Call:
                for (auto vr : ins.args)
                {
                    if (!bs.def.count(vr.id)) bs.use.insert(vr.id);
                }
                defReg(ins.dst); break;
            case Instr::Kind::Ret:
                useReg(ins.a); break;
            case Instr::Kind::Jmp:
                break;
            case Instr::Kind::JmpIfZero:
            case Instr::Kind::JmpIfNonZero:
                useReg(ins.a); break;
            }
        }
    }
}

void RegAllocator::computeLiveInOut(const FunctionIR& f,
    const std::vector<BlockSets>& sets,
    std::vector<std::unordered_set<int>>& liveIn,
    std::vector<std::unordered_set<int>>& liveOut)
{
    const size_t n = f.blocks.size();
    liveIn.assign(n, {});
    liveOut.assign(n, {});

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (int bi = (int)n - 1; bi >= 0; --bi)
        {
            std::unordered_set<int> outSet;
            for (int s : f.blocks[bi].succ)
            {
                outSet = setUnion(outSet, liveIn[(size_t)s]);
            }

            std::unordered_set<int> inSet = setUnion(sets[(size_t)bi].use,
                setDiff(outSet, sets[(size_t)bi].def));

            if (outSet != liveOut[(size_t)bi]) { liveOut[(size_t)bi] = std::move(outSet); changed = true; }
            if (inSet != liveIn[(size_t)bi]) { liveIn[(size_t)bi] = std::move(inSet);  changed = true; }
        }
    }
}

std::unordered_set<int> RegAllocator::computeSpansCallRegs(
    const FunctionIR& f,
    const std::vector<BlockSets>&,
    const std::vector<std::unordered_set<int>>& liveOut)
{

    std::unordered_set<int> spans;

    for (size_t bi = 0; bi < f.blocks.size(); ++bi)
    {
        auto live = liveOut[bi];
        const auto& insv = f.blocks[bi].ins;

        for (int i = (int)insv.size() - 1; i >= 0; --i)
        {
            const auto& ins = insv[(size_t)i];
            auto liveAfter = live;

            std::unordered_set<int> defs, uses;
            auto addUse = [&](const std::optional<VReg>& vr) { if (vr) uses.insert(vr->id); };
            auto addDef = [&](const std::optional<VReg>& vr) { if (vr) defs.insert(vr->id); };

            switch (ins.k)
            {
            case Instr::Kind::Mov:     addUse(ins.a); addDef(ins.dst); break;
            case Instr::Kind::LoadImm: addDef(ins.dst); break;
            case Instr::Kind::Bin:     addUse(ins.a); addUse(ins.b); addDef(ins.dst); break;
            case Instr::Kind::Un:      addUse(ins.a); addDef(ins.dst); break;
            case Instr::Kind::Cmp:     addUse(ins.a); addUse(ins.b); addDef(ins.dst); break;
            case Instr::Kind::Call:
                for (auto vr : ins.args) uses.insert(vr.id);
                addDef(ins.dst);
                break;
            case Instr::Kind::Ret:     addUse(ins.a); break;
            case Instr::Kind::Jmp:     break;
            case Instr::Kind::JmpIfZero:
            case Instr::Kind::JmpIfNonZero:
                addUse(ins.a); break;
            }

            auto liveBefore = setUnion(setDiff(liveAfter, defs), uses);

            if (ins.k == Instr::Kind::Call)
            {
                for (int v : liveBefore)
                {
                    if (liveAfter.count(v)) spans.insert(v);
                }
            }

            live = std::move(liveBefore);
        }
    }

    return spans;
}

std::vector<RegAllocator::Interval> RegAllocator::buildIntervals(
    const FunctionIR& f,
    const std::vector<std::unordered_set<int>>& liveOut,
    const std::unordered_set<int>& spansCallRegs)
{

    const int V = maxVRegId(f);
    std::vector<int> start(V, 1e9);
    std::vector<int> end(V, -1);

    int pos = 0;
    std::vector<int> blockEndPos(f.blocks.size(), 0);

    for (size_t bi = 0; bi < f.blocks.size(); ++bi)
    {
        for (const auto& ins : f.blocks[bi].ins)
        {
            auto touch = [&](const std::optional<VReg>& vr) {
                if (!vr) return;
                int v = vr->id;
                start[v] = std::min(start[v], pos);
                end[v] = std::max(end[v], pos);
                };

            switch (ins.k)
            {
            case Instr::Kind::Mov:
                touch(ins.a); touch(ins.dst); break;
            case Instr::Kind::LoadImm:
                touch(ins.dst); break;
            case Instr::Kind::Bin:
                touch(ins.a); touch(ins.b); touch(ins.dst); break;
            case Instr::Kind::Un:
                touch(ins.a); touch(ins.dst); break;
            case Instr::Kind::Cmp:
                touch(ins.a); touch(ins.b); touch(ins.dst); break;
            case Instr::Kind::Call:
                for (auto vr : ins.args) touch(std::optional<VReg>(vr));
                touch(ins.dst);
                break;
            case Instr::Kind::Ret:
                touch(ins.a); break;
            case Instr::Kind::Jmp:
                break;
            case Instr::Kind::JmpIfZero:
            case Instr::Kind::JmpIfNonZero:
                touch(ins.a); break;
            }

            ++pos;
        }
        blockEndPos[bi] = std::max(0, pos - 1);
    }

    for (size_t bi = 0; bi < f.blocks.size(); ++bi)
    {
        for (int v : liveOut[bi])
        {
            if (end[v] >= 0) end[v] = std::max(end[v], blockEndPos[bi]);
        }
    }

    std::vector<Interval> intervals;
    intervals.reserve((size_t)V);

    for (int v = 0; v < V; ++v)
    {
        if (end[v] < 0) continue;
        Interval in;
        in.v = v;
        in.start = start[v];
        in.end = end[v];
        in.spansCall = spansCallRegs.count(v) > 0;
        intervals.push_back(in);
    }

    std::sort(intervals.begin(), intervals.end(),
        [](const Interval& x, const Interval& y) { return x.start < y.start; });

    return intervals;
}

bool RegAllocator::isCalleeSaved(PhysReg r)
{
    return r == PhysReg::EBX || r == PhysReg::ESI || r == PhysReg::EDI;
}

std::vector<PhysReg> RegAllocator::allRegs()
{
    return { PhysReg::ECX, PhysReg::EDX, PhysReg::EBX, PhysReg::ESI, PhysReg::EDI };
}


std::vector<PhysReg> RegAllocator::calleeSavedRegs()
{
    return { PhysReg::EBX, PhysReg::ESI, PhysReg::EDI };
}

int RegAllocator::regIndex(PhysReg r)
{
    switch (r)
    {
    case PhysReg::EAX: return 0;
    case PhysReg::ECX: return 1;
    case PhysReg::EDX: return 2;
    case PhysReg::EBX: return 3;
    case PhysReg::ESI: return 4;
    case PhysReg::EDI: return 5;
    default: return -1;
    }
}

PhysReg RegAllocator::idxReg(int i)
{
    switch (i)
    {
    case 0: return PhysReg::EAX;
    case 1: return PhysReg::ECX;
    case 2: return PhysReg::EDX;
    case 3: return PhysReg::EBX;
    case 4: return PhysReg::ESI;
    case 5: return PhysReg::EDI;
    default: return PhysReg::NONE;
    }
}

void RegAllocator::expireOld(std::vector<Interval*>& active, int curStart, std::array<bool, 6>& free)
{
    while (!active.empty())
    {
        Interval* in = active.front();
        if (in->end >= curStart) break;
        if (in->isReg)
        {
            int idx = regIndex(in->pr);
            if (idx >= 0) free[(size_t)idx] = true;
        }
        active.erase(active.begin());
    }
}

AllocResult RegAllocator::allocate(const FunctionIR& f)
{
    std::vector<BlockSets> sets;
    computeUseDef(f, sets);

    std::vector<std::unordered_set<int>> liveIn, liveOut;
    computeLiveInOut(f, sets, liveIn, liveOut);

    auto spansCallRegs = computeSpansCallRegs(f, sets, liveOut);
    auto intervals = buildIntervals(f, liveOut, spansCallRegs);

    AllocResult res;
    res.loc.assign((size_t)maxVRegId(f), Location{ true, PhysReg::NONE, -1 });

    std::array<bool, 6> free{};
    free.fill(true);
    free[(size_t)regIndex(PhysReg::EAX)] = false;

    std::vector<Interval*> active;
    active.reserve(intervals.size());

    int nextSpillSlot = 0;

    auto insertActiveSorted = [&](Interval* in) {
        auto it = std::upper_bound(active.begin(), active.end(), in,
            [](const Interval* a, const Interval* b) { return a->end < b->end; });
        active.insert(it, in);
        };

    for (auto& cur : intervals)
    {
        expireOld(active, cur.start, free);

        const auto pool = cur.spansCall ? calleeSavedRegs() : allRegs();

        PhysReg chosen = PhysReg::NONE;
        for (auto pr : pool)
        {
            int idx = regIndex(pr);
            if (idx >= 0 && free[(size_t)idx]) { chosen = pr; break; }
        }

        if (chosen != PhysReg::NONE)
        {
            cur.isReg = true;
            cur.pr = chosen;
            int idx = regIndex(chosen);
            if (idx < 0) throw std::runtime_error("regIndex(chosen) < 0");
            free[(size_t)idx] = false;

            insertActiveSorted(&cur);

            if (isCalleeSaved(chosen)) res.usedCalleeSaved.insert(chosen);
            continue;
        }

        auto regAllowed = [&](const Interval& it, PhysReg pr) {
            if (pr == PhysReg::EAX) return false;              // EAX niealokowalny (zgodnie z wcześniejszą poprawką)
            if (it.spansCall) return isCalleeSaved(pr);        // przez call tylko callee-saved
            return true;
            };

        // znajdź kandydata do spill z pasującym rejestrem
        Interval* spill = nullptr;
        for (auto it = active.rbegin(); it != active.rend(); ++it)
        {
            if ((*it)->isReg && regAllowed(cur, (*it)->pr))
            {
                spill = *it;
                break;
            }
        }

        if (spill && spill->end > cur.end && spill->isReg)
        {
            cur.isReg = true;
            cur.pr = spill->pr;

            spill->isReg = false;
            spill->spillSlot = nextSpillSlot++;

            // usuń 'spill' z active
            auto posIt = std::find(active.begin(), active.end(), spill);
            if (posIt != active.end()) active.erase(posIt);

            insertActiveSorted(&cur);

            if (isCalleeSaved(cur.pr)) res.usedCalleeSaved.insert(cur.pr);
        }
        else
        {
            cur.isReg = false;
            cur.spillSlot = nextSpillSlot++;
        }
    }

    for (const auto& in : intervals)
    {
        if (in.isReg)
        {
            res.loc[(size_t)in.v] = Location{ true, in.pr, -1 };
        }
        else
        {
            res.loc[(size_t)in.v] = Location{ false, PhysReg::NONE, in.spillSlot };
        }
    }
    res.spillSlots = nextSpillSlot;
    return res;
}
