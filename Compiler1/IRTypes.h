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

// General Binary Operations for High/Mid IR & TAC
//  Binary Arithmetic & Bitwise
enum class BinaryOp : uint8_t {
	Add, Sub, Mul, Div, Mod,
	BitAnd, BitOr, BitXor,
	Shl, Shr
};

// Unary Operations
enum class UnaryOp : uint8_t {
	Negate,     // -x
	BitNot,     // ~x
	LogicalNot, // !x
	Deref,      // *ptr
	AddressOf   // &var
};

// Relational / Comparison Operations
enum class RelationalOp : uint8_t {
	Equal,          // ==
	NotEqual,       // !=
	Less,           // <
	LessEqual,      // <=
	Greater,        // >
	GreaterEqual    // >=
};

// Short-Circuit Logical Operations
enum class LogicalOp : uint8_t {
	And, // &&
	Or   // ||
};

// Compound Assignment Operations
enum class CompoundAssignmentOp : uint8_t {
	AddEqual,    // +=
	SubEqual,    // -=
	MulEqual,    // *=
	DivEqual,    // /=
	ModEqual,    // %=
	ShlEqual,    // <<=
	ShrEqual,    // >>=
	AndEqual,    // &=
	OrEqual,     // |=
	XorEqual     // ^=
};
constexpr BinaryOp TokenToBinaryOp(TokenType token)
{
	switch (token) {
	case TokenType::plus:          return BinaryOp::Add;
	case TokenType::sub:		   return BinaryOp::Sub;
	case TokenType::star:          return BinaryOp::Mul;
	case TokenType::div:           return BinaryOp::Div;
	default:                      assert(false && "Not a Binary token");
	}
}
constexpr RelationalOp TokenToRelationalOp(const TokenType& op) {
	switch (op) {
	case TokenType::equal_equal:   return RelationalOp::Equal;
	case TokenType::not_equal:	   return RelationalOp::NotEqual;
	case TokenType::less:          return RelationalOp::Less;
	case TokenType::less_equal:    return RelationalOp::LessEqual;
	case TokenType::greater:       return RelationalOp::Greater;
	case TokenType::greater_equal: return RelationalOp::GreaterEqual;
	default:                      assert(false && "Not a relational token");
	}
}
constexpr LogicalOp TokenToLogicalOp(const TokenType& op) {
	switch (op) {
	case TokenType::_and:   return LogicalOp::And;
	case TokenType::_or:   return LogicalOp::Or;
	default:                      assert(false && "Not a logical operator token");
	}
}
constexpr std::variant<BinaryOp, RelationalOp, LogicalOp> ConvertToIROp(const TokenType& op) {
	switch (op) {
		// --- Relational Operators ---
	case TokenType::equal_equal:   return RelationalOp::Equal;
	case TokenType::not_equal:     return RelationalOp::NotEqual;
	case TokenType::less:          return RelationalOp::Less;
	case TokenType::less_equal:    return RelationalOp::LessEqual;
	case TokenType::greater:       return RelationalOp::Greater;
	case TokenType::greater_equal: return RelationalOp::GreaterEqual;

		// --- Short-Circuit Logical Operators ---
	case TokenType::_and:          return LogicalOp::And; // &&
	case TokenType::_or:           return LogicalOp::Or;  // ||

		// --- Binary Arithmetic / Bitwise ---
	case TokenType::plus:          return BinaryOp::Add;
	case TokenType::sub:           return BinaryOp::Sub;
	case TokenType::star:          return BinaryOp::Mul;
	case TokenType::div:           return BinaryOp::Div;

	default:
		throw std::runtime_error("Unsupported comparison or logical token type");
	}
}
constexpr bool IsRelationOp(const TokenType& op) 
{
	return (op >= TokenType::less) && (op <= TokenType::not_equal);
}
constexpr RelationalOp Invert(RelationalOp op) {
	switch (op) {
	case RelationalOp::Equal:        return RelationalOp::NotEqual;
	case RelationalOp::NotEqual:     return RelationalOp::Equal;
	case RelationalOp::Less:         return RelationalOp::GreaterEqual;
	case RelationalOp::LessEqual:    return RelationalOp::Greater;
	case RelationalOp::Greater:      return RelationalOp::LessEqual;
	case RelationalOp::GreaterEqual: return RelationalOp::Less;
	}
	return op;
}

constexpr BinaryOp ToBinaryOp(CompoundAssignmentOp op) {
	switch (op) {
	case CompoundAssignmentOp::AddEqual: return BinaryOp::Add;
	case CompoundAssignmentOp::SubEqual: return BinaryOp::Sub;
	case CompoundAssignmentOp::MulEqual: return BinaryOp::Mul;
	case CompoundAssignmentOp::DivEqual: return BinaryOp::Div;
	case CompoundAssignmentOp::ModEqual: return BinaryOp::Mod;
	case CompoundAssignmentOp::ShlEqual: return BinaryOp::Shl;
	case CompoundAssignmentOp::ShrEqual: return BinaryOp::Shr;
	case CompoundAssignmentOp::AndEqual: return BinaryOp::BitAnd;
	case CompoundAssignmentOp::OrEqual:  return BinaryOp::BitOr;
	case CompoundAssignmentOp::XorEqual: return BinaryOp::BitXor;
	}
	return BinaryOp::Add;
}



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

struct IRExprBinop {
	std::variant<BinaryOp, RelationalOp, LogicalOp> op;
	IRExpr* left = nullptr;
	IRExpr* right = nullptr;

	bool IsRelational() const {
		return std::holds_alternative<RelationalOp>(op);
	}

	bool IsArithmetic() const {
		return std::holds_alternative<BinaryOp>(op);
	}

	bool IsLogical() const {
		return std::holds_alternative<LogicalOp>(op);
	}

	RelationalOp GetRelationalOp() const {
		return std::get<RelationalOp>(op);
	}

	BinaryOp GetBinaryOp() const {
		return std::get<BinaryOp>(op);
	}
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

	std::variant<BinaryOp, RelationalOp> op;

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


struct TACLabel
{
	std::string label;
};

struct TACJump
{
	std::string label;
};

// Jump to the target label if false
struct TACCJump
{
	IRExpr* cond;
	std::string labelFalse;

};

struct TACRelationalCJump
{
	RelationalOp op;
	IRExpr* left;
	IRExpr* right;
	std::string labelFalse;
};
// Variant type representing any TAC/IR instruction variant.
typedef std::variant<TACBinop, TACMove, TACMoveMem, TACLoad, TACAddressOf, TACRelationalCJump, TACCJump, TACJump, TACLabel, TACCall, TACReturn> AnyInstuction;

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