#pragma once
#include "parser.h"



class ConstantFolder
{
public:
	ConstantFolder(NodeProg& program, std::pmr::polymorphic_allocator<NodeExpr>& allocator) :  ast(&program), _allocator(&allocator){}

	void FoldProgram()
	{
		AnalyzeProg(*ast);
	}
private:
	NodeExpr* Fold(NodeExpr* expr) {
		
		if (std::holds_alternative<NodeExprIntLit>(expr->var)) {
			return expr; 
		}
		else if (auto* identifier = std::get_if<NodeExprIndent>(&expr->var))
		{

			auto find = std::find_if(
				constants.rbegin(), constants.rend(),
				[&](const Scope& s) {
					return s.variable.contains(identifier->name);
				}
			);
			if (find != constants.rend())
			{
				return _allocator->new_object<NodeExpr>(NodeExprIntLit{ find->variable.at(identifier->name)});
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
		return expr;
	}

	int Compute(Token op, int left, int right)
	{

		switch (op.type)
		{
		case TokenType::plus:
			return left + right;
			break;
		case TokenType::mult:
			return left * right;
			break;
		case TokenType::sub:
			return left - right;
			break;
		case TokenType::greater:
			return (left > right);
		case TokenType::greater_equal:
			return (left >= right);
		case TokenType::less:
			return (left < right);
		case TokenType::less_equal:
			return (left <= right);
		case TokenType::equal_equal:
			return (left == right);
		default:

			return 0;
			break;
		}

	}

	std::optional<NodeExpr*> FoldBinaryExpr(NodeExprBinary* bin)
	{
		auto* leftInd = std::get_if<NodeExprIndent>(&bin->left->var);
		auto* rightInd = std::get_if<NodeExprIndent>(&bin->right->var);
		if (leftInd && rightInd) // case a - a; = 0
		{
			if (leftInd->name == rightInd->name)
			{
				if (bin->op.type == TokenType::sub)
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				}
			}
		}

		bin->left = Fold(bin->left);
		bin->right = Fold(bin->right);

		auto* leftLit = std::get_if<NodeExprIntLit>(&bin->left->var);
		auto* rightLit = std::get_if<NodeExprIntLit>(&bin->right->var);

		if (leftLit && rightLit) {
			int result = Compute(bin->op, leftLit->value, rightLit->value);

			return _allocator->new_object<NodeExpr>(NodeExprIntLit{ result });
		}
		else if (rightLit)
		{
			if (bin->op.type == TokenType::plus)
			{
				if (rightLit->value == 0)
				{
					return bin->left;
				}
			}
			else if (bin->op.type == TokenType::sub)
			{
				if (rightLit->value == 0)
				{
					return bin->left;
				}
			}
			else if (bin->op.type == TokenType::mult)
			{
				if (rightLit->value == 0)
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				}
				else if (rightLit->value == 1)
				{
					return bin->left;

				}
			}

		}
		else if (leftLit)
		{
			if (bin->op.type == TokenType::mult)
			{
				if (leftLit->value == 0)
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				}
				else if (leftLit->value == 1)
				{
					return bin->right;

				}
			}
		}
		return {};
	}
	std::optional<NodeExpr*> FoldCompExpr(NodeExprCompare* comp) 
	{
		comp->left = Fold(comp->left);
		comp->right = Fold(comp->right);

		auto* leftLit = std::get_if<NodeExprIntLit>(&comp->left->var);
		auto* rightLit = std::get_if<NodeExprIntLit>(&comp->right->var);


		if (leftLit && rightLit) {
			int result = Compute(comp->op, leftLit->value, rightLit->value);

			return _allocator->new_object<NodeExpr>(NodeExprIntLit{ result });
		}

		auto* leftBool = std::get_if<NodeExprBoolLit>(&comp->left->var);
		auto* rightBool = std::get_if<NodeExprBoolLit>(&comp->right->var);


		if (leftBool)
		{
			if (comp->op.type == TokenType::_and)
			{
				if (leftBool->value == false)
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				}
			}
			if (comp->op.type == TokenType::_or)
			{
				if (leftBool->value == true)
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 1 });
				}
			}
		}
		if (rightBool)
		{
			if (comp->op.type == TokenType::_and)
			{
				if (rightBool->value == false)
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				}
			}
			if (comp->op.type == TokenType::_or)
			{
				if (rightBool->value == true)
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 1 });
				}
			}
		}

		return {};
	}
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
					analyzer->constants.back().variable[stat.name] = value->value;
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
				_while.condition = analyzer->Fold(_while.condition);


				if (auto value = std::get_if<NodeExprIntLit>(&_while.condition->var)) {
					if (value->value) {
						analyzer->AnalyzeBlock(_while.block);
						return NodeStatement{ _while.block };
					}
					else
					{
						return std::nullopt;
					}
				}
				
				return NodeStatement{ _while };
			}

			std::optional<NodeStatement> operator()(const NodeStatementExpr& stat)
			{
				return NodeStatement{ stat };
			}
			std::optional<NodeStatement> operator()(NodeStatementReturn& _ret)
			{
				_ret.expr = analyzer->Fold(_ret.expr);
				return NodeStatement{ _ret };
			}
		};
		Visitor visitor{ this };
		return std::visit(visitor, statement.stat);
	}


	void AnalyzeBlock(NodeBlock& block)
	{
		std::vector<NodeStatement*> newStatements;
		newStatements.reserve(block.statements.size());

		for (NodeStatement* stat : block.statements) {
			if (auto updatedStat = AnalyzeStatement(*stat)) {
				// Overwrite the original statement with the optimized version
				*stat = std::move(*updatedStat);
				newStatements.push_back(stat);
			}
			// If updatedStat is std::nullopt, stat is skipped (dropped from newStatements)
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
	void ExitScope()  { constants.pop_back();  }

	struct Scope
	{
		std::unordered_map<std::string, int> variable;
	};
private:


	NodeProg* ast;
	std::pmr::polymorphic_allocator<NodeExpr>* _allocator;
	std::vector<Scope> constants;
};