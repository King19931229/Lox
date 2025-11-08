#pragma once
#include <memory>
#include <vector>
#include "ForwardDeclaration.h"
#include "TokenType.h"

$(VISITOR_DEFINE_BODY)

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

$(DEFINE_BODY)