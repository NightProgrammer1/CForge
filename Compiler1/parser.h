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
            auto expr = ParseBoolean(); // Can parse math, comparisons, AND logical operations
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
		//Handle the case where we have a term, and then we have a binary operator (+ or -) that combines two terms.
        auto left = ParseTerm();

        auto p = PeekToken();
        while (p && (p->type == TokenType::plus || p->type == TokenType::sub)) // If its a binary expr
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
                PeekToken()->type == TokenType::equal_equal))
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


        // 2. Loop until we see '}' without consuming it in the condition
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

        // 3. Consume closing brace
        Consume(TokenType::close_brace);

        return block;
    }
    std::unique_ptr<NodeStatement> ParseStat()
    {
        NodeStatementExit exit_node;
        if (Match(TokenType::exit))
        {
            if (auto node_expr = ParseBoolean())
            {
                exit_node.expr = std::move(node_expr);
            }
            else
            {
                std::cerr << "Invalid (exit) expression" << std::endl;
                exit(EXIT_FAILURE);
            }
            if (TryConsume(TokenType::close_paren)) { Advance(); };
            Consume(TokenType::semicolon);

            return std::make_unique <NodeStatement>(NodeStatement{ .stat = std::move(exit_node) });
        }
        else if (Match(TokenType::_int))
        {
            bool has_identifier = PeekToken().has_value() && (PeekToken().value().type == TokenType::_identifier);
            bool has_equal = PeekToken(1).has_value() && (PeekToken(1).value().type == TokenType::equal);

            if (has_identifier && has_equal)
            {
                auto statement_int = NodeStatementVar{.ident = GetToken()};

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
            statement_var.type = VarType::Bool;

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
        else  if (Match(TokenType::open_brace))
        {

            auto block = ParseBlock();
            return std::make_unique< NodeStatement>(NodeStatement{.stat = std::move(block)});
        }


        if (auto expr = ParseBoolean())
        {
            Consume(TokenType::semicolon);

            return std::make_unique<NodeStatement>(
                NodeStatement{
                    .stat = NodeStatementExpr{
                        std::move(expr)
                    }
                });
        }
        return {};
    }
    std::optional<NodeProg> ParseProgram()
    {
        NodeProg program;
        std::vector<std::unique_ptr<NodeStatement>> stats;
        while (PeekToken().has_value())
        {
            if (auto statement = ParseStat())
            {
                stats.emplace_back(std::move(statement));
            }
            else
            {
                std::cerr << "Invalid statement" << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        program.root.statements = std::move(stats);
        return std::move(program);
    }
    
    std::unique_ptr<NodeStatement> ParseIf()
    {
        Consume(TokenType::open_paren);

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
    void Consume(TokenType type)
    {
        if (!PeekToken().has_value()) { return; }
        auto error = [&](const std::string& token) { if ((PeekToken().has_value() && PeekToken()->type != type) ) { std::cerr << "Expected: " << (token) << "  " << "Got: " << PeekToken().value().value << std::endl; exit(EXIT_FAILURE); } };
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
        case TokenType::plus:
            break;
        case TokenType::sub:
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
            break;
        case TokenType::_while:
            error("while");
            break;
        case TokenType::_else:
            break;
        default:
            break;
        }

        _currentIndex++;
    }
    Token GetToken()
    {
        return  _tokens.at(_currentIndex++);
    }


    int _currentIndex = 0;
	std::vector<Token> _tokens;
};