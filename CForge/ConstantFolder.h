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

	void FoldProgram(bool optimize)
	{
		AnalyzeProg(*ast, optimize);
	}

private:
	std::optional<ConstValue> Fold(NodeExpr* expr, ExprContext ctx = ExprContext::RValue) {
		if (!expr) return std::nullopt;



		return std::visit([&](auto& node) -> std::optional<ConstValue> {
			using T = std::decay_t<decltype(node)>;

			if constexpr (std::is_same_v<T, NodeExprBinary> || std::is_same_v<T, NodeExprCompare>)
			{
				auto lhsVal = ASTEvaluator::EvaluateExpr(node.left, constants, ctx);
				auto rhsVal = ASTEvaluator::EvaluateExpr(node.right, constants, ctx);

				if (!lhsVal && !rhsVal) return std::nullopt;

				(!lhsVal && rhsVal) ? node.right = ToNodeExpr(*rhsVal) : node.left = ToNodeExpr(*lhsVal);
				
			}
			else if constexpr (std::is_same_v<T, NodeExprInitList>) {
				std::vector<ConstValue> arrayVal;
				bool canFold = true;
				for (size_t i = 0; i < node.values.size(); ++i)
				{
					if (auto value = Fold(node.values[i]))
					{
						node.values[i] = ToNodeExpr(*value);
						arrayVal.push_back(*value);
					}
					else { canFold = false; }
				}
				if (canFold) {
					return arrayVal;
				}
				return std::nullopt;
			}
			else if constexpr (std::is_same_v<T, NodeExprAssignment>) {
				if (auto value = Fold(node.expr)) {
					node.expr = ToNodeExpr(*value);
				}

				return std::nullopt;
			}
			if (auto evalResult = ASTEvaluator::EvaluateExpr(expr, constants, ctx)) {

				return evalResult;
			}
			return std::nullopt;
			}, expr->var);
		
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
			std::optional<NodeStatement> operator()(NodeStatementVar& var)
			{

				auto baseType = TypeInfo::GetBaseType(var.type);
				auto value = analyzer->Fold(var.initializer);

				if (value && value->is_scalar())
				{
					var.initializer = analyzer->ToNodeExpr(*value);
				}
				if (!baseType.mod.is_const || var.symbol->isAddressTaken) { return NodeStatement{ var }; }
				if (!var.initializer) {

					auto dims = TypeInfo::GetArrayDimensions(var.type);
					ConstValue defaultValue = !dims.empty()
						? ConstValue::create_array(dims)
						: ConstValue{};

					if (!analyzer->GenerateVariable(var.id, std::move(defaultValue))) {
						std::cerr << "ConstFolder: Failed to generate const value for variable: "
							<< var.name << std::endl;
						return NodeStatement{ var };
					}
					return std::nullopt;
				}
				if (value)
				{
					if (analyzer->GenerateVariable(var.id, std::move(*value))) {
						return std::nullopt;
					}
				}
				std::cerr << "ConstFolder: Failed to generate const value for variable: " << var.name << std::endl;
				
				return NodeStatement{ var };
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
					bool is_true = false;
					std::visit([&is_true, this](auto&& arg) {
						using T = std::decay_t<decltype(arg)>;

						if constexpr (std::is_same_v<T, ConstValue::Scalar>) {
							is_true = std::visit([](auto&& s) -> bool {
								return s != 0;
								}, arg);
						}
						else if constexpr (std::is_same_v<T, std::vector<ConstValue>>) {
							is_true = true;
						}
						else if constexpr (std::is_same_v<T, ConstValue::Pointer>) {
							is_true = (analyzer->GetVariable(arg.variable_id) != nullptr);
						}
						}, value->data);


					if (is_true) {
						return analyzer->AnalyzeStatement(*_if.thenBody);
					}
					if (_if.elseBody) {
						return analyzer->AnalyzeStatement(*_if.elseBody);
					}
					return std::nullopt; // Eliminate the entire if statement (DCE)
					
				}

				// Dynamic condition: analyze branches normally without flattening
				if (_if.thenBody) {
					if (auto analyzedThen = analyzer->AnalyzeStatement(*_if.thenBody)) {
						*_if.thenBody = *analyzedThen;
					}
				}

				if (_if.elseBody) {
					if (auto analyzedElse = analyzer->AnalyzeStatement(*_if.elseBody)) {
						*_if.elseBody = *analyzedElse;
					}
				}

				return NodeStatement{ _if };
			}
			std::optional<NodeStatement> operator()(NodeStatementWhile& _while)
			{
				auto condVal = analyzer->Fold(_while.condition);
				if (!condVal) { analyzer->AnalyzeStatement(*_while.body); return NodeStatement{ _while }; };

				bool is_true = false;
				std::visit([&is_true, this](auto&& arg) {
					using T = std::decay_t<decltype(arg)>;

					if constexpr (std::is_same_v<T, ConstValue::Scalar>) {
						is_true = std::visit([](auto&& s) -> bool {
							return s != 0;
							}, arg);
					}
					else if constexpr (std::is_same_v<T, std::vector<ConstValue>>) {
						is_true = true;
					}
					else if constexpr (std::is_same_v<T, ConstValue::Pointer>) {
						is_true = (analyzer->GetVariable(arg.variable_id) != nullptr);
					}
					}, condVal->data);
				if(!is_true){ return std::nullopt; }

				analyzer->AnalyzeStatement(*_while.body);
				if (_while.update) {
					analyzer->Fold(_while.update);
				}
				
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
				if (auto value = analyzer->Fold(assign.expr)) {
					if (value->is_scalar())
					{
						assign.expr = analyzer->ToNodeExpr(*value);
					}
				}
				return NodeStatement{ assign };
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

	void AnalyzeProg(NodeProg& prog, bool optimize)
	{
		EnterScope();
		for (auto& stat : prog.items) {

			std::visit([&](auto* node) {
				using T = std::decay_t<decltype(*node)>;

				if constexpr (std::is_same_v<T, NodeFunction>) {
					if (optimize) {
						AnalyzeFunction(*node);
					}
				}
				else if constexpr (std::is_same_v<T, NodeStatementVar>) {
					if (node->initializer) {
						auto evaluated = ASTEvaluator::EvaluateExpr(node->initializer, constants);

						if (!evaluated) {
							std::cerr << "Error: Global variable '" << node->name
								<< "' initializer must be a compile-time constant.\n";
							std::exit(EXIT_FAILURE);
						}

						node->compiled_constant = *evaluated;
						GenerateVariable(node->id, *evaluated);
					}
					else {
						node->compiled_constant = ConstValue{ 0 };
					}
				}
				
				}, stat.decl);
			
		}


		

		ExitScope();
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


		
		if (!value.is_scalar()) {
			std::cerr << "ICE: Cannot convert ConstValue to NodeExpr" << std::endl;
			exit(EXIT_FAILURE);
		}
		return std::visit([this](ConstValue::Scalar&& Val)-> NodeExpr*
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

	bool GenerateVariable(const size_t& ID, ConstValue&& value)
	{
		if (ID == ValueID::INVALID_ID) return false;
		else if (constants.empty()) { constants.emplace_back(); }
		constants.back().variable[ID] = std::move(value);


		return true;

	}

	bool GenerateVariable(const size_t& ID, const ConstValue& value)
	{
		if (ID == ValueID::INVALID_ID) return false;
		else if (constants.empty()) { constants.emplace_back(); }
		constants.back().variable[ID] = value;

		return true;

	}
	
	void SetVariableValue(const size_t& ID, ConstValue&& value)
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
			find->variable.at(ID) = std::move(value);
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


	
	std::unordered_map<size_t, ConstValue> _allEnvVariables;
	std::vector<Scope> constants;
	NodeProg* ast;
	std::pmr::polymorphic_allocator<char>* _allocator;
	std::optional<int> _lastReturnValue;


};