#pragma once
#include "PreMachineInstruction.h"


class PostMachineIR
{
public:
    PostMachineIR(MachineProgram& program) : program(program) {}
    void Lower()
    {
        for (auto& function : program.functions)
        {

            for (auto& block : function.blocks)
            {
                block.instructions = std::move(FixMachineInstructions(block.instructions));
            }
        }
    }
private:
    std::vector<MachineInstruction> FixMachineInstructions(const std::vector<MachineInstruction>& ins)
    {
        std::vector<MachineInstruction> fixed_stream;
        for (const auto& instruction : ins)
        {
            FixInstruction(instruction, fixed_stream);
        }


        return fixed_stream;
    }
   

    MachineInstruction FixBinaryOp(const MachineInstruction& machIns, std::vector<MachineInstruction>& fixed_stream)
    {
        MachineInstruction newIns(machIns);
        auto& dst = newIns.operands[0];
        auto& left = newIns.operands[1];
        auto& right = newIns.operands[2];
        //prevents dst being overwritten if dst and right are the same
        if (SameOperand(dst, right))
        {
            //if its already a register no need to save the value we can do the addition directly
            if (right.kind == MachineOperand::Kind::PhysicalReg)
            {

            }
            else
            {
                Move(MachineOperand::PhysicalReg(SCRATCH), left, dst.size, fixed_stream);
                left = MachineOperand::PhysicalReg(SCRATCH);
            }
        }
        else if (left.kind == MachineOperand::Kind::StackSlot && right.kind == MachineOperand::Kind::StackSlot)
        {
            Move(MachineOperand::PhysicalReg(SCRATCH), left, dst.size, fixed_stream);
            Move(dst, MachineOperand::PhysicalReg(SCRATCH), dst.size, fixed_stream);
            left = right;
        }
        else
        {
            Move(dst, left, dst.size, fixed_stream);
            left = right;
        }

        return newIns;
    }

    void FixInstruction(const MachineInstruction& machIns, std::vector<MachineInstruction>& fixed_stream)
    {
        using namespace Memory;

        switch (machIns.opcode)
        {
		case MachineOpcode::MOV:
		{
			auto dst = machIns.operands[0];
			auto src = machIns.operands[1];
			auto size = dst.size;
            Move(dst, src, size, fixed_stream);
            break;
		}
		case MachineOpcode::ADD:
        {
            
            assert(machIns.operands.size() >= 3);
            MachineInstruction newIns = FixBinaryOp(machIns, fixed_stream);
            fixed_stream.push_back(newIns);
            break;
        }
        case MachineOpcode::MOVSXD:
        {
            fixed_stream.push_back(machIns);
            break;
        }
        case MachineOpcode::STORE:
        {
            auto dst = machIns.operands[0];
            auto src = machIns.operands[1];
            auto size = dst.size;
            Move(dst, src, size, fixed_stream);
            break;
        }
        case MachineOpcode::SUB:
        {
            MachineInstruction newIns(machIns);
            auto& dst = newIns.operands[0];
            auto& left = newIns.operands[1];
            auto& right = newIns.operands[2];
            //prevents dst being overwritten if dst and right are the same
            if (SameOperand(dst, right))
            {
                Move(MachineOperand::PhysicalReg(SCRATCH, dst.size), left, dst.size, fixed_stream);
                dst = MachineOperand::PhysicalReg(SCRATCH, dst.size);
                left = right;
                fixed_stream.push_back(newIns);

                Move(left, MachineOperand::PhysicalReg(SCRATCH, dst.size), dst.size, fixed_stream);

            }
            else if (left.kind == MachineOperand::Kind::StackSlot && right.kind == MachineOperand::Kind::StackSlot)
            {
                Move(MachineOperand::PhysicalReg(SCRATCH), left, dst.size, fixed_stream);
                Move(dst, MachineOperand::PhysicalReg(SCRATCH), dst.size, fixed_stream);
                left = right;
                fixed_stream.push_back(newIns);

            }
            else
            {
                Move(dst, left, dst.size, fixed_stream);
                left = right;
                fixed_stream.push_back(newIns);

            }
            break;
        }
        case MachineOpcode::IMUL:
        {
            assert(machIns.operands.size() >= 3);
            MachineInstruction newIns(machIns);
            auto& dst = newIns.operands[0];
            auto& left = newIns.operands[1];
            auto& right = newIns.operands[2];


           if (SameOperand(dst, right))
            {
                Move(MachineOperand::PhysicalReg(SCRATCH), left, dst.size, fixed_stream);
                left = MachineOperand::PhysicalReg(SCRATCH);
            }
            if (dst.kind == MachineOperand::Kind::StackSlot)
            {
               Move(MachineOperand::PhysicalReg(SCRATCH), left, dst.size, fixed_stream);
               left = right;
            }
            else
            {
               Move(dst, left, dst.size, fixed_stream);
               left = right;
            }
           fixed_stream.push_back(newIns);

            break;
        }
        case MachineOpcode::CMP:
        {
            assert(machIns.operands.size() >= 2 && "Instruction is missing operands");
            MachineInstruction newIns(machIns);
            auto& left = newIns.operands[0];
            auto& right = newIns.operands[1];

            if (left.kind == MachineOperand::Kind::StackSlot)
            {
                Move(MachineOperand::PhysicalReg(SCRATCH, left.size), left, left.size, fixed_stream);
                left = MachineOperand::PhysicalReg(SCRATCH, left.size);
            }
            fixed_stream.push_back(newIns);

            break;
        }
        case MachineOpcode::TEST:
        {
            assert(machIns.operands.size() >= 1 && "Instruction is missing operands");
            MachineInstruction newIns(machIns);
            auto& left = newIns.operands[0];

            if (left.kind == MachineOperand::Kind::StackSlot)
            {
                Move(MachineOperand::PhysicalReg(SCRATCH, left.size), left, left.size, fixed_stream);
                left = MachineOperand::PhysicalReg(SCRATCH, left.size);
            }
            fixed_stream.push_back(newIns);

            break;
        }
        default:
        {
            fixed_stream.push_back(machIns);
            break;
        }
        }
    }
    void Move(const MachineOperand& dst, const MachineOperand& src, const Memory::OperandSize& size, std::vector<MachineInstruction>& fixed_stream)
    {
        if (SameOperand(dst, src)) { return ; }

        bool dstMemory = IsMemory(dst);
        bool srcMemory = IsMemory(src);
        auto moveSize = dst.size;

        auto destination = dst;
        auto source = src;


        if (dstMemory && srcMemory)
        {
            auto scratch = MachineOperand::PhysicalReg(SCRATCH, size);
            Move(scratch, src, size, fixed_stream);
            Move(dst, scratch, size, fixed_stream);
            return;
        }
        MachineInstruction move;

        auto ResolveStackSlot = [&](const MachineOperand& op) -> MachineOperand {
            if (auto mem = std::get_if<MemoryOperand>(&op.operand))
            {
                if (mem->address && mem->address->kind == MachineOperand::Kind::StackSlot)
                {
                    auto scratchBase = MachineOperand::PhysicalReg(SCRATCH, mem->address->size);
                    Move(scratchBase, *mem->address, mem->address->size, fixed_stream);

                    MachineOperand resolvedMem = MachineOperand::Memory(scratchBase, op.size);
                    return resolvedMem;
                }
            }
            return op;
            };

        if (moveSize == Memory::OperandSize::Qword && src.size < Memory::OperandSize::Qword && src.kind != MachineOperand::Kind::Immediate) //movsxd
        {
            move.opcode = MachineOpcode::MOVSXD;
        }
        else if (dst.size >= Memory::OperandSize::Dword && src.size == Memory::OperandSize::Byte && src.kind != MachineOperand::Kind::Immediate)
        {
            move.opcode = MachineOpcode::MOVZX;
        }
        else {
            move.opcode = MachineOpcode::MOV;

        }

        move.operands.push_back(ResolveStackSlot(dst));
        move.operands.push_back(ResolveStackSlot(src));
        fixed_stream.push_back(move);
    }


    bool IsMemory(const MachineOperand& op)
    {
        return (op.kind == MachineOperand::Kind::Memory || op.kind == MachineOperand::Kind::StackSlot);
    }

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


        if (auto vregA = std::get_if<VRegOperand>(&a.operand))
        {
            auto vregB = std::get_if<VRegOperand>(&b.operand);

            return vregB && vregA->vreg == vregB->vreg;
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



    MachineOpcode GetJumpOpcode(TokenType op)
    {
        switch (op)
        {
        case TokenType::equal_equal: return MachineOpcode::JE;
        case TokenType::not_equal:   return MachineOpcode::JNE;
        case TokenType::less:        return MachineOpcode::JL;
        case TokenType::less_equal:  return MachineOpcode::JLE;
        case TokenType::greater:     return MachineOpcode::JG;
        case TokenType::greater_equal:return MachineOpcode::JGE;

        default:
            throw std::runtime_error("Invalid comparison");
        }
    }
    Register::Reg SCRATCH = Register::Reg::R11;
    MachineProgram& program;

};