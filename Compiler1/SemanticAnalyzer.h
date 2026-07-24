#pragma once
#include "types.h"
#include "SymbolTable.h"


class SemanticAnalyzer
{
public:
	SemanticAnalyzer(NodeProg& program) : ast(program) {}
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
			Type operator()(const NodeExprCall& call) {
				auto funcSymbol = analyzer->symbols.LookupFunction(call.name.value);



				if (!funcSymbol.has_value()) {
					std::cerr << "Semantic Error: Call to undeclared function '"
						<< call.name.value << "'\n";
					exit(EXIT_FAILURE);
				}

				if (call.args.size() != funcSymbol->paramTypes.size()) {
					std::cerr << "Semantic Error: Incorrect argument count for function '"
						<< call.name.value << "'\n";
					exit(EXIT_FAILURE);
				}
				for (size_t i = 0; i < call.args.size(); ++i) {
					Type argType = analyzer->AnalyzeExpression(*call.args[i]);
					Type expectedType = funcSymbol->paramTypes[i];
					if (argType != expectedType) {
						std::cerr << "Type Error: Argument " << i + 1 << " in call to '"
							<< call.name.value << "' has mismatched type.\n";
						exit(EXIT_FAILURE);
					}
				}

				return funcSymbol->returnType;
			
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
			Type operator()(const NodeExprVoid&) 
			{
				return Type::Void;
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
				Type declaredType = (stat.type == Type::Bool) ? Type::Bool : Type::Int;

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
				analyzer->AnalyzeBlock(block);
			}
			void operator()(const NodeStatementIf& _if)
			{
				auto condType = analyzer->AnalyzeExpression(*_if.condition);
				if (condType != Type::Bool) {
					std::cerr << "Type Error: IF condition must be boolean.\n";
					exit(EXIT_FAILURE);
				}

				analyzer->AnalyzeBlock(_if.thenBlock);

				if (_if.elseBlock.has_value()) {
					analyzer->AnalyzeBlock(_if.elseBlock.value());
				}
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
				if (!analyzer->currentFunctionReturnType.has_value()) {
					std::cerr << "Semantic Error: Return statement outside of function.\n";
					exit(EXIT_FAILURE);
				}

				analyzer->currentFunctionHasReturn = true;
				Type expectedType = analyzer->currentFunctionReturnType.value();
				Type actualType = analyzer->AnalyzeExpression(*_ret.expr);
				if (_ret.expr) {
					if (expectedType == Type::Void && actualType != Type::Void) {
						std::cerr << "Type Error: Cannot return a value from a void function.\n";
						exit(EXIT_FAILURE);
					}

					if (actualType != expectedType) {
						std::cerr << "Type Error: Function return type mismatch in '"
							<< analyzer->currentFunctionName.value_or("function") << "'.\n";
						exit(EXIT_FAILURE);
					}
				}
				else {
					if (expectedType != Type::Void) {
						std::cerr << "Type Error: Non-void function must return a value.\n";
						exit(EXIT_FAILURE);
					}
				}
			}
		};
		Visitor visitor{ this };
		std::visit(visitor, statement.stat);
	}

	int ff()
	{

	}
	void AnalyzeFunction(NodeFunction& func)
	{
		
		currentFunctionName = func.name.value; 
		currentFunctionReturnType = func.retType;
		currentFunctionHasReturn = false;


		symbols.stackOffset = 0;
		symbols.EnterScope();
		for (const auto& var : func.parameters)
		{
			symbols.DeclareVariable(var.ident.value, var.type);
		}

		symbols.stackOffset;
		AnalyzeBlock(func.body.value(), false);


		if (func.retType != Type::Void && !currentFunctionHasReturn) {
			std::cerr << "Semantic Error: Function '" << func.name.value
				<< "' has non-void return type but is missing a return statement.\n";
			exit(EXIT_FAILURE);
		}
		symbols.SetFunctionHasReturn(func.name.value, currentFunctionHasReturn);
		// Align total stack size to 16 bytes
		size_t total_bytes = symbols.stackOffset;
		if (total_bytes % 16 != 0) {
			total_bytes += (16 - (total_bytes % 16));
		}

		func.stack_size = total_bytes;
		symbols.ExitScope();

		currentFunctionName.reset();
		currentFunctionReturnType = std::nullopt;

	}
	void AnalyzeBlock(const NodeBlock& block, bool createScope = true) {
		if (createScope) symbols.EnterScope();

		for (const auto& stat : block.statements) {
			AnalyzeStatement(*stat);
		}

		if (createScope) symbols.ExitScope();
	}

	void AnalyzeProg(NodeProg& prog)
	{
		for (const auto& func : prog.functions)
		{
			std::vector<Type> paramTypes;
			for (const auto& param : func.parameters) {
				paramTypes.push_back(param.type);
			}

			auto existing = symbols.LookupFunction(func.name.value);


			if (existing.has_value()) {
				// Check for prototype mismatch
				if (existing->paramTypes != paramTypes || existing->returnType != func.retType) {
					std::cerr << "Error: Redeclaration of " << func.name.value << " with different signature!\n";
					exit(EXIT_FAILURE);
				}
				if (func.body.has_value() && existing->hasBody) {
					std::cerr << "Error: Redefinition of function " << func.name.value << "\n";
					exit(EXIT_FAILURE);
				}
			}

			bool hasBody = func.body.has_value();
			symbols.DeclareFunction(func.name.value, func.retType, paramTypes, hasBody);
		}

		for (auto& func : prog.functions)
		{
			if (func.body.has_value()) {
				AnalyzeFunction(func);
			}
		}



	}
	std::optional<std::string> currentFunctionName;
	std::optional<Type> currentFunctionReturnType;
	bool currentFunctionHasReturn = false;
	SymbolTable symbols;
	NodeProg& ast;
};