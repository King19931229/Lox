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
	ValuePtr InterpretExpr(const ExprPtr& expr);
	void Interpret(const std::vector<StatPtr>& statements);
protected:
	Environment environment;

	std::string Stringify(ValuePtr value);
	bool Trueify(ValuePtr value);

	ValuePtr Evaluate(const ExprPtr& expr);

	virtual ValuePtr DoVisitTernaryExpr(const Ternary* expr) override;

	virtual ValuePtr DoVisitBinaryExpr(const Binary* expr) override;

	virtual ValuePtr DoVisitGroupingExpr(const Grouping* expr) override;

	virtual ValuePtr DoVisitLiteralExpr(const Literal* expr) override;

	virtual ValuePtr DoVisitUnaryExpr(const Unary* expr) override;

	virtual ValuePtr DoVisitVariableExpr(const Variable* Expr) override;	

	virtual bool DoVisitExpressionStat(const Expression* Stat) override;

	virtual bool DoVisitPrintStat(const Print* Stat) override;

	virtual bool DoVisitVarStat(const Var* Stat) override;
};