#pragma once
#include "BasicBlockBuilder.h"
#include <unordered_set>
#include <unordered_map>


struct UseDef
{
    std::unordered_set<Register::VReg> use = {};
    std::unordered_set<Register::VReg> def = {};
};

struct BlockLiveness
{
    UseDef useDefs;

    std::unordered_set<Register::VReg> liveIn = {};
    std::unordered_set<Register::VReg> liveOut = {};
};
struct InstructionLiveness
{
    std::unordered_set<size_t> liveIn;
    std::unordered_set<size_t> liveOut;
};


struct LiveInterval
{
    size_t start = SIZE_MAX;
    size_t end = 0;

    bool crossesCall = false;
};


class LivenessAnalysis
{
public:
    

    std::unordered_map<size_t, BlockLiveness> Analyze(CFG& cfg)
    {
		info.clear();
        // First calculate USE/DEF for every block.
        for (auto& block : cfg.blocks)
        {
            auto& blockInfo = info[block.id];

            for (auto ins : block.ins)
            {
               // auto ud = Collect(ins);
                /*                if (ud) {
                    // USE = things used before being defined in this block
                    for (const auto& temp : ud->use)
                    {
                        if (!blockInfo.useDefs.def.contains(temp))
                            blockInfo.useDefs.use.insert(temp);
                    }

                    // DEF = everything defined in this block
                    for (const auto& temp : ud->def)
                    {
                        blockInfo.useDefs.def.insert(temp);
                    }
                }
                */
            }
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (auto it = cfg.blocks.rbegin(); it != cfg.blocks.rend(); ++it)
            {
                auto& block = *it;
                auto& B = info[block.id];

                auto oldLiveIn = B.liveIn;
                auto oldLiveOut = B.liveOut;

                // LiveOut = union of successor LiveIns
                B.liveOut.clear();

                for (auto successor : GetSuccessors(cfg, block.id))
                {
                    auto& successorInfo = info[successor];

                    B.liveOut.insert(
                        successorInfo.liveIn.begin(),
                        successorInfo.liveIn.end()
                    );
                }

                // LiveIn = USE ∪ (LiveOut - DEF)
                B.liveIn = B.useDefs.use;

                for (auto temp : B.liveOut)
                {
                    if (!B.useDefs.def.contains(temp))
                        B.liveIn.insert(temp);
                }

                if (B.liveIn != oldLiveIn ||
                    B.liveOut != oldLiveOut)
                {
                    changed = true;
                }
            }
        }

        return info;
    }

    std::vector<std::pair<Register::VReg, LiveInterval>>
        GetLiveIntervals(
            const MachineFunction& func)
    {

        std::unordered_map<Register::VReg, LiveInterval> intervalMap;
        std::vector<size_t> callPositions;
        size_t position = 1;

        for (const auto& param : func.parameters) {
            LiveInterval interval;
            interval.start = 0;
            interval.end = 0;
            intervalMap[Register::VReg{ param }] = interval;
        }
        for (const auto& block : func.cfg.blocks)
        {
            for (auto* ins : block.ins)
            {
                if (std::holds_alternative<TACCall>(ins->expr)) {
                    callPositions.push_back(position);
                }
                auto ud = Collect(ins);

                if (ud)
                {
                    for (const auto& vreg : ud->def)
                    {
                        auto& interval = intervalMap[vreg];

                        interval.start = std::min(interval.start, position);
                        interval.end = std::max(interval.end, position);

                    }

                    for (const auto& vreg : ud->use)
                    {
                        auto& interval = intervalMap[vreg];
                        
                        interval.start = std::min(interval.start, position);
                        interval.end = std::max(interval.end, position);
                    }
                }
                position++;
            }
        }
        for (auto& [vreg, interval] : intervalMap)
        {
            for (size_t callPos : callPositions)
            {
                if (interval.start < callPos && interval.end > callPos)
                {
                    interval.crossesCall = true;
                    break;
                }
            }
        }
        std::vector<std::pair<Register::VReg, LiveInterval>> intervals;

        for (auto& [vreg, interval] : intervalMap)
        {
            intervals.emplace_back(vreg, interval);
        }

      
        return intervals;
    }
private:
    std::vector<size_t> GetSuccessors(const CFG& cfg, size_t blockId)
    {
        std::vector<size_t> successors;

        for (const auto& edge : cfg.edges)
        {
            if (edge.first == blockId)
                successors.push_back(edge.second);
        }

        return successors;
    }
    std::optional<UseDef> Collect(Instruction* ins)
    {

        if (auto move = std::get_if<TACMove>(&ins->expr))
        {
            UseDef ud;

            // destination
            if (auto u = CollectUses(move->dst))
                ud.def.insert(*u);
            // source
            if(auto u = CollectUses(move->src))
                ud.use.insert(*u);

            return ud;
        }
        else if (auto move = std::get_if<TACMoveMem>(&ins->expr))
        {
            UseDef ud;

            // destination
            if (auto u = CollectUses(move->dst))
                ud.def.insert(*u);
            // source
            if (auto u = CollectUses(move->src))
                ud.use.insert(*u);

            return ud;
        }
        else if (auto addressOf = std::get_if<TACAddressOf>(&ins->expr))
        {
            UseDef ud;

            // destination
            if (auto u = CollectUses(addressOf->dst))
                ud.def.insert(*u);
            // source
            if (auto u = CollectUses(addressOf->object))
                ud.use.insert(*u);

            return ud;
        }
        else if (auto bin = std::get_if<TACBinop>(&ins->expr))
        {
            UseDef ud;

            if (auto u = CollectUses(bin->dst))
                ud.def.insert(*u);
            // source
            if(auto u = CollectUses(bin->left))
                ud.use.insert(*u);
            if(auto u = CollectUses(bin->right))
                ud.use.insert(*u);


            return ud;
        }
        else if (auto cjump = std::get_if<TACCJump>(&ins->expr))
        {
            UseDef ud;
            if(auto u = CollectUses(cjump->cond))
                ud.use.insert(*u);

            return ud;
        }
        else if (auto rcjump = std::get_if<TACRelationalCJump>(&ins->expr))
        {
            UseDef ud;
            if (auto u = CollectUses(rcjump->left))
                ud.use.insert(*u);
            if (auto u = CollectUses(rcjump->right))
                ud.use.insert(*u);

            return ud;
        }
        else if (auto ret = std::get_if<TACReturn>(&ins->expr))
        {
            UseDef ud;
            if(auto u = CollectUses(ret->value))
                ud.use.insert(*u);

            return ud;
        }
        else if (auto call = std::get_if<TACCall>(&ins->expr))
        {
            UseDef ud;
            if (auto u = CollectUses(call->dst))
                ud.def.insert(*u);
            for (auto arg : call->args)
            {
                if (auto u = CollectUses(arg))
                    ud.use.insert(*u);
            }
         
            return ud;
        }
        else if (auto load = std::get_if<TACLoad>(&ins->expr))
        {
            UseDef ud;
            if (auto u = CollectUses(load->dst))
                ud.def.insert(*u);
            if (auto u = CollectUses(load->src))
                ud.use.insert(*u);

            return ud;
        }
        return {};
    }

    std::optional<Register::VReg> CollectUses(IRExpr* src)
    {
        if (auto temp = std::get_if<IRExprTemp>(&src->expr))
        {
            return temp->virtualRegister;
        }
        else if (auto mem = std::get_if<IRExprMem>(&src->expr))
        {
            return { CollectUses(mem->address) };
        }
        else if (auto stack = std::get_if<IRExprStackObject>(&src->expr))
        {
            return {};
        }

        return {};
    }
    std::unordered_map<size_t, BlockLiveness> info;
};