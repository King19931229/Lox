#include "Parser.h"
#include "Lox.h"

Parser::Parser(const std::vector<Token>& inTokens)
	: tokens(inTokens)
	, current(0)
	, error(false)
{
}

Parser::~Parser()
{
}

Token Parser::Consume(TokenType type, const std::string& errorMessage)
{
	if (Check(type))
	{
		return Advance();
	}
	return Error(Peek(), errorMessage);
}

Token Parser::Error(const Token& token, const std::string& errorMessage)
{
	if (token.type == TokenType::END_OF_FILE)
	{
		Lox::GetInstance().Error(token.line, token.column, "at end: %s", errorMessage.c_str());
	}
	else
	{
		Lox::GetInstance().Error(token.line, token.column, "at '%s': %s", token.lexeme.c_str(), errorMessage.c_str());
	}
	error = true;
	Synchronize();
	return Token();
}

void Parser::Synchronize()
{
	// Consume tokens until reaching a statement boundary
	if (!IsAtEnd()) Advance();
	while (!IsAtEnd())
	{
		if (Previous().type == TokenType::SEMICOLON) return;
		switch (Peek().type)
		{
		case TokenType::CLASS:
		case TokenType::FUN:
		case TokenType::VAR:
		case TokenType::FOR:
		case TokenType::IF:
		case TokenType::WHILE:
		case TokenType::PRINT:
		case TokenType::RETURN:
			return;
		default:
			break;
		}
		Advance();
	}
}

ExprPtr Parser::Assignment()
{
	ExprPtr expr = Comma();
	if (Match(TokenType::EQUAL))
	{
		Token equal = Previous();
		ExprPtr value = Assignment();

		if (Variable* var =	dynamic_cast<Variable*>(expr.get()))
		{
			expr = Assign::Create(var->name, value);
			return expr;
		}
		Lox::GetInstance().RuntimeError(equal.line, equal.column, "Invalid assignment target.");
		return nullptr;
	}
	return expr;
}

ExprPtr Parser::Comma()
{
	ExprPtr expr = Ternary();

	while (Match(TokenType::COMMA))
	{
		Token op = Previous();
		ExprPtr right = Ternary();
		expr = Binary::Create(expr, op, right);
	}

	return expr;
}

ExprPtr Parser::Ternary()
{
	if (Match(TokenType::QUESTION))
	{
		Error(Previous(), "Expect expression before '?'.");
		return nullptr;
	}

	ExprPtr expr = Equality();

	if (Match(TokenType::QUESTION))
	{
		Token opLeft = Previous();
		ExprPtr middle = Comma();
		Consume(TokenType::COLON, "Expect ':' after expression.");
		Token opRight = Previous();
		ExprPtr right = Ternary();
		expr = Ternary::Create(expr, opLeft, middle, opRight, right);
	}

	return expr;
}

ExprPtr Parser::Equality()
{
	if (Match(TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL))
	{
		Error(Previous(), "Expect expression before equality operator.");
		return nullptr;
	}

	ExprPtr expr = Comparsion();

	while (Match(TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL))
	{
		Token op = Previous();
		ExprPtr right = Comparsion();
		expr = Binary::Create(expr, op, right);
	}
	
	return expr;
}

ExprPtr Parser::Comparsion()
{
	if (Match(TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL))
	{
		Error(Previous(), "Expect expression before comparison operator.");
		return nullptr;
	}

	ExprPtr expr = Term();

	while (Match(TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL))
	{
		Token op = Previous();
		ExprPtr right = Term();
		expr = Binary::Create(expr, op, right);
	}

	return expr;
}

ExprPtr Parser::Term()
{
	ExprPtr expr = Factor();

	while (Match(TokenType::MINUS, TokenType::PLUS))
	{
		Token op = Previous();
		ExprPtr right = Factor();
		expr = Binary::Create(expr, op, right);
	}

	return expr;
}

ExprPtr Parser::Factor()
{
	if (Match(TokenType::SLASH, TokenType::STAR))
	{
		Error(Previous(), "Expect expression before factor operator.");
		return nullptr;
	}

	ExprPtr expr = Unary();

	while (Match(TokenType::SLASH, TokenType::STAR))
	{
		Token op = Previous();
		ExprPtr right = Unary();
		expr = Binary::Create(expr, op, right);
	}

	return expr;
}

ExprPtr Parser::Unary()
{
	if (Match(TokenType::BANG, TokenType::MINUS))
	{
		Token op = Previous();
		ExprPtr right = Unary();
		return Unary::Create(op, right);
	}

	return Primary();
}

ExprPtr Parser::Expression()
{
	return Assignment();
}

ExprPtr Parser::Primary()
{
	if (Match(TokenType::FALSE, TokenType::TRUE, TokenType::NIL, TokenType::NUMBER, TokenType::STRING))
	{
		return Literal::Create(Previous());
	}

	if (Match(TokenType::LEFT_PAREN))
	{
		ExprPtr expr = Expression();
		Consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
		return expr; // Should wrap in Grouping, omitted for brevity
	}

	if (Match(TokenType::IDENTIFIER))
	{
		return Variable::Create(Previous());
	}

	Error(Peek(), "Expect expression.");
	return nullptr;
}

ExprPtr Parser::ParseExpr()
{
	return Expression();
}

std::vector<StatPtr> Parser::Parse()
{
	std::vector<StatPtr> statments;
	while (!IsAtEnd())
	{
		statments.push_back(Declaration());
	}
	return statments;
}

StatPtr Parser::Statment()
{
	if (Match(TokenType::PRINT))
	{
		return PrintStatement();
	}
	else if (Match(TokenType::LEFT_BRACE))
	{
		return BlockStatement();
	}
	return ExpressionStatment();
}

StatPtr Parser::Declaration()
{
	if (Match(TokenType::VAR))
	{
		return VarDeclaration();
	}
	else
	{
		StatPtr statment = Statment();
		return statment;
	}
}

StatPtr Parser::VarDeclaration()
{
	Consume(TokenType::IDENTIFIER, "Expect variable name.");
	Token name = Previous();
	ExprPtr initializer = nullptr;
	if (Match(TokenType::EQUAL))
	{
		initializer = Assignment();
	}
	Consume(TokenType::SEMICOLON, "Expect ';' after variable declaration.");
	return Var::Create(name, initializer);
}

StatPtr Parser::PrintStatement()
{
	ExprPtr expr = Assignment();
	Consume(TokenType::SEMICOLON, "Expect ';' after value.");
	return Print::Create(expr);
}

StatPtr Parser::BlockStatement()
{
	std::vector<StatPtr> statements;
	while (!Check(TokenType::RIGHT_BRACE) && !IsAtEnd())
	{
		statements.push_back(Declaration());
	}
	Consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
	return Block::Create(statements);
}

StatPtr Parser::ExpressionStatment()
{
	ExprPtr expr = Assignment();
	Consume(TokenType::SEMICOLON, "Expect ';' after value.");
	return Expression::Create(expr);
}