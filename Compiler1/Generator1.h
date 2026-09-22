#pragma once
#include "parser.h"
#include <unordered_map>
#include <sstream>
#include <algorithm>



class Generator
{

public:
	Generator(NodeProg&& prog) : root(std::move(prog))
	{ 
		// Ensure there is always at least one scope (e.g., global scope)
		scopes.emplace_back(); // std::unordered_map<std::string, Var>()
	}

	void GenerateExpression(const NodeExpr& expression)
	{
		struct Visitor
		{

			Visitor(Generator* gen) : generator(gen) {}
			void operator()(const NodeExprIntLit& expr)
			{

				generator->_output << "    mov rax, " << expr.value << "\n";

			}
			void operator()(const NodeExprIdent& stat)
			{
				bool found = false;
				for (size_t i = generator->scopes.size(); i-- > 0; )
				{
					if (generator->scopes[i].variables.contains(stat.name))
					{
						const auto& var = generator->scopes[i].variables.at(stat.name);
						size_t offset = var.frame_offset;

						if (generator->_currentContext == ExprContext::LValue)
						{
							generator->_output << "    lea rbx, [rbp - " << offset << "]\n";
						}
						else
						{
							generator->_output << "    mov rax, QWORD [rbp - " << offset << "]\n";
						}

						found = true;
						break;
					}
				}

				if (!found)
				{
					std::cerr << "This variable (" << stat.name << ") is undeclared." << std::endl;
					exit(EXIT_FAILURE);
				}
				generator->push("rax");
			}

			void operator()(const NodeExprBinary& expr)
			{

				Type leftType = generator->GetExpressionType(*expr.left);
				Type rightType = generator->GetExpressionType(*expr.right);
				if (leftType.pointer_level > 0 && rightType.pointer_level == 0) {
					Type targetType = leftType;
					targetType.pointer_level--;
					size_t elem_size = GetTypeSize(targetType);

					generator->GenerateExpression(*expr.right);

					if (elem_size == 8)      generator->_output << "    shl rax, 3\n";
					else if (elem_size == 4) generator->_output << "    shl rax, 2\n";
					else if (elem_size == 2) generator->_output << "    shl rax, 1\n";
					else if (elem_size > 1)  generator->_output << "    imul rax, " << elem_size << "\n";

					generator->_output << "    push rax\n";

					generator->GenerateExpression(*expr.left);
					generator->pop("rax");


					generator->_output << "    pop rbx\n";

					if (expr.op.type == TokenType::sub) {
						generator->_output << "    sub rax, rbx\n";
					}
					else {
						generator->_output << "    add rax, rbx\n";
					}
					generator->push("rax");
				}
				else if (leftType.pointer_level == 0 && rightType.pointer_level > 0) {
					Type targetType = rightType;
					targetType.pointer_level--;
					size_t elem_size = GetTypeSize(targetType);

					generator->GenerateExpression(*expr.left);

					if (elem_size == 8)      generator->_output << "    shl rax, 3\n";
					else if (elem_size == 4) generator->_output << "    shl rax, 2\n";
					else if (elem_size == 2) generator->_output << "    shl rax, 1\n";
					else if (elem_size > 1)  generator->_output << "    imul rax, " << elem_size << "\n";

					generator->push("rax");

					generator->GenerateExpression(*expr.right);

					generator->_output << "    pop rbx\n";
					if (expr.op.type == TokenType::sub) {
						generator->_output << "    sub rax, rbx\n";
					}
					else {
						generator->_output << "    add rax, rbx\n";
					}

					generator->push("rax");

				}
				else {
					// Evaluate left and right subexpressions
					generator->GenerateExpression(*expr.left);
					generator->push("rax");
					generator->GenerateExpression(*expr.right);
					generator->push("rax");
					// Pop right into rdi, left into rax
					generator->pop("rdi");
					generator->pop("rax");
					if (expr.op.type == TokenType::plus)
					{
						generator->_output << "    add rax, rdi\n";
					}
					else if (expr.op.type == TokenType::sub)
					{
						generator->_output << "    sub rax, rdi\n";
					}
					else if (expr.op.type == TokenType::star)
					{
						generator->_output << "    imul rax, rdi\n";
					}
					else if (expr.op.type == TokenType::div)
					{
						generator->_output << "    xor rdx, rdx\n"; // Clear upper bits for unsigned division
						generator->_output << "    div rdi\n";
					}
				}
			}
			void operator()(const NodeExprBoolLit& expr)
			{
				generator->_output
					<< "    mov rax, "
					<< (expr.value ? 1 : 0)
					<< "\n";

				generator->push("rax");
			}
			void operator()(const NodeExprCompare& expr)
			{
				if (expr.op.type == TokenType::_or)
				{
					std::string trueLabel = generator->CreateLabel("or_true");
					std::string endLabel = generator->CreateLabel("or_end");

					// Evaluate left
					generator->GenerateExpression(*expr.left);
					generator->pop("rax");

					generator->_output << "    test rax, rax\n";
					generator->_output << "    jnz " << trueLabel << "\n";


					// Evaluate right
					generator->GenerateExpression(*expr.right);
					generator->pop("rax");

					generator->_output << "    test rax, rax\n";
					generator->_output << "    jnz " << trueLabel << "\n";


					// false case
					generator->_output << "    mov rax, 0\n";
					generator->_output << "    jmp " << endLabel << "\n";


					// true case
					generator->_output << "    " << trueLabel << ":\n";
					generator->_output << "    mov rax, 1\n";


					// done
					generator->_output << "    " << endLabel << ":\n";

					generator->push("rax");

					return;
				}

				if (expr.op.type == TokenType::_and)
				{
					std::string falseLabel = generator->CreateLabel("and_false");
					std::string endLabel = generator->CreateLabel("and_end");

					// 1. Evaluate left operand
					generator->GenerateExpression(*expr.left);
					generator->pop("rax");

					// if left is false (0), jump directly to false case
					generator->_output << "    test rax, rax\n";
					generator->_output << "    jz " << falseLabel << "\n";

					// 2. Evaluate right operand 
					generator->GenerateExpression(*expr.right);
					generator->pop("rax");

					// if right is false (0), jump to false case
					generator->_output << "    test rax, rax\n";
					generator->_output << "    jz " << falseLabel << "\n";

					// TRUE case 
					generator->_output << "    mov rax, 1\n";
					generator->_output << "    jmp " << endLabel << "\n";

					// FALSE case
					generator->_output << "    " << falseLabel << ":\n";
					generator->_output << "    mov rax, 0\n";

					// Done
					generator->_output << "    " << endLabel << ":\n";

					generator->push("rax");

					return;
				}
				generator->GenerateExpression(*expr.left);
				generator->push("rax");
				generator->GenerateExpression(*expr.right);
				generator->push("rax");
				// Pop right into rdi, left into rax
				generator->pop("rdi");
				generator->pop("rax");


				generator->_output << "    cmp rax, rdi\n";

				if (expr.op.type == TokenType::greater)
				{
					generator->_output << "    setg al\n";
				}
				else if (expr.op.type == TokenType::greater_equal) // >=
				{
					generator->_output << "    setge al\n";
				}
				else if (expr.op.type == TokenType::less)
				{
					generator->_output << "    setl al\n";
				}
				else if (expr.op.type == TokenType::less_equal) // <=
				{
					generator->_output << "    setle al\n";
				}
				else if (expr.op.type == TokenType::equal_equal) // ==
				{
					generator->_output << "    sete al\n";
				}
				else if (expr.op.type == TokenType::not_equal) // !=
				{
					generator->_output << "    setne al\n";
				}

				generator->_output << "    movzx rax, al\n";
			}
			void operator()(const NodeExprUnary& expr)
			{
				ExprContext parentContext = generator->_currentContext;
				/*
				if (auto identExpr = std::get_if<NodeExprIdent>(&expr.expr->var))
				{
					const std::string& varName = identExpr->name;

					size_t varOffset = generator->GetVariableOffset(varName);
					if (expr.op.type == TokenType::post_increment)
					{

						generator->_output << "    add QWORD [rbp - " << varOffset << "], 1\n";

					}
					else if (expr.op.type == TokenType::_not)
					{
						generator->GenerateExpression(*expr.expr);
						generator->pop("rax");

						generator->_output << "    test rax, rax\n";

						generator->_output << "    sete al\n";

						generator->_output << "    movzx rax, al\n";

						generator->push("rax");


					}
					else if(expr.op.type == TokenType::negate)
					{
						generator->GenerateExpression(*expr.expr);
						generator->pop("rax");
						generator->_output << "    neg rax\n";
						generator->push("rax");
					}
					else if (expr.op.type == TokenType::ampersand)
					{
						generator->_currentContext = ExprContext::LValue;
						generator->GenerateExpression(*expr.expr);
						generator->_currentContext = ExprContext::RValue;
						generator->_output << "    mov rax, rbx\n";
					}
					else if (expr.op.type == TokenType::star)
					{
						generator->_currentContext = ExprContext::RValue;

						generator->GenerateExpression(*expr.expr);
						generator->_output << "    mov rax, QWORD [rax]\n";
					}
				}
				*/
				if (expr.op.type == TokenType::star)
				{
					generator->_currentContext = ExprContext::RValue;
					generator->GenerateExpression(*expr.expr);
					generator->_currentContext = parentContext;

					if (parentContext == ExprContext::RValue) {
						generator->pop("rax");
						generator->_output << "    mov rax, QWORD [rax]\n";
					}
				}
				else if (expr.op.type == TokenType::post_increment)
				{
					// Post-increment (x++):
					// 1. Get memory address of target in RBX
					generator->_currentContext = ExprContext::LValue;
					generator->GenerateExpression(*expr.expr);
					generator->_currentContext = parentContext;

					// 2. Load ORIGINAL value into RAX (for expression return)
					generator->_output << "    mov rax, QWORD [rbx]\n";

					// 3. Increment value in memory (or scale by element size if pointer)
					generator->_output << "    add QWORD [rbx], 1\n";
				}
			}
			void operator()(const NodeExprCall& call) 
			{
				auto function = std::find_if(
					generator->root.functions.begin(),
					generator->root.functions.end(),
					[&](const NodeFunction& func) {
						return func.name == call.name;
					}
				);

				if (function == generator->root.functions.end()) {
					std::cerr << "Undeclared function: " << call.name << std::endl;
					exit(EXIT_FAILURE);
				}

				if (call.args.size() != function->parameters.size())
				{
					std::stringstream paramsDef;
					for (auto& parms : function->parameters)
					{
						switch (parms.type_info)
						{
						case Type::Int:
							paramsDef << "int";
							break;
						default:
							break;
						}
						paramsDef <<  " " << parms.name << ", ";
					}

					std::string def = paramsDef.str();
					if (def.length() >= 2) {
						def.erase(def.length() - 2); // Strips trailing ", "
					}

					std::cerr << "The number of parameters does not match function definition: "
						<< call.name << "(" << def << ")" << std::endl;
					exit(EXIT_FAILURE);
				}
				for (size_t i = 0; i < call.args.size(); ++i)
				{
					auto varInfo = generator->GetVariableInfo(function->parameters[i].name);
					if (varInfo.type_info.is_array())
					{
						generator->SetContext(ExprContext::LValue);
						generator->GenerateExpression(*call.args[i]);

						generator->push("rbx");
					}
					else
					{
						generator->SetContext(ExprContext::RValue);
						generator->GenerateExpression(*call.args[i]);
					}
				}

				for (size_t i = call.args.size(); i-- > 0; )
				{
					if (i < generator->PARAM_REGS.size())
					{
						generator->pop(generator->PARAM_REGS[i]);
					}
				}
				generator->_output << "    call " << call.name << "\n";
			}
			void operator()(const NodeExprVoid&){}
			void operator()(const NodeExprAssignment&) {}
			void operator()(const NodeExprInitList& initializer) {
				
			}
			void operator()(const NodeExprArrayAccess& access) {
				size_t baseOffset = generator->GetVariableOffset(access.array_name);
				const auto& varInfo = generator->GetVariableInfo(access.array_name);

				generator->_output << "    xor rcx, rcx\n"; // rcx = 0

				

				size_t elemSize = CalculateSizeInBytes(varInfo.type_info);

				


				for (size_t k = 0; k < access.indices.size(); ++k) {
					if (k > 0) {
						if (auto value = std::get_if<NodeExprIntLit>(&varInfo.type_info.dimensions[k]->var)) {
							size_t stride = value->value;
							generator->_output << "    imul rcx, " << stride << "\n";
						}
					}

					generator->GenerateExpression(*access.indices[k]);

					generator->_output << "    add rcx, rax\n";
				}

				//Target Address = rbp - BaseOffset - Flat Index * elemSize)
				if (elemSize == 8) {
					generator->_output << "    shl rcx, 3\n";
				}
				else if (elemSize == 4) {
					generator->_output << "    shl rcx, 2\n";
				}
				else if (elemSize == 2) {
					generator->_output << "    shl rcx, 1\n";
				}
				else if (elemSize > 1) {
					generator->_output << "    imul rcx, " << elemSize << "\n";
				}

				size_t total_elements = 1;
				for (auto dim : varInfo.type_info.dimensions) {
					if (auto value = std::get_if<NodeExprIntLit>(&dim->var)) {
						total_elements *= value->value;
					}
				}
				if (varInfo.type_info.is_pointer()) {
					// POINTER / PARAMETER: Load address stored in pointer slot
					generator->_output << "    mov rbx, QWORD [rbp - " << varInfo.frame_offset << "]\n";
					generator->_output << "    add rbx, rcx\n";
				}
				else {
					// LOCAL STACK ARRAY: Calculate address relative to element 0
					size_t total_elements = 1;
					for (auto dim : varInfo.type_info.dimensions) {
						if (auto value = std::get_if<NodeExprIntLit>(&dim->var)) {
							total_elements *= value->value;
						}
					}


					generator->_output << "    lea rbx, [rbp - " << baseOffset << "]\n";
					generator->_output << "    add rbx, rcx\n";
					generator->push("rbx");
				}
				if (generator->_currentContext == ExprContext::RValue) {
					generator->pop("rbx");
					if (elemSize == 8)      generator->_output << "    mov rax, QWORD [rbx]\n";
					else if (elemSize == 4) generator->_output << "    mov eax, DWORD [rbx]\n";
					else if (elemSize == 2) generator->_output << "    movzx rax, WORD [rbx]\n";
					else if (elemSize == 1) generator->_output << "    movzx rax, BYTE [rbx]\n";
				}
			}
			void operator()(const NodeExprRef& access){}
			Generator* generator;
		};
		Visitor visit(this);
		std::visit(visit, expression.var);
	}

	
	Type GetExpressionType(const NodeExpr& expression)
	{
		struct Visitor
		{
			Visitor(Generator* gen) : generator(gen) {}
			Type operator()(const NodeExprIntLit&) { return Type{ Type::Int }; }
			Type operator()(const NodeExprIdent& ident)
			{
				return ident.Type;
			}
			Type operator()(const NodeExprBinary& expr)
			{
				return expr.Type; 
			}
			Type operator()(const NodeExprBoolLit&) { return Type{ Type::Bool }; }
			Type operator()(const NodeExprCompare&) { return Type{ Type::Bool }; }
			Type operator()(const NodeExprVoid&) { return Type{ Type::Void }; }
			Type operator()(const NodeExprUnary& expr)
			{
				return expr.Type;
			}
			Type operator()(const NodeExprCall& call)
			{
				return call.Type;
			}
			Type operator()(const NodeExprAssignment& assign)
			{
				return assign.Type;
			}
			Type operator()(const NodeExprInitList&) { return Type{ Type::Void }; } 
			Type operator()(const NodeExprArrayAccess& access)
			{
				return access.Type;
			}
			Type operator()(const NodeExprRef&) { return Type{ Type::Void }; } 

			Generator* generator;
		};
		Visitor visit(this);
		return std::visit(visit, expression.var);
	}





	void GenerateStatement(const NodeStatement& statement)
	{
		struct Visitor
		{
			Visitor(Generator* gen) : generator(gen){}
			Generator* generator;
			void operator()(const NodeStatementExit& stat)
			{
				generator->GenerateExpression(*(stat.expr));

				generator->_output << "    mov rdi, rax\n";

				generator->_output << "    mov rax, 60\n";
				generator->_output << "    syscall\n";
			}
			void operator()(const NodeStatementVar& stat)
			{
				
				bool exist = generator->scopes.back().variables.contains(stat.name);
				if (exist)
				{
					std::cerr << "This variable is arealdy declared: " << stat.name << std::endl;
					exit(EXIT_FAILURE);
				}
				
				size_t varSize = CalculateSizeInBytes(stat.Type);

				generator->_stackSize += varSize;
				size_t absolute_offset = generator->_stackSize;

				generator->scopes.back().variables.insert({ stat.name, Var{.frame_offset = absolute_offset, .type_info = Type{stat.Type}} });

				generator->_currentContext = ExprContext::RValue;
				generator->GenerateExpression(*(stat.expr));

				generator->_output << "    mov QWORD [rbp - " << std::to_string(absolute_offset) << "], rax\n";

			}
			void operator()(const NodeBlock& block)
			{
				generator->GenerateBlock(block);

			}
			void operator()(const NodeStatementIf& _if)
			{
				generator->GenerateIf(_if);
			}
			void operator()(const NodeStatementWhile& _while)
			{
				std::string startLabel = generator->CreateLabel("while_start");
				std::string endLabel = generator->CreateLabel("while_end");
				generator->_output << "    " << startLabel << ":\n";
				generator->GenerateExpression(*_while.condition);
				generator->_output << "    test rax, rax\n";
				generator->_output << "    jz " << endLabel << "\n";

				generator->PushLoop(startLabel, endLabel);


				generator->GenerateBlock(_while.block);


				generator->PopLoop();

				generator->_output << "    jmp " << startLabel << "\n";
				generator->_output << "    " << endLabel << ":\n";
			}


			void operator()(const NodeStatementBreak& _break)
			{
				if (generator->breakLabels.empty()) {
					std::cerr << "Error: 'break' outside of loop context\n";
					exit(EXIT_FAILURE);
				}
				// Jump directly to the active loop's exit label
				generator->_output << "    jmp " << generator->breakLabels.back() << "\n";
			}
			void operator()(const NodeStatementContinue&) {
				if (generator->continueLabels.empty()) {
					std::cerr << "Error: 'continue' outside of loop context\n";
					exit(EXIT_FAILURE);
				}
				// Jump directly to the active loop's start label
				generator->_output << "    jmp " << generator->continueLabels.back() << "\n";
			}
			void operator()(const NodeStatementExpr& stat)
			{
				generator->GenerateExpression(*stat.expr);
			}
			void operator()(const NodeStatementReturn& _ret)
			{
				generator->SetContext(ExprContext::RValue);
				generator->GenerateExpression(*_ret.expr);

			}
			void operator()(const NodeStatementAssignment& assign)
			{
				generator->SetContext(ExprContext::RValue);
				generator->GenerateExpression(*assign.expr);

				generator->push("rax");

				generator->SetContext(ExprContext::LValue);
				generator->GenerateExpression(*assign.target);

				generator->pop("rbx");
				generator->pop("rax");
				generator->_output << "    mov QWORD [rbx], rax\n";

				generator->SetContext(ExprContext::RValue);
				
			
			}
			void operator()(const NodeStatementArray& arr)
			{
				if (generator->scopes.back().variables.contains(arr.name)) {
					std::cerr << "Array already declared: " << arr.name << std::endl;
					exit(EXIT_FAILURE);
				}

				size_t elem_count = 1;
				std::vector<size_t> dim;
				for (auto expr : arr.Type.dimensions)
				{
					auto int_lit = std::get_if<NodeExprIntLit>(&expr->var);
					elem_count *= int_lit->value;
					dim.push_back(int_lit->value);
				}

				size_t elemSize = CalculateSizeInBytes(arr.Type);
				size_t total_array_bytes = elem_count * elemSize;

				// 1. Reserve memory for ALL elements from your function stack counter
				generator->_stackSize += total_array_bytes;
				size_t base_frame_offset = generator->_stackSize; // Points to arr[0][0]

				generator->scopes.back().variables.insert({
					arr.name,
					Var{.frame_offset = base_frame_offset, .type_info = arr.Type }
					});

				if (arr.initializer)
				{
					size_t running_offset = generator->_stackSize - total_array_bytes + elemSize;
					generator->ArrayInitializer(*arr.initializer, arr.Type, elemSize, base_frame_offset);
				}
			}
		};
		Visitor visit(this);
		std::visit(visit, statement.stat);
	}


	
	std::string GenerateProgram()
	{
		_output << "global _start\n_start:\n";
		_output << "    call main\n";
		_output << "    mov rdi, rax\n";
		_output << "    mov rax, 60\n";
		_output << "    syscall\n";
		GenerateFunction(root);
		

		return _output.str();
	}

	void GenerateFunction(NodeProg& prog)
	{
		auto main_it = std::find_if(
			prog.functions.rbegin(),
			prog.functions.rend(),
			[](const NodeFunction& func) {
				return func.name == "main";
			}
		);

		if (main_it == prog.functions.rend())
		{
			std::cerr << "Missing main entry point\n";
			exit(EXIT_FAILURE);
		}

		// System V AMD64 ABI parameter registers (first 6 args)
		const std::vector<std::string> PARAM_REGS = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

		for (auto& func : prog.functions)
		{
			if (!func.body.has_value()) {
				continue; 
			}

			_varCount = 0;
			_stackSize = 0;
			scopes.clear();
			scopes.emplace_back(); 

			_output << func.name << ":\n";

			push("rbp");
			_output << "    mov rbp, rsp\n";

			size_t bytes_needed = func.stack_size;
			if (bytes_needed % 16 != 0) {
				bytes_needed += 16 - (bytes_needed % 16);
			}

			if (bytes_needed > 0) {
				_output << "    sub rsp, " << std::to_string(bytes_needed) << "\n";
			}

			for (size_t i = 0; i < func.parameters.size(); ++i)
			{
				const auto& param = func.parameters[i];

				if (i < PARAM_REGS.size())
				{
					size_t slot = ++_varCount; // Reserve slots 1, 2, 3...
					size_t offset = slot * 8;

					scopes.back().variables.insert({
						param.name,
						Var{.frame_offset = offset, .type_info = param.type_info }
						});

					_output << "    mov QWORD [rbp - " << std::to_string(offset)
						<< "], " << PARAM_REGS[i] << "\n";
				}
				else
				{
					//size_t caller_offset = 16 + (i - 6) * 8;

					//scopes.back().variables.insert({
					//	param.name,
						//Var{.stack_location = 0, .frame_offset = caller_offset, .is_caller_stack = true }
						//});
				}
			}

			GenerateBlock(func.body.value(), false);

			// Epilogue
			_output << "    mov rsp, rbp\n";
			pop("rbp");
			_output << "    ret\n";
		}
	}
	
	void GenerateBlock(const NodeBlock& block, bool createScope = true)
	{
		if (createScope)
			EnterScope();

		for (auto& stat : block.statements)
		{
			GenerateStatement(*stat);
		}

		if (createScope)
			ExitScope();
	}
private:

	struct Var
	{
		size_t frame_offset;
		Type type_info;
	};
	enum class ExprContext {
		RValue, 
		LValue  
	};
	void ArrayInitializer(const NodeExpr& expr, Type baseType, size_t elemSize, size_t& currentByteOffset)
	{
		if (auto* initList = std::get_if<NodeExprInitList>(&expr.var))
		{
			for (const auto* childExpr : initList->values)
			{
				ArrayInitializer(*childExpr, baseType, elemSize, currentByteOffset);
			}
		}
		else
		{
			GenerateExpression(expr);

			_output << "    mov QWORD [rbp - " << currentByteOffset << "], rax\n";

			currentByteOffset -= elemSize;
		}
	}
	size_t GetVariableOffset(const std::string& varName)
	{
		for (size_t i = scopes.size(); i-- > 0; )
		{
			if (scopes[i].variables.contains(varName))
			{
				return scopes[i].variables.at(varName).frame_offset;
			}
		}
		std::cerr << "Variable (" << varName << ") undeclared." << std::endl;
		exit(EXIT_FAILURE);
		

	}
	Var GetVariableInfo(const std::string& varName)
	{
		for (size_t i = scopes.size(); i-- > 0; )
		{
			if (scopes[i].variables.contains(varName))
			{
				const auto& var = scopes[i].variables.at(varName);
				return var;
			}
		}
		std::cerr << "Variable (" << varName << ") undeclared." << std::endl;
		exit(EXIT_FAILURE);
	}

	void GenerateIf(const NodeStatementIf& node)
	{
		GenerateExpression(*node.condition);
		pop("rax");

		bool hasElse = node.elseBlock.has_value();

		std::string falseLabel = CreateLabel(hasElse ? "else" : "ifend");
		std::string endLabel;

		if (hasElse)
			endLabel = CreateLabel("ifend");

		_output << "    test rax, rax\n";
		_output << "    jz " << falseLabel << "\n";

		GenerateBlock(node.thenBlock);

		if (hasElse)
		{
			_output << "    jmp " << endLabel << "\n";
		}

		_output << "    " << falseLabel << ":\n";

		if (hasElse)
		{
			GenerateBlock(*node.elseBlock);
			_output << "    " << endLabel << ":\n";
		}
	}

	void SetContext(ExprContext context) { _currentContext = context; }
	void EnterScope()
	{
		scopes.push_back({
		{},
		_stackSize
			});
	}

	void ExitScope()
	{
		scopes.pop_back();
	}
	void push(const std::string& reg)
	{
		_output << "    push " << reg << "\n";
	}

	void pop(const std::string& reg)
	{
		_output << "    pop " << reg << "\n";
	}

	void PushLoop(const std::string& start, const std::string& end) {
		continueLabels.push_back(start);
		breakLabels.push_back(end);
	}

	void PopLoop() {
		continueLabels.pop_back();
		breakLabels.pop_back();
	}
	std::string CreateLabel(const std::string& label)
	{
		return label + std::to_string(_labelCount++);
	}
	
	struct Scope
	{
		std::unordered_map<std::string, Var> variables;
		size_t stackSizeAtStart;
	};

	std::stringstream _output;
	NodeProg root;

	std::vector<Scope> scopes;
	std::vector<std::string> breakLabels;
	std::vector<std::string> continueLabels;
	const std::vector<std::string> PARAM_REGS = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

	size_t _labelCount = 0;
	size_t _stackSize = 0;
	size_t _varCount = 0;


	int _currentScope = 0;

	ExprContext _currentContext;


	
};