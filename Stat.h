#pragma once
#include <memory>
#include "TokenType.h"

struct Stat;

struct IStatVisitor
{
	virtual ~IStatVisitor() = default;
};

template<typename R>
struct StatVisitor : public IStatVisitor
{
	R result; // 用于存储访问结果
	
	R VisitStat(const Stat* Stat);
	
	
	protected:
};


struct Stat
{
	virtual ~Stat() = default;

	// Accept 方法现在是非模板的，并且接受基接口的引用
	virtual void Accept(IStatVisitor& visitor) const = 0;
};
typedef std::shared_ptr<Stat> StatPtr;

template<typename R>
R StatVisitor<R>::VisitStat(const Stat* stat)
{
	stat->Accept(*this);
	return result;
}

