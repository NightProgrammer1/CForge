#pragma once
#include "parser.h"
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <memory_resource>
#include "ASTEvaluator.h"
#include <span>


class ConstantFolder
{
public:
	ConstantFolder(NodeProg& program, std::pmr::polymorphic_allocator<char>& allocator)
		: ast(&program), _allocator(&allocator) {
	}

	void FoldProgram()
	{
		AnalyzeProg(*ast);
	}

private:
	std::optional<ConstValue> Fold(NodeExpr* expr, ExprContext ctx = ExprContext::RValue) {
		if (!expr) return std::nullopt;

		if (auto evalResult = ASTEvaluator::EvaluateExpr(expr, constants, ctx)){

			return evalResult;
		}
		return std::nullopt;
	}

	
	std::optional<NodeStatement> AnalyzeStatement(NodeStatement& statement)
	{
		struct Visitor
		{
			ConstantFolder* analyzer;

			std::optional<NodeStatement> operator()(NodeStatementExit& stat)
			{
				return NodeStatement{ stat };
			}
			std::optional<NodeStatement> operator()(NodeStatementVar& stat)
			{

				if (auto value = analyzer->Fold(stat.initializer))
				{
					if (analyzer->GenerateVariable(stat.id, *value)) {
						return std::nullopt;

					}
				}
				std::cerr << "ConstFolder: Failed to generate const value for variable: " << stat.name << std::endl;
				
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
				if (auto value = ASTEvaluator::EvaluateExpr(_if.condition, analyzer->constants)) {
					bool is_true = std::visit([](auto Val) -> bool {
						return (Val != 0);
						}, value->get_scalar());

					if (is_true) {
						return analyzer->AnalyzeStatement(*_if.thenBody);
					}
					else {
						if (_if.elseBody) {
							return analyzer->AnalyzeStatement(*_if.elseBody);
						}
						else {
							return std::nullopt; // Eliminate the entire if statement (DCE)
						}
					}
				}

				// Dynamic condition: analyze branches normally without flattening
				if (_if.thenBody) {
					auto analyzedThen = analyzer->AnalyzeStatement(*_if.thenBody);
					if (analyzedThen) {
						*_if.thenBody = *analyzedThen;
					}
				}

				if (_if.elseBody) {
					auto analyzedElse = analyzer->AnalyzeStatement(*_if.elseBody);
					if (analyzedElse) {
						*_if.elseBody = *analyzedElse;
					}
				}

				return NodeStatement{ _if };
			}
			std::optional<NodeStatement> operator()(NodeStatementWhile& _while)
			{
				auto testEnv = analyzer->constants;
				// Evaluate the loop entirely at compile time
				EvalResult result = ASTEvaluator::EvaluateWhile(_while, testEnv);

				if (result.status != EvalResult::Status::Failure) {

					for (size_t i = 0; i < analyzer->constants.size() && i < testEnv.size(); ++i) {
						for (const auto& [id, val] : testEnv[i].variable) {
							analyzer->constants[i].variable[id] = val;
						}
					}

					if (result.status == EvalResult::Status::Return) {
						// Handle return statement if needed
						return NodeStatement{ NodeStatementReturn{analyzer->ToNodeExpr((result.value))} };
					}

					return std::nullopt;
				}

				// If it's a dynamic loop, fold condition and analyze body normally
				if (auto value = analyzer->Fold(_while.condition))
				{
					_while.condition = analyzer->ToNodeExpr(*value);
				}
				analyzer->AnalyzeStatement(*_while.body);

				return NodeStatement{ _while };
			}

			std::optional<NodeStatement> operator()(NodeStatementBreak& _break)
			{
				return NodeStatement{ _break };
			}
			std::optional<NodeStatement> operator()(NodeStatementContinue& _continue)
			{
				return NodeStatement{ _continue };
			}
			std::optional<NodeStatement> operator()(NodeStatementExpr& stat)
			{

				if (auto value = analyzer->Fold(stat.expr))
				{
					stat.expr = analyzer->ToNodeExpr(*value);
				}
				return NodeStatement{ stat };
			}
			std::optional<NodeStatement> operator()(NodeStatementReturn& _ret)
			{
				if (auto value = analyzer->Fold(_ret.expr))
				{
					_ret.expr = analyzer->ToNodeExpr(*value);
				}
				return NodeStatement{ _ret };
			}
			std::optional<NodeStatement> operator()(NodeStatementAssignment& assign)
			{
				// Try to evaluate the RHS to a constant value
				if (auto value = ASTEvaluator::EvaluateExpr(assign.expr, analyzer->constants)) {

					if (auto* ident = std::get_if<NodeExprIdent>(&assign.target->var)) {
						analyzer->SetVariableValue(ident->id, *value);
						return std::nullopt;
					}
					else if (auto lhs = ASTEvaluator::EvaluateExpr(assign.target, analyzer->constants, ExprContext::LValue)) {

						if (lhs->is_pointer())
						{
							auto ptr = lhs->get_pointer();
							if (auto varVal = analyzer->GetVariable(ptr->variable_id))
							{
								if (varVal->is_scalar()) {
									analyzer->SetVariableValue(ptr->variable_id, *value);
								}
								else if (varVal->is_array()) {
									ConstValue updatedArray = *varVal;
									std::vector<size_t> finalIndices = ptr->indices;
									if (finalIndices.empty()) {
										finalIndices.push_back(ptr->element_offset);
									}
									else {
										finalIndices.back() += ptr->element_offset;
									}
									updatedArray.set_value_at_indices(finalIndices, *value);
									analyzer->SetVariableValue(ptr->variable_id, updatedArray);
									return std::nullopt;
								}
							}
							
						}
					}
					
				}
				else {
					if (auto* ident = std::get_if<NodeExprIdent>(&assign.target->var)) {
						analyzer->InvalidateVariable(ident->id);
					}
					else {
						analyzer->InvalidateVariable(analyzer->GetID(assign.target));
					}
				}
				return NodeStatement{ assign };
			}
			std::optional<NodeStatement> operator()(NodeStatementArray& arr)
			{
				if (!arr.initializer) {

					auto dims = TypeInfo::GetArrayDimensions(arr.element_type);
					ConstValue defaultArray;
					if (!dims.empty()) {
						defaultArray = analyzer->create_nested_array(dims);
						if (!analyzer->GenerateVariable(arr.id, defaultArray))
						{
							std::cerr << "ConstFolder: Failed to generate const value for variable: " << arr.name << std::endl;
							return NodeStatement{ arr };
						}
					}
					else {
						size_t elementCount = arr.element_type->Size() / TypeInfo::GetBaseType(arr.element_type)->Size();
						defaultArray = ConstValue::create_array(elementCount);
						
					}
					if (!analyzer->GenerateVariable(arr.id, defaultArray))
					{
						std::cerr << "ConstFolder: Failed to generate const value for variable: " << arr.name << std::endl;
						return NodeStatement{ arr };

					}
					return std::nullopt;
				}

				if (auto value = analyzer->Fold(arr.initializer))
				{
					arr.initializer = analyzer->ToNodeExpr(*value);

					return std::nullopt;

				}
				std::cerr << "ConstFolder: Failed to generate const value for variable: " << arr.name << std::endl;
				return NodeStatement{ arr };
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
			if (!stat) continue;

			auto updatedStat = AnalyzeStatement(*stat);

			if (updatedStat) {
				*stat = std::move(*updatedStat);
				newStatements.push_back(stat);

			}
		}

		block.statements = std::move(newStatements);
	}

	void AnalyzeFunction(NodeFunction& func)
	{

		if (!func.parameters.empty()) {
			return;
		}
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


	std::optional<NodeExpr*> FoldBinaryExpr(NodeExprBinary* bin)
	{
		/*
		auto* leftInd = std::get_if<NodeExprIdent>(&bin->left->var);
		auto* rightInd = std::get_if<NodeExprIdent>(&bin->right->var);
		if (leftInd && rightInd)
		{
			if (leftInd->name == rightInd->name)
			{
				if (bin->op == TokenType::sub)
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				}
				if (bin->op == TokenType::div)
				{
					auto value = GetVariable(leftInd->id);
					//if (value && *value != 0) {
					//	return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 1 });
					//}
				}
			}
		}

		//NodeExpr* foldedLeft = ToRValue(Fold(bin->left));
		//NodeExpr* foldedRight = ToRValue(Fold(bin->right));

		//auto leftLit = ASTEvaluator::EvaluateExpr(foldedLeft, constants);
		//auto rightLit = ASTEvaluator::EvaluateExpr(foldedRight, constants);


		//if (result) {
			//return ToNodeExpr(*result);
		//}
		//if (leftLit && rightLit) {
			//auto result = Compute(bin->op, *leftLit, *rightLit);

			//if (result) {
				//return ToNodeExpr(*result);
			//}
			return nullptr;
		}
		else if (rightLit)
		{
			if (bin->op == TokenType::plus || bin->op == TokenType::sub)
			{
				//if (rightLit->value == 0) return foldedLeft;
			}
			else if (bin->op == TokenType::star)
			{
				//if (rightLit->value == 0) return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				//if (rightLit->value == 1) return foldedLeft;
			}
			else if (bin->op == TokenType::div)
			{
				//if (leftLit->value == 1) return foldedRight;
			}
		}
		else if (leftLit)
		{
			if (bin->op == TokenType::star)
			{
				//if (leftLit->value == 0) return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 0 });
				//if (leftLit->value == 1) return foldedRight;
			}

		}
		return {};
		*/
	}



	void EnterScope() { constants.emplace_back(); }
	void ExitScope() { constants.pop_back(); }

	NodeExpr* ToNodeExpr(const ConstValue& value)
	{
		return std::visit([this](ConstValue::Scalar Val)-> NodeExpr*
			{
				if (auto result = std::get_if<int>(&Val))
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ *result });
				}
				else if (auto result = std::get_if<bool>(&Val))
				{
					return _allocator->new_object<NodeExpr>(NodeExprBoolLit{ *result });
				}
				return nullptr;
			}
		, value.get_scalar());
	}
	
	ConstValue* GetVariable(const size_t ID)
	{
		auto find = std::find_if(
			constants.rbegin(), constants.rend(),
			[&](const Scope& s) {
				return s.variable.contains(ID);
			}
		);
		if (find != constants.rend())
		{
			return &find->variable.at(ID);
		}
		return nullptr;
	}

	size_t GetID(const NodeExpr* expr)
	{
		if (auto* identifier = std::get_if<NodeExprIdent>(&expr->var))
		{
			return identifier->id;
		}
		else if (auto* access = std::get_if<NodeExprArrayAccess>(&expr->var))
		{
			return GetID(access->array);
		}

		return ValueID::INVALID_ID;
	}

	bool GenerateVariable(const size_t& ID, const ConstValue& value)
	{
		if (ID == ValueID::INVALID_ID) return false;
		else if (constants.empty()) { constants.emplace_back(); }
		constants.back().variable[ID] = value;


		return true;

	}
	
	void SetVariableValue(const size_t& ID, const ConstValue& value)
	{
		if (ID == ValueID::INVALID_ID) return;

		auto find = std::find_if(
			constants.rbegin(), constants.rend(),
			[&](const Scope& s) {
				return s.variable.contains(ID);
			}
		);
		if (find != constants.rend())
		{
			find->variable.at(ID) = value;
		}
	}

	void InvalidateVariable(const size_t& ID)
	{

		if (ID == ValueID::INVALID_ID) return;
		auto find = std::find_if(
			constants.rbegin(), constants.rend(),
			[&](const Scope& s) {
				return s.variable.contains(ID);
			}
		);

		if (find != constants.rend())
		{
			find->variable.erase(ID);
		}
	}

	ConstValue create_nested_array(const std::vector<size_t>& dims, size_t depth = 0) {
		if (depth == dims.size() - 1) {
			return ConstValue::create_array(dims[depth]);
		}

		std::vector<ConstValue> innerElements;
		innerElements.reserve(dims[depth]);
		for (size_t i = 0; i < dims[depth]; ++i) {
			innerElements.push_back(create_nested_array(dims, depth + 1));
		}
		return ConstValue{ innerElements };
	}

	
	std::unordered_map<size_t, ConstValue> _allEnvVariables;
	std::vector<Scope> constants;
	NodeProg* ast;
	std::pmr::polymorphic_allocator<char>* _allocator;
	std::optional<int> _lastReturnValue;


};