#pragma once
#include "IRLowering.h"
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include "PostMachineInstruction.h"
#include "RegisterAllocation.h"

class X86Generator
{

public:
	X86Generator(MachineProgram& prog) : program(std::move(prog))
	{

	}

	std::string GenerateProgram()
	{
		_output << "global _start\n_start:\n";
		_output << "    call main\n";
		_output << "    mov rdi, rax\n";
		_output << "    mov rax, 60\n";
		_output << "    syscall\n";
	

		for (auto function : program.functions)
		{
			_output << function.name << ":\n";
			_output << "    push rbp\n";
			_output << "    mov rbp, rsp\n";

			for (Register::Reg reg : function.usedCalleeSaved) {
				Emit("push " + std::string(Register::RegToString(reg)) + "\n");
			}
			size_t pushedBytes = function.usedCalleeSaved.size() * 8;
			size_t alignedTotal = AlignStackSize(function.stack_size + pushedBytes);
			size_t subBytes = alignedTotal - pushedBytes;

			if (subBytes > 0) {
				_output << "    sub rsp, " + std::to_string(subBytes) <<"\n";
			}
			for (auto block : function.blocks)
			{
				GenerateBlock(block, function);
			}

			
		}
		
		return _output.str();
	}
private:

	void Emit(const std::string_view& text)
	{
		_output << "    " << text;
	}
	void EmitOperand(const MachineOperand& operand)
	{
		if (auto physicalReg =
			std::get_if<PhysicalRegOperand>(&operand.operand))
		{
			_output << Register::RegToString(physicalReg->reg);
		}
		else if (auto immediate =
			std::get_if<ImmediateOperand>(&operand.operand))
		{
			std::visit(
				[this](auto value)
				{
					_output << value;
				},
				immediate->value
			);
		}
		else if (auto vreg =
			std::get_if<VRegOperand>(&operand.operand))
		{
			throw std::runtime_error(
				"Unallocated VReg reached assembly generator"
			);
		}
		else if (auto stackSlot =
			std::get_if<StackSlotOperand>(&operand.operand))
		{
			_output << "[rbp - " << stackSlot->value << "]";
		}
		else if (auto mem = std::get_if<MemoryOperand>(&operand.operand))
		{
			_output << "[";

			if (mem->address->kind == MachineOperand::Kind::StackSlot)
			{
				auto stack = std::get<StackSlotOperand>(
					mem->address->operand
				);

				_output << "rbp - " << stack.value;
			}
			else if (mem->address->kind == MachineOperand::Kind::VReg)
			{
			}
			else
			{
				EmitOperand(*mem->address);
			}

			_output << "]";
		}
		else if (auto label =
			std::get_if<LabelOperand>(&operand.operand))
		{
			_output << label->label;
		}
	}
	bool IsMemory(const MachineOperand& op)
	{
		return op.kind == MachineOperand::Kind::Memory;
	}
	void LabelJMP(const std::string& x86_ins, const MachineInstruction& ins)
	{
		if (ins.operands.size() < 1) return;
		_output << "    " << x86_ins << " ";
		EmitOperand(ins.operands[0]);
		_output << "\n";
	}

	void Move(const MachineOperand& dst, const MachineOperand& src, const Memory::OperandSize& moveSize)
	{
		_output << "    mov ";
		_output << FormatOperand(dst);
		_output << ", ";
		_output << FormatOperand(src);
		_output << "\n";

	}
	void GenerateAssembly(const MachineInstruction& ins, const MachineFunction& fnContext)
	{
		using namespace Memory;
		switch (ins.opcode)
		{
		case MachineOpcode::MOV:
		{
			assert(ins.operands.size() >= 2 && "Instruction is missing operands");
			const auto& dst = ins.operands[0];
			const auto& src = ins.operands[1];
			Move(dst, src, dst.size);
			break;
		}
		case MachineOpcode::MOVSXD:
		{
			assert(ins.operands.size() >= 2 && "MOVSXD requires destination and source operands");
			const auto& dst = ins.operands[0];
			const auto& src = ins.operands[1];

			_output << "    movsxd ";
			_output << FormatOperand(dst); 
			_output << ", ";
			_output << FormatOperand(src);
			_output << "\n";
			break;
		}
		case MachineOpcode::MOVZX:
		{
			assert(ins.operands.size() >= 2 && "MOVZX requires destination and source operands");
			const auto& dst = ins.operands[0];
			const auto& src = ins.operands[1];

			_output << "    movzx ";
			_output << FormatOperand(dst);
			_output << ", ";
			_output << FormatOperand(src);
			_output << "\n";
			break;
		}
		case MachineOpcode::STORE:
		{
			auto& memDst = ins.operands[0];
			auto& src = ins.operands[1];

			_output << "    mov "
				<< FormatOperand(memDst) << ", "
				<< FormatOperand(src) << "\n";
			break;
		}
		case MachineOpcode::LEA:
		{
			const auto& dst = ins.operands[0];
			const auto& src = ins.operands[1];

			if (SameOperand(dst, src)) { return; }

			_output << "    lea ";
			EmitOperand(dst);
			_output << ", ";
			EmitOperand(src);
			_output << "\n";

			break;
		}
		case MachineOpcode::ADD:
		{
			if (ins.operands.size() < 2)
				return;

			auto dst = ins.operands[0];
			auto right = ins.operands[1];


			_output << "    add ";
			_output << FormatOperand(dst);
			_output << ", ";
			_output << FormatOperand(right);
			_output << "\n";


			break;
		}
		case MachineOpcode::SUB:
		{
			
			if (ins.operands.size() < 2)
				return;

			auto dst = ins.operands[0];
			auto subtractor = ins.operands[1];

			_output << "    sub ";
			_output << FormatOperand(dst);
			_output << ", ";
			_output << FormatOperand(subtractor);
			_output << "\n";


			break;
		}
		case MachineOpcode::IMUL:
		{
			if (ins.operands.size() < 2)
				return;

			auto dst = ins.operands[0];
			auto left = ins.operands[1];
			auto right = ins.operands[2];

			_output << "    imul ";
			_output << FormatOperand(dst);
			_output << ", ";
			_output << FormatOperand(left);
			_output << "\n";

			break;
		}
		case MachineOpcode::IDIV:
		{
			assert(ins.operands.size() >= 1 && "IDIV requires a divisor operand");
			const auto& divisor = ins.operands[0];

			// Handle immediate divisor (idiv cannot take immediates on x86-64)
			if (divisor.kind == MachineOperand::Kind::Immediate)
			{
				_output << "    mov r10, ";
				EmitOperand(divisor);
				_output << "\n    idiv r10\n";
			}
			else
			{
				_output << "    idiv ";
				EmitOperand(divisor);
				_output << "\n";
			}
			break;
		}
		case MachineOpcode::CQO:
		{
			_output << "    cqo\n";
			break;
		}
		case MachineOpcode::CMP:
		{
			if (ins.operands.size() < 2) return;
			auto left = ins.operands[0];
			auto right = ins.operands[1];
			_output << "    cmp ";
			_output << FormatOperand(left);
			_output << ", ";

			_output << FormatOperand(right, left.size);
			_output << "\n";
			break;
		}
		case MachineOpcode::JMP:
		{
			LabelJMP("jmp", ins);
			break;
		}
		case MachineOpcode::JL:
		{
			LabelJMP("jl", ins);
			break;
		}
		case MachineOpcode::JE:
		{
			LabelJMP("je", ins);
			break;
		}
		case MachineOpcode::JG:
		{
			LabelJMP("jg", ins);
			break;
		}
		case MachineOpcode::JGE:{
			LabelJMP("jge", ins);

			break;
		}
		case MachineOpcode::JNE:
		{
			LabelJMP("jne", ins);
			break;
		}
		case MachineOpcode::SETL:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    setl " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::SETE:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    sete " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::SETNE:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    setne " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::SETLE:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    setle " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::SETG:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    setg " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::SETGE:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    setge " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::SETB:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    setb " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::SETBE:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    setbe " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::SETA:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    seta " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::SETAE:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto dst = ins.operands[0];
			_output << "    setae " << FormatOperand(dst) << "\n";
			break;
		}
		case MachineOpcode::TEST:
		{
			assert(ins.operands.size() >= 1 && "Instruction is missing operands");
			auto left = ins.operands[0];

			_output << "    test " << FormatOperand(left) <<  ", " << FormatOperand(left) << "\n";
			break;
		}
		case MachineOpcode::LABEL:
		{
			if (ins.operands.size() < 1) { return; }
			_output << "    "; EmitOperand(ins.operands[0]);
			_output << ":\n";
			break;
		}
		case MachineOpcode::CALL: {
			_output << "    call " << ins.label << "\n";
			break;
		}
		case MachineOpcode::RET:
		{
			//Epilogue
			size_t pushedBytes = fnContext.usedCalleeSaved.size() * 8;
			size_t alignedTotal = AlignStackSize(fnContext.stack_size + pushedBytes);
			size_t subBytes = alignedTotal - pushedBytes;
			if (subBytes > 0) {
				//Emit("add rsp, " + std::to_string(subBytes) + "\n");
			}
			for (auto it = fnContext.usedCalleeSaved.rbegin(); it != fnContext.usedCalleeSaved.rend(); ++it) {
				Emit("pop " + std::string(Register::RegToString(*it)) + "\n");
			}

			_output << "    mov rsp, rbp\n";
			_output << "    pop rbp\n";
			_output << "    ret\n";
			break;
		}
		default:
			return;
		}

	}


	void GenerateBlock(const MachineBasicBlock& block, const MachineFunction& fnContext)
	{
		for (const auto& ins : block.instructions)
			GenerateAssembly(ins, fnContext);
	}
	



private:

	
	bool SameOperand(
		const MachineOperand& a,
		const MachineOperand& b)
	{
		if (a.kind != b.kind)
			return false;

		if (auto regA = std::get_if<PhysicalRegOperand>(&a.operand))
		{
			auto regB = std::get_if<PhysicalRegOperand>(&b.operand);

			return regB && regA->reg == regB->reg;
		}

		if (auto stackA = std::get_if<StackSlotOperand>(&a.operand))
		{
			auto stackB = std::get_if<StackSlotOperand>(&b.operand);

			return stackB && stackA->value == stackB->value;
		}

		if (auto immA = std::get_if<ImmediateOperand>(&a.operand))
		{
			auto immB = std::get_if<ImmediateOperand>(&b.operand);

			return immB && immA->value == immB->value;
		}

		return false;
	}

	
	

	// Formats a MachineOperand to assembly string syntax
	std::string FormatOperand(const MachineOperand& op, Memory::OperandSize overrideSize) {
		// If overrideSize is provided (not default/unknown), use it; otherwise use operand's inherent size

		using namespace Memory;
		OperandSize effectiveSize = (overrideSize != static_cast<OperandSize>(0)) ? overrideSize : op.size;


		struct Visitor {
			OperandSize size;
			X86Generator* gen;
			std::string operator()(const VRegOperand& vreg) {
				return "v" + std::to_string(vreg.vreg.id);
			}

			std::string operator()(const PhysicalRegOperand& phys) {
				return std::string(GetPhysicalRegName(phys.reg, size));
			}

			std::string operator()(const ImmediateOperand& imm) {
				// Format immediate values
				if (std::holds_alternative<int>(imm.value)) {
					return std::to_string(std::get<int>(imm.value));
				}
				else if (std::holds_alternative<size_t>(imm.value)) {
					return std::to_string(std::get<size_t>(imm.value));
				}
				return "0";
			}

			std::string operator()(const StackSlotOperand& slot) {
				// Stack slot offset relative to RBP: [rbp - slot.value]
				std::string sizeSpecifier = GetSizeSpecifier(size);

				return sizeSpecifier + " [rbp - " + std::to_string(slot.value) + "]";
			}

			std::string operator()(const StackObjectOperand& obj) {
				// Stack object pointer offset relative to RBP
				return "[rbp - " + std::to_string(obj.object.size) + "]";
			}

			std::string operator()(const LabelOperand& lbl) {
				return lbl.label;
			}

			std::string operator()(const MemoryOperand& mem) {
				// Memory address registers are always 64-bit pointers (Qword)
				std::string innerAddr = gen->FormatOperand(*mem.address, OperandSize::Qword);
				std::string sizeSpecifier = GetSizeSpecifier(size);

				// Format nested expression: [rax]
				return sizeSpecifier + " [" + innerAddr + "]";
			}
		};

		return std::visit(Visitor{ effectiveSize, this }, op.operand);
	}

	// Overload taking only 1 parameter
	std::string FormatOperand(const MachineOperand& op) {
		return FormatOperand(op, op.size);
	}
	size_t AlignStackSize(size_t unalignedSize, size_t alignment = 16) {
		return (unalignedSize + alignment - 1) & ~(alignment - 1);
	}
	NodeProg root;

	std::stringstream _output;
	size_t _stackSize = 0;
	size_t _varCount = 0;
	MachineProgram program;
	Register::Reg SCRATCH = Register::Reg::R11;
	int _currentScope = 0;




};