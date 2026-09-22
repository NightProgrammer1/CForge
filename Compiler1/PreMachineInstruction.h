#pragma once
#include "IRTypes.h"
#include "register.h"
#include <memory>
#include <array>
#include <set>

struct MachineOperand;
enum class MachineOpcode
{
    MOV, //x = y
    MOVSXD,
    MOVZX,
    ADD,
    SUB,
    IMUL,
    IDIV,
    CMP,
    JMP,
    JE,
    JNE,
    JL,
    JLE,
    JG,
    JGE,
    CALL,
    LABEL,
    RET,
    STORE, //*p = x
    LEA, //p = &x
    LOAD, //x = *p
    CQO,
    SETL,
    SETE,
    SETNE,
    SETG,
    SETGE,
    SETLE,
    SETA,
    SETAE,
    SETB,
    SETBE
};

struct VRegOperand
{
    Register::VReg vreg;
};

struct PhysicalRegOperand
{
    Register::Reg reg;
};

struct StackSlotOperand
{
    size_t value;
};

struct StackObjectOperand
{
    Register::StackObject object;
};
struct ImmediateOperand
{
    ConstValue::Scalar value;
};

struct LabelOperand
{
    std::string label;
};
struct MemoryOperand
{
    std::shared_ptr<MachineOperand> address;
};


struct MachineOperand
{
  
    enum class Kind
    {
        VReg,
        PhysicalReg,
        Immediate,
        StackSlot,
        StackObject,
        Label,
        Memory
    };
    std::variant< VRegOperand, LabelOperand, PhysicalRegOperand, StackSlotOperand, StackObjectOperand, ImmediateOperand, MemoryOperand> operand;
    Kind kind;
    Memory::OperandSize size = Memory::OperandSize::Dword;
    bool IsSizeSpecified = false;
    static MachineOperand VReg(const Register::VReg& reg, Memory::OperandSize size, bool isSizeSpecified = false)
    {
        return { VRegOperand(reg), Kind::VReg, size, isSizeSpecified };
    }
    static MachineOperand VReg(const Register::VReg& reg)
    {
        return { VRegOperand(reg), Kind::VReg, static_cast<Memory::OperandSize>(reg.size) };
    }
    static MachineOperand Label(const std::string& label)
    {
        return { LabelOperand(label), Kind::Label};
    }
    static MachineOperand PhysicalReg(const Register::Reg& reg, Memory::OperandSize size = Memory::OperandSize::Dword, bool isSizeSpecified = false)
    {
        
        return {  PhysicalRegOperand(reg), Kind::PhysicalReg, size, isSizeSpecified};
    }
	static MachineOperand StackSlot(const size_t& id, const size_t& size, bool isSizeSpecified = false) //rbp - id
	{
		return {  StackSlotOperand(id), Kind::StackSlot, static_cast<Memory::OperandSize>(size), isSizeSpecified};
	}
    static MachineOperand StackObject(const Register::StackObject& object, const Memory::OperandSize& size) //rbp - id
    {
        return { StackObjectOperand(object), Kind::StackObject, size };
    }
    static MachineOperand Immediate(ConstValue::Scalar& value, const Memory::OperandSize& size)
    {
        return { ImmediateOperand(value), Kind::Immediate, size };
    }
    static MachineOperand Memory(const MachineOperand& operand, bool isSizeSpecified = true)
    {
        return { MemoryOperand( std::make_shared<MachineOperand>(operand)), Kind::Memory, operand.size, isSizeSpecified};
    }
    static MachineOperand Memory(const MachineOperand& operand, Memory::OperandSize size, bool isSizeSpecified = true)
    {
        return { MemoryOperand(std::make_shared<MachineOperand>(operand)), Kind::Memory, size, isSizeSpecified };
    }
};


struct MachineInstruction
{
    MachineOpcode opcode;
    std::vector<MachineOperand> operands;
	std::string label; // for label instructions
};

struct MachineBasicBlock
{
    std::string label;
    std::vector<MachineInstruction> instructions;
};

struct MachineFunction
{
    std::string name;
    std::vector<Register::VReg> parameters;
    std::vector<MachineBasicBlock> blocks;
    size_t stack_size = 0;
    std::set<Register::Reg> usedCalleeSaved;
	CFG cfg;

};

struct MachineProgram
{
    std::vector<MachineFunction> functions;
};
class PreMachineIR
{
public:
    PreMachineIR(CFGProgram& cfg) : cfg(cfg) {}
    MachineProgram Lower()
    {

		MachineProgram machProg;
        for (auto& function : cfg.functions)
        {
            MachineFunction machFunc;
            machFunc.parameters = function.parameters;
            machFunc.stack_size = function.stackSize;
           
            for (auto& block : function.blocks)
            {
                ins.clear();
                for (size_t i = 0; i < block.ins.size(); i++)
                {
                    if (auto bin = std::get_if<TACBinop>(&block.ins[i]->expr))// For condition
                    {
                        if (i + 1 < block.ins.size())
                        {
                            if (auto cjump = std::get_if<IRStatementCJump>(&block.ins[i + 1]->expr))
                            {
                                auto condition = std::get_if<IRExprTemp>(&cjump->cond->expr);
                                auto dst = std::get<IRExprTemp>(bin->dst->expr);
                                if (condition && condition->virtualRegister.id == dst.virtualRegister.id)
                                {
                                    auto left = LowerTacExpr(bin->left);
                                    auto right = LowerTacExpr(bin->right);

                                    Emit(MachineInstruction{
                                        MachineOpcode::CMP,
                                        { std::move(left), std::move(right) }
                                        });

                                    Emit(MachineInstruction{
                                        GetJumpOpcode(bin->op),
                                        { MachineOperand::Label(cjump->labelTrue) }
                                        });

                                    Emit(MachineInstruction{
                                        MachineOpcode::JMP,
                                        { MachineOperand::Label(cjump->labelFalse) }
                                        });
                                }

                                i++;
                                continue;

                            }
                        }

                    }

                    LowerTac(block.ins[i]);
                }

                machFunc.blocks.emplace_back(block.label, ins);
                
            }
			machFunc.cfg = function;
            machFunc.name = function.name;
            ins.clear();
            machProg.functions.push_back(std::move(machFunc));
        }


      
        return machProg;
    }
private:

    MachineOperand LowerTacExpr(IRExpr* expr)
    {

        using namespace Memory;
        if (auto temp = std::get_if<IRExprTemp>(&expr->expr))
        {
            auto vreg = temp->virtualRegister;
            size_t bytes = expr->type ? expr->type->Size() : 8;
            return MachineOperand::VReg(vreg, static_cast<OperandSize>(bytes));
        }
        else if (auto mem = std::get_if<IRExprMem>(&expr->expr))
        {
            return MachineOperand::Memory(
                LowerTacExpr(mem->address), expr->type->Size()
            );
        }
        else if (auto constant = std::get_if<IRExprConst>(&expr->expr))
        {
            return MachineOperand::Immediate(
                constant->data, constant->GetSize()
            );
        
        }

        else if (auto addressOf = std::get_if<IRExprAddressOf>(&expr->expr))
        {
            auto loweredAdd = LowerTacExpr(addressOf->object);
            loweredAdd.size = static_cast<OperandSize>(expr->type->Size());
            return loweredAdd;
        }
        else if (auto object = std::get_if<IRExprStackObject>(&expr->expr))
        {
            return MachineOperand::StackSlot(object->id.offset, expr->type->Size());
        }

    }


    void LowerTac(Instruction* tacInstruction)
    {
        using namespace Memory;

        if (auto bin = std::get_if<TACBinop>(&tacInstruction->expr)){

            auto dst = LowerTacExpr(bin->dst);
            auto left = LowerTacExpr(bin->left);
            auto right = LowerTacExpr(bin->right);
            
            if ((bin->op == TokenType::less) || (bin->op == TokenType::less_equal) || (bin->op == TokenType::greater) ||
                (bin->op == TokenType::greater_equal) || (bin->op == TokenType::equal_equal) || (bin->op == TokenType::not_equal)

                )
            {
                MachineInstruction cmp;
                cmp.opcode = MachineOpcode::CMP;
                cmp.operands.push_back(left);
                cmp.operands.push_back(right);


                MachineInstruction code;
                code.opcode = GetMachineOpcode(bin->op, bin->dst->type->is_signed);
                code.operands.push_back(dst);

                ins.push_back(cmp);
                ins.push_back(code);
            }
            else if (bin->op == TokenType::div)
            {

                auto leftOp = LowerTacExpr(bin->left);
                auto rightOp = LowerTacExpr(bin->right);
                auto dstOp = LowerTacExpr(bin->dst);
                auto rax = MachineOperand::PhysicalReg(Register::Reg::RAX);
                auto rdx = MachineOperand::PhysicalReg(Register::Reg::RDX);

                MachineInstruction movLeft;
                movLeft.opcode = MachineOpcode::MOV;
                movLeft.operands = { rax, leftOp };
                ins.push_back(movLeft);

                MachineInstruction cqo;
                cqo.opcode = MachineOpcode::CQO;
                ins.push_back(cqo);

                MachineInstruction idiv;
                idiv.opcode = MachineOpcode::IDIV;
                idiv.operands = { rightOp };
                ins.push_back(idiv);

                MachineInstruction movDst;
                movDst.opcode = MachineOpcode::MOV;
                movDst.operands = { dstOp, rax };
                ins.push_back(movDst);
            }

            else
            {
                MachineInstruction code;


                code.opcode = GetMachineOpcode(bin->op, bin->dst->type->is_signed);
                code.operands.push_back(
                    dst
                );
                code.operands.push_back(
                    left
                );

                code.operands.push_back(
                    right
                );

                ins.push_back(code);
            }
            
        }
        else if (auto move = std::get_if<TACMove>(&tacInstruction->expr))
        {
            MachineInstruction code;
            auto dstOp = LowerTacExpr(move->dst);
            auto srcOp = LowerTacExpr(move->src);

            code.opcode = MachineOpcode::MOV;

            code.operands.push_back(dstOp);
            code.operands.push_back(srcOp);
            ins.push_back(code);
        }
        else if (auto address = std::get_if<TACAddressOf>(&tacInstruction->expr))
        {
            MachineInstruction code;

            code.opcode = MachineOpcode::LEA;
            code.operands.push_back(LowerTacExpr(address->dst));
            code.operands.push_back(LowerTacExpr(address->object));

            ins.push_back(code);
        }
        else if (auto load = std::get_if<TACLoad>(&tacInstruction->expr))
        {
            MachineInstruction code;
            auto dstOp = LowerTacExpr(load->dst);
            auto srcOp = LowerTacExpr(load->src);


            OperandSize accessSize = dstOp.size;
            auto memOp = MachineOperand::Memory(
                srcOp,
                accessSize, false
            );
            if (accessSize == OperandSize::Dword && dstOp.size == OperandSize::Qword)
            {
                code.opcode = MachineOpcode::MOVSXD;

            }
            else
            {
                code.opcode = MachineOpcode::MOV;

            }

            code.operands.push_back(dstOp);
            code.operands.push_back(memOp);
            ins.push_back(code);
               
        }
        else if (auto move = std::get_if<TACMoveMem>(&tacInstruction->expr))
        {
            MachineInstruction code;

            auto address = LowerTacExpr(move->dst);
            code.opcode = MachineOpcode::STORE;


            auto src = LowerTacExpr(move->src);
            auto dest = MachineOperand::Memory(address, src.size); // destination size is always the same as source size


            code.operands.push_back(
                dest
            );

            code.operands.push_back(
                src
            );

            ins.push_back(code);

        }
        else if (auto jump = std::get_if<IRStatementJump>(&tacInstruction->expr)) {
            Emit(MachineInstruction{ 
                 MachineOpcode::JMP,
                { MachineOperand::Label(jump->label) }
                });
        }
        else if (auto call = std::get_if<TACCall>(&tacInstruction->expr)) {
            // System V AMD64 ABI argument registers
            const std::array<Register::Reg, 6> PARAM_REGS = {
                Register::Reg::RDI,
                Register::Reg::RSI,
                Register::Reg::RDX,
                Register::Reg::RCX,
                Register::Reg::R8,
                Register::Reg::R9
            };

            MachineOperand destination;
            if (call->dst)
            {
                destination = LowerTacExpr(call->dst);
            }
            // Evaluate each argument into a temporary Virtual Register / Operand
            std::vector<MachineOperand> evaluatedArgs;
            for (const auto& arg : call->args) {
                evaluatedArgs.push_back(LowerTacExpr(arg));
            }

            // Emit explicit MOV instructions to copy evaluated args into ABI registers
            for (size_t i = 0; i < evaluatedArgs.size(); ++i) {
                if (i < PARAM_REGS.size()) {
                    MachineInstruction movArg;
                    auto size = evaluatedArgs[i].size;
                    movArg.opcode = MachineOpcode::MOV;
                    movArg.operands = {
                        MachineOperand::PhysicalReg(PARAM_REGS[i], size),
                        evaluatedArgs[i]
                    };
                    ins.push_back(movArg);
                }
                else {
                    // Push remaining args onto stack (for > 6 parameters)
                   // MachineInstruction pushArg;
                   // pushArg.opcode = MachineOpcode::PUSH;
                    //pushArg.operands = { evaluatedArgs[i] };
                    //ins.push_back(pushArg);
                }
            }

            MachineInstruction callCode;
            callCode.opcode = MachineOpcode::CALL;
            callCode.label = call->function;
            ins.push_back(callCode);

            if (call->dst) {
                //Move return value from RAX into call's destination register
                MachineInstruction movRet;
                movRet.opcode = MachineOpcode::MOV;
                movRet.operands = {
                    destination,
                    MachineOperand::PhysicalReg(Register::Reg::RAX, destination.size),
                };
                ins.push_back(movRet);
            }
}
		else if (auto ret = std::get_if<TACReturn>(&tacInstruction->expr)) {
			MachineInstruction code;
			code.opcode = MachineOpcode::MOV;
			if (ret->value) {
                auto valueOperand = LowerTacExpr(ret->value);
                code.operands.push_back(MachineOperand::PhysicalReg(Register::Reg::RAX, GetDestinationSize(valueOperand.size)));
                code.operands.push_back(valueOperand);
                ins.push_back(code);
			}

            MachineInstruction returnCode;
            returnCode.opcode = MachineOpcode::RET;
            ins.push_back(returnCode);

		}
    }
    std::optional<MachineInstruction> Move(const MachineOperand& dst, const MachineOperand& src, const Memory::OperandSize& size)
    {
        if (SameOperand(dst, src)) { return {}; }

        bool dstMemory = IsMemory(dst);
        bool srcMemory = IsMemory(src);
        auto moveSize = dst.size;
        MachineInstruction move;


        if (moveSize == Memory::OperandSize::Qword && src.size < Memory::OperandSize::Qword && src.kind != MachineOperand::Kind::Immediate) //movsxd
        {
            move.opcode = MachineOpcode::MOVSXD;
        }
        else {
            move.opcode = MachineOpcode::MOV;

        }
        if (dstMemory && srcMemory)
        {
            if (auto move = Move(MachineOperand::PhysicalReg(SCRATCH, size), src, size))
            {
                ins.push_back(*move);
            }
            return Move(dst, MachineOperand::PhysicalReg(SCRATCH, size), size);
        }
        else
        {
            move.operands.push_back(dst);
            move.operands.push_back(src);

        }
        return move;
    }


    bool IsMemory(const MachineOperand& op)
    {
        return op.kind == MachineOperand::Kind::Memory;
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

    MachineOpcode GetMachineOpcode(TokenType op, bool isSigned)
    {
        if (isSigned) {
            switch (op)
            {
            case TokenType::equal_equal: return MachineOpcode::SETE;
            case TokenType::not_equal:   return MachineOpcode::SETNE;
            case TokenType::less:        return MachineOpcode::SETL;
            case TokenType::less_equal:  return MachineOpcode::SETLE;
            case TokenType::greater:     return MachineOpcode::SETG;
            case TokenType::greater_equal:return MachineOpcode::SETGE;
            case TokenType::plus:        return MachineOpcode::ADD;
            case TokenType::sub:         return MachineOpcode::SUB;
            case TokenType::star:        return MachineOpcode::IMUL;
            case TokenType::div:         return MachineOpcode::IDIV;


            default:
                throw std::runtime_error("Invalid comparison");
            }
        }
        else
        {
            switch (op)
            {
            case TokenType::equal_equal: return MachineOpcode::SETE;
            case TokenType::not_equal:   return MachineOpcode::SETNE;
            case TokenType::less:        return MachineOpcode::SETB;
            case TokenType::less_equal:  return MachineOpcode::SETBE;
            case TokenType::greater:     return MachineOpcode::SETA;
            case TokenType::greater_equal:return MachineOpcode::SETAE;
            case TokenType::plus:        return MachineOpcode::ADD;
            case TokenType::sub:         return MachineOpcode::SUB;
            case TokenType::star:        return MachineOpcode::IMUL;
            case TokenType::div:         return MachineOpcode::IDIV;


            default:
                throw std::runtime_error("Invalid comparison");
            }
        }
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
    void Emit(MachineInstruction&& machIns)
    {
        ins.push_back(machIns);
    }
    
    Register::Reg SCRATCH = Register::Reg::R11;
    std::vector<MachineInstruction> ins;
    CFGProgram& cfg;

};