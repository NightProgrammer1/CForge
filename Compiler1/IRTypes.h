#pragma once
#include <variant>
#include "types.h"
#include "register.h"
struct IRExpr;



struct IRExprConst
{
	ConstValue::Scalar data;

	Memory::OperandSize GetSize() const {
		return std::visit([](auto&& arg) -> Memory::OperandSize {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (sizeof(T) == 8) return Memory::OperandSize::Qword;
			if constexpr (sizeof(T) == 4) return Memory::OperandSize::Dword;
			if constexpr (sizeof(T) == 2) return Memory::OperandSize::Word;
			if constexpr (sizeof(T) == 1) return Memory::OperandSize::Byte;

			return Memory::OperandSize::Byte;
			}, data);
	}
};


struct IRExprName
{
	std::string name;
};

struct IRExprTemp
{
	Register::VReg virtualRegister;
};


struct IRExprStackObject
{
	Register::StackObject id;
};

struct IRExprBinop
{
	TokenType op;

	IRExpr* left = nullptr;
	IRExpr* right = nullptr;

};

struct IRExprMem
{
	IRExpr* address;
};

struct IRExprDereference
{
	IRExpr* address;

};


struct IRExprCall
{
	IRExpr* func;                 
	std::vector<IRExpr*> args;    
	IRExpr* dst = nullptr;       
};
struct IRExprESEQ
{
	IRExpr* expr;
};

struct IRExprAddressOf
{
	IRExpr* object;
};
struct IRExpr
{
	std::variant<IRExprConst, 
				IRExprName  ,
				IRExprTemp  ,
				IRExprStackObject,
				IRExprBinop ,
				IRExprMem   ,
				IRExprDereference,
				IRExprAddressOf,
				IRExprCall  ,
				IRExprESEQ> expr;

	std::shared_ptr<Type> type;

};

struct IRStatement;

struct IRStatementMove
{
	IRExpr* dst;
	IRExpr* src;
};

struct IRStatementExp
{
	IRExpr* func;
	IRExpr* expr;
};
struct IRStatementReturn
{
	IRExpr* value;
};
struct IRStatementJump
{
	std::string label;
};

struct IRStatementCJump
{
	IRExpr* cond;
	std::string labelTrue;
	std::string labelFalse;
};

struct IRStatementLabel
{
	std::string label;
};

struct IRStatement
{
	std::variant <
		IRStatementMove,
		IRStatementExp,
		IRStatementJump,
		IRStatementCJump,
		IRStatementLabel,
		IRStatementReturn> stat;

};


typedef std::variant <
	IRStatementMove,
	IRStatementExp,
	IRStatementJump,
	IRStatementCJump,
	IRStatementLabel,
	IRStatementReturn> AnyIRStatement;
struct Mir
{
	std::vector<IRStatement*> statements;
};

struct TACBinop
{
	IRExpr* dst;

	TokenType op;

	IRExpr* left;
	IRExpr* right;
};

struct TACMove
{
	IRExpr* dst; // can either be a vreg or stack-object
	IRExpr* src;
};

struct TACMoveMem
{
	IRExpr* dst;
	IRExpr* src;
};

struct TACLoad // x = *p;
{
	IRExpr* dst;
	IRExpr* src;
};
struct TACAddressOf
{
	IRExpr* dst; //temp
	IRExpr* object;
};
struct TACReturn
{
	IRExpr* value = nullptr;
};
struct TACCall
{
	IRExpr* dst;
	std::string function;
	std::vector<IRExpr*> args;
};

// Variant type representing any TAC/IR instruction variant. Placed before
typedef std::variant<TACBinop, TACMove, TACMoveMem, TACLoad, TACAddressOf, IRStatementCJump, IRStatementJump, IRStatementLabel, TACCall, TACReturn> AnyInstuction;

struct Instruction
{
	AnyInstuction expr;

};


struct TAC
{
	std::string name;
	std::vector<Register::VReg> parameters;
	std::vector<Instruction*> instructions;
	size_t stackSize = 0;
};



struct TACProgram
{
	std::vector<TAC> functions;
};



struct BasicBlock
{
	size_t id;
	std::string label = "";
	std::vector<Instruction*> ins;
};

struct CFG
{
	std::string name;
	std::vector<Register::VReg> parameters;
	std::vector<BasicBlock> blocks;
	std::vector<std::pair<size_t, size_t>> edges;
	size_t stackSize = 0;
};

struct CFGProgram
{
	std::vector<CFG> functions;
};
struct IRFunction
{
	std::string name;

	std::shared_ptr<Type> returnType;

	std::vector<Register::VReg> parameters;


	std::vector<IRStatement*> statements;
	std::vector<BasicBlock> blocks;

	size_t stackSize = 0;
};

struct IRProgram
{
	std::vector<IRFunction> functions;
};