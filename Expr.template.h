#pragma once
#include "TokenType.h"
#include <memory>

struct Expr
{
	virtual ~Expr() = default;
};
typedef std::shared_ptr<Expr> ExprPtr;

$(EXPR_DEFINE_BODY)