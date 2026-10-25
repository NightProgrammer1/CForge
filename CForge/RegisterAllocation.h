#include <string>
#include <algorithm>
#include "LivenessAnalysis.h"
#include "PreMachineInstruction.h"
#include "register.h"
#include <array>
#include <set>


class RegisterAllocator
{

public:
	std::pair<size_t, std::set<Register::Reg>> AllocateReg(std::vector<std::pair<Register::VReg, LiveInterval>> liveIntervals, MachineFunction& ins)
	{
		using namespace Register;

		
		std::sort(
			liveIntervals.begin(),
			liveIntervals.end(),
			[](const auto& a, const auto& b)
			{
				if (a.second.start == b.second.start)
				{
					return a.first.id < b.first.id;
				}
				return a.second.start < b.second.start;
			}
		);

		// intervals that are live sorted by their end position
		std::vector< std::pair<Register::Reg, LiveInterval>> active;

		


		for (size_t i = 0; i < liveIntervals.size(); ++i)
		{
			auto& [vreg, interval] = liveIntervals[i];
			const size_t position = interval.start;
			// Expire old intervals
			auto new_end = std::remove_if(
				active.begin(),
				active.end(),
				[position](const auto& it)
				{
					return it.second.end <= position;
				});

			active.erase(new_end, active.end());

			if (i < ins.parameters.size()) { //if the position is less than params size, it is automatically a parameter.
				allocation[vreg] = PARAM_REGS[i];
				active.emplace_back(PARAM_REGS[i], interval);
			}
			else {
				AllocateReg(vreg, interval, active);
			}
			

			std::sort(
				active.begin(),
				active.end(),
				[](const auto& a, const auto& b)
				{
					return a.second.end < b.second.end;
				});

		}

		RewriteRegisters(ins);

		
		/*
		for (const auto& [vreg, reg] : allocation)
		{
			std::cout << "v" << vreg << " -> "
				<< RegisterAllocation::RegToString(static_cast<size_t>(reg)) << '\n';
		}

		for (const auto& [vreg, offset] : spillSlots)
		{
			std::cout << "v" << vreg << " -> [rbp - "
				<< offset << "]\n";
		}
		*/

		return std::make_pair(ins.stack_size, std::move(usedCalleeSavedRegs) );
	}
	
private:

	void AllocateReg(const Register::VReg& vreg, const LiveInterval& interval,std::vector<std::pair<Register::Reg, LiveInterval>>& active) {

		
		if (interval.crossesCall) {
			if (auto reg = TryGetFreeCalleeSavedReg(active)) {
				usedCalleeSavedRegs.insert(*reg); 
				allocation[vreg] = *reg;
				return;
			}
			else if (!spillSlots.contains(vreg)) {
				AllocateSpillSlot(vreg);
				return;
			}
		}
		auto reg = GetNextAvailableRegister(active);
		if (reg)
		{
			allocation[vreg] = *reg;
			active.emplace_back(*reg, interval);
		}
		else{
			if(!spillSlots.contains(vreg))
				AllocateSpillSlot(vreg);
		}
		
		
	}
	void RewriteRegisters(MachineFunction& function)
	{
		for (auto& block : function.blocks)
		{
			for (auto& ins : block.instructions) {
				for (auto& operand : ins.operands)
				{
					
					if (auto stack = std::get_if<StackSlotOperand>(&operand.operand))
					{
					}
					else if (auto vreg = std::get_if<VRegOperand>(&operand.operand))
					{
						RewriteOperand(operand);
					}
					else if (auto mem = std::get_if<MemoryOperand>(&operand.operand))
					{
						RewriteOperand(*mem->address);
					}
				}
			}
		}
	}

	void RewriteOperand(MachineOperand& operand)
	{
		if (operand.kind == MachineOperand::Kind::VReg)
		{
			auto vreg = std::get<VRegOperand>(operand.operand).vreg;

			if (auto it = allocation.find(vreg); it != allocation.end())
			{
				operand = MachineOperand::PhysicalReg(it->second, operand.size);
				return;
			}

			std::cerr << "No register allocated for v"
				<< vreg.id << '\n';
			std::exit(EXIT_FAILURE);
		}

		else if (operand.kind == MachineOperand::Kind::Memory)
		{
			auto& mem = std::get<MemoryOperand>(operand.operand);
			RewriteOperand(*mem.address);
		}

	}
	bool IsRegisterActive(
		Register::Reg reg,
		const std::vector<std::pair<Register::Reg, LiveInterval>>& active) const
	{
		for (const auto& [reg2, interval] : active)
		{
			if (reg2 == reg)
				return true;
		}

		return false;
	}
	std::optional<Register::Reg> GetNextAvailableRegister(
		const std::vector<std::pair<Register::Reg, LiveInterval>>& active)
	{
		for (Register::Reg reg : Register::registers)
		{
			if (!IsRegisterActive(reg, active))
				return reg;
		}

		return std::nullopt;
	}

	std::optional<Register::Reg> TryGetFreeCalleeSavedReg(const std::vector<std::pair<Register::Reg, LiveInterval>>& active) const
	{
		for (Register::Reg reg : Register::calleSavedRegisters)
		{
			if (!IsRegisterActive(reg, active))
				return reg;
		}

		return std::nullopt;
	}
	size_t AllocateSpillSlot(const Register::VReg& vreg)
	{
		nextStackOffset = AlignUp(nextStackOffset, vreg.size);//fix alignment
		nextStackOffset +=  vreg.size;
		size_t offset = nextStackOffset;

		spillSlots[vreg] = offset;

		return offset;
	}

	size_t AllocateStackObject(Register::StackObject object)
	{

		if (stackObjectSlots.contains(object)) { return 0; }
		nextStackOffset = AlignUp(nextStackOffset, object.alignment);

		nextStackOffset += object.size;
		size_t offset = nextStackOffset;

		stackObjectSlots[object] = offset;

		return offset;
	}
	
	size_t AlignUp(size_t value, size_t alignment) const
	{
		if (alignment == 0) return value;
		return (value + alignment - 1) & ~(alignment - 1);
	}
	
	std::unordered_map<Register::VReg, Register::Reg> allocation;
	std::unordered_map<Register::VReg, size_t> spillSlots;
	std::unordered_map<Register::StackObject, size_t> stackObjectSlots;
	std::set<Register::Reg> usedCalleeSavedRegs;
	size_t nextStackOffset = 0;
	size_t stackSize = 0;


};