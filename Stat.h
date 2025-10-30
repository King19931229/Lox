#pragma once
#include <memory>
#include "TokenType.h"

struct Expression;
struct Print;
struct Var;
struct Stat;

struct IStatVisitor
{
	virtual ~IStatVisitor() = default;
	virtual void VisitExpressionStat(const Expression* Stat) = 0;
	virtual void VisitPrintStat(const Print* Stat) = 0;
	virtual void VisitVarStat(const Var* Stat) = 0;
};

template<typename R>
struct StatVisitor : public IStatVisitor
{
	R result; // 用于存储访问结果
	
	R VisitStat(const Stat* Stat);
	
	void VisitExpressionStat(const Expression* Stat) override { result = DoVisitExpressionStat(Stat); }
	void VisitPrintStat(const Print* Stat) override { result = DoVisitPrintStat(Stat); }
	void VisitVarStat(const Var* Stat) override { result = DoVisitVarStat(Stat); }
	
	protected:
	virtual R DoVisitExpressionStat(const Expression* Stat) = 0;
	virtual R DoVisitPrintStat(const Print* Stat) = 0;
	virtual R DoVisitVarStat(const Var* Stat) = 0;
};


struct Stat
{
	virtual ~Stat() = default;

	// Accept 方法现在是非模板的，并且接受基接口的引用
	virtual void Accept(IStatVisitor& visitor) const = 0;
};
typedef std::shared_ptr<Stat> StatPtr;

template<typename R>
R StatVisitor<R>::VisitStat(const Stat* stat)
{
	stat->Accept(*this);
	return result;
}

struct Expression;
typedef std::shared_ptr<Expression> ExpressionPtr;

struct Expression : public Stat
{
	ExprPtr expression;
	
	Expression(const ExprPtr& inExpression)
	{
		this->expression = inExpression;
	}
	
	static ExpressionPtr Create(const ExprPtr& inExpression)
	{
		return std::make_shared<Expression>(inExpression);
	}
	
	void Accept(IStatVisitor& visitor) const override
	{
		visitor.VisitExpressionStat(this);
	}
};
struct Print;
typedef std::shared_ptr<Print> PrintPtr;

struct Print : public Stat
{
	ExprPtr expression;
	
	Print(const ExprPtr& inExpression)
	{
		this->expression = inExpression;
	}
	
	static PrintPtr Create(const ExprPtr& inExpression)
	{
		return std::make_shared<Print>(inExpression);
	}
	
	void Accept(IStatVisitor& visitor) const override
	{
		visitor.VisitPrintStat(this);
	}
};
struct Var;
typedef std::shared_ptr<Var> VarPtr;

struct Var : public Stat
{
	Token name;
	ExprPtr initializer;
	
	Var(const Token& inName, const ExprPtr& inInitializer)
	{
		this->name = inName;
		this->initializer = inInitializer;
	}
	
	static VarPtr Create(const Token& inName, const ExprPtr& inInitializer)
	{
		return std::make_shared<Var>(inName, inInitializer);
	}
	
	void Accept(IStatVisitor& visitor) const override
	{
		visitor.VisitVarStat(this);
	}
};
