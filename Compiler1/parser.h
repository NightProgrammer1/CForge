#pragma once
#include "types.h"


class Parser
{
public:
	Parser(const std::vector<Token>& tokens) : _tokens(tokens)
	{

	}
    std::unique_ptr<NodeExpr> ParseFactor()
    {
        auto p = PeekToken();

        

        if (p && p->type == TokenType::int_literal)
        {
            return  std::make_unique<NodeExpr>(NodeExpr{
                NodeExprIntLit{
                    .int_lit = GetToken()
                }
            });
        }
        if (Match(TokenType::false_literal) )
        {
            return  std::make_unique<NodeExpr>(NodeExpr{
                NodeExprBoolLit{
                    .value = false
                }
                });
        }
        else if (Match(TokenType::true_literal))
        {
            return  std::make_unique<NodeExpr>(NodeExpr{
                NodeExprBoolLit{
                    .value = true
                }
                });
        }

        if (Match(TokenType::_not))
        {
            Token identifier = GetToken();

            auto identExpr = std::make_unique<NodeExpr>();
            identExpr->var = NodeExprIndent{
                .ident = identifier
            };

            auto unary = std::make_unique<NodeExpr>();
            unary->var = NodeExprUnary{
                Token{TokenType::_not},
                std::move(identExpr)
            };

            return unary;
        }
        if (p && p->type == TokenType::_identifier)
        {
            Token identifier = GetToken();

            auto identExpr = std::make_unique<NodeExpr>();
            identExpr->var = NodeExprIndent{
                .ident = identifier
            };

            if (Match(TokenType::open_paren)) {

                auto expr = std::make_unique<NodeExpr>();

                NodeExprCall funcCall;
                funcCall.name = identifier;

                if (Match(TokenType::close_paren))
                {
                    expr->var = std::move(funcCall);
                    return expr;
                }

            
                while (true) {
                    if (auto node_expr = ParseBoolean()) {
                        funcCall.args.push_back(std::move(node_expr));
                    }
                    else {
                        std::cerr << "Invalid expression in function call '"
                            << identifier.value << "'" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    if (Match(TokenType::comma)) {
                        continue;
                    }

                    break;
                }
                expr->var = std::move(funcCall);

                Consume(TokenType::close_paren);
                return expr;
            }

            if (Match(TokenType::increment))
            {
                auto unary = std::make_unique<NodeExpr>();
                unary->var = NodeExprUnary{
                    Token{TokenType::increment},
                    std::move(identExpr)
                };

                return unary;
            }

            return identExpr;
        }


        if (Match(TokenType::open_paren))
        {
            auto expr = ParseBoolean();
            Consume(TokenType::close_paren);
            return expr;
        }


        return {};
    }
    std::unique_ptr<NodeExpr> ParseTerm()
    {
		//Handle the case where we have a factor, and then we have a binary operator (* or /) that combines two factors.
        auto left = ParseFactor();

        auto p = PeekToken();
        while (p && (p->type == TokenType::mult || p->type == TokenType::div))
        {
            Token op = GetToken();
            auto right = ParseFactor();

            auto binary = std::make_unique<NodeExpr>();

            binary->var = NodeExprBinary{
                op,
                std::move(left),
                std::move(right)
            };
            left = std::move(binary);

            p = PeekToken();
        }

        return left;
    }
    std::unique_ptr<NodeExpr> ParseExpr()
    {
        auto left = ParseTerm();

        auto p = PeekToken();
        while (p && (p->type == TokenType::plus || p->type == TokenType::sub)) 
        {
            Token op = GetToken();
            auto right = ParseTerm();

            auto binary = std::make_unique<NodeExpr>();

            binary->var = NodeExprBinary{
                op,
                std::move(left),
                std::move(right)
            };
            left = std::move(binary);
            p = PeekToken();
        }

        

        return left;
    }
    std::unique_ptr<NodeExpr> ParseComparison()
    {
		//Handle the case where we have a comparison expression, and then we have a comparison operator (<, >, ==) that combines two expressions.
        auto left = ParseExpr();
        auto p = PeekToken();

        while (PeekToken() &&
            (   PeekToken()->type == TokenType::less ||
                PeekToken()->type == TokenType::greater ||
                PeekToken()->type == TokenType::equal_equal ||
                PeekToken()->type == TokenType::less_equal))
        {
            Token op = GetToken();
            auto right = ParseExpr();
            auto binary = std::make_unique<NodeExpr>();

            binary->var = NodeExprCompare{
               op,
               std::move(left),
               std::move(right)
            };

            left = std::move(binary);
            p = PeekToken();
        }

        return left;
    }
    std::unique_ptr<NodeExpr> ParseBoolean()
    {
        auto left = ParseLogicalAnd();

        while (PeekToken() && PeekToken()->type == TokenType::_or)
        {
            Token op = GetToken();
            auto right = ParseLogicalAnd();

            auto binary = std::make_unique<NodeExpr>();
            binary->var = NodeExprCompare{
                op,
                std::move(left),
                std::move(right)
            };

            left = std::move(binary);
        }

        return left;
    }

    std::unique_ptr<NodeExpr> ParseLogicalAnd()
    {
        auto left = ParseComparison();


        auto p = PeekToken();


        while (PeekToken() &&
            (PeekToken()->type == TokenType::_and))
        {
            Token op = GetToken();
            auto right = ParseComparison();
            auto binary = std::make_unique<NodeExpr>();

            binary->var = NodeExprCompare{
               op,
               std::move(left),
               std::move(right)
            };

            left = std::move(binary);
            p = PeekToken();
        }

        return left;
    }
    std::unique_ptr<NodeExpr> ParseLogicalOr()
    {
        auto left = ParseLogicalAnd();


        auto p = PeekToken();


        while (PeekToken() &&
            (PeekToken()->type == TokenType::_or))
        {
            Token op = GetToken();
            auto right = ParseLogicalAnd();
            auto binary = std::make_unique<NodeExpr>();

            binary->var = NodeExprCompare{
               op,
               std::move(left),
               std::move(right)
            };

            left = std::move(binary);
            p = PeekToken();
        }

        return left;
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
    std::unique_ptr<NodeStatement> ParseStat()
    {
        if (Match(TokenType::exit))
        {
            NodeStatementExit exit_node;

            if (auto node_expr = ParseBoolean())
            {
                exit_node.expr = std::move(node_expr);
            }
            else
            {
                std::cerr << "Invalid (exit) expression" << std::endl;
                exit(EXIT_FAILURE);
            }
            Consume(TokenType::semicolon);

            return std::make_unique <NodeStatement>(NodeStatement{ .stat = std::move(exit_node) });
        }
        else if (Match(TokenType::_int))
        {
            bool has_identifier = PeekToken().has_value() && (PeekToken().value().type == TokenType::_identifier);
            bool has_equal = PeekToken(1).has_value() && (PeekToken(1).value().type == TokenType::equal);
            auto statement_int = NodeStatementVar{ .ident = GetToken() };

            if (has_identifier && has_equal)
            {

                Consume(TokenType::equal);
                

                if (auto expr = ParseExpr())
                {
                    statement_int.expr = std::move(expr);
                }
                else
                {
                    std::cerr << "Invalid (this) expression" << std::endl;
                    exit(EXIT_FAILURE);
                }
                Consume(TokenType::semicolon);
                
                return std::make_unique <NodeStatement>(NodeStatement{ .stat = std::move(statement_int) });
            }

            
        }
        else if (Match(TokenType::_bool))
        {
            if (!TryConsume(TokenType::_identifier))
            {
                std::cerr << "Missing variable name after bool" << std::endl;
            }

            auto statement_var = NodeStatementVar{ .ident = GetToken()};
            statement_var.type = Type::Bool;

            Consume(TokenType::equal);


            if (auto expr = ParseBoolean())
            {
                statement_var.expr = std::move(expr);
            }
            else
            {
                std::cerr << "Invalid bolean expression" << std::endl;
                exit(EXIT_FAILURE);
            }
            Consume(TokenType::semicolon);
            return std::make_unique <NodeStatement>(NodeStatement{ .stat = std::move(statement_var) });


        }
        else if (Match(TokenType::_if))
        {
            return ParseIf();
        }
		else if (Match(TokenType::_while))
		{
			return ParseWhile();
		}
        else if (Match(TokenType::open_brace))
        {

            auto block = ParseBlock();
            return std::make_unique< NodeStatement>(NodeStatement{.stat = std::move(block)});
        }
        else if (Match(TokenType::_return))
        {
            NodeStatementReturn ret;

            if (auto expr = ParseBoolean())
            {
                ret.expr = std::move(expr);

            }
            else
            {
                std::cerr << "Invalid return expression\n";
                exit(EXIT_FAILURE);
            }

            Consume(TokenType::semicolon);

            return std::make_unique <NodeStatement>(NodeStatement{ .stat = std::move(ret) });


        }
        
        if (auto expr = ParseBoolean())
        {
            Consume(TokenType::semicolon);
            NodeStatementExpr p;
            p.expr = std::move(expr);
            return std::make_unique<NodeStatement>(NodeStatement{ .stat = std::move(p) });
        }
        return {};
    }


    std::vector<Parameter> ParseParameter()
    {
        Consume(TokenType::open_paren);
        
        auto p = PeekToken();
        std::vector<Parameter> parameterList;

        while (p && p.value().type != TokenType::close_paren)
        {
            if (Match(TokenType::_int))
            {
                Parameter par = { Type::Int, GetToken() };
                parameterList.push_back(par);
            }

            if (!TryMatch(TokenType::comma))
            {
                break;

            }
            Consume(TokenType::comma);

            p = PeekToken();
        }

        Consume(TokenType::close_paren);
        return parameterList;
    }
    NodeFunction ParseFunctions()
    {
        NodeFunction func;

        if (GetToken().type == TokenType::_int)
        {
            func.retType = Type::Int;

        }
        func.name = Token{TokenType::_identifier, Consume(TokenType::_identifier).value().value};

        func.parameters = ParseParameter();

        if (Match(TokenType::semicolon)) { //forward declaration

            func.body = std::nullopt; 
        }
        else if (PeekToken().value().type == TokenType::open_brace) {
            Consume(TokenType::open_brace);

            auto block = ParseBlock();

            func.body = std::move(block);
        }
        else {
            std::cerr << "Expected ';' or '{' after function signature." << std::endl;
            exit(EXIT_FAILURE);
        }
        
        return func;
    }
    std::optional<NodeProg> ParseProgram()
    {
        NodeProg prog;
        while (PeekToken().has_value()) {
            if (IsFunctionDeclaration()) {
                prog.functions.push_back(ParseFunctions());
            }
            else {
                std::cerr << "Syntax Error: Top-level code must be a function declaration or definition." << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        return prog;
       

    }
    
   
    std::unique_ptr<NodeStatement> ParseIf()
    {

        NodeStatementIf _if;
        if (auto node_expr = ParseBoolean())
        {
            _if.condition = std::move(node_expr);
        }
        else
        {
            std::cerr << "Invalid (if) expression" << std::endl;
            exit(EXIT_FAILURE);
        }


        Consume(TokenType::open_brace);
        _if.thenBlock = std::move(ParseBlock());
        if (Match(TokenType::_else))
        {
            if (Match(TokenType::_if))
            {
                _if.elseBlock = NodeBlock{};
                _if.elseBlock->statements.push_back(ParseIf());
            }
            else
            {
                TryConsume(TokenType::open_brace);
                _if.elseBlock = ParseBlock();
            }
        }
      






        return std::make_unique<NodeStatement>(
            NodeStatement{
                .stat = std::move(_if)
            }
        );
    }
	std::unique_ptr<NodeStatement> ParseWhile()
	{
		NodeStatementWhile _while;
		if (auto node_expr = ParseBoolean())
		{
			_while.condition = std::move(node_expr);
		}
		else
		{
			std::cerr << "Invalid (while) expression" << std::endl;
			exit(EXIT_FAILURE);
		}
		Consume(TokenType::open_brace);
		_while.block = std::move(ParseBlock());


		return std::make_unique<NodeStatement>(
			NodeStatement{
				.stat = std::move(_while)
			}
		);
	}

private:

    bool IsFunctionDeclaration() {
        if (PeekToken()->type == TokenType::_int || PeekToken()->type == TokenType::_bool) {
            if (PeekToken(1).has_value() && PeekToken(1)->type == TokenType::_identifier) {
                if (PeekToken(2).has_value() && PeekToken(2)->type == TokenType::open_paren) {
                    return true;
                }
            }
        }
        return false;
    }
    std::optional<Token> TryConsume(TokenType type)
    {
        if (!PeekToken() || PeekToken()->type != type) {
            return {};
        }
        return  _tokens.at(_currentIndex);
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
        auto error = [&](const std::string& token) { if ((PeekToken().has_value() && PeekToken()->type != type) ) { std::cerr << "Expected: " << (token) << " at line: " << PeekToken().value().line << "  " << "Got: " << PeekToken().value().value << std::endl; exit(EXIT_FAILURE); } };
        switch (type)
        {
        case TokenType::exit:
            error("exit");
            break;
        case TokenType::_identifier:
            error("Indentifier");
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
        case TokenType::mult:
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
        default:
            break;
        }

        return  _tokens.at(_currentIndex++); 
    }
    Token GetToken()
    {
        return  _tokens.at(_currentIndex++);
    }


    int _currentIndex = 0;
	std::vector<Token> _tokens;
};