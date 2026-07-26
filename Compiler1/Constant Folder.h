#pragma once
#include "parser.h"
#include <algorithm>
#include <optional>
#include <unordered_map>
#include <vector>
#include <memory_resource>

class ConstantFolder
{
public:
	ConstantFolder(NodeProg& program, std::pmr::polymorphic_allocator<NodeExpr>& allocator)
		: ast(&program), _allocator(&allocator) {
	}

	void FoldProgram()
	{
		AnalyzeProg(*ast);
	}

private:
	NodeExpr* Fold(NodeExpr* expr) {
		if (!expr) return nullptr;

		if (std::holds_alternative<NodeExprIntLit>(expr->var)) {
			return expr;
		}
		else if (auto* identifier = std::get_if<NodeExprIndent>(&expr->var))
		{
			if (auto value = GetVariableValue(identifier->name))
			{
				return _allocator->new_object<NodeExpr>(NodeExprIntLit{ *value });
			}
			return expr;
		}
		else if (auto* bin = std::get_if<NodeExprBinary>(&expr->var)) {
			auto newExpr = FoldBinaryExpr(bin);
			return (newExpr) ? newExpr.value() : expr;
		}
		else if (auto* comp = std::get_if<NodeExprCompare>(&expr->var))
		{
			auto newExpr = FoldCompExpr(comp);
			return (newExpr) ? newExpr.value() : expr;
		}
		else if (auto* funcCall = std::get_if<NodeExprCall>(&expr->var))
		{
			for (auto arg : funcCall->args)
			{
				arg->var = Fold(arg)->var;
			}
		}
		else if (auto* unary = std::get_if<NodeExprUnary>(&expr->var))
		{
			if (auto* ident = std::get_if<NodeExprIndent>(&unary->expr->var))
			{
				std::string varName = ident->name;
				if (auto currentVal = GetVariableValue(varName))
				{
					if (unary->op.type == TokenType::increment)
					{
						int oldValue = *currentVal;
						SetVariableValue(varName, oldValue + 1);
						return _allocator->new_object<NodeExpr>(NodeExprIntLit{ oldValue + 1 });
					}
				}
				else
				{
					InvalidateVariable(varName);
				}
				return expr;
			}
		}
		return expr;
	}

	int Compute(Token op, int left, int right)
	{
		switch (op.type)
		{
		case TokenType::plus:          return left + right;
		case TokenType::mult:          return left * right;
		case TokenType::sub:           return left - right;
		case TokenType::greater:       return (left > right);
		case TokenType::greater_equal: return (left >= right);
		case TokenType::less:          return (left < right);
		case TokenType::less_equal:    return (left <= right);
		case TokenType::equal_equal:   return (left == right);
		default:                        return 0;
		}
	}

	std::optional<NodeExpr*> FoldBinaryExpr(NodeExprBinary* bin)
	{
		auto* leftInd = std::get_if<NodeExprIndent>(&bin->left->var);
		auto* rightInd = std::get_if<NodeExprIndent>(&bin->right->var);
		if (leftInd && rightInd)
		{
			if (leftInd->name == rightInd->name)
			{
				if (bin->op.type == TokenType::sub)
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				}
			}
		}

		NodeExpr* foldedLeft = Fold(bin->left);
		NodeExpr* foldedRight = Fold(bin->right);

		auto* leftLit = std::get_if<NodeExprIntLit>(&foldedLeft->var);
		auto* rightLit = std::get_if<NodeExprIntLit>(&foldedRight->var);

		if (leftLit && rightLit) {
			int result = Compute(bin->op, leftLit->value, rightLit->value);
			return _allocator->new_object<NodeExpr>(NodeExprIntLit{ result });
		}
		else if (rightLit)
		{
			if (bin->op.type == TokenType::plus || bin->op.type == TokenType::sub)
			{
				if (rightLit->value == 0) return foldedLeft;
			}
			else if (bin->op.type == TokenType::mult)
			{
				if (rightLit->value == 0) return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				if (rightLit->value == 1) return foldedLeft;
			}
		}
		else if (leftLit)
		{
			if (bin->op.type == TokenType::mult)
			{
				if (leftLit->value == 0) return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				if (leftLit->value == 1) return foldedRight;
			}
		}
		return {};
	}

	std::optional<NodeExpr*> FoldCompExpr(NodeExprCompare* comp)
	{
		NodeExpr* foldedLeft = Fold(comp->left);
		NodeExpr* foldedRight = Fold(comp->right);

		auto* leftLit = std::get_if<NodeExprIntLit>(&foldedLeft->var);
		auto* rightLit = std::get_if<NodeExprIntLit>(&foldedRight->var);

		if (leftLit && rightLit) {
			int result = Compute(comp->op, leftLit->value, rightLit->value);
			return _allocator->new_object<NodeExpr>(NodeExprIntLit{ result });
		}

		auto* leftBool = std::get_if<NodeExprBoolLit>(&foldedLeft->var);
		auto* rightBool = std::get_if<NodeExprBoolLit>(&foldedRight->var);

		if (leftBool)
		{
			if (comp->op.type == TokenType::_and && leftBool->value == false)
				return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
			if (comp->op.type == TokenType::_or && leftBool->value == true)
				return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 1 });
		}
		if (rightBool)
		{
			if (comp->op.type == TokenType::_and && rightBool->value == false)
				return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
			if (comp->op.type == TokenType::_or && rightBool->value == true)
				return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 1 });
		}

		return {};
	}

	// -------------------------------------------------------------------------
	// SIMULATION PASS: Executes variable side-effects WITHOUT mutating the AST
	// -------------------------------------------------------------------------
	void SimulateStatement(NodeStatement& statement)
	{
		struct SimVisitor
		{
			ConstantFolder* analyzer;

			void operator()(NodeStatementExit& stat) {}
			void operator()(NodeStatementReturn& stat) {}
			void operator()(NodeStatementExpr& stat) {
				analyzer->Fold(stat.expr);
			}
			void operator()(NodeStatementVar& stat) {
				NodeExpr* val = analyzer->Fold(stat.expr);
				if (auto lit = std::get_if<NodeExprIntLit>(&val->var)) {
					analyzer->constants.back().variable[stat.name] = lit->value;
				}
				else {
					analyzer->InvalidateVariable(stat.name);
				}
			}
			void operator()(NodeStatementAssignment& stat) {
				NodeExpr* val = analyzer->Fold(stat.expr);
				if (auto lit = std::get_if<NodeExprIntLit>(&val->var)) {
					analyzer->SetVariableValue(stat.ident.value, lit->value);
				}
				else {
					analyzer->InvalidateVariable(stat.ident.value);
				}
			}
			void operator()(NodeBlock& block) {
				analyzer->EnterScope();
				analyzer->SimulateBlock(block);
				analyzer->ExitScope();
			}
			void operator()(NodeStatementIf& _if) {
				NodeExpr* cond = analyzer->Fold(_if.condition);
				if (auto val = std::get_if<NodeExprIntLit>(&cond->var)) {
					if (val->value) {
						analyzer->SimulateBlock(_if.thenBlock);
					}
					else if (_if.elseBlock.has_value()) {
						analyzer->SimulateBlock(_if.elseBlock.value());
					}
				}
			}
			void operator()(NodeStatementWhile& _while) {
				size_t iterations = 0;
				while (true) {
					NodeExpr* cond = analyzer->Fold(_while.condition);
					auto* lit = std::get_if<NodeExprIntLit>(&cond->var);
					if (!lit || lit->value == 0 || ++iterations > 10000) break;
					analyzer->SimulateBlock(_while.block);
				}
			}
		};

		SimVisitor simVisitor{ this };
		std::visit(simVisitor, statement.stat);
	}

	void SimulateBlock(NodeBlock& block)
	{
		for (NodeStatement* stat : block.statements) {
			SimulateStatement(*stat);
		}
	}

	// -------------------------------------------------------------------------
	// AST TRANSFORMATION PASS
	// -------------------------------------------------------------------------
	std::optional<NodeStatement> AnalyzeStatement(NodeStatement& statement)
	{
		struct Visitor
		{
			ConstantFolder* analyzer;

			std::optional<NodeStatement> operator()(NodeStatementExit& stat)
			{
				stat.expr = analyzer->Fold(stat.expr);
				return NodeStatement{ stat };
			}
			std::optional<NodeStatement> operator()(NodeStatementVar& stat)
			{
				stat.expr = analyzer->Fold(stat.expr);
				if (auto value = std::get_if<NodeExprIntLit>(&stat.expr->var)) {
					if (analyzer->GetVariableValue(stat.name).has_value()) {
						analyzer->SetVariableValue(stat.name, value->value);
					}
					else {
						analyzer->constants.back().variable[stat.name] = value->value;
					}
				}
				else {
					analyzer->InvalidateVariable(stat.name);
				}
				return NodeStatement{ stat };
			}
			std::optional<NodeStatement> operator()(NodeBlock& block)
			{
				analyzer->EnterScope();
				analyzer->AnalyzeBlock(block);
				analyzer->ExitScope();
				return NodeStatement{ block };
			}
			std::optional<NodeStatement> operator()(NodeStatementIf& _if)
			{
				_if.condition = analyzer->Fold(_if.condition);

				if (auto value = std::get_if<NodeExprIntLit>(&_if.condition->var)) {
					if (value->value) {
						analyzer->AnalyzeBlock(_if.thenBlock);
						return NodeStatement{ _if.thenBlock };
					}
					else {
						if (_if.elseBlock.has_value()) {
							analyzer->AnalyzeBlock(_if.elseBlock.value());
							return NodeStatement{ _if.elseBlock.value() };
						}
						else {
							return std::nullopt;
						}
					}
				}

				analyzer->AnalyzeBlock(_if.thenBlock);
				if (_if.elseBlock.has_value()) {
					analyzer->AnalyzeBlock(_if.elseBlock.value());
				}

				return NodeStatement{ _if };
			}
			std::optional<NodeStatement> operator()(NodeStatementWhile& _while)
			{
				// 1. Check condition with current variable state
				NodeExpr* foldedCond = analyzer->Fold(_while.condition);
				auto* lit = std::get_if<NodeExprIntLit>(&foldedCond->var);

				if (!lit) {
					analyzer->AnalyzeBlock(_while.block);
					return NodeStatement{ _while };
				}

				if (lit->value == 0) {
					return std::nullopt;
				}

				size_t iterations = 0;
				const size_t MAX_ITERATIONS = 10000;

				while (lit && lit->value != 0) {
					if (++iterations > MAX_ITERATIONS) {
						return NodeStatement{ _while };
					}

					analyzer->SimulateBlock(_while.block);

					foldedCond = analyzer->Fold(_while.condition);
					lit = std::get_if<NodeExprIntLit>(&foldedCond->var);
				}

				return std::nullopt;
			}
			std::optional<NodeStatement> operator()(NodeStatementExpr& stat)
			{
				stat.expr = analyzer->Fold(stat.expr);
				return NodeStatement{ stat };
			}
			std::optional<NodeStatement> operator()(NodeStatementReturn& _ret)
			{
				_ret.expr = analyzer->Fold(_ret.expr);
				return NodeStatement{ _ret };
			}
			std::optional<NodeStatement> operator()(NodeStatementAssignment& assign)
			{
				assign.expr = analyzer->Fold(assign.expr);

				if (auto lit = std::get_if<NodeExprIntLit>(&assign.expr->var)) {
					analyzer->SetVariableValue(assign.ident.value, lit->value);
				}
				else {
					analyzer->InvalidateVariable(assign.ident.value);
				}

				return NodeStatement{ assign };
			}
		};

		Visitor visitor{ this };
		return std::visit(visitor, statement.stat);
	}

	std::optional<int> GetVariableValue(const std::string& name)
	{
		auto find = std::find_if(
			constants.rbegin(), constants.rend(),
			[&](const Scope& s) {
				return s.variable.contains(name);
			}
		);
		if (find != constants.rend())
		{
			return find->variable.at(name);
		}
		return {};
	}

	void SetVariableValue(const std::string& name, int value)
	{
		auto find = std::find_if(
			constants.rbegin(), constants.rend(),
			[&](const Scope& s) {
				return s.variable.contains(name);
			}
		);
		if (find != constants.rend())
		{
			find->variable.at(name) = value;
		}
	}

	void InvalidateVariable(const std::string& name)
	{
		auto find = std::find_if(
			constants.rbegin(), constants.rend(),
			[&](const Scope& s) {
				return s.variable.contains(name);
			}
		);

		if (find != constants.rend())
		{
			find->variable.erase(name);
		}
	}

	void AnalyzeBlock(NodeBlock& block)
	{
		std::vector<NodeStatement*> newStatements;
		newStatements.reserve(block.statements.size());

		for (NodeStatement* stat : block.statements) {
			if (auto updatedStat = AnalyzeStatement(*stat)) {
				*stat = std::move(*updatedStat);
				newStatements.push_back(stat);
			}
		}

		block.statements = std::move(newStatements);
	}

	void AnalyzeFunction(NodeFunction& func)
	{
		EnterScope();
		AnalyzeBlock(func.body.value());
		ExitScope();
	}

	void AnalyzeProg(NodeProg& prog)
	{
		for (auto& func : prog.functions)
		{
			if (func.body.has_value()) {
				AnalyzeFunction(func);
			}
		}
	}

	void EnterScope() { constants.push_back({}); }
	void ExitScope() { constants.pop_back(); }

	struct Scope
	{
		std::unordered_map<std::string, int> variable;
	};

private:
	NodeProg* ast;
	std::pmr::polymorphic_allocator<NodeExpr>* _allocator;
	std::vector<Scope> constants;
};