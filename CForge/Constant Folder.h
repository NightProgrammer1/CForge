#pragma once
#include "parser.h"
#include <algorithm>
#include <optional>
#include <unordered_map>
#include <vector>
#include <span>
#include <memory_resource>
#include "ASTEvaluator.h"
#include <unordered_set>

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
		else if (auto* identifier = std::get_if<NodeExprIdent>(&expr->var))
		{
			if (auto value = GetVariable(identifier->name))
			{
				return _allocator->new_object<NodeExpr>(NodeExprRef{value});
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
			std::vector<ConstValue> argValues;
			for (NodeExpr* arg : funcCall->args) {
				NodeExpr* foldedArg = Fold(arg);
				if(auto value = ToConstValue(foldedArg))
					argValues.push_back(*value);
			}
			if (!funcCall->func || !funcCall->func->body.has_value()) return {};


			EnterScope();

			auto returnValue = EvaluateFunctionCall(*funcCall->func, argValues);

			ExitScope();

			if (returnValue)
			{
				return ToNodeExpr(*returnValue);
			}
			return expr;
		}
		else if (auto* unary = std::get_if<NodeExprUnary>(&expr->var))
		{
			if (auto* ident = std::get_if<NodeExprIdent>(&unary->expr->var))
			{
				std::string varName = ident->name;
				if (auto currentVal = GetVariable(varName))
				{
					switch (unary->op){
						case TokenType::post_increment:
							currentVal->data = std::visit([](auto& val) -> ConstValue::Scalar { return ConstValue::Scalar{ val + 1 }; }, currentVal->get_scalar());
							break;
						case TokenType::pre_increment:
							currentVal->data = std::visit([](auto& val) -> ConstValue::Scalar { return ConstValue::Scalar{ val + 1 }; }, currentVal->get_scalar());
							break;
						case TokenType::post_decrement:
							currentVal->data = std::visit([](auto& val) -> ConstValue::Scalar { return ConstValue::Scalar{ val - 1 }; }, currentVal->get_scalar());
							break;
						case TokenType::pre_decrement:
							currentVal->data = std::visit([](auto& val) -> ConstValue::Scalar { return ConstValue::Scalar{ val - 1 }; }, currentVal->get_scalar());
							break;
						case TokenType::negate:
							currentVal->data = std::visit([](auto& val) -> ConstValue::Scalar { return ConstValue::Scalar{ -val }; }, currentVal->get_scalar());
							break;
						case TokenType::_not:
							currentVal->data = std::visit([](auto& val) -> ConstValue::Scalar { return ConstValue::Scalar{ !val }; }, currentVal->get_scalar());
						case TokenType::ampersand:
							// Do nothing for ampersand, as it doesn't change the value
							break;
						case TokenType::star:
							// Do nothing for star, as it doesn't change the value
							break;
					default:
						break;
					}

					auto node = _allocator->new_object<NodeExpr>();
					node->var = NodeExprRef{ currentVal };
					return node;
				}
				else
				{
					InvalidateVariable(varName);
				}
				return expr;
			}
		}
		else if (auto* assign = std::get_if<NodeExprAssignment>(&expr->var))
		{
			NodeExpr* val = Fold(assign->expr);
			if (auto value = ToConstValue(expr)) {
				SetVariableValue(assign->name, ConstValue{ *value });
			}
			else {
				InvalidateVariable(assign->name->name);
			}
		}
		else if (auto* list = std::get_if<NodeExprInitList>(&expr->var))
		{
			
			for (size_t i = 0; i < list->values.size(); i++)
			{
				list->values[i] = Fold(list->values[i]);
			}
			{
				auto node = _allocator->new_object<NodeExpr>();
				node->var = *list;
				return node;
			}
		}
		else if (auto* access = std::get_if<NodeExprArrayAccess>(&expr->var))
		{
			ASTEvaluator evaluator(*ast);
			std::vector<size_t> indices;
			for (size_t i = 0; i < access->indices.size(); i++)
			{
				auto index = access->indices[i];
				if (auto indexVal = std::get_if<NodeExprIntLit>(&index->var)) { // if already an int literal
					if (indexVal->value >= access->targetArrayDimension[i])
					{
						std::cerr << "Warning index " << indexVal->value
							<< " is out of bound of array '" << access->name->name << "'\n";

					}
					indices.push_back(indexVal->value);
					continue;
				}
				if (auto value = evaluator.EvaluateExpr(index, _allEnvVariables)) //Evaluate index to constant expr
				{
					
					auto newIndex = ToNodeExpr(*value); //Todo add valid checking
					access->indices[i] = newIndex;

					auto indexValue = std::get_if<NodeExprIntLit>(&newIndex->var);

					if (indexValue->value >= access->targetArrayDimension[i])
					{
						std::cerr << "Warning index " << indexValue->value
							<< " is out of bound of array '" << access->name->name << "'\n";

					}
					indices.push_back(indexValue->value);
					continue;
				}
			}
			auto array = GetVariable(access->name->name);
			if (auto result = ConstValue::get_value_from_array(&*array, indices)) {
				{
					auto node = _allocator->new_object<NodeExpr>();
					node->var = NodeExprRef{ result };
					return node;
				}
			}
				
		}
		return expr;
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
					analyzer->GenerateVariable(stat.name->name, ConstValue{ value->value });
					
				}
				else if (auto value = std::get_if<NodeExprBoolLit>(&stat.expr->var)) {
					analyzer->GenerateVariable(stat.name->name, ConstValue{ value->value });
		
				}
				else {
					analyzer->InvalidateVariable(stat.name->name);
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
						return analyzer->AnalyzeStatement(*_if.thenBody);
					}
					else {
						if (_if.elseBody) {
							return analyzer->AnalyzeStatement(*_if.elseBody);
						}
						else {
							return std::nullopt;
						}
					}
				}

				*_if.thenBody = *analyzer->AnalyzeStatement(*_if.thenBody);
				if (_if.elseBody.) {
					analyzer->AnalyzeBlock(_if.elseBody);
				}

				return NodeStatement{ _if };
			}
			std::optional<NodeStatement> operator()(NodeStatementWhile& _while)
			{

				std::unordered_map<std::string, ConstValue> testEnv;

				for (const auto& scope : analyzer->constants) {
					for (const auto& [name, value] : scope.variable) {
						testEnv[name] = value;
					}
				}


				std::unordered_set<std::string> existingVars;
				for (const auto& [name, _] : testEnv) {
					existingVars.insert(name);
				}

				ASTEvaluator evaluator(*analyzer->ast);
				EvalResult result = evaluator.EvaluateWhile(_while, testEnv);

				if (result.status != EvalResult::Status::Failure) {
					for (const auto& [name, val] : testEnv) {
						if (existingVars.count(name)) {
							analyzer->SetVariableValue(name, val);
						}
					}

					if (result.status == EvalResult::Status::Return) {
						//return NodeStatement{ NodeStatementReturn{
							//analyzer->_allocator->new_object<NodeExpr>(NodeExprIntLit{ result.ConstValue })
						//} };
					}

					return std::nullopt;
				}

				_while.condition = analyzer->Fold(_while.condition);
				analyzer->AnalyzeBlock(_while.block);

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
				stat.expr = analyzer->Fold(stat.expr);
				return NodeStatement{ stat };
			}
			std::optional<NodeStatement> operator()(NodeStatementReturn& _ret)
			{
				_ret.expr = analyzer->Fold(_ret.expr);
				_ret.expr = analyzer->ToRValue(_ret.expr);
	
				return NodeStatement{ _ret };
			}
			std::optional<NodeStatement> operator()(NodeStatementAssignment& assign)
			{
				assign.expr = analyzer->Fold(assign.expr);
				if (auto value = analyzer->ToConstValue(assign.expr)) {

					assign.target = analyzer->Fold(assign.target);
					if (auto* ref = std::get_if<NodeExprRef>(&assign.target->var)) {
						*(ref->ptr) = *value; 
					}
					
				}
				else {
					analyzer->InvalidateVariable(assign.name);
				}
				return NodeStatement{ assign };
				
			}
			std::optional<NodeStatement> operator()(NodeStatementArray& arr)
			{
				auto list = NodeExpr{*arr.initializer};
				//arr.initializer = analyzer->Fold(&list);


				std::vector<ConstValue> data;

				std::vector<size_t> dim;

				for (auto dimExpr : arr.name->Type.dimensions)
				{
					if (auto dimLit = std::get_if<NodeExprIntLit>(&dimExpr->var))
					{
						dim.push_back(dimLit->value);
					}
					else
					{
						std::cerr << "Error: Array dimension is not a constant integer literal.\n";
						return std::nullopt;
					}
				}
				if (arr.initializer)
				{
					std::span<const size_t> element_dims = std::span(dim).subspan(1);

					for (auto value : arr.initializer->values)
					{
						auto const_val = analyzer->ToConstValue(value, arr.element_type, element_dims);
						if (const_val.has_value()) {
							data.push_back(const_val.value());
						}
						else {
						}
					}
				}

				analyzer->GenerateVariable(arr.name, ConstValue{ data });
				return NodeStatement{ arr };
			}

		};

		Visitor visitor{ this };
		return std::visit(visitor, statement.stat);
	}

	std::optional<ConstValue> Compute(TokenType op, ConstValue lhs, ConstValue rhs)
	{
		if (!lhs.is_scalar() || !rhs.is_scalar()) {
			return std::nullopt;
		}
		return std::visit([op](auto L, auto R) -> std::optional<ConstValue> {
			using CommonType = std::common_type_t<decltype(L), decltype(R)>;
			auto l_promoted = static_cast<CommonType>(L);
			auto r_promoted = static_cast<CommonType>(R);

			switch (op) {
			case TokenType::plus:
				return ConstValue{ ConstValue::Scalar{ l_promoted + r_promoted } };
			case TokenType::sub:
				return ConstValue{ ConstValue::Scalar{ l_promoted - r_promoted } };
			case TokenType::star:
				return ConstValue{ ConstValue::Scalar{ l_promoted * r_promoted } };
			case TokenType::div:
				if (r_promoted == 0) return std::nullopt;
				return ConstValue{ ConstValue::Scalar{ l_promoted / r_promoted } };
			case TokenType::less:
				return ConstValue{ ConstValue::Scalar{ l_promoted < r_promoted } };
			case TokenType::less_equal:
				return ConstValue{ ConstValue::Scalar{ l_promoted <= r_promoted } };
			case TokenType::greater:
				return ConstValue{ ConstValue::Scalar{ l_promoted > r_promoted } };
			case TokenType::greater_equal:
				return ConstValue{ ConstValue::Scalar{ l_promoted >= r_promoted } };
			case TokenType::equal_equal:
				return ConstValue{ ConstValue::Scalar{ l_promoted == r_promoted } };
			case TokenType::not_equal:
				return ConstValue{ ConstValue::Scalar{ l_promoted != r_promoted } };
			default:
				return std::nullopt;
			}
			}, lhs.get_scalar(), rhs.get_scalar());
	}

	std::optional<NodeExpr*> FoldBinaryExpr(NodeExprBinary* bin)
	{
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
					auto value = GetVariable(leftInd->name);
					//if (value && *value != 0) {
					//	return _allocator->new_object<NodeExpr>(NodeExprIntLit{ 1 });
					//}
				}
			}
		}

		NodeExpr* foldedLeft = ToRValue(Fold(bin->left));
		NodeExpr* foldedRight = ToRValue(Fold(bin->right));

		auto leftLit = ToConstValue(foldedLeft);
		auto rightLit = ToConstValue(foldedRight);

		if (leftLit && rightLit) {
			auto result = Compute(bin->op, *leftLit, *rightLit);

			if (result) {
				return ToNodeExpr(*result);
			}
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
	}

	
	std::optional<NodeExpr*> FoldCompExpr(NodeExprCompare* comp)
	{
		NodeExpr* foldedLeft = Fold(comp->left);
		auto lhs = ToConstValue(foldedLeft);

		if (lhs && lhs->is_scalar())
		{
			auto result = std::visit([&](auto L) -> NodeExpr* {
				if (comp->op == TokenType::_and && L == 0)
					return _allocator->new_object<NodeExpr>(NodeExprBoolLit{ false });
				else if (comp->op == TokenType::_or && L != 0)
					return _allocator->new_object<NodeExpr>(NodeExprBoolLit{ true });
				return nullptr;
			}, lhs->get_scalar());

			if (result) return result;
		}
		NodeExpr* foldedRight = Fold(comp->right);
		auto rhs = ToConstValue(foldedRight);

		if (rhs && rhs->is_scalar())
		{
			auto result = std::visit([&](auto R) -> NodeExpr* {
				if (comp->op == TokenType::_and && R == 0)
					return _allocator->new_object<NodeExpr>(NodeExprBoolLit{ false });
				else if (comp->op == TokenType::_or && R != 0)
					return _allocator->new_object<NodeExpr>(NodeExprBoolLit{ true });
				return nullptr;
				}, rhs->get_scalar());
			if (result) return result;

		}
		

		if (lhs && rhs) {
			if (auto result = Compute(comp->op, *lhs, *rhs))
			{
				return ToNodeExpr(*result);
			}
		}

		return {};
	}
	
	

	void AnalyzeBlock(NodeBlock& block)
	{
		std::vector<NodeStatement*> newStatements;
		newStatements.reserve(block.statements.size());

		for (NodeStatement* stat : block.statements) {
			if (!stat) continue;

			// 1. Analyze and fold the current statement
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

	std::optional<ConstValue> EvaluateFunctionCall(const NodeFunction& func, const std::vector<ConstValue>& argValues) {
		std::unordered_map<std::string, ConstValue> localEnv;
		for (size_t i = 0; i < func.parameters.size(); ++i) {
			//localEnv[func.parameters[i].name] = argValues[i]; // set the function local environnement
		}
		assert(func.body && "Function has no body");
		ASTEvaluator evaluator(*ast);
		EvalResult result = evaluator.EvaluateBlock(func.body.value(), localEnv);

		if (result.status == EvalResult::Status::Return) {
			return result.value;
		}

		return std::nullopt; // Evaluation failed or did not return
	}

	void EnterScope() { constants.push_back({}); }
	void ExitScope() { constants.pop_back(); }


	
	//Transfrom NodeExpr to ConstValue
	std::optional<ConstValue> ToConstValue(const NodeExpr* expr)
	{
		if (auto val = std::get_if<NodeExprIntLit>(&expr->var))
		{
			return ConstValue{ ConstValue::Scalar{val->value} };
		}
		else if (auto val = std::get_if<NodeExprArrayAccess>(&expr->var))
		{
			return ConstValue{ ConstValue::Scalar{} };
		}
		else if (auto val = std::get_if<NodeExprInitList>(&expr->var))
		{
			std::vector<ConstValue> data;
			for (auto value : val->values)
			{
				if (auto v = ToConstValue(value)) {
					data.push_back(*v);
				}
			}

			return ConstValue{ data };
		}
		else if (auto val = std::get_if<NodeExprRef>(&expr->var))
		{
			return *val->ptr;
		}


		return std::nullopt;
	}

	std::optional<ConstValue> ToConstValue(
		const NodeExpr* expr,
		Type base_type,
		std::span<const size_t> dims
	) {
		if (!expr) return std::nullopt;

		if (auto list = std::get_if<NodeExprInitList>(&expr->var))
		{
			if (list->values.empty()) {
				return CreateDefaultConstValue(base_type, dims);
			}

			std::span<const size_t> child_dims = dims.empty() ? dims : dims.subspan(1);

			std::vector<ConstValue> inner_data;
			for (auto child : list->values) {
				auto val = ToConstValue(child, base_type, child_dims);
				if (!val) return std::nullopt;
				inner_data.push_back(*val);
			}
			return ConstValue{ inner_data };
		}

		return ToConstValue(expr);
	}


	ConstValue CreateDefaultConstValue(Type base_type, std::span<const size_t> dims)
	{
		if (dims.empty()) {
			switch (base_type.kind) {
			case Type::Kind::Float: return ConstValue{ ConstValue::Scalar{ 0.0 } };
			case Type::Kind::Bool:  return ConstValue{ ConstValue::Scalar{ int64_t(0) } };
			case Type::Kind::Int:
			default:          return ConstValue{ ConstValue::Scalar{ int64_t(0) } };
			}
		}

		size_t current_dim_size = dims[0];
		auto child_dims = dims.subspan(1);

		std::vector<ConstValue> elements;
		elements.reserve(current_dim_size);
		for (size_t i = 0; i < current_dim_size; ++i) {
			elements.push_back(CreateDefaultConstValue(base_type, child_dims));
		}
		return ConstValue{ elements };
	}
	NodeExpr* ToNodeExpr(const ConstValue& value)
	{
		return std::visit([this](ConstValue::Scalar Val)-> NodeExpr*
			{
				if (auto result = std::get_if<int64_t>(&Val))
				{
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ *result });
				}

				return nullptr;
			}
		, value.get_scalar());
	}
	NodeExpr* ToRValue(NodeExpr* expr) {
		if (!expr) return nullptr;

		if (auto* ref = std::get_if<NodeExprRef>(&expr->var)) {
			if (ref->ptr) {
				if (auto scalarInt = std::get_if<int64_t>(&ref->ptr->get_scalar())) {
					return _allocator->new_object<NodeExpr>(NodeExprIntLit{ *scalarInt });
				}
			}
		}

		return expr;
	}
	ConstValue* GetVariable(const std::string& name)
	{
		auto find = std::find_if(
			constants.rbegin(), constants.rend(),
			[&](const Scope& s) {
				return s.variable.contains(name);
			}
		);
		if (find != constants.rend())
		{
			return &find->variable.at(name);
		}
		return nullptr;
	}

	void GenerateVariable(const std::string& name, const ConstValue& value)
	{
		constants.back().variable[name] = value;
		_allEnvVariables[name] = value;

	}
	void SetVariableValue(const std::string& name, const ConstValue& value)
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
			_allEnvVariables.at(name) = value;
		}
	}
	void SetVariableValue(const std::string& name, const ConstValue& value, std::vector<size_t> indices)
	{

		auto array = GetVariable(name);

		if (array) {

			if (auto data = ConstValue::get_value_from_array(array, indices)) {
				*data = value;
			}
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
			_allEnvVariables.erase(name);
		}
	}



	struct Scope
	{
		std::unordered_map<std::string, ConstValue> variable;
	};
	std::unordered_map<std::string, ConstValue> _allEnvVariables;
	std::vector<Scope> constants;
	NodeProg* ast;
	std::pmr::polymorphic_allocator<NodeExpr>* _allocator;
	std::optional<int> _lastReturnValue;

};