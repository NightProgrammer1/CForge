#pragma once
#include <memory_resource>
#include "include/fmt/core.h"
#include "ASTTypes.h"
class Parser
{
public:
	Parser(const std::vector<Token>& tokens, std::pmr::polymorphic_allocator<char>& allocator) : _tokens(tokens), _allocator(&allocator)
	{

	}
   
	std::optional<NodeProg> ParseProgram()
	{
		NodeProg prog;
		while (PeekToken().has_value()) {
			if (IsFunctionDeclaration()) {
                prog.items.push_back(NodeTopLevel{ ParseFunctions() });
			}
			else {
				if (auto statement = ParseStat()) {
					// Only variable declarations are allowed in global space
					if (auto stat = std::get_if<NodeStatementVar>(&statement->stat)) {
                        prog.items.push_back(NodeTopLevel{ stat });
					}
					else {
						std::cerr << "Compile-Time Error: Only variable declarations are allowed in global scope.\n";
						std::exit(EXIT_FAILURE);
					}
				}
				else {
					std::cerr << "Compile-Time Error: Invalid statement in global scope.\n";
					std::exit(EXIT_FAILURE);
				}
			}
		}
		return prog;
	}


private:
	NodeExpr* ParsePrimary()
	{
		auto p = PeekToken();

		if (p && p->type == TokenType::int_literal) {
			return _allocator->new_object<NodeExpr>(NodeExprLiteral{ std::stoi(GetToken().value) }, QualifiedType{ Type::MakeInt() });
		}
		if (p && p->type == TokenType::float_literal) {
			return _allocator->new_object<NodeExpr>(NodeExprLiteral{ std::stof(GetToken().value) }, QualifiedType{ Type::MakeFloat() });
		}
		if (p && p->type == TokenType::_char_literal) {
			return _allocator->new_object<NodeExpr>(NodeExprLiteral{ std::stoi(GetToken().value) }, QualifiedType{ Type::MakeChar() });
		}
		if (Match(TokenType::false_literal)) {
			return _allocator->new_object<NodeExpr>(NodeExprLiteral{ .value = false }, QualifiedType{ Type::MakeBool() });
		}
		
		if (Match(TokenType::true_literal)) {
            return _allocator->new_object<NodeExpr>(NodeExprLiteral{ .value = true }, QualifiedType{ Type::MakeBool() });
		}
		if (p && p->type == TokenType::_identifier) {
			std::string identifier = GetToken().value;
			auto identExpr = _allocator->new_object<NodeExpr>();
			identExpr->var = NodeExprIdent{ identifier };
			return identExpr;
		}
		if (Match(TokenType::open_paren)) {
			auto expr = ParseBoolean();
			Consume(TokenType::close_paren);
			return expr;
		}
		if (p && p->type == TokenType::open_brace) {
			return ParseInitializerList();
		}
		if (Match(TokenType::_NULL))
		{
			return _allocator->new_object<NodeExpr>(
				NodeExprLiteral{ 0 },
                QualifiedType::MakePointer(QualifiedType{ Type::MakeVoid() }, Modifier{})
			);
		}
		return nullptr;
	}
    NodeExpr* ParsePostfix()
    {
        // 1. First parse the base primary expression
        auto expr = ParsePrimary();
        if (!expr) return nullptr;

        // 2. Loop continuously to consume any attached postfix operators
        while (true)
        {
            // Function Call: expr(...)
            if (Match(TokenType::open_paren))
            {
                NodeExprCall funcCall;
                // Extract the function name if expr is an identifier
                if (auto* ident = std::get_if<NodeExprIdent>(&expr->var)) {
                    funcCall.name = ident->name;
                }

                if (!Match(TokenType::close_paren))
                {
                    while (true)
                    {
                        if (auto arg = ParseBoolean()) {
                            funcCall.args.push_back(std::move(arg));
                        }
                        else {
                            std::cerr << "Invalid expression in function call\n";
                            exit(EXIT_FAILURE);
                        }

                        if (Match(TokenType::comma)) continue;
                        break;
                    }
                    Consume(TokenType::close_paren);
                }

                auto callExpr = _allocator->new_object<NodeExpr>();
                callExpr->var = std::move(funcCall);
                expr = callExpr; // Update expr to allow chaining! (e.g. func()++)
            }
            // Postfix Increment: expr++
            else if (Match(TokenType::increment))
            {
                auto unary = _allocator->new_object<NodeExpr>();
                unary->var = NodeExprUnary{ TokenType::post_increment, expr };
                expr = unary;
            }
            // Postfix Decrement: expr--
            else if (Match(TokenType::decrease))
            {
                auto unary = _allocator->new_object<NodeExpr>();
                unary->var = NodeExprUnary{ TokenType::post_decrement, expr };
                expr = unary;
            }
            // Array Subscript: expr[i]
            else if (TryMatch(TokenType::open_bracket))
            {
                NodeExprArrayAccess access{ expr };
                while (Match(TokenType::open_bracket)) {
                    while (PeekToken() && PeekToken()->type != TokenType::close_bracket)
                    {
                        if (auto indexExpr = ParseLogicalOr()) {
                            access.indices.push_back(indexExpr);
                        }
                        else {
                            std::cerr << "Invalid array access expression\n";
                            exit(EXIT_FAILURE);
                        }
                    }
                    Consume(TokenType::close_bracket);
                }

                auto accessExpr = _allocator->new_object<NodeExpr>();
                accessExpr->var = std::move(access);
                expr = accessExpr;
            }
            else
            {
                break; // No more postfix operators
            }
        }

        return expr;
    }
    NodeExpr* ParseFactor()
    {
        // Prefix Unary Operators
        if (Match(TokenType::_not)) {
            auto sub = ParseFactor();
            auto unary = _allocator->new_object<NodeExpr>();
            unary->var = NodeExprUnary{ TokenType::_not, sub };
            return unary;
        }
        if (Match(TokenType::increment)) { // Prefix ++x
            auto sub = ParseFactor();
            auto unary = _allocator->new_object<NodeExpr>();
            unary->var = NodeExprUnary{ TokenType::pre_increment, sub };
            return unary;
        }
        if (Match(TokenType::decrease)) { // Prefix --x
            auto sub = ParseFactor();
            auto unary = _allocator->new_object<NodeExpr>();
            unary->var = NodeExprUnary{ TokenType::pre_decrement, sub };
            return unary;
        }
        if (Match(TokenType::ampersand)) { // &x
            auto sub = ParseFactor();
            auto unary = _allocator->new_object<NodeExpr>();
            unary->var = NodeExprUnary{ TokenType::ampersand, sub };
            return unary;
        }
        if (Match(TokenType::star)) { // *x
            auto sub = ParseFactor();
            auto unary = _allocator->new_object<NodeExpr>();
            unary->var = NodeExprUnary{ TokenType::star, sub };
            return unary;
        }

        // Fallthrough to Postfix and Primary expressions
        return ParsePostfix();
    }
    
    NodeExpr* ParseTerm()
    {
        //Handle the case where we have a factor, and then we have a binary operator (* or /) that combines two factors.
        auto left = ParseFactor();

        auto p = PeekToken();
        while (p && (p->type == TokenType::star || p->type == TokenType::div))
        {
            Token op = GetToken();
            auto right = ParseFactor();

            auto binary = _allocator->new_object<NodeExpr>();

            binary->var = NodeExprBinary{
                op.type,
                std::move(left),
                std::move(right)
            };
            left = std::move(binary);

            p = PeekToken();
        }

        return left;
    }
    NodeExpr* ParseExpr()
    {
        auto left = ParseTerm();

        auto p = PeekToken();
        while (p && (p->type == TokenType::plus || p->type == TokenType::sub))
        {
            Token op = GetToken();
            auto right = ParseTerm();

            auto binary = _allocator->new_object<NodeExpr>();

            binary->var = NodeExprBinary{
                op.type,
                std::move(left),
                std::move(right)
            };
            left = std::move(binary);
            p = PeekToken();
        }



        return left;
    }
    NodeExpr* ParseComparison()
    {
        auto left = ParseExpr();
        auto p = PeekToken();
        int a;


        while (PeekToken() &&
            (PeekToken()->type == TokenType::less ||
                PeekToken()->type == TokenType::greater ||
                PeekToken()->type == TokenType::equal_equal ||
                PeekToken()->type == TokenType::less_equal ||
                PeekToken()->type == TokenType::greater_equal ||
                PeekToken()->type == TokenType::not_equal))
        {
            Token op = GetToken();
            auto right = ParseExpr();
            auto binary = _allocator->new_object<NodeExpr>();

            binary->var = NodeExprCompare{
               op.type,
               left,
               right
            };

            left = std::move(binary);
            p = PeekToken();
        }

        return left;
    }
    NodeExpr* ParseBoolean()
    {
        auto left = ParseLogicalAnd();

        while (PeekToken() && PeekToken()->type == TokenType::_or)
        {
            Token op = GetToken();
            auto right = ParseLogicalAnd();

            auto binary = _allocator->new_object<NodeExpr>();
            binary->var = NodeExprCompare{
                op.type,
                std::move(left),
                std::move(right)
            };

            left = std::move(binary);
        }

        return left;
    }

    NodeExpr* ParseLogicalAnd()
    {
        auto left = ParseComparison();


        auto p = PeekToken();


        while (PeekToken() &&
            (PeekToken()->type == TokenType::_and))
        {
            Token op = GetToken();
            auto right = ParseComparison();
            auto binary = _allocator->new_object<NodeExpr>();

            binary->var = NodeExprCompare{
               op.type,
               std::move(left),
               std::move(right)
            };

            left = std::move(binary);
            p = PeekToken();
        }

        return left;
    }
    NodeExpr* ParseLogicalOr()
    {
        auto left = ParseLogicalAnd();


        auto p = PeekToken();


        while (PeekToken() &&
            (PeekToken()->type == TokenType::_or))
        {
            Token op = GetToken();
            auto right = ParseLogicalAnd();
            auto binary = _allocator->new_object<NodeExpr>();

            binary->var = NodeExprCompare{
               op.type,
               left,
               right
            };

            left = std::move(binary);
            p = PeekToken();
        }

        return left;
    }
    NodeExpr* ParseCompoundAssign()
    {
        auto left = ParseLogicalOr();


        auto p = PeekToken();


        while (PeekToken() &&
            (PeekToken()->type == TokenType::plus_equal))
        {
            Token op = GetToken();
            auto right = ParseLogicalOr();
            auto binary = _allocator->new_object<NodeExpr>();

            binary->var = NodeExprBinary{ TokenType::plus, left, right };
            
            auto assign = _allocator->new_object<NodeExpr>();
            assign->var = NodeExprAssignment{ left, binary };

            left = std::move(assign);
            p = PeekToken();
        }

        return left;
    }

    std::shared_ptr<NodeBlock> ParseGlobalBlock()
    {
        static std::shared_ptr<NodeBlock> block = std::make_shared<NodeBlock>();

      
        Consume(TokenType::close_brace);

        return block;
    }
    NodeBlock ParseBlock()
    {
        auto block = NodeBlock{};


        while (PeekToken().has_value() && PeekToken()->type != TokenType::close_brace)
        {
            if (auto statement = ParseStat())
            {
                block.statements.push_back(std::move(statement));
            }
            else
            {
                std::cerr << "Invalid statement in block\n";
                exit(EXIT_FAILURE);
            }
        }

        Consume(TokenType::close_brace);

        return block;
    }


    NodeStatement* ParseStat()
    {
        // Control Flow & Keywords
        if (Match(TokenType::exit))
        {
            NodeStatementExit exit_node;
            if (auto node_expr = ParseLogicalOr()) {
                exit_node.expr = node_expr;
            }
            else {
                std::cerr << "Invalid (exit) expression\n";
                exit(EXIT_FAILURE);
            }
            Consume(TokenType::semicolon);
            return _allocator->new_object<NodeStatement>(exit_node);
        }
        if (Match(TokenType::_if))       return ParseIf();
        if (Match(TokenType::_while))    return ParseWhile();
        if (Match(TokenType::_for))      return ParseFor();
        if (Match(TokenType::open_brace)) return _allocator->new_object<NodeStatement>(ParseBlock());

        if (Match(TokenType::_return))
        {
            NodeStatementReturn ret;
            if (Match(TokenType::semicolon)) {
                ret.expr = _allocator->new_object<NodeExpr>(NodeExprVoid{});
                return _allocator->new_object<NodeStatement>(ret);
            }

            if (auto expr = ParseLogicalOr()) {
                ret.expr = expr;
            }
            else {
                std::cerr << "Invalid return expression\n";
                exit(EXIT_FAILURE);
            }
            Consume(TokenType::semicolon);
            return _allocator->new_object<NodeStatement>(ret);
        }

        if (Match(TokenType::_break)) {
            Consume(TokenType::semicolon);
            return _allocator->new_object<NodeStatement>(NodeStatementBreak{});
        }
        if (Match(TokenType::_continue)) {
            Consume(TokenType::semicolon);
            return _allocator->new_object<NodeStatement>(NodeStatementContinue{});
        }

        //Variable Declarations (Types: int, int*, bool, int[2])
        if (IsTypeSpecifier(PeekToken()))
        {
            if (auto decl = ParseVariableDeclaration()) {
                return *decl;
            }
            std::cerr << "Invalid variable declaration\n";
            exit(EXIT_FAILURE);
        }

        // Assignments & Expression Statements
        if (auto stmt = ParseAssignmentOrExprStatement()) {
            return stmt;
        }

        return nullptr;
    }

	bool IsTypeSpecifier(std::optional<Token> tok) {
		if (!tok.has_value()) return false;
		TokenType t = tok->type;
        return t == TokenType::_int ||
            t == TokenType::_char ||
            t == TokenType::_float ||
            t == TokenType::_double ||
            t == TokenType::_short ||
            t == TokenType::_bool ||
            t == TokenType::_void ||
            t == TokenType::_unsigned ||
            t == TokenType::_signed ||
            t == TokenType::_long ||
            t == TokenType::_const ||
            t == TokenType::_volatile;
	}
    NodeExpr* ParseExpression()
    {
        auto lhs = ParseFactor();
        if (!lhs) return nullptr;

        if (Match(TokenType::equal))
        {
            NodeExprAssignment assign;
            assign.target = lhs;

            if (Match(TokenType::open_brace)) {
                //  assign.expr = ParseInitializerList();
            }
            else {
                assign.expr = ParseLogicalOr();
            }

            return _allocator->new_object<NodeExpr>(assign);
        }

        if (Match(TokenType::plus_equal))
        {
            auto rhs = ParseLogicalOr();

            auto binary = _allocator->new_object<NodeExpr>();
            binary->var = NodeExprBinary{
               TokenType::plus,
                lhs,
                rhs
            };

            NodeExprAssignment assign;
            assign.target = lhs;
            assign.expr = binary;
            return _allocator->new_object<NodeExpr>(assign);
        }


        return lhs;
    }
    NodeStatement* ParseAssignmentOrExprStatement()
    {
       
        auto expr = ParseExpression();
        if (!expr) return nullptr;

        Consume(TokenType::semicolon); 

        if (std::holds_alternative<NodeExprAssignment>(expr->var)) 
        {
            auto& assignExpr = std::get<NodeExprAssignment>(expr->var);

            NodeStatementAssignment stmtAssign;
            stmtAssign.target = assignExpr.target;
            stmtAssign.expr = assignExpr.expr;

            return _allocator->new_object<NodeStatement>(stmtAssign);
        }

        NodeStatementExpr expr_stmt{ expr };
        return _allocator->new_object<NodeStatement>(expr_stmt);
    }
    NodeFunction* ParseFunctions()
    {
        NodeFunction func;
        auto type = ParseType();
        func.retType = type->type;

        func.name = type->declarator.name;

        if (Match(TokenType::open_paren))
        {
			if (auto param = ParseParameter())
			{
				func.parameters.push_back(*param);
			}
			while (Match(TokenType::comma))
			{
				if (auto param = ParseParameter())
				{
					func.parameters.push_back(*param);
				}
			}


			auto currentToken = PeekToken();
            Consume(TokenType::close_paren);
        }


		auto p = PeekToken();
        if (Match(TokenType::semicolon)) { //forward declaration

            func.body = std::nullopt;
        }
        else if (Match(TokenType::open_brace)) {

            auto block = ParseBlock();

            func.body = std::move(block);
        }
        else {
            std::cerr << "Expected ';' or '{' after function signature." << std::endl;
            exit(EXIT_FAILURE);
        }

        return _allocator->new_object<NodeFunction>(func);
    }


    NodeStatement* ParseIf()
    {

        NodeStatementIf _if;
        if (auto node_expr = ParseLogicalOr())
        {
            
            _if.condition = node_expr;
        }
        else
        {
            std::cerr << "Invalid (if) expression" << std::endl;
            exit(EXIT_FAILURE);
        }


        Consume(TokenType::open_brace);
        _if.thenBody = _allocator->new_object<NodeStatement>(ParseBlock());
        if (Match(TokenType::_else))
        {
            if (Match(TokenType::_if))
            {
                auto body = NodeBlock{};
                body.statements.push_back(ParseIf());
                _if.elseBody = _allocator->new_object<NodeStatement>(body);
            }
            else
            {
                TryConsume(TokenType::open_brace);
                _if.elseBody = _allocator->new_object<NodeStatement>(ParseBlock());
            }
        }







        return _allocator->new_object<NodeStatement>(_if);
    }
    NodeStatement* ParseWhile()
    {
        NodeStatementWhile _while;
        if (auto node_expr = ParseExpression())
        {
            _while.condition = node_expr;
        }
        else
        {
            std::cerr << "Invalid (while) expression" << std::endl;
            exit(EXIT_FAILURE);
        }
        Consume(TokenType::open_brace);
        _while.body = _allocator->new_object<NodeStatement>(ParseBlock());


        return _allocator->new_object<NodeStatement>(_while);

    }
    NodeStatement* ParseFor()
    {

        NodeExpr* updateExpr = nullptr;
        NodeBlock block;
        NodeStatementWhile _while;
        Consume(TokenType::open_paren);
        //initialization
        if (PeekToken() && PeekToken()->type != TokenType::semicolon) {
            if (auto stat = ParseStat()) {
                if (std::holds_alternative<NodeStatementVar>(stat->stat) ||
                    std::holds_alternative<NodeStatementAssignment>(stat->stat)) {
                    block.statements.push_back(stat);
                }
                else {
                    std::cerr << "Expected expression or declaration in for loop initialization\n";
                    exit(EXIT_FAILURE);
                }
            }
            else if (auto expr = ParseLogicalOr()) {
                auto exprStat = _allocator->new_object<NodeStatement>(NodeStatementExpr{ expr });
                block.statements.push_back(exprStat);
            }
        }
        else
        {
            Consume(TokenType::semicolon);
        }

        //condition
        if (PeekToken() && PeekToken()->type != TokenType::semicolon) {
            if (auto expr = ParseLogicalOr()) {
                _while.condition = expr;
            }
        }
        else {
            // Default to true
            _while.condition = _allocator->new_object<NodeExpr>(NodeExprLiteral{ true }, QualifiedType{ Type::MakeBool() });
        }
        Consume(TokenType::semicolon);
        //update
        if (PeekToken() && PeekToken()->type != TokenType::close_paren) {
            if (auto expr = ParseExpression()) {
                updateExpr = expr;
            }
        }
        Consume(TokenType::close_paren);
        //body
        Consume(TokenType::open_brace);

        _while.body = _allocator->new_object<NodeStatement>(NodeBlock{ ParseBlock() });

        if (updateExpr) {
            _while.update = updateExpr;
        }

        block.statements.push_back(_allocator->new_object<NodeStatement>(_while));

        return _allocator->new_object<NodeStatement>(block);
    }
	std::optional<NodeStatement*> ParseVariableDeclaration()
	{
		auto parsed = ParseType();
		if (!parsed)
			return std::nullopt;

		auto& type = parsed->type;
		auto& declarator = parsed->declarator;

		const std::string& name = declarator.name;
		NodeExpr* initializer = nullptr;

		if (Match(TokenType::equal))
		{
			if (TypeInfo::IsArray(type))
			{
				initializer = ParseInitializerList(); 
			}
			else
			{
				initializer = ParseLogicalOr();      
			}
		}

		Consume(TokenType::semicolon);

		// Always create a single NodeStatementVar
		NodeStatementVar var;
		var.name = name;
		var.type = type;
		var.initializer = initializer;

		return _allocator->new_object<NodeStatement>(var);
	}
    NodeExpr* ParseInitializerList()
    {
        NodeExprInitList list;
        while (Match(TokenType::open_brace)) {
            while (PeekToken() && PeekToken()->type != TokenType::close_brace)
            {

                if (auto expr = ParseLogicalOr())
                {
                    list.values.push_back(expr);
                }
                else if (auto l = ParseInitializerList())
                {
                    list.values.push_back(_allocator->new_object<NodeExpr>(*l));
                }
                else
                {
                    std::cerr << "Parser Error: Invalid expression in initializer list\n";
                    exit(EXIT_FAILURE);
                }

                if (!TryConsume(TokenType::comma))
                {
                    break;

                }
            }

            Consume(TokenType::close_brace);
        }
        return _allocator->new_object<NodeExpr>(list);

    }
    std::optional<Parameter> ParseParameter()
    {

        auto type_info = ParseType();
        Parameter parameter;

        if (!type_info) {
            return std::nullopt;
        }
        auto identifier = type_info->declarator.name;

        parameter = Parameter(identifier, type_info->type);

        return { parameter };
    }

	bool IsFunctionDeclaration()
	{
		size_t offset = 0;

		// Skip leading storage/type qualifiers (const, static, volatile)
		while (auto token = PeekToken(offset)) {
			if (token->type == TokenType::_const ||
				token->type == TokenType::_static ||
				token->type == TokenType::_volatile) {
				offset++;
			}
			else {
				break;
			}
		}

		size_t type_start_offset = offset;
		bool has_builtin = false;
		int int_count = 0, bool_count = 0, void_count = 0, char_count = 0;
		int short_count = 0, long_count = 0, signed_count = 0, unsigned_count = 0;
		int float_count = 0, double_count = 0; // Added float and double counters

		while (auto token = PeekToken(offset)) {
			bool matched = true;
			if (token->type == TokenType::_int) { int_count++; }
			else if (token->type == TokenType::_bool) { bool_count++; }
			else if (token->type == TokenType::_void) { void_count++; }
			else if (token->type == TokenType::_char) { char_count++; }
			else if (token->type == TokenType::_short) { short_count++; }
			else if (token->type == TokenType::_long) { long_count++; }
			else if (token->type == TokenType::_signed) { signed_count++; }
			else if (token->type == TokenType::_unsigned) { unsigned_count++; }
			else if (token->type == TokenType::_float) { float_count++; }     
			else if (token->type == TokenType::_double) { double_count++; }   
			else {
				matched = false;
			}

			if (matched) {
				has_builtin = true;
				offset++;
			}
			else {
				break;
			}
		}

		// If no built-in keywords were found, check for a single custom type identifier
		if (!has_builtin) {
			auto token = PeekToken(offset);
			if (token && token->type == TokenType::_identifier) {
				size_t temp_offset = offset + 1;
				while (auto pt = PeekToken(temp_offset)) {
					if (pt->type == TokenType::star || pt->type == TokenType::_const) {
						temp_offset++;
					}
					else {
						break;
					}
				}
				auto next_id = PeekToken(temp_offset);
				auto after_next = PeekToken(temp_offset + 1);

				if (next_id && next_id->type == TokenType::_identifier &&
					after_next && after_next->type == TokenType::open_paren) {
					// It's a valid custom return type. Consume the type identifier.
					offset++;
				}
				else {
					return false;
				}
			}
			else {
				std::cerr << "Invalid or missing return type in function declaration" << std::endl;
				return false;
			}
		}
		else {
			// Validate built-in type duplicate rules and illegal combinations
			if (int_count > 1 || bool_count > 1 || void_count > 1 || char_count > 1 ||
				short_count > 1 || long_count > 2 || signed_count > 1 || unsigned_count > 1 ||
				float_count > 1 || double_count > 1) {
				std::cerr << "Malformed type specifier (duplicate types)" << std::endl;
				return false;
			}
			if ((void_count > 0 || bool_count > 0 || float_count > 0) && (offset - type_start_offset > 1)) {
				std::cerr << "Invalid compound type combination" << std::endl;
				return false;
			}

			// Specific validations for float and double rules
			if (float_count > 0) {
				if (double_count > 0 || long_count > 0 || signed_count > 0 || unsigned_count > 0 ||
					int_count > 0 || char_count > 0 || short_count > 0) {
					std::cerr << "Invalid compound type combination with float" << std::endl;
					return false;
				}
			}

			if (double_count > 0) {
				if (float_count > 0 || signed_count > 0 || unsigned_count > 0 ||
					int_count > 0 || char_count > 0 || short_count > 0) {
					std::cerr << "Invalid compound type combination with double" << std::endl;
					return false;
				}
				// 'long double' is legal (long_count == 1), but 'long long double' is not.
				if (long_count > 1) {
					std::cerr << "Invalid long double specifier" << std::endl;
					return false;
				}
			}
		}

		while (auto token = PeekToken(offset)) {
			if (token->type == TokenType::star || token->type == TokenType::_const) {
				offset++;
			}
			else {
				break;
			}
		}

		auto funcName = PeekToken(offset);
		if (!funcName || funcName->type != TokenType::_identifier) {
			return false;
		}
		offset++;

		auto openParen = PeekToken(offset);
		return openParen && openParen->type == TokenType::open_paren;
	}
    std::optional<Token> TryConsume(TokenType type)
    {
        if (!PeekToken() || PeekToken()->type != type) {
            return {};
        }

       return Consume(type);
    }

    void Advance() { _currentIndex++; }
    std::optional<Token> PeekToken(int i = 0) const
    {
        if (_currentIndex + i >= _tokens.size())
        {
            return {};
        }
        return  _tokens.at(_currentIndex + i);

    }

    bool Match(TokenType type, int peak = 0)
    {
        if (PeekToken(peak) && PeekToken(peak)->type == type)
        {
            Advance();
            return true;
        }

        return false;
    }
    bool TryMatch(TokenType type, int peak = 0)
    {
        if (PeekToken(peak) && PeekToken(peak)->type == type)
        {
            return true;
        }

        return false;
    }
    std::optional<Token> Consume(TokenType type)
    {
        if (!PeekToken().has_value()) { return{}; }
        auto error = [&](const std::string& token) { if ((PeekToken().has_value() && PeekToken()->type != type) ) {
            std::cerr << "Expected: " << (token) << " at line: " << PeekToken().value().line << "  " << "Got: " << PeekToken().value().value << std::endl; exit(EXIT_FAILURE); } };
        switch (type)
        {
        case TokenType::exit:
            error("exit");
            break;
        case TokenType::_identifier:
            error("Identifier");
            break;
        case TokenType::semicolon:
            error(";");
            break;
        case TokenType::int_literal:
            error("Integer");
            break;
        case TokenType::open_paren:
            error("(");
            break;
        case TokenType::close_paren:
            error(")");
            break;
        case TokenType::_int:
            error("int");
            break;
        case TokenType::equal:
            error("=");
            break;
        case TokenType::equal_equal:
            error("=");
            break;
        case TokenType::plus:
            error("+");
            break;
        case TokenType::sub:
            error("-");
            break;
        case TokenType::star:
            break;
        case TokenType::div:
            break;
        case TokenType::open_brace:
            error("{");
            break;
        case TokenType::close_brace:
            error("}");
            break;
        case TokenType::_if:
            error("if");
            break;
        case TokenType::_while:
            error("while");
            break;
        case TokenType::_else:
            break;
        case TokenType::_return:
            error("return");
            break;
        case TokenType::post_increment:
            error("++");
        default:
            break;
        }

        return  _tokens.at(_currentIndex++); 
    }
    Token GetToken()
    {
        return  _tokens.at(_currentIndex++);
    }
    void WrapModifier(std::unique_ptr<DeclaratorModifier>& existing_root,
        std::unique_ptr<DeclaratorModifier> new_outer)
    {
        new_outer->child = std::move(existing_root);
        existing_root = std::move(new_outer);
    }
    std::optional<ParsedType> ParseType()
    {
        QualifiedType base;
        bool type_found = false;
		bool is_unsigned = false;
		bool sign_specified = false;
		while (PeekToken()) {
			if (Match(TokenType::_int))
			{

				if (type_found)
				{
					if (base.base_type->kind == Type::Kind::Short)
					{
                        continue;
					}
					std::cerr << " Error: two or more data types in declaration specifiers" << std::endl;
					exit(EXIT_FAILURE);
				}
				base.base_type = std::make_shared<Type>();
				base.base_type->kind = Type::Kind::Int;
				type_found = true;
			}
			else if (Match(TokenType::_short))
			{
				if (type_found)
				{
					if (base.base_type && base.base_type->kind == Type::Kind::Int)
					{
						// Allows "int short" (normalizes to short)
						base.base_type->kind = Type::Kind::Short;
						continue;
					}
					std::cerr << " Error: two or more data types in declaration specifiers" << std::endl;
					exit(EXIT_FAILURE);
				}
				base.base_type = std::make_shared<Type>();
				base.base_type->kind = Type::Kind::Short;
				type_found = true;
			}
			else if (Match(TokenType::_char))
			{
				if (type_found)
				{
					std::cerr << " Error: two or more data types in declaration specifiers" << std::endl;
					exit(EXIT_FAILURE);
				}
				base.base_type = std::make_shared<Type>();
				base.base_type->kind = Type::Kind::Char;
				
				type_found = true;

			}
			else if (Match(TokenType::_bool))
			{
				if (type_found)
				{
					std::cerr << " Error: two or more data types in declaration specifiers" << std::endl;
					exit(EXIT_FAILURE);
				}
				base.base_type = std::make_shared<Type>();
				base.base_type->kind = Type::Kind::Bool;
				
				type_found = true;
			}
			else if (Match(TokenType::_float))
			{
				if (type_found)
				{
					std::cerr << " Error: two or more data types in declaration specifiers" << std::endl;
					exit(EXIT_FAILURE);
				}
				base.base_type = std::make_shared<Type>();
				base.base_type->kind = Type::Kind::Float;

				type_found = true;
			}
			else if (Match(TokenType::_double))
			{
				if (type_found)
				{
					std::cerr << " Error: two or more data types in declaration specifiers" << std::endl;
					exit(EXIT_FAILURE);
				}
				base.base_type = std::make_shared<Type>();
				base.base_type->kind = Type::Kind::Double;

				type_found = true;
			}
			else if (Match(TokenType::_void))
			{
				if (type_found)
				{
					std::cerr << " Error: two or more data types in declaration specifiers" << std::endl;
					exit(EXIT_FAILURE);
				}
				base.base_type = std::make_shared<Type>();
				base.base_type->kind = Type::Kind::Void;
				type_found = true;
			}
			else if (Match(TokenType::_const))
			{
				base.mod.is_const = true;
			}
			else if (Match(TokenType::_volatile))
			{
				base.mod.is_volatile = true;
			}
			else if (Match(TokenType::_signed))
			{
				if (sign_specified && is_unsigned)
				{
					std::cerr << " Error: both 'signed' and 'unsigned' specified" << std::endl;
					exit(EXIT_FAILURE);
				}
				is_unsigned = false;
				sign_specified = true;
			}
			else if (Match(TokenType::_unsigned))
			{
				if (sign_specified && !is_unsigned)
				{
					std::cerr << " Error: both 'signed' and 'unsigned' specified" << std::endl;
					exit(EXIT_FAILURE);
				}
				is_unsigned = true;
				sign_specified = true;
			}
			else
			{
				// Not a type specifier or qualifier, break out of type header parsing
				break;
			}
		}

		if (!type_found || !base.base_type) {
			return std::nullopt;
		}

        base.base_type->is_signed = !is_unsigned;
        auto declarator = ParseDeclarator();

        auto type = BuildType(base, declarator.modifier.get());

        return ParsedType{
            std::move(type),
            std::move(declarator)
        };
    }


    Declarator ParseDeclarator()
    {
        Declarator decl;

        // Count prefix pointers
		struct PointerInfo {
			bool is_const = false;
			bool is_volatile = false;
		};
		std::vector<PointerInfo> pointers;

		while (Match(TokenType::star))
		{
			PointerInfo ptr_info;
			// Check for qualifiers right after the '*' ("* const" or "* volatile")
			while (true) {
				if (Match(TokenType::_const)) {
					ptr_info.is_const = true;
				}
				else if (Match(TokenType::_volatile)) {
					ptr_info.is_volatile = true;
				}
				else {
					break;
				}
			}
			pointers.push_back(ptr_info);
		}
        
        // Parse identifier/group and postfix suffixes
        decl = ParseDirectDeclarator();

        // Wrap prefix pointers around the direct declarator modifier chain
		for (const auto& ptr_info : pointers)
		{
			auto ptr = std::make_unique<DeclaratorModifier>();
			ptr->kind = DeclaratorModifier::Kind::Pointer;
			ptr->is_const = ptr_info.is_const;
			ptr->is_volatile = ptr_info.is_volatile;

			WrapModifier(decl.modifier, std::move(ptr));
		}

        return decl;
    }

    Declarator ParseDirectDeclarator(){
        Declarator decl;

        // A. Primary: Identifier OR Parenthesized Group
        if (PeekToken() && PeekToken()->type == TokenType::_identifier)
        {
            decl.name = GetToken().value;
        }
        else if(Match(TokenType::open_paren))
        {
            decl = ParseDeclarator();
            Consume(TokenType::close_paren);
        }
        else
        {
            throw std::runtime_error("Expected identifier or '('");
        }

        while (Match(TokenType::open_bracket))
        {
            size_t size = 0;
            if (PeekToken() && PeekToken()->type == TokenType::int_literal)
            {
                size = std::stoull(GetToken().value);
            }
            Consume(TokenType::close_bracket);

            // Add Array suffix: Postfix modifiers wrap OVER current direct modifiers
            auto arr = std::make_unique<DeclaratorModifier>();
            arr->kind = DeclaratorModifier::Kind::Array;
            arr->array_size = size;

            WrapModifier(decl.modifier, std::move(arr));
        }

        return decl;
    }

    QualifiedType BuildType(
        const QualifiedType& base_type,
        const DeclaratorModifier* mod)
    {
        if (!mod) return base_type;

        switch (mod->kind) {
        case DeclaratorModifier::Kind::Pointer:
            return BuildType(QualifiedType::MakePointer(base_type, Modifier{mod->is_const, mod->is_volatile}), mod->child.get());

        case DeclaratorModifier::Kind::Array:
            return BuildType(QualifiedType::MakeArray(base_type, mod->array_size), mod->child.get());
        }

        return base_type;
    }

   
	std::vector<Token> _tokens;

    std::pmr::polymorphic_allocator<char>* _allocator;
    int _currentIndex = 0;
};