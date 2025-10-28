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
	R result; // ���ڴ洢���ʽ��
	
	R VisitStat(const Stat* Stat);
	
	
	protected:
};


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

