#pragma once
#include <memory>
#include "TokenType.h"

$(VISITOR_DEFINE_BODY)

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

$(EXPR_DEFINE_BODY)