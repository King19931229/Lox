#pragma once
#include "TokenType.h"
#include <memory>

struct Expr
{
	virtual ~Expr() = default;
};
typedef std::shared_ptr<Expr> ExprPtr;

struct Binary;
typedef std::shared_ptr<Binary> BinaryPtr;

struct Binary : public Expr
{
	ExprPtr left;
	Token op;
	ExprPtr right;
	
	Binary(const ExprPtr& inLeft, const Token& inOp, const ExprPtr& inRight)
	{
		left = inLeft;
		op = inOp;
		right = inRight;
	}
};

struct Grouping;
typedef std::shared_ptr<Grouping> GroupingPtr;

struct Grouping : public Expr
{
	ExprPtr expression;
	
	Grouping(const ExprPtr& inExpression)
	{
		expression = inExpression;
	}
};

struct Literal;
typedef std::shared_ptr<Literal> LiteralPtr;

struct Literal : public Expr
{
	Lexeme value;
	
	Literal(const Lexeme& inValue)
	{
		value = inValue;
	}
};

struct Unary;
typedef std::shared_ptr<Unary> UnaryPtr;

struct Unary : public Expr
{
	Token op;
	ExprPtr right;
	
	Unary(const Token& inOp, const ExprPtr& inRight)
	{
		op = inOp;
		right = inRight;
	}
};

