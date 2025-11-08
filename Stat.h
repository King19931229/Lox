#pragma once
#include <memory>
#include <vector>
#include "TokenType.h"

struct Expression;
struct Print;
struct Var;
struct Block;
struct If;
struct Function;
struct Return;
struct Stat;

struct IStatVisitor
{
	virtual ~IStatVisitor() = default;
	virtual void VisitExpressionStat(const Expression* Stat) = 0;
	virtual void VisitPrintStat(const Print* Stat) = 0;
	virtual void VisitVarStat(const Var* Stat) = 0;
	virtual void VisitBlockStat(const Block* Stat) = 0;
	virtual void VisitIfStat(const If* Stat) = 0;
	virtual void VisitFunctionStat(const Function* Stat) = 0;
	virtual void VisitReturnStat(const Return* Stat) = 0;
};

template<typename R>
struct StatVisitor : public IStatVisitor
{
	R result; // 用于存储访问结果
	
	R VisitStat(const Stat* Stat);
	
	void VisitExpressionStat(const Expression* Stat) override { result = DoVisitExpressionStat(Stat); }
	void VisitPrintStat(const Print* Stat) override { result = DoVisitPrintStat(Stat); }
	void VisitVarStat(const Var* Stat) override { result = DoVisitVarStat(Stat); }
	void VisitBlockStat(const Block* Stat) override { result = DoVisitBlockStat(Stat); }
	void VisitIfStat(const If* Stat) override { result = DoVisitIfStat(Stat); }
	void VisitFunctionStat(const Function* Stat) override { result = DoVisitFunctionStat(Stat); }
	void VisitReturnStat(const Return* Stat) override { result = DoVisitReturnStat(Stat); }
	
	protected:
	virtual R DoVisitExpressionStat(const Expression* Stat) = 0;
	virtual R DoVisitPrintStat(const Print* Stat) = 0;
	virtual R DoVisitVarStat(const Var* Stat) = 0;
	virtual R DoVisitBlockStat(const Block* Stat) = 0;
	virtual R DoVisitIfStat(const If* Stat) = 0;
	virtual R DoVisitFunctionStat(const Function* Stat) = 0;
	virtual R DoVisitReturnStat(const Return* Stat) = 0;
};


struct Stat
{
	virtual ~Stat() = default;

	// Accept 方法现在是非模板的，并且接受基接口的引用
	virtual void Accept(IStatVisitor& visitor) const = 0;
};

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
struct Block;
typedef std::shared_ptr<Block> BlockPtr;

struct Block : public Stat
{
	std::vector<StatPtr> statements;
	
	Block(const std::vector<StatPtr>& inStatements)
	{
		this->statements = inStatements;
	}
	
	static BlockPtr Create(const std::vector<StatPtr>& inStatements)
	{
		return std::make_shared<Block>(inStatements);
	}
	
	void Accept(IStatVisitor& visitor) const override
	{
		visitor.VisitBlockStat(this);
	}
};
struct If;
typedef std::shared_ptr<If> IfPtr;

struct If : public Stat
{
	ExprPtr condition;
	StatPtr thenBranch;
	StatPtr elseBranch;
	
	If(const ExprPtr& inCondition, const StatPtr& inThenBranch, const StatPtr& inElseBranch)
	{
		this->condition = inCondition;
		this->thenBranch = inThenBranch;
		this->elseBranch = inElseBranch;
	}
	
	static IfPtr Create(const ExprPtr& inCondition, const StatPtr& inThenBranch, const StatPtr& inElseBranch)
	{
		return std::make_shared<If>(inCondition, inThenBranch, inElseBranch);
	}
	
	void Accept(IStatVisitor& visitor) const override
	{
		visitor.VisitIfStat(this);
	}
};
struct Function;
typedef std::shared_ptr<Function> FunctionPtr;

struct Function : public Stat
{
	Token name;
	std::vector<Token> params;
	std::vector<StatPtr> body;
	
	Function(const Token& inName, const std::vector<Token>& inParams, const std::vector<StatPtr>& inBody)
	{
		this->name = inName;
		this->params = inParams;
		this->body = inBody;
	}
	
	static FunctionPtr Create(const Token& inName, const std::vector<Token>& inParams, const std::vector<StatPtr>& inBody)
	{
		return std::make_shared<Function>(inName, inParams, inBody);
	}
	
	void Accept(IStatVisitor& visitor) const override
	{
		visitor.VisitFunctionStat(this);
	}
};
struct Return;
typedef std::shared_ptr<Return> ReturnPtr;

struct Return : public Stat
{
	Token keyword;
	ExprPtr value;
	
	Return(const Token& inKeyword, const ExprPtr& inValue)
	{
		this->keyword = inKeyword;
		this->value = inValue;
	}
	
	static ReturnPtr Create(const Token& inKeyword, const ExprPtr& inValue)
	{
		return std::make_shared<Return>(inKeyword, inValue);
	}
	
	void Accept(IStatVisitor& visitor) const override
	{
		visitor.VisitReturnStat(this);
	}
};
