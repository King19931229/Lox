#pragma once
#include <memory>
#include "TokenType.h"

$(VISITOR_DEFINE_BODY)

struct Expr
{
	virtual ~Expr() = default;

	// Accept ���������Ƿ�ģ��ģ����ҽ��ܻ��ӿڵ�����
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