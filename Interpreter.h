#pragma once
#include "Value.h"
#include "Expr.h"
#include "Stat.h"
#include "TokenType.h"
#include "Environment.h"
#include <vector>
#include <stdexcept>

class Interpreter : public ExprVisitor<ValuePtr>, public StatVisitor<bool>
{
public:
	Interpreter();
	~Interpreter();
	ValuePtr InterpretExpr(const ExprPtr& expr);
	void Interpret(const std::vector<StatPtr>& statements);
	std::string Stringify(ValuePtr value);
	bool Trueify(ValuePtr value);
protected:
	friend struct LoxLambda;
	friend struct LoxFunction;
	Environment* globalEnvironment = nullptr;
	Environment* environment = nullptr;

	ValuePtr CallFunction(const LoxFunction* function, const std::vector<ValuePtr>& arguments);
	ValuePtr CallLambda(const LoxLambda* lambda, const std::vector<ValuePtr>& arguments);
	void ExecuteBlock(const std::vector<StatPtr>& statements, Environment* newEnv);

	ValuePtr Evaluate(const ExprPtr& expr);
	void Execute(const StatPtr& stat);	

	virtual ValuePtr DoVisitTernaryExpr(const Ternary* expr) override;

	virtual ValuePtr DoVisitBinaryExpr(const Binary* expr) override;

	virtual ValuePtr DoVisitGroupingExpr(const Grouping* expr) override;

	virtual ValuePtr DoVisitLiteralExpr(const Literal* expr) override;

	virtual ValuePtr DoVisitUnaryExpr(const Unary* expr) override;

	virtual ValuePtr DoVisitVariableExpr(const Variable* expr) override;

	virtual ValuePtr DoVisitAssignExpr(const Assign* expr) override;

	virtual ValuePtr DoVisitLogicalExpr(const Logical* expr) override;

	virtual ValuePtr DoVisitCallExpr(const Call* expr) override;

	virtual ValuePtr DoVisitLambdaExpr(const Lambda* expr) override;

	virtual bool DoVisitExpressionStat(const Expression* stat) override;

	virtual bool DoVisitPrintStat(const Print* stat) override;

	virtual bool DoVisitVarStat(const Var* stat) override;

	virtual bool DoVisitBlockStat(const Block* stat) override;

	virtual bool DoVisitIfStat(const If* stat) override;

	virtual bool DoVisitFunctionStat(const Function* stat) override;

	virtual bool DoVisitReturnStat(const Return* stat) override;
};