#pragma once
#include "Expr.h"
#include <vector>

class Parser
{
protected:
	std::vector<Token> tokens;
	size_t current;
	bool error;

	inline bool IsAtEnd()
	{
		return current >= tokens.size();
	}

	inline bool Check(TokenType type)
	{
		if (IsAtEnd()) return false;
		return tokens[current].type == type;
	}

	inline Token Advance()
	{
		if (current < tokens.size()) return tokens[current++];
		return Token();
	}

	inline Token Previous()
	{
		if (current == 0) return Token();
		return tokens[current - 1];
	}

	inline Token Peek()
	{
		if (IsAtEnd()) return Token();
		return tokens[current];
	}

	Token Consume(TokenType type, const std::string& errorMessage);
	Token Error(const Token& token, const std::string& errorMessage);
	void Synchronize();

	ExprPtr Comma();
	ExprPtr Ternary();
	ExprPtr Equality();
	ExprPtr Comparsion();
	ExprPtr Term();
	ExprPtr Factor();
	ExprPtr Unary();
	ExprPtr Primary();

	template<typename... TokenTypes>
	bool Match(TokenTypes... types)
	{
		for (TokenType tokenType : { types... })
		{
			if (Check(tokenType))
			{
				Advance();
				return true;
			}
		}
		return false;
	}
public:
	Parser(const std::vector<Token>& inTokens);
	~Parser();

	ExprPtr Parse();
};