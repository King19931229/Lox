#pragma once
#include <memory>
#include "TokenType.h"

$(VISITOR_DEFINE_BODY)

struct Stat
{
	virtual ~Stat() = default;

	// Accept ���������Ƿ�ģ��ģ����ҽ��ܻ��ӿڵ�����
	virtual void Accept(IStatVisitor& visitor) const = 0;
};
typedef std::shared_ptr<Stat> StatPtr;

template<typename R>
R StatVisitor<R>::VisitStat(const Stat* stat)
{
	stat->Accept(*this);
	return result;
}

$(DEFINE_BODY)