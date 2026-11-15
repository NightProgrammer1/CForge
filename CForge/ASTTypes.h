#pragma once
#include "types.h"

struct SourceLocation
{

};
struct Identifier {
	std::string name;
	SourceLocation location; // Tracks line/col for compiler error messages
};

struct NodeFunction;

struct NodeExprBinary
{
	TokenType op;

	NodeExpr* left = nullptr;
	NodeExpr* right = nullptr;

};

struct NodeExprUnary
{
	TokenType op;
	NodeExpr* expr = nullptr;

};

struct NodeExprCompare
{
	TokenType op;
	NodeExpr* left = nullptr;
	NodeExpr* right = nullptr;

};

struct NodeExprLiteral
{
	std::variant<
		bool,          // 1-bit boolean (true / false)
		int8_t,        // Signed 8-bit integer   (signed char)
		uint8_t,       // Unsigned 8-bit integer (unsigned char)
		int16_t,       // Signed 16-bit integer  (short)
		uint16_t,      // Unsigned 16-bit integer(unsigned short)
		int32_t,       // Signed 32-bit integer  (int)
		uint32_t,      // Unsigned 32-bit integer(unsigned int)
		int64_t,       // Signed 64-bit integer  (long / long long on x86-64 Linux)
		uint64_t,      // Unsigned 64-bit integer(unsigned long / unsigned long long)
		float,         // 32-bit single-precision float
		double         // 64-bit double-precision float
	> value;
};

struct NodeExprIdent
{
	std::string name;
	size_t id = 0;
	QualifiedType type;
	std::shared_ptr<Symbol> symbol;

};

struct NodeExprVoid
{

};

struct NodeExprAssignment
{
	NodeExpr* target;
	NodeExpr* expr = nullptr;

};

struct NodeExprCall
{
	NodeFunction* func = nullptr;
	std::vector<NodeExpr*> args;
	std::string name;

};

struct NodeExprInitList {
	std::vector<NodeExpr*> values;
};

struct NodeExprArrayAccess {
	NodeExpr* array;
	std::vector<NodeExpr*> indices;
};


enum class ExprContext {
	RValue,
	LValue
};

typedef std::variant<
	NodeExprLiteral, //1
	NodeExprIdent, // 2
	NodeExprBinary, // 3
	NodeExprCompare, // 4
	NodeExprVoid,    // 5
	NodeExprUnary,   // 6
	NodeExprCall,    // 7
	NodeExprAssignment, // 8
	NodeExprInitList, // 9
	NodeExprArrayAccess // 10
> NodeExpression;
struct NodeExpr
{

	NodeExpression var;
	QualifiedType type;
};


struct NodeStatementExit
{
	NodeExpr* expr = nullptr;
};



struct NodeStatementVar
{
	size_t id = 0;
	QualifiedType type;
	std::string name;
	std::shared_ptr<Symbol> symbol = nullptr;
	NodeExpr* initializer = nullptr;

	std::optional<ConstValue> compiled_constant = std::nullopt;
};


struct NodeStatement;


struct NodeBlock
{
	std::vector<NodeStatement*> statements;

};

struct NodeStatementIf
{
	NodeExpr* condition = nullptr;
	NodeStatement* thenBody = nullptr;
	NodeStatement* elseBody = nullptr;
};

struct NodeStatementWhile
{
	NodeExpr* condition = nullptr;
	NodeStatement* body = nullptr;
	NodeExpr* update = nullptr;
};

struct NodeStatementExpr
{
	NodeExpr* expr = nullptr;
};
struct NodeStatementReturn
{
	NodeExpr* expr = nullptr;

};
struct NodeStatementBreak
{

};

struct NodeStatementContinue
{

};
struct NodeStatementAssignment
{
	std::string name;
	NodeExpr* target = nullptr;
	NodeExpr* expr = nullptr;

};

struct NodeStatement
{
	std::variant<
		NodeStatementExit,
		NodeStatementVar, // 1 
		NodeBlock, // 2
		NodeStatementIf, // 3
		NodeStatementWhile, // 4
		NodeStatementBreak, // 5
		NodeStatementContinue, // 6
		NodeStatementExpr, // 7
		NodeStatementReturn, // 8
		NodeStatementAssignment // 9
	> stat;
};


struct Parameter
{
	std::string name;
	QualifiedType type;
	size_t id = 0;
};
struct NodeFunction
{
	std::string name;
	std::vector<Parameter> parameters;
	std::optional<NodeBlock> body;
	QualifiedType retType;
	size_t stack_size = 0;
};

struct NodeTopLevel {
	std::variant<NodeFunction*, NodeStatementVar*> decl;
};

struct NodeProg {
	std::vector<NodeTopLevel> items;
};
