#pragma once
#include "TokenType.h"
#include <memory>

struct Expr
{
	virtual ~Expr() = default;
};
typedef std::shared_ptr<Expr> ExprPtr;

struct Ternary;
typedef std::shared_ptr<Ternary> TernaryPtr;

struct Ternary : public Expr
{
	ExprPtr left;
	Token opLeft;
	ExprPtr middle;
	Token opRight;
	ExprPtr right;
	
	Ternary(const ExprPtr& inLeft, const Token& inOpLeft, const ExprPtr& inMiddle, const Token& inOpRight, const ExprPtr& inRight)
	{
		this->left = inLeft;
		this->opLeft = inOpLeft;
		this->middle = inMiddle;
		this->opRight = inOpRight;
		this->right = inRight;
	}
	
	static TernaryPtr Create(const ExprPtr& inLeft, const Token& inOpLeft, const ExprPtr& inMiddle, const Token& inOpRight, const ExprPtr& inRight)
	{
		return std::make_shared<Ternary>(inLeft, inOpLeft, inMiddle, inOpRight, inRight);
	}
};
struct Binary;
typedef std::shared_ptr<Binary> BinaryPtr;

struct Binary : public Expr
{
	ExprPtr left;
	Token op;
	ExprPtr right;
	
	Binary(const ExprPtr& inLeft, const Token& inOp, const ExprPtr& inRight)
	{
		this->left = inLeft;
		this->op = inOp;
		this->right = inRight;
	}
	
	static BinaryPtr Create(const ExprPtr& inLeft, const Token& inOp, const ExprPtr& inRight)
	{
		return std::make_shared<Binary>(inLeft, inOp, inRight);
	}
};
struct Grouping;
typedef std::shared_ptr<Grouping> GroupingPtr;

struct Grouping : public Expr
{
	ExprPtr expression;
	
	Grouping(const ExprPtr& inExpression)
	{
		this->expression = inExpression;
	}
	
	static GroupingPtr Create(const ExprPtr& inExpression)
	{
		return std::make_shared<Grouping>(inExpression);
	}
};
struct Literal;
typedef std::shared_ptr<Literal> LiteralPtr;

struct Literal : public Expr
{
	Lexeme value;
	
	Literal(const Lexeme& inValue)
	{
		this->value = inValue;
	}
	
	static LiteralPtr Create(const Lexeme& inValue)
	{
		return std::make_shared<Literal>(inValue);
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
		this->op = inOp;
		this->right = inRight;
	}
	
	static UnaryPtr Create(const Token& inOp, const ExprPtr& inRight)
	{
		return std::make_shared<Unary>(inOp, inRight);
	}
};
