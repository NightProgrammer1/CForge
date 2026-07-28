#pragma once
#include "parser.h"
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <sstream>



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
				generator->push("rax");

			}
			void operator()(const NodeExprIdent& stat)
			{
				bool found = false;
				for (size_t i = generator->scopes.size(); i-- > 0; )
				{
					if (generator->scopes[i].variables.contains(stat.name))
					{
						const auto& var = generator->scopes[i].variables.at(stat.name);
						size_t offset = var.stack_location * 8;

						generator->_output << "    mov rax, QWORD [rbp - "
							<< std::to_string(offset) << "]\n";

						generator->push("rax");
						found = true;
						break;
					}
				}

				if (!found)
				{
					std::cerr << "This variable (" << stat.name << ") is undeclared." << std::endl;
					exit(EXIT_FAILURE);
				}
			}

			void operator()(const NodeExprBinary& expr)
			{
				// Evaluate left and right subexpressions
				generator->GenerateExpression(*expr.left);
				generator->GenerateExpression(*expr.right);
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
				else if (expr.op.type == TokenType::mult)
				{
					generator->_output << "    imul rax, rdi\n";
				}
				else if (expr.op.type == TokenType::div)
				{
					generator->_output << "    xor rdx, rdx\n"; // Clear upper bits for unsigned division
					generator->_output << "    div rdi\n";
				}
				// push result
				generator->push("rax");
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
				generator->GenerateExpression(*expr.right);
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
				generator->push("rax");
			}
			void operator()(const NodeExprUnary& expr)
			{
				// Ensure the operand is an identifier (variable)
				if (auto identExpr = std::get_if<NodeExprIdent>(&expr.expr->var))
				{
					const std::string& varName = identExpr->name;

					// 1. Locate the variable on the stack
					size_t varOffset = generator->GetVariableOffset(varName);
					if (expr.op.type == TokenType::increment)
					{
						// 2. Load the OLD value into rax 
						generator->_output << "    mov rax, QWORD [rsp + " << varOffset << "]\n";

						// 3. Increment the actual variable stored in memory
						generator->_output << "    add QWORD [rsp + " << varOffset << "], 1\n";

						// 4. Push the OLD value onto the stack for surrounding operations
						generator->push("rax");
					}
				}
				if (expr.op.type == TokenType::_not)
				{
					generator->GenerateExpression(*expr.expr);
					generator->pop("rax");

					generator->_output << "    test rax, rax\n";

					generator->_output << "    sete al\n";

					generator->_output << "    movzx rax, al\n";

					generator->push("rax");


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
						switch (parms.type)
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
					generator->GenerateExpression(*(call.args[i]));
				}

				for (size_t i = call.args.size(); i-- > 0; )
				{
					if (i < generator->PARAM_REGS.size())
					{
						generator->pop(generator->PARAM_REGS[i]);
					}
				}
				generator->_output << "    call " << call.name << "\n";
				generator->push("rax");
			}
			void operator()(const NodeExprVoid&){}
			Generator* generator;
		};
		Visitor visit(this);
		std::visit(visit, expression.var);
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
				
				size_t var_slot = ++generator->_varCount;
				generator->scopes.back().variables.insert({ stat.name, Var{.stack_location = var_slot} });

				generator->GenerateExpression(*(stat.expr));


				size_t offset = var_slot * 8;
				generator->_output << "    mov QWORD [rbp - " << std::to_string(offset) << "], rax\n";

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
				generator->pop("rax");
				generator->_output << "    test rax, rax\n";
				generator->_output << "    jz " << endLabel << "\n";
				generator->GenerateBlock(_while.block);
				generator->_output << "    jmp " << startLabel << "\n";
				generator->_output << "    " << endLabel << ":\n";
			}

			void operator()(const NodeStatementExpr& stat)
			{
				generator->GenerateExpression(*stat.expr);
			}
			void operator()(const NodeStatementReturn& _ret)
			{
				generator->GenerateExpression(*_ret.expr);
				generator->pop("rax"); 

			
			}
			void operator()(const NodeStatementAssignment& assign)
			{
				bool found = false;
				for (size_t i = generator->scopes.size(); i-- > 0; )
				{
					if (generator->scopes[i].variables.contains(assign.ident.value))
					{
						const auto& var = generator->scopes[i].variables.at(assign.ident.value);
						size_t offset = var.stack_location * 8;

						generator->GenerateExpression(*assign.expr);
						generator->_output << "    mov QWORD [rbp - "
							<< std::to_string(offset) << "], rax\n";

						generator->push("rax");
						found = true;
						break;
					}
				}

				if (!found)
				{
					std::cerr << "This variable (" << assign.ident.value << ") is undeclared." << std::endl;
					exit(EXIT_FAILURE);
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

		// 1. Verify main exists
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

		// System V AMD64 ABI parameter registers
		const std::vector<std::string> PARAM_REGS = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

		for (auto& func : prog.functions)
		{
			if (!func.body.has_value()) {
				continue; // Skip forward declarations
			}

			_varCount = 0;
			_stackSize = 0;
			scopes.clear();
			scopes.emplace_back(); 

			_output << func.name<< ":\n";

			// Prologue
			push("rbp");
			_output << "    mov rbp, rsp\n";

			size_t bytes_needed = func.stack_size;

			
			if (bytes_needed > 0) {
				_output << "    sub rsp, " << std::to_string(bytes_needed) << "\n";
			}

			for (size_t i = 0; i < func.parameters.size(); ++i)
			{
				const auto& param = func.parameters[i];
				size_t slot = ++_varCount; // Reserve slot 1, 2, ...
				size_t offset = slot * 8;

				scopes.back().variables.insert({ param.name, Var{.stack_location = slot} });

				_output << "    mov QWORD [rbp - " << std::to_string(offset)
					<< "], " << PARAM_REGS[i] << "\n";
			}

			GenerateBlock(func.body.value(), false);

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

	size_t GetVariableOffset(const std::string& varName)
	{
		for (size_t i = scopes.size(); i-- > 0; )
		{
			if (scopes[i].variables.contains(varName))
			{
				const auto& var = scopes[i].variables.at(varName);
				return (scopes[i].stackSizeAtStart + var.stack_location) * 8;
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
		_stackSize++;
	}

	void pop(const std::string& reg)
	{
		_output << "    pop " << reg << "\n";
	}


	std::string CreateLabel(const std::string& label)
	{
		return label + std::to_string(_labelCount++);
	}
	struct Var
	{
		size_t stack_location;
	};
	struct Scope
	{
		std::unordered_map<std::string, Var> variables;
		size_t stackSizeAtStart;
	};

	NodeProg root;
	std::stringstream _output;

	std::vector<Scope> scopes;

	size_t _labelCount = 0;

	size_t _stackSize = 0;

	int _currentScope = 0;

	size_t _varCount = 0;


	const std::vector<std::string> PARAM_REGS = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
};