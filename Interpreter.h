#pragma once
#include "Value.h"
#include "Expr.h"
#include "Stat.h"
#include "TokenType.h"
#include <stdexcept>

class Interpreter : public ExprVisitor<ValuePtr>, public StatVisitor<bool>
{
public:
	ValuePtr Interpret(const ExprPtr& expr);

protected:
	std::string Stringify(ValuePtr value);

	ValuePtr Evaluate(const ExprPtr& expr);

	virtual ValuePtr DoVisitTernaryExpr(const Ternary* expr) override;

	virtual ValuePtr DoVisitBinaryExpr(const Binary* expr) override;

	virtual ValuePtr DoVisitGroupingExpr(const Grouping* expr) override;

	virtual ValuePtr DoVisitLiteralExpr(const Literal* expr) override;

	virtual ValuePtr DoVisitUnaryExpr(const Unary* expr) override;
};