#pragma once
#include <string>
#include <vector>
#include <optional>
#include <iostream>


enum class TokenType
{
	exit,
    _return,
	_identifier,
	semicolon,
	int_literal,
    open_paren,
    close_paren,
    _int,
    equal,
	increment,

    plus,
    sub,
    mult,
    div,


    open_brace,
    close_brace,
    _if,
    _else,
    _while,

    _bool,
    true_literal,
    false_literal,
    less,
    greater,
    equal_equal,
    _or,
    _and,
    _not,



    func
};
struct Token
{
	TokenType type;
	std::string value;
};




class Tokenizer
{
public:
	Tokenizer(const std::string& source) : _src(source)
	{

	}

    std::vector<Token> tokenize(const std::string& content)
    {
        std::vector<Token> tokens;

        while (PeekChar().has_value())
        {
            char c = PeekChar().value();

            if (isspace(c))
            {
                _currentIndex++;
                continue;
            }

            else if (isalpha(c) || c == '_')
            {
                std::string identifier;

                while (PeekChar().has_value() &&
                    (isalnum(PeekChar().value()) || PeekChar().value() == '_'))
                {
                    identifier += GetChar();

                }

                if (identifier == "exit")
                {
                    tokens.push_back({ TokenType::exit, identifier });

                }
                else if (identifier == "int")
                {
                    tokens.push_back({ TokenType::_int, identifier });
                }
                else if (identifier == "if")
                {
                    tokens.push_back({ TokenType::_if, identifier });

                }
                else if (identifier == "else")
                {
                    tokens.push_back({ TokenType::_else, identifier });

                }
                else if (identifier == "bool")
                {
                    tokens.push_back({ TokenType::_bool, identifier });

                }
                else if (identifier == "true")
                {
                    tokens.push_back({ TokenType::true_literal, identifier });

                }
                else if (identifier == "false")
                {
                    tokens.push_back({ TokenType::false_literal, identifier });

                }
				else if (identifier == "while")
				{
					tokens.push_back({ TokenType::_while, identifier });

				}
                else if (identifier == "return")
                {
                    tokens.push_back({ TokenType::_return, identifier });

                }
                else
                {
                    tokens.push_back({ TokenType::_identifier, identifier });
                }
            }

            else if (isdigit(c))
            {
                std::string number;

                while (PeekChar().has_value() && isdigit(PeekChar().value()))
                {
                    number += GetChar();

                }

                tokens.push_back({ TokenType::int_literal, number });
            }

            else if (c == ';')
            {
                tokens.push_back({ TokenType::semicolon, ";" });
                _currentIndex++;

            }

            else if (c == '(')
            {
                tokens.push_back({ TokenType::open_paren, "(" });
                _currentIndex++;
                continue;

            }
            else if (c == ')')
            {
                tokens.push_back({ TokenType::close_paren, ")" });
                _currentIndex++;
                continue;
            }
            else if (c == '=')
            {
                _currentIndex++;
                if(PeekChar().has_value() && PeekChar().value() == '=')
                { 
                    tokens.push_back({ TokenType::equal_equal, "==" });
                    _currentIndex++;
                    continue;
                }
                else { tokens.push_back({ TokenType::equal, "=" }); }
            }
            else if (c == '+')
            {
				_currentIndex++;
				if (PeekChar().has_value() && PeekChar().value() == '+')
				{
					tokens.push_back({ TokenType::increment, "++" });
					_currentIndex++;
					continue;
                }
                else
                {
                    tokens.push_back({ TokenType::plus, "+" });
                }
            }
            else if (c == '*')
            {
                tokens.push_back({ TokenType::mult, "*" });
                _currentIndex++;
            }
            else if (c == '/')
            {
                tokens.push_back({ TokenType::div, "/" });
                _currentIndex++;
            }
            else if (c == '-')
            {
                tokens.push_back({ TokenType::sub, "-" });
                _currentIndex++;
            }
            else if (c == '{')
            {
                tokens.push_back({ TokenType::open_brace, "{" });
                _currentIndex++;
            }
            else if (c == '}')
            {
                tokens.push_back({ TokenType::close_brace, "}" });
                _currentIndex++;
            }
            else if (c == '<')
            {
                tokens.push_back({ TokenType::less, "<" });
                _currentIndex++;
            }
            else if (c == '>')
            {
                tokens.push_back({ TokenType::greater, ">" });
                _currentIndex++;
            }
            else if (c == '|')
            {
                _currentIndex++;
                if (PeekChar().has_value() && PeekChar().value() == '|') {
                    _currentIndex++;
                    tokens.push_back({ TokenType::_or, "||" });
                }
            }
            else if (c == '&')
            {
                _currentIndex++;
                if (PeekChar().has_value() && PeekChar().value() == '&') {
                    _currentIndex++;
                    tokens.push_back({ TokenType::_and, "&&" });
                }
                else
                {

                }
             }
            else if (c == '!')
            {
                tokens.push_back({ TokenType::_not, "!" });
                _currentIndex++;

            }
        }

        _currentIndex = 0;
        return tokens;
    }


private:

    void Advance() { _currentIndex++;}
    std::optional<char> PeekChar(int i = 0) const
    {
        if (_currentIndex + i >= _src.length())
        {
            return {};
        }
        return _src.at(_currentIndex + i);

    }

    char GetChar()
    {
        return _src.at(_currentIndex++);
    }
	std::string _src;

    
    int _currentIndex = 0;
};