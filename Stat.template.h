#pragma once
#include <memory>
#include <vector>
#include "TokenType.h"

$(VISITOR_DEFINE_BODY)

struct Stat
{
	virtual ~Stat() = default;

	template <typename T>
	T* As() const
	{
		return static_cast<T*>(this);
	}

	template <typename T>
	T* As()
	{
		return static_cast<T*>(this);
	}

	// Accept 方法现在是非模板的，并且接受基接口的引用
	virtual void Accept(IStatVisitor& visitor) const = 0;
};

template<typename R>
R StatVisitor<R>::VisitStat(const Stat* stat)
{
	stat->Accept(*this);
	return result;
}

$(DEFINE_BODY)