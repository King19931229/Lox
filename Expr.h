#pragma once
#include <memory>
#include "TokenType.h"

struct Ternary;
struct Binary;
struct Grouping;
struct Literal;
struct Unary;
struct Expr;

struct IExprVisitor
{
	virtual ~IExprVisitor() = default;
	virtual void VisitTernaryExpr(const Ternary* Expr) = 0;
	virtual void VisitBinaryExpr(const Binary* Expr) = 0;
	virtual void VisitGroupingExpr(const Grouping* Expr) = 0;
	virtual void VisitLiteralExpr(const Literal* Expr) = 0;
	virtual void VisitUnaryExpr(const Unary* Expr) = 0;
};

template<typename R>
struct ExprVisitor : public IExprVisitor
{
	R result; // 用于存储访问结果
	
	R VisitExpr(const Expr* Expr);
	
	void VisitTernaryExpr(const Ternary* Expr) override { result = DoVisitTernaryExpr(Expr); }
	void VisitBinaryExpr(const Binary* Expr) override { result = DoVisitBinaryExpr(Expr); }
	void VisitGroupingExpr(const Grouping* Expr) override { result = DoVisitGroupingExpr(Expr); }
	void VisitLiteralExpr(const Literal* Expr) override { result = DoVisitLiteralExpr(Expr); }
	void VisitUnaryExpr(const Unary* Expr) override { result = DoVisitUnaryExpr(Expr); }
	
	protected:
	virtual R DoVisitTernaryExpr(const Ternary* Expr) = 0;
	virtual R DoVisitBinaryExpr(const Binary* Expr) = 0;
	virtual R DoVisitGroupingExpr(const Grouping* Expr) = 0;
	virtual R DoVisitLiteralExpr(const Literal* Expr) = 0;
	virtual R DoVisitUnaryExpr(const Unary* Expr) = 0;
};


struct Expr
{
	virtual ~Expr() = default;

	// Accept 方法现在接受 IExprVisitor
	virtual void Accept(IExprVisitor& visitor) const = 0;
};
typedef std::shared_ptr<Expr> ExprPtr;

// Visitor -> ExprVisitor
template<typename R>
R ExprVisitor<R>::VisitExpr(const Expr* expr)
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
	
	void Accept(IExprVisitor& visitor) const override
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
	
	void Accept(IExprVisitor& visitor) const override
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
	
	void Accept(IExprVisitor& visitor) const override
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
	
	void Accept(IExprVisitor& visitor) const override
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
	
	void Accept(IExprVisitor& visitor) const override
	{
		visitor.VisitUnaryExpr(this);
	}
};
