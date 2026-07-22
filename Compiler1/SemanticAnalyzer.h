#pragma once
#include "types.h"
#include "SymbolTable.h"


class SemanticAnalyzer
{
public:
	SemanticAnalyzer(const NodeProg& program) : ast(program) {}
	void Analyze()
	{
		symbols.EnterScope(); // Global scope
		AnalyzeProg(ast);
		symbols.ExitScope();
	}

private:

	Type AnalyzeExpression(const NodeExpr& expr) 
	{
		struct Visitor
		{
			SemanticAnalyzer* analyzer;

			Type operator()(const NodeExprIntLit&) {
				return Type::Int;
			}
			Type operator()(const NodeExprBoolLit&) {
				return Type::Bool;
			}
			Type operator()(const NodeExprCall&) {
				return Type::Int;
			}
			Type operator()(const NodeExprIndent& stat) {
				auto symbol = analyzer->symbols.LookupVariable(stat.ident.value);

				if (!symbol.has_value())
				{
					std::cerr << "Semantic Error: Undeclared variable '"
						      << stat.ident.value << "'\n";

					exit(EXIT_FAILURE);
				}
				return symbol->type;
			}
			Type operator()(const NodeExprCompare& expr) {
				auto left = analyzer->AnalyzeExpression(*expr.left);
				auto right = analyzer->AnalyzeExpression(*expr.right);

				if (expr.op.type == TokenType::_and || expr.op.type == TokenType::_or) {
					if (left != Type::Bool || right != Type::Bool) {
						std::cerr << "Type Error: Logical operators (&&, ||) require booleans.\n";
						exit(EXIT_FAILURE);
					}
				}
				else {
					// Comparisons (<, >, ==) require matching types
					if (left != right) {
						std::cerr << "Type Error: Comparison requires matching types.\n";
						exit(EXIT_FAILURE);
					}
				}
				return Type::Bool;
			}
			Type operator()(const NodeExprBinary& expr)
			{
				auto leftType = analyzer->AnalyzeExpression(*expr.left);
				auto rightType = analyzer->AnalyzeExpression(*expr.right);

				if (leftType != Type::Int || rightType != Type::Int) {
					std::cerr << "Type Error: Binary math operations require integers.\n";
					exit(EXIT_FAILURE);
				}

				return Type::Int;
			}
			Type operator()(const NodeExprUnary& expr)
			{
				return Type::Bool;
			}
		};

		Visitor visitor{ this };
		return std::visit(visitor, expr.var);
	}
	void AnalyzeStatement(const NodeStatement& statement)
	{
		struct Visitor
		{
			SemanticAnalyzer* analyzer;
			void operator()(const NodeStatementExit& stat)
			{
				auto exitType = analyzer->AnalyzeExpression(*stat.expr);
				if (exitType != Type::Int) {
					std::cerr << "Type Error: Exit code must be an integer.\n";
					exit(EXIT_FAILURE);
				}
			}
			void operator()(const NodeStatementVar& stat)
			{
				auto exprType = analyzer->AnalyzeExpression(*stat.expr);
				Type declaredType = (stat.type == VarType::Bool) ? Type::Bool : Type::Int;

				if (exprType != declaredType) {
					std::cerr << "Type Error: Cannot assign expression of type "
						<< (exprType == Type::Bool ? "Bool" : "Int")
						<< " to variable '" << stat.ident.value << "'\n";
					exit(EXIT_FAILURE);
				}

				if (!analyzer->symbols.DeclareVariable(stat.ident.value, declaredType)) {
					std::cerr << "Semantic Error: Variable '" << stat.ident.value
						<< "' already declared in this scope.\n";
					exit(EXIT_FAILURE);
				}
			}
			void operator()(const NodeBlock& block)
			{
			}
			void operator()(const NodeStatementIf& _if)
			{
			}
			void operator()(const NodeStatementWhile& _while)
			{
				auto exprType = analyzer->AnalyzeExpression(*_while.condition);
				if (exprType != Type::Bool) {
					std::cerr << "Type Error: Condition must return a bool.\n";
					exit(EXIT_FAILURE);
				}
			}

			void operator()(const NodeStatementExpr& stat)
			{

			}
			void operator()(const NodeStatementReturn& _ret)
			{
			}
		};
		Visitor visitor{ this };
		std::visit(visitor, statement.stat);
	}
	void AnalyzeFunction(NodeFunction& func)
	{
		AnalyzeBlock(func.body);
		func.stack_size = symbols.stackOffset;

	}
	void AnalyzeBlock(const NodeBlock& block, bool createScope = true) {
		if (createScope) symbols.EnterScope();

		for (const auto& stat : block.statements) {
			AnalyzeStatement(*stat);
		}

		if (createScope) symbols.ExitScope();
	}

	void AnalyzeProg(const NodeProg& prog)
	{
		for (auto& func : prog.functions)
		{
			AnalyzeFunction(*func);
		}
	}
	SymbolTable symbols;
	const NodeProg& ast;
};