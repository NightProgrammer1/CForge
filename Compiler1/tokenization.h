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
    comment,
    semicolon,
    comma,
    int_literal,
    open_paren,
    close_paren,
    equal,
    plus_equal,
    sub_equal,
    star_equal,
    div_equal,
    increment,
	pre_increment,
    post_increment,
    decrease,
    pre_decrement,
    post_decrement,

    plus,
    sub,
    star,
    div,




    _int,
    _void,
    _bool,
    _const,
    _static,
    _volatile,

    open_brace,
    close_brace,
    open_bracket,
    close_bracket,
    _if,
    _else,
    _while,
    _for,
    _break,
    _continue,

    true_literal,
    false_literal,
    less,
    greater,
    equal_equal,
    greater_equal,
    less_equal,
    not_equal,
    negate,
    _or,
    _and,
    _not,



    func,
    ampersand,
    _NULL
};
struct Token
{
	TokenType type;
	std::string value;
    int line;

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
        int lineCount = 1;
        while (PeekChar().has_value())
        {
            char c = PeekChar().value();

            

            if (isalpha(c) || c == '_')
            {
                std::string identifier;

                while (PeekChar().has_value() &&
                    (isalnum(PeekChar().value()) || PeekChar().value() == '_'))
                {
                    identifier += GetChar();

                }

                if (identifier == "exit")
                {
                    tokens.push_back({  TokenType::exit, identifier, lineCount });

                }
                else if (identifier == "int")
                {
                    tokens.push_back({  TokenType::_int, identifier, lineCount });
                }
                else if (identifier == "void")
                {
                    tokens.push_back({ TokenType::_void, identifier, lineCount });
                }
                else if (identifier == "if")
                {
                    tokens.push_back({  TokenType::_if, identifier, lineCount });

                }
                else if (identifier == "else")
                {
                    tokens.push_back({ TokenType::_else, identifier, lineCount });

                }
                else if (identifier == "bool")
                {
                    tokens.push_back({ TokenType::_bool, identifier,  lineCount });

                }
                else if (identifier == "true")
                {
                    tokens.push_back({  TokenType::true_literal, identifier, lineCount });

                }
                else if (identifier == "false")
                {
                    tokens.push_back({  TokenType::false_literal, identifier, lineCount });

                }
				else if (identifier == "while")
				{
					tokens.push_back({  TokenType::_while, identifier, lineCount });

				}
                else if (identifier == "for")
                {
                    tokens.push_back({ TokenType::_for, identifier, lineCount });

                }
                else if (identifier == "return")
                {
                    tokens.push_back({ TokenType::_return, identifier,  lineCount });

                }
                else if (identifier == "break")
                {
                    tokens.push_back({ TokenType::_break, identifier,  lineCount });

                }
                else if (identifier == "continue")
                {
                    tokens.push_back({ TokenType::_continue, identifier,  lineCount });

                }
                else if (identifier == "NULL")
                {
                    tokens.push_back({ TokenType::_NULL, identifier,  lineCount });
                }
                else
                {
                    tokens.push_back({  TokenType::_identifier, identifier, lineCount });
                }
            }

            else if (isdigit(c))
            {
                std::string number;

                while (PeekChar().has_value() && isdigit(PeekChar().value()))
                {
                    number += GetChar();

                }

                tokens.push_back({  TokenType::int_literal, number, lineCount });
            }

            else if (c == '\n')
            {
                _currentIndex++;
                lineCount++;
            }
            else if (isspace(c) || c == '\t' || c == '\r')
            {
                _currentIndex++;
                continue;
            }
            else if (c == ';')
            {
                tokens.push_back({  TokenType::semicolon, ";", lineCount });
                _currentIndex++;

            }
            else if (c == ',')
            {
                tokens.push_back({  TokenType::comma, ",", lineCount, });
                _currentIndex++;
            }
            else if (c == '(')
            {
                tokens.push_back({ TokenType::open_paren, "(", lineCount });
                _currentIndex++;
                continue;

            }
            else if (c == ')')
            {
                tokens.push_back({  TokenType::close_paren, ")", lineCount });
                _currentIndex++;
                continue;
            }
            else if (c == '[')
            {
                tokens.push_back({ TokenType::open_bracket, "[", lineCount });
                _currentIndex++;
                continue;
            }
            else if (c == ']')
            {
                tokens.push_back({ TokenType::close_bracket, "]", lineCount });
                _currentIndex++;
                continue;
            }
            else if (c == '=')
            {
                _currentIndex++;
                if(PeekChar().has_value() && PeekChar().value() == '=')
                { 
                    tokens.push_back({ TokenType::equal_equal, "==", lineCount });
                    _currentIndex++;
                    continue;
                }
                else { tokens.push_back({ TokenType::equal, "=" , lineCount}); }
            }

            else if (c == '+')
            {
				_currentIndex++;
				if (PeekChar().has_value() && PeekChar().value() == '+')
				{
					tokens.push_back({ TokenType::increment, "++", lineCount});
					_currentIndex++;
					continue;
                }
                else if (PeekChar().has_value() && PeekChar().value() == '=')
                {
                    tokens.push_back({ TokenType::plus_equal, "+=", lineCount });
                    _currentIndex++;
                }
                else
                {
                    tokens.push_back({  TokenType::plus, "+", lineCount });
                }
            }
            else if (c == '*')
            {
                _currentIndex++;

                if (PeekChar().has_value() && PeekChar().value() == '=')
                {
                    tokens.push_back({ TokenType::star_equal, "*=", lineCount });
                    _currentIndex++;
                    continue;
                }
                else {
                    tokens.push_back({ TokenType::star, "*", lineCount });
                }
            }
            else if (c == '/')
            {
                _currentIndex++;

                if (PeekChar() && PeekChar().value() == '=')
                {
                    tokens.push_back({ TokenType::div_equal, "/=", lineCount });
                    _currentIndex++;
                    continue;
                }
                else if (PeekChar() && PeekChar().value() == '/')
                {
                    auto next_line = _src.find('\n', _currentIndex);
                    _currentIndex += (next_line - _currentIndex); // skips the line

                    auto value = _src[_currentIndex];


                    continue;
                }
                else {
                    tokens.push_back({ TokenType::div, "/", lineCount });
                }
            }
            else if (c == '-')
            {
                _currentIndex++;

                if (PeekChar().has_value() && PeekChar().value() == '=')
                {
                    tokens.push_back({ TokenType::sub_equal, "-=", lineCount });
                    _currentIndex++;
                    continue;
                }
                else if (PeekChar().has_value() && PeekChar().value() == '-')
                {
                    tokens.push_back({ TokenType::decrease, "--", lineCount });
                    _currentIndex++;
                    continue;
                }
                else
                {
                    tokens.push_back({ TokenType::sub, "-", lineCount });

                }
            }
            else if (c == '{')
            {
                tokens.push_back({  TokenType::open_brace, "{", lineCount });
                _currentIndex++;
            }
            else if (c == '}')
            {
                tokens.push_back({  TokenType::close_brace, "}", lineCount });
                _currentIndex++;
            }
            else if (c == '<')
            {
                _currentIndex++;
                if (PeekChar().has_value() && PeekChar().value() == '=')
                {
                    tokens.push_back({ TokenType::less_equal, "<=", lineCount });
                    _currentIndex++;
                    continue;
                }
                else
                {
                    tokens.push_back({ TokenType::less, "<", lineCount });
                }
            }
            else if (c == '>')
            {
                _currentIndex++;
                if (PeekChar().has_value() && PeekChar().value() == '=')
                {
                    tokens.push_back({ TokenType::greater_equal, ">=", lineCount });
                    _currentIndex++;
                    continue;
                }
                tokens.push_back({ TokenType::greater, ">" , lineCount });
            }
            else if (c == '|')
            {
                _currentIndex++;
                if (PeekChar().has_value() && PeekChar().value() == '|') {
                    _currentIndex++;
                    tokens.push_back({TokenType::_or, "||" });
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
                    tokens.push_back({ TokenType::ampersand, "&" });

                }
             }
            else if (c == '!')
            {
                _currentIndex++;
                if (PeekChar().has_value() && PeekChar().value() == '=')
                {
                    tokens.push_back({ TokenType::not_equal, "!=", lineCount });
                    _currentIndex++;
                    continue;
                }
                else { tokens.push_back({ TokenType::_not, "!" , lineCount }); }
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