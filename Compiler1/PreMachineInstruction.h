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
    MUL,
    IMUL,
    IDIV,
    DIV,
    CMP,
    TEST,
    JMP,
    JE,
    JNE,
    JL,
    JLE,
    JG,
    JGE,
    JB,
    JBE,
    JA,
    JAE,
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
    SETBE,


    AND,
    OR,
    XOR,
    SHL,
    SAR,
    SHR,

    INVALID
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
    static MachineOperand Immediate(const ConstValue::Scalar& value, const Memory::OperandSize& size)
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

        if (!tacInstruction) return;
        struct Visitor
        {
            PreMachineIR& ir;

            void operator()(const TACBinop& bin) {
                auto dst = ir.LowerTacExpr(bin.dst);
                auto left = ir.LowerTacExpr(bin.left);
                auto right = ir.LowerTacExpr(bin.right);

                // =========================================================================
                // 1. Relational Operations (==, !=, <, <=, >, >=)
                // =========================================================================
                if (auto op = std::get_if<RelationalOp>(&bin.op)) {
                    MachineInstruction cmpInst;

                    bool is_right_zero = false;
                    if (auto im = std::get_if<ImmediateOperand>(&right.operand)) {
                        is_right_zero = std::visit([](auto&& arg) -> bool { return arg == 0; }, im->value);
                    }

                    // Optimization: Use TEST reg, reg when checking (reg == 0) or (reg != 0)
                    if (is_right_zero && (*op == RelationalOp::Equal || *op == RelationalOp::NotEqual)) {
                        cmpInst.opcode = MachineOpcode::TEST;
                        cmpInst.operands = { left, left };
                    }
                    else {
                        cmpInst.opcode = MachineOpcode::CMP;
                        cmpInst.operands = { left, right };
                    }
                    ir.Emit(cmpInst);

                    MachineInstruction setInst;
                    setInst.opcode = ir.GetMachineOpcode(*op, bin.dst->type->is_signed);
                    setInst.operands = { dst }; // dst must be an 8-bit byte register/mem
                    ir.Emit(setInst);
                }

                // =========================================================================
                // Binary Operations (+, -, *, /, %, &, |, ^, <<, >>)
                // =========================================================================
                else if (auto op = std::get_if<BinaryOp>(&bin.op)) {
                    bool isSigned = bin.dst->type->is_signed;

                    // Special Case: Division (/) and Modulo (%)
                    if (*op == BinaryOp::Div || *op == BinaryOp::Mod) {
                        auto rax = MachineOperand::PhysicalReg(Register::Reg::RAX);
                        auto rdx = MachineOperand::PhysicalReg(Register::Reg::RDX);

                        ir.Emit(MachineInstruction{ MachineOpcode::MOV, { rax, left } });

                        if (isSigned) {
                            ir.Emit(MachineInstruction{ MachineOpcode::CQO, {} }); // RAX -> RDX:RAX
                        }
                        else {
                            ir.Emit(MachineInstruction{ MachineOpcode::XOR, { rdx, rdx } }); // RDX = 0
                        }

                        MachineOpcode divOp = isSigned ? MachineOpcode::IDIV : MachineOpcode::DIV;
                        ir.Emit(MachineInstruction{ divOp, { right } });

                        MachineOperand resultReg = (*op == BinaryOp::Div) ? rax : rdx;
                        ir.Emit(MachineInstruction{ MachineOpcode::MOV, { dst, resultReg } });
                    }
                    else {
                        MachineInstruction arithInst;
                        arithInst.opcode = ir.GetMachineOpcode(*op, isSigned);
                        arithInst.operands = { dst, left, right };
                        ir.Emit(arithInst);
                    }
                }
            }
            void operator()(const TACMove& move){
                MachineInstruction code;
                auto dstOp = ir.LowerTacExpr(move.dst);
                auto srcOp = ir.LowerTacExpr(move.src);

                code.opcode = MachineOpcode::MOV;

                code.operands.push_back(dstOp);
                code.operands.push_back(srcOp);
                ir.Emit(code);
            }
            void operator()(const TACAddressOf& address){
                MachineInstruction code;

                code.opcode = MachineOpcode::LEA;
                code.operands.push_back(ir.LowerTacExpr(address.dst));
                code.operands.push_back(ir.LowerTacExpr(address.object));

                ir.Emit(code);
            }
            void operator()(const TACLoad& load) {
                MachineInstruction code;
                auto dstOp = ir.LowerTacExpr(load.dst);
                auto srcOp = ir.LowerTacExpr(load.src);


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
                ir.Emit(code);

            }
            void operator()(const TACMoveMem& move) {
                MachineInstruction code;

                auto address = ir.LowerTacExpr(move.dst);
                code.opcode = MachineOpcode::STORE;


                auto src = ir.LowerTacExpr(move.src);
                auto dest = MachineOperand::Memory(address, src.size); // destination size is always the same as source size


                code.operands.push_back(
                    dest
                );

                code.operands.push_back(
                    src
                );

                ir.Emit(code);
            }
            void operator()(const TACCJump& cjump) {
               
                auto cond = ir.LowerTacExpr(cjump.cond);
                ir.Emit(MachineInstruction{
                    MachineOpcode::TEST,
                    { cond}
                    });
                ir.Emit(MachineInstruction{
                    ir.GetJumpOpcode(RelationalOp::Equal),
                    { MachineOperand::Label(cjump.labelFalse) }
                    });
                
            }
            void operator()(const TACRelationalCJump& cjump)
            {
                auto left = ir.LowerTacExpr(cjump.left);
                auto right = ir.LowerTacExpr(cjump.right);


                MachineInstruction cmpInst;

                bool is_right_zero = false;
                if (auto im = std::get_if<ImmediateOperand>(&right.operand)) {
                    is_right_zero = std::visit([](auto&& arg) -> bool { return arg == 0; }, im->value);
                }

                if (is_right_zero && (cjump.op == RelationalOp::Equal || cjump.op == RelationalOp::NotEqual)) {
                    cmpInst.opcode = MachineOpcode::TEST;
                    cmpInst.operands = { left, left };
                }
                else {
                    cmpInst.opcode = MachineOpcode::CMP;
                    cmpInst.operands = { left, right };
                }
                ir.Emit(cmpInst);

                ir.Emit(MachineInstruction{
                    ir.GetJumpOpcode(cjump.op), 
                    { MachineOperand::Label(cjump.labelFalse) }
                    });
                
            }
            void operator()(const TACJump& jump) {
                ir.Emit(MachineInstruction{
                MachineOpcode::JMP,
               { MachineOperand::Label(jump.label) }
                    });
            }
            void operator()(const TACLabel& label) {
                ir.Emit(MachineInstruction{
                 MachineOpcode::LABEL,
                { MachineOperand::Label(label.label) }
                    });
            }
            void operator()(const TACCall& call) {
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
                if (call.dst)
                {
                    destination = ir.LowerTacExpr(call.dst);
                }
                // Evaluate each argument into a temporary Virtual Register / Operand
                std::vector<MachineOperand> evaluatedArgs;
                for (const auto& arg : call.args) {
                    evaluatedArgs.push_back(ir.LowerTacExpr(arg));
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
                        ir.Emit(movArg);
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
                callCode.label = call.function;
                ir.Emit(callCode);


                if (call.dst) {
                    //Move return value from RAX into call's destination register
                    MachineInstruction movRet;
                    movRet.opcode = MachineOpcode::MOV;
                    movRet.operands = {
                        destination,
                        MachineOperand::PhysicalReg(Register::Reg::RAX, destination.size),
                    };
                    ir.Emit(movRet);
                }
            }
            void operator()(const TACReturn& ret) {
                MachineInstruction code;
                code.opcode = MachineOpcode::MOV;
                if (ret.value) {
                    auto valueOperand = ir.LowerTacExpr(ret.value);
                    code.operands.push_back(MachineOperand::PhysicalReg(Register::Reg::RAX, GetDestinationSize(valueOperand.size)));
                    code.operands.push_back(valueOperand);
                    ir.Emit(code);
                }

                MachineInstruction returnCode;
                returnCode.opcode = MachineOpcode::RET;

                ir.Emit(returnCode);
            }

        }visitor{ *this };
        std::visit(visitor, tacInstruction->expr);
       
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



    void Emit(const MachineInstruction& instruction)
    {
        ins.push_back(instruction);
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

    MachineOpcode GetMachineOpcode(const std::variant<BinaryOp, RelationalOp>& opVariant, bool isSigned)
    {
        return std::visit([isSigned](auto&& op) -> MachineOpcode {
            using T = std::decay_t<decltype(op)>;

            // Arithmetic & Bitwise Operations
            if constexpr (std::is_same_v<T, BinaryOp>) {
                switch (op) {
                case BinaryOp::Add: return MachineOpcode::ADD;
                case BinaryOp::Sub: return MachineOpcode::SUB;
                case BinaryOp::Mul: return isSigned ? MachineOpcode::IMUL : MachineOpcode::MUL;
                case BinaryOp::Div: return isSigned ? MachineOpcode::IDIV : MachineOpcode::DIV;
                case BinaryOp::Mod: return isSigned ? MachineOpcode::IDIV : MachineOpcode::DIV; // x86 IDIV/DIV handles % via RDX
                case BinaryOp::BitAnd: return MachineOpcode::AND;
                case BinaryOp::BitOr:  return MachineOpcode::OR;
                case BinaryOp::BitXor: return MachineOpcode::XOR;
                case BinaryOp::Shl:    return MachineOpcode::SHL;
                case BinaryOp::Shr:    return isSigned ? MachineOpcode::SAR : MachineOpcode::SHR;
                }
            }

            // Relational Operations (SETcc)
            else if constexpr (std::is_same_v<T, RelationalOp>) {
                if (isSigned) {
                    switch (op) {
                    case RelationalOp::Equal:        return MachineOpcode::SETE;
                    case RelationalOp::NotEqual:     return MachineOpcode::SETNE;
                    case RelationalOp::Less:         return MachineOpcode::SETL;
                    case RelationalOp::LessEqual:    return MachineOpcode::SETLE;
                    case RelationalOp::Greater:      return MachineOpcode::SETG;
                    case RelationalOp::GreaterEqual: return MachineOpcode::SETGE;
                    }
                }
                else {
                    switch (op) {
                    case RelationalOp::Equal:        return MachineOpcode::SETE;
                    case RelationalOp::NotEqual:     return MachineOpcode::SETNE;
                    case RelationalOp::Less:         return MachineOpcode::SETB;   // Below
                    case RelationalOp::LessEqual:    return MachineOpcode::SETBE;  // Below or Equal
                    case RelationalOp::Greater:      return MachineOpcode::SETA;   // Above
                    case RelationalOp::GreaterEqual: return MachineOpcode::SETAE;  // Above or Equal
                    }
                }
            }

            throw std::runtime_error("Invalid op variant in GetMachineOpcode");
            }, opVariant);
    }

    MachineOpcode GetJumpOpcode(RelationalOp op, bool isSigned = true) {
    if (isSigned) {
        switch (op) {
            case RelationalOp::Equal:        return MachineOpcode::JE;
            case RelationalOp::NotEqual:     return MachineOpcode::JNE;
            case RelationalOp::Less:         return MachineOpcode::JL;
            case RelationalOp::LessEqual:    return MachineOpcode::JLE;
            case RelationalOp::Greater:      return MachineOpcode::JG;
            case RelationalOp::GreaterEqual: return MachineOpcode::JGE;
        }
    } else {
        switch (op) {
            case RelationalOp::Equal:        return MachineOpcode::JE;
            case RelationalOp::NotEqual:     return MachineOpcode::JNE;
            case RelationalOp::Less:         return MachineOpcode::JB;
            case RelationalOp::LessEqual:    return MachineOpcode::JBE;
            case RelationalOp::Greater:      return MachineOpcode::JA;
            case RelationalOp::GreaterEqual: return MachineOpcode::JAE;
        }
    }
    return MachineOpcode::JNE;
}
    
    Register::Reg SCRATCH = Register::Reg::R11;
    std::vector<MachineInstruction> ins;
    CFGProgram& cfg;

};