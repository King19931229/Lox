#pragma once
#include <memory>
#include "TokenType.h"

struct Ternary;
struct Binary;
struct Grouping;
struct Literal;
struct Unary;
struct Expr;

struct IVisitor
{
	virtual ~IVisitor() = default;
	virtual void VisitTernaryExpr(const Ternary* expr) = 0;
	virtual void VisitBinaryExpr(const Binary* expr) = 0;
	virtual void VisitGroupingExpr(const Grouping* expr) = 0;
	virtual void VisitLiteralExpr(const Literal* expr) = 0;
	virtual void VisitUnaryExpr(const Unary* expr) = 0;
};

template<typename R>
struct Visitor : public IVisitor
{
	R result; // 用于存储访问结果
	
	R Visit(const Expr* expr);
	
	void VisitTernaryExpr(const Ternary* expr) override { result = DoVisitTernaryExpr(expr); }
	void VisitBinaryExpr(const Binary* expr) override { result = DoVisitBinaryExpr(expr); }
	void VisitGroupingExpr(const Grouping* expr) override { result = DoVisitGroupingExpr(expr); }
	void VisitLiteralExpr(const Literal* expr) override { result = DoVisitLiteralExpr(expr); }
	void VisitUnaryExpr(const Unary* expr) override { result = DoVisitUnaryExpr(expr); }
	
	protected:
	virtual R DoVisitTernaryExpr(const Ternary* expr) = 0;
	virtual R DoVisitBinaryExpr(const Binary* expr) = 0;
	virtual R DoVisitGroupingExpr(const Grouping* expr) = 0;
	virtual R DoVisitLiteralExpr(const Literal* expr) = 0;
	virtual R DoVisitUnaryExpr(const Unary* expr) = 0;
};


struct Expr
{
	virtual ~Expr() = default;

	// Accept 方法现在是非模板的，并且接受基接口的引用
	virtual void Accept(IVisitor& visitor) const = 0;
};
typedef std::shared_ptr<Expr> ExprPtr;

template<typename R>
R Visitor<R>::Visit(const Expr* expr)
{
	expr->Accept(*this);
	return result;
}

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
	
	void Accept(IVisitor& visitor) const override
	{
		visitor.VisitTernaryExpr(this);
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
	
	void Accept(IVisitor& visitor) const override
	{
		visitor.VisitBinaryExpr(this);
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
	
	void Accept(IVisitor& visitor) const override
	{
		visitor.VisitGroupingExpr(this);
	}
};
struct Literal;
typedef std::shared_ptr<Literal> LiteralPtr;

struct Literal : public Expr
{
	Token value;
	
	Literal(const Token& inValue)
	{
		this->value = inValue;
	}
	
	static LiteralPtr Create(const Token& inValue)
	{
		return std::make_shared<Literal>(inValue);
	}
	
	void Accept(IVisitor& visitor) const override
	{
		visitor.VisitLiteralExpr(this);
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
	
	void Accept(IVisitor& visitor) const override
	{
		visitor.VisitUnaryExpr(this);
	}
};
