#pragma once
#include <memory>
#include <vector>
#include "ForwardDeclaration.h"
#include "TokenType.h"

struct Ternary;
struct Binary;
struct Grouping;
struct Literal;
struct Unary;
struct Variable;
struct Assign;
struct Logical;
struct Call;
struct Lambda;
struct Expr;

struct IExprVisitor
{
	virtual ~IExprVisitor() = default;
	virtual void VisitTernaryExpr(const Ternary* Expr) = 0;
	virtual void VisitBinaryExpr(const Binary* Expr) = 0;
	virtual void VisitGroupingExpr(const Grouping* Expr) = 0;
	virtual void VisitLiteralExpr(const Literal* Expr) = 0;
	virtual void VisitUnaryExpr(const Unary* Expr) = 0;
	virtual void VisitVariableExpr(const Variable* Expr) = 0;
	virtual void VisitAssignExpr(const Assign* Expr) = 0;
	virtual void VisitLogicalExpr(const Logical* Expr) = 0;
	virtual void VisitCallExpr(const Call* Expr) = 0;
	virtual void VisitLambdaExpr(const Lambda* Expr) = 0;
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
	void VisitVariableExpr(const Variable* Expr) override { result = DoVisitVariableExpr(Expr); }
	void VisitAssignExpr(const Assign* Expr) override { result = DoVisitAssignExpr(Expr); }
	void VisitLogicalExpr(const Logical* Expr) override { result = DoVisitLogicalExpr(Expr); }
	void VisitCallExpr(const Call* Expr) override { result = DoVisitCallExpr(Expr); }
	void VisitLambdaExpr(const Lambda* Expr) override { result = DoVisitLambdaExpr(Expr); }
	
	protected:
	virtual R DoVisitTernaryExpr(const Ternary* Expr) = 0;
	virtual R DoVisitBinaryExpr(const Binary* Expr) = 0;
	virtual R DoVisitGroupingExpr(const Grouping* Expr) = 0;
	virtual R DoVisitLiteralExpr(const Literal* Expr) = 0;
	virtual R DoVisitUnaryExpr(const Unary* Expr) = 0;
	virtual R DoVisitVariableExpr(const Variable* Expr) = 0;
	virtual R DoVisitAssignExpr(const Assign* Expr) = 0;
	virtual R DoVisitLogicalExpr(const Logical* Expr) = 0;
	virtual R DoVisitCallExpr(const Call* Expr) = 0;
	virtual R DoVisitLambdaExpr(const Lambda* Expr) = 0;
};


struct Expr
{
	virtual ~Expr() = default;

	// Accept 方法现在接受 IExprVisitor
	virtual void Accept(IExprVisitor& visitor) const = 0;
};

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
struct Variable;
typedef std::shared_ptr<Variable> VariablePtr;

struct Variable : public Expr
{
	Token name;
	
	Variable(const Token& inName)
	{
		this->name = inName;
	}
	
	static VariablePtr Create(const Token& inName)
	{
		return std::make_shared<Variable>(inName);
	}
	
	void Accept(IExprVisitor& visitor) const override
	{
		visitor.VisitVariableExpr(this);
	}
};
struct Assign;
typedef std::shared_ptr<Assign> AssignPtr;

struct Assign : public Expr
{
	Token name;
	ExprPtr value;
	
	Assign(const Token& inName, const ExprPtr& inValue)
	{
		this->name = inName;
		this->value = inValue;
	}
	
	static AssignPtr Create(const Token& inName, const ExprPtr& inValue)
	{
		return std::make_shared<Assign>(inName, inValue);
	}
	
	void Accept(IExprVisitor& visitor) const override
	{
		visitor.VisitAssignExpr(this);
	}
};
struct Logical;
typedef std::shared_ptr<Logical> LogicalPtr;

struct Logical : public Expr
{
	ExprPtr left;
	Token op;
	ExprPtr right;
	
	Logical(const ExprPtr& inLeft, const Token& inOp, const ExprPtr& inRight)
	{
		this->left = inLeft;
		this->op = inOp;
		this->right = inRight;
	}
	
	static LogicalPtr Create(const ExprPtr& inLeft, const Token& inOp, const ExprPtr& inRight)
	{
		return std::make_shared<Logical>(inLeft, inOp, inRight);
	}
	
	void Accept(IExprVisitor& visitor) const override
	{
		visitor.VisitLogicalExpr(this);
	}
};
struct Call;
typedef std::shared_ptr<Call> CallPtr;

struct Call : public Expr
{
	ExprPtr callee;
	Token paren;
	std::vector<ExprPtr> arguments;
	
	Call(const ExprPtr& inCallee, const Token& inParen, const std::vector<ExprPtr>& inArguments)
	{
		this->callee = inCallee;
		this->paren = inParen;
		this->arguments = inArguments;
	}
	
	static CallPtr Create(const ExprPtr& inCallee, const Token& inParen, const std::vector<ExprPtr>& inArguments)
	{
		return std::make_shared<Call>(inCallee, inParen, inArguments);
	}
	
	void Accept(IExprVisitor& visitor) const override
	{
		visitor.VisitCallExpr(this);
	}
};
struct Lambda;
typedef std::shared_ptr<Lambda> LambdaPtr;

struct Lambda : public Expr
{
	Token keyword;
	std::vector<Token> params;
	std::vector<StatPtr> body;
	
	Lambda(const Token& inKeyword, const std::vector<Token>& inParams, const std::vector<StatPtr>& inBody)
	{
		this->keyword = inKeyword;
		this->params = inParams;
		this->body = inBody;
	}
	
	static LambdaPtr Create(const Token& inKeyword, const std::vector<Token>& inParams, const std::vector<StatPtr>& inBody)
	{
		return std::make_shared<Lambda>(inKeyword, inParams, inBody);
	}
	
	void Accept(IExprVisitor& visitor) const override
	{
		visitor.VisitLambdaExpr(this);
	}
};
