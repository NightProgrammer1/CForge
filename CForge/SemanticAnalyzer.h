#pragma once
#include "types.h"
#include "SymbolTable.h"
#include <algorithm>
class SemanticAnalyzer
{
public:
	SemanticAnalyzer(NodeProg& program, std::pmr::polymorphic_allocator<char>& allocator) : ast(program), _allocator(allocator) {}
	void Analyze()
	{
		symbols.EnterScope(); // Global scope
		AnalyzeProg(ast);
		symbols.ExitScope();
	}
private:

	std::shared_ptr<Type> AnalyzeExpression(NodeExpr& expr)
	{
		struct Visitor
		{
			SemanticAnalyzer* analyzer;
			NodeExpr& expr;
			std::shared_ptr<Type> operator()(const NodeExprIntLit& ) const {
				return expr.type;
			}

			std::shared_ptr<Type> operator()(const NodeExprBoolLit&) const {
				return expr.type;
			}
			std::shared_ptr<Type> operator()(NodeExprCall& call) const {
				auto funcSymbol = analyzer->symbols.LookupFunction(call.name);

				if (!funcSymbol->hasBody) {
					std::cerr << "Semantic Error: Call to undeclared function '"
						<< call.name << "'\n";
					exit(EXIT_FAILURE);
				}
				
				if (call.args.size() != funcSymbol->paramTypes.size()) {
					std::cerr << "Semantic Error: Incorrect argument count for function '"
						<< call.name << "'\n";
					exit(EXIT_FAILURE);
				}
				for (size_t i = 0; i < call.args.size(); ++i) {
					auto argType = analyzer->AnalyzeExpression(*call.args[i]);
					auto expectedType = funcSymbol->paramTypes[i];
					//Decay array to pointer if the parameter is a pointer
					if (TypeInfo::IsArray(argType) && TypeInfo::IsPointer(expectedType))
					{
						argType = Type::MakePointer(argType->element);
					}
					if (!TypeInfo::SameType(argType, expectedType)) {
						std::cerr << "Type Error: Argument " << i + 1 << ": " << TypeInfo::TypeToString(argType) << " in call to '"
							<< call.name << "' has mismatched type.\n";
						exit(EXIT_FAILURE);
					}
				}

				auto function = std::find_if(analyzer->ast.functions.begin(), analyzer->ast.functions.end(), [&](NodeFunction& func) { if (!func.body.has_value()) { return false; } return (func.name == call.name) ? true : false; });
				if (function == analyzer->ast.functions.end() || !function->body.has_value()) { exit(EXIT_FAILURE); };


				call.func = &(*function);

				return funcSymbol->returnType;
			
			}
			std::shared_ptr<Type> operator()(NodeExprAssignment& assign) const
			{
				auto exprType = analyzer->AnalyzeExpression(*assign.expr);
				auto targetType = analyzer->AnalyzeExpression(*assign.target);
				if (!TypeInfo::SameType(exprType, targetType) || (TypeInfo::GetPointerDepth(targetType) != (TypeInfo::GetPointerDepth(exprType)))) {
					std::cerr << "Type Error: Cannot convert expression of type "
						<< TypeInfo::TypeToString(exprType)
						<< " to " << TypeInfo::TypeToString(targetType) << "\n";
					exit(EXIT_FAILURE);
				}

				return assign.target->type;
				
			}
			std::shared_ptr<Type> operator()(NodeExprIdent& expr) const  {
				auto symbol = analyzer->symbols.LookupVariable(expr.name);

				if (!symbol)
				{
					std::cerr << "Semantic Error: Identifier '" << expr.name << "' not found";
					exit(EXIT_FAILURE);
				}
				expr.symbol = symbol;
				expr.type = symbol->type;
				expr.id = symbol->id;
				expr.name = symbol->name;
				return symbol->type;
			}
			std::shared_ptr<Type> operator()(const NodeExprVoid&) const
			{
				return std::make_shared<Type>( Type::Kind::Void );
			}

			std::shared_ptr<Type> operator()(const NodeExprCompare& expr) const
			{
				auto leftType = analyzer->AnalyzeExpression(*expr.left);
				auto rightType = analyzer->AnalyzeExpression(*expr.right);

				if (!leftType || !rightType) {
					std::cerr << "Type Error: Invalid operand types in comparison.\n";
					exit(EXIT_FAILURE);
				}

				if (expr.op == TokenType::_and || expr.op == TokenType::_or) {
					if (!TypeInfo::IsScalar(leftType) || !TypeInfo::IsScalar(rightType)) {
						std::cerr << "Type Error: Logical operators (&&, ||) require scalar operands.\n";
						exit(EXIT_FAILURE);
					}
				}
				else {
					// Relational / Equality comparison (<, <=, >, >=, ==, !=)
					if (*leftType != *rightType) {
						if (!TypeInfo::CanCompare(leftType, rightType)) {
							std::cerr << "Type Error: Comparison operands must have matching types.\n";
							exit(EXIT_FAILURE);
						}
					}
				}

				// Comparisons and logical operations ALWAYS evaluate to Bool 
				return Type::MakeBool();
			}
			std::shared_ptr<Type> operator()(NodeExprBinary& expr) const
			{
				auto leftType = analyzer->AnalyzeExpression(*expr.left);
				auto rightType = analyzer->AnalyzeExpression(*expr.right);

				//Handles array decay
				if (TypeInfo::IsArray(leftType))
				{
					expr.left->type = Type::MakePointer(leftType->element);
				}
				if (TypeInfo::IsArray(rightType))
				{
					expr.right->type = Type::MakePointer(rightType->element);
				}
				if (auto commonType = Type::GetCommonType(leftType, rightType))
				{
					return commonType;
				}

				std::cerr << "Invalid binary type combination in binary expression: " << TypeInfo::TypeToString(leftType) << " - " << TypeInfo::TypeToString(rightType);
				exit(EXIT_FAILURE);
				
			}
			std::shared_ptr<Type> operator()(NodeExprUnary& unary) const
			{
				auto child_type = analyzer->AnalyzeExpression(*unary.expr);

				if (unary.op == TokenType::ampersand) {
					auto result = TypeInfo::MakePointerType(child_type);
					return result;
				}
				else if (unary.op == TokenType::star)
				{
					if (!TypeInfo::IsPointer(child_type) && !TypeInfo::IsArray(child_type)) {
						std::cerr << "Semantic Error: Cannot dereference non-pointer/non-array type!\n";
						exit(EXIT_FAILURE);
					}

					if (!child_type->element) {
						std::cerr << "Internal Error: Pointer or Array missing element type!\n";
						exit(EXIT_FAILURE);
					}

					// Dereferencing yields the pointed-to/element type
					auto result_type = child_type->element;
					
					// it automatically decays to a pointer to its first element (int*)
					if (TypeInfo::IsArray(result_type)) {
						result_type = TypeInfo::MakePointerType(result_type->element);
					}
					//unary.expr->type = result_type;
					return result_type;
				}
				else if (unary.op == TokenType::post_increment || unary.op == TokenType::post_decrement || unary.op == TokenType::pre_increment || unary.op == TokenType::pre_decrement)
				{
					if (!TypeInfo::IsArithmetic(child_type) && !TypeInfo::IsPointer(child_type))
					{
						std::cerr << "Error: Post-increment/decrement requires a numeric or pointer operand!\n";
						exit(EXIT_FAILURE);
					}
					if (!analyzer->IsLValue(*unary.expr))
					{
						std::cerr << "Error: Expression is not assignable (must be an lvalue)!\n";
						exit(EXIT_FAILURE);
					}
				}
				return child_type;
			}
			std::shared_ptr<Type> operator()(const NodeExprInitList& list) const
			{
				return std::make_shared<Type>();
			}
			std::shared_ptr<Type> operator()(NodeExprArrayAccess& access) const
			{
				//  Resolve base variable type
				auto currentType = analyzer->AnalyzeExpression(*access.array);

				// Validate indices and peel type layers
				for (const auto& indexExpr : access.indices) {
					// Validate index expression
					auto indexType = analyzer->AnalyzeExpression(*indexExpr);
					if (indexType->kind != Type::Kind::Int) {
						std::cerr << "Type Error: Array indices must evaluate to integer type.\n";
						exit(EXIT_FAILURE);
					}

					// Handle Array Decay if currentType is an array lvalue
					if (TypeInfo::IsArray(currentType)) {
						currentType = currentType->element;
					}
					// Handle Pointer Subscripting (e.g., ptr[i])
					else if (TypeInfo::IsPointer(currentType)) {
						currentType = currentType->element;
					}
					else {
						std::cerr << "Type Error: Subscripted object is not an array or pointer.\n";
						exit(EXIT_FAILURE);
					}
				}

				return currentType;
			}
			
		};

		Visitor visitor{ this, expr };
		std::shared_ptr<Type> calculated_type = std::visit(visitor, expr.var);
		expr.type = calculated_type;

		return calculated_type;
	}
	void AnalyzeStatement( NodeStatement& statement)
	{
		struct Visitor
		{
			SemanticAnalyzer* analyzer;
			void operator()(const NodeStatementExit& stat)
			{
				auto exitType = analyzer->AnalyzeExpression(*stat.expr);
				if (*exitType != Type{ Type::Kind::Int }) {
					std::cerr << "Type Error: Exit code must be an integer.\n";
					exit(EXIT_FAILURE);
				}
			}
			void operator()(NodeStatementVar& stat)
			{
				auto exprType = analyzer->AnalyzeExpression(*stat.initializer);
				auto& declaredType = stat.element_type;
				if (TypeInfo::IsArray(exprType)) {
					exprType = TypeInfo::MakePointerType(exprType->element);
				}
				if (!TypeInfo::SameType(exprType, declaredType)) {

					std::cerr << "Type Error: Cannot assign expression of type "
						<< TypeInfo::TypeToString(exprType)
						<< " to variable '" << stat.name << "'\n";
					exit(EXIT_FAILURE);
				}
				stat.id = ValueID::Create();

				auto symbol = std::make_shared<Symbol>( stat.name, stat.element_type, stat.id, 0, false );
				stat.symbol = symbol;
				if (!analyzer->symbols.DeclareVariable(symbol)) {
					std::cerr << "Semantic Error: Variable '" << stat.name
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
				if (!analyzer->EvaluatesToBool(*condType)) {
					std::cerr << "Type Error: IF condition must be boolean.\n";
					exit(EXIT_FAILURE);
				}

				analyzer->AnalyzeStatement(*_if.thenBody);

				if (_if.elseBody) {
					analyzer->AnalyzeStatement(*_if.elseBody);
				}
			}
			void operator()(const NodeStatementWhile& _while)
			{
				auto exprType = analyzer->AnalyzeExpression(*_while.condition);
				if (!analyzer->EvaluatesToBool(*exprType)) {
					std::cerr << "Type Error: IF condition must be boolean.\n";
					exit(EXIT_FAILURE);
				}
				analyzer->AnalyzeStatement(*_while.body);
				if (_while.update) {
					analyzer->AnalyzeExpression(*_while.update);
				}
			}

			void operator()(const NodeStatementBreak& _break)
			{

			}
			void operator()(const NodeStatementContinue& _continue)
			{

			}
			void operator()(NodeStatementExpr& stat)
			{
				analyzer->AnalyzeExpression(*(stat.expr));
			}
			void operator()(const NodeStatementReturn& _ret)
			{
				if (!analyzer->currentFunctionReturnType) {
					std::cerr << "Semantic Error: Return statement outside of function.\n";
					exit(EXIT_FAILURE);
				}

				analyzer->currentFunctionHasReturn = true;
				auto expectedType = analyzer->currentFunctionReturnType;
				auto actualType = analyzer->AnalyzeExpression(*_ret.expr);
				if (_ret.expr) {
					if (*expectedType == Type{} && *actualType != Type{}) {
						std::cerr << "Type Error: Cannot return a value from a void function.\n";
						exit(EXIT_FAILURE);
					}

					if (!TypeInfo::SameType(actualType, expectedType)) {
						std::cerr << "Type Error: Function return type mismatch in '"
							<< analyzer->currentFunctionName.value_or("function") << "' : \n"
							<< "Expected: " << TypeInfo::TypeToString(expectedType) << " "
							<< "Got: " << TypeInfo::TypeToString(actualType);
						exit(EXIT_FAILURE);
					}
				}
				else {
					if (*expectedType != Type{}) {
						std::cerr << "Type Error: Non-void function must return a value.\n";
						exit(EXIT_FAILURE);
					}
				}
			}

			void operator()(const NodeStatementAssignment& assign)
			{
				
				auto varType = analyzer->AnalyzeExpression(*assign.target);
				
				auto exprType = analyzer->AnalyzeExpression(*assign.expr);
				if (!TypeInfo::SameType(exprType, varType) || (TypeInfo::GetPointerDepth(varType) != (TypeInfo::GetPointerDepth(exprType)))) {
					std::cerr << "Type Error: Cannot convert expression of type "
						<< TypeInfo::TypeToString(exprType)
						<< " to " << TypeInfo::TypeToString(varType) << "\n";
					exit(EXIT_FAILURE);
				}
				

				return;


			}
			void operator()(NodeStatementArray& arr)
			{
				std::vector<size_t> dimSizes = TypeInfo::GetArrayDimensions(arr.element_type);

				for (size_t i = 1; i < dimSizes.size(); ++i)
				{
					if (dimSizes[i] == 0)
					{
						std::cerr << "Error: Declaration of array '" << arr.name
							<< "' must have bounds for all dimensions except the first\n";
						exit(EXIT_FAILURE);
					}
				}

				if (arr.initializer)
				{

					// Deduce dimension[0] if omitted 
					if (!dimSizes.empty() && dimSizes[0] == 0)
					{
						size_t inferredFirstDim = 0;

						if (auto* initList = std::get_if<NodeExprInitList>(&arr.initializer->var))
						{
							inferredFirstDim = initList->values.size();
						}

						if (inferredFirstDim == 0)
						{
							std::cerr << "Error: Array initializer for '" << arr.name << "' cannot be empty\n";
							exit(EXIT_FAILURE);
						}

						dimSizes[0] = inferredFirstDim;
						arr.element_type = TypeInfo::SetArrayDimensionSize(arr.element_type, 0, inferredFirstDim);
					}
				}
				else if (!dimSizes.empty() && dimSizes[0] == 0)
				{
					std::cerr << "Error: Definition of array '" << arr.name << "' with missing explicit bound requires an initializer\n";
					exit(EXIT_FAILURE);
				}


				arr.id = ValueID::Create();

				auto symbol = std::make_shared<Symbol>(arr.name, arr.element_type, arr.id);

				arr.symbol = symbol;
				analyzer->ValidateArrayInitializer(arr);
				analyzer->symbols.DeclareVariable(symbol);

			}
		};
		Visitor visitor{ this };
		std::visit(visitor, statement.stat);
	}


	bool EvaluatesToBool(const Type& type)
	{
		switch (type.kind)
		{
		case Type::Kind::Pointer:
			return true;
		case Type::Kind::Int:
			return true;
		case Type::Kind::Bool:
			return true;
		default:
			return false;
		}
	}

	void FlattenInitList(NodeExpr& expr, std::vector<const NodeExpr*>& out_scalars, std::shared_ptr<Type> arrType) {
		if (auto* initList = std::get_if<NodeExprInitList>(&expr.var)) {

			for (auto* child : initList->values) {
				if (child != nullptr) {
					FlattenInitList(*child, out_scalars, arrType);
				}
			}
		}
		else {
			// It's a scalar leaf expression (NodeExprIntLit, NodeExprIdent)
			auto exprType = AnalyzeExpression(expr);
			if (!TypeInfo::SameType(exprType, TypeInfo::GetBaseType(arrType))) {
				std::cerr << "Type Error: Cannot assign expression of type "
					<< TypeInfo::TypeToString(exprType)
					<< " to: " << TypeInfo::TypeToString(arrType)<< "\n";
				exit(EXIT_FAILURE);
			}
			out_scalars.push_back(&expr);
		}
	}
	void ValidateArrayInitializer( const NodeStatementArray& arr ) {
		if (!arr.initializer) return;

		// Calculate overall array capacity
		std::vector<int> resolved_dims;
		size_t total_capacity = 1;
		auto dimensions = TypeInfo::GetArrayDimensions(arr.element_type);
		for (auto dim : dimensions) {
			resolved_dims.push_back(dim);
			total_capacity *= dim;
		}

		std::vector<const NodeExpr*> flat_scalars;
		auto list = arr.initializer;
		FlattenInitList(*list, flat_scalars, arr.element_type);


		
		if (flat_scalars.size() > total_capacity) {
			std::cerr << "Excess elements in array initializer: capacity is "  << total_capacity << ", but got " << flat_scalars.size() << "\n";

			exit(EXIT_FAILURE);
			return;
		}

		ValidateInitList(*list, resolved_dims, 0);
	}

	void ValidateInitList(
		NodeExpr& expr,
		const std::vector<int>& resolved_dims,
		size_t depth
	) {

		if (auto* initList = std::get_if<NodeExprInitList>(&expr.var)) {

			if (initList->values.empty()) {
				return;
			}
			if (depth >= resolved_dims.size()) {
				std::cerr << "Too many levels of braces in initializer list\n";
				exit(EXIT_FAILURE);
			}

			size_t max_allowed_elements = resolved_dims[depth];

			if (initList->values.size() > max_allowed_elements) {
				std::cerr << "Too many initializers for dimension " + std::to_string(depth) +
					": expected at most " + std::to_string(max_allowed_elements) +
					", got " + std::to_string(initList->values.size()) << "\n";
				exit(EXIT_FAILURE);
			}
			else if (initList->values.size() < max_allowed_elements)
			{
				for (size_t i = 0; i <( max_allowed_elements - initList->values.size()); i++) {
					auto val = _allocator.new_object<NodeExpr>(NodeExprIntLit{ 0 });
					initList->values.push_back(val);
				}
			}
			for (auto* child : initList->values) {
				if (child != nullptr) {
					if (std::holds_alternative<NodeExprInitList>(child->var)) {
						ValidateInitList(*child, resolved_dims, depth + 1);
					}
					
				}

			}
		}
	}
	void AnalyzeFunction(NodeFunction& func)
	{
		
		currentFunctionName = func.name; 
		currentFunctionReturnType = func.retType;
		currentFunctionHasReturn = false;

		symbols.stackOffset = 0;
		symbols.EnterScope();
		for (auto& var : func.parameters)
		{
			var.id = ValueID::Create();
			if (TypeInfo::IsArray(var.type))
			{
				var.type = Type::MakePointer(var.type->element);
			}
			symbols.DeclareParameter(var.name, var.type, var.id);
			
		}

		AnalyzeBlock(func.body.value(), false);


		if (func.retType->kind != Type::Kind::Void && !currentFunctionHasReturn) {
			std::cerr << "Semantic Error: Function '" << func.name
				<< "' has non-void return type but is missing a return statement.\n";
			exit(EXIT_FAILURE);
		}
		size_t total_bytes = symbols.stackOffset;
		if (total_bytes % 16 != 0) {
			total_bytes += (16 - (total_bytes % 16));
		}
		func.stack_size = total_bytes;
		symbols.SetFunctionHasReturn(func.name, currentFunctionHasReturn);
		
		symbols.ExitScope();

		currentFunctionName.reset();
		currentFunctionReturnType = nullptr;

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
		AnalyzeBlock(prog.global_space, false);
		for (const auto& func : prog.functions)
		{
			std::vector<std::shared_ptr<Type>> paramTypes;
			for (const auto& param : func.parameters) {
				paramTypes.push_back(param.type);
			}

			auto existing = symbols.LookupFunction(func.name);


			if (existing.has_value()) {
				// Check for prototype mismatch
				if(!TypeInfo::SameType(existing->returnType,func.retType) || existing->paramTypes.size() != paramTypes.size())
				{
					std::cerr << "Error: Redeclaration of " << func.name << " with different signature!\n";
					exit(EXIT_FAILURE);
				}
				for (size_t i = 0; i < paramTypes.size(); i++)
				{
					auto funcParamType = paramTypes[i];
					auto existingFuncParamType = existing->paramTypes[i];
					if (!TypeInfo::SameType(existingFuncParamType, funcParamType)) {
						std::cerr << "Error: Redeclaration of " << func.name << " with different signature!\n";
						exit(EXIT_FAILURE);
					}
				}
				
				if (func.body.has_value() && existing->hasBody) {
					std::cerr << "Error: Redefinition of function " << func.name << "\n";
					exit(EXIT_FAILURE);
				}
			}

			bool hasBody = func.body.has_value();
			symbols.DeclareFunction(func.name, func.retType, paramTypes, hasBody);
		}

		for (auto& func : prog.functions)
		{
			if (func.body.has_value()) {
				AnalyzeFunction(func);
			}
		}



	}

	bool IsLValue(const NodeExpr& expr)
	{
		return std::visit(
			[this](const auto& node) -> bool {
				using T = std::decay_t<decltype(node)>;

				if constexpr (std::is_same_v<T, NodeExprIdent>) {
					return true;
				}
				else if constexpr (std::is_same_v<T, NodeExprArrayAccess>) {
					return true;
				}

				// Only Pointer Dereference (*p) produces an LValue
				else if constexpr (std::is_same_v<T, NodeExprUnary>) {
					if (node.op == TokenType::star) { // *ptr
						return true;
					}
					return false; 
				}
				// Everything else (Literals, Binary Ops, Function Calls, Casts) is an RValue
				else {
					return false;
				}
			}, expr.var);
	}
	SymbolTable symbols;
	std::optional<std::string> currentFunctionName;
	std::shared_ptr<Type> currentFunctionReturnType;
	NodeProg& ast;
	std::pmr::polymorphic_allocator<char>& _allocator;
	bool currentFunctionHasReturn = false;
};