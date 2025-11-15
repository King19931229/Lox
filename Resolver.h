#pragma once
#include "Value.h"
#include "Expr.h"
#include "Stat.h"
#include <unordered_map>

class Resolver : public ExprVisitor<bool>, public StatVisitor<bool>
{
protected:
	class Interpreter* interpreter;

	// Scope: variable name to whether it is defined
	typedef std::unordered_map<std::string, bool> Scope;

	enum WhileType
	{
		NOT_IN_WHILE,
		IN_WHILE
	};
	WhileType currentWhileType = NOT_IN_WHILE;

	enum FunctionType
	{
		NOT_IN_FUNCTION,
		IN_FUNCTION
	};
	FunctionType currentFunctionType = NOT_IN_FUNCTION;

	std::vector<Scope> scopes;

	void BeginScope();
	void EndScope();

	void Declare(const Token& name);
	void Define(const Token& name);

	void ResolveLocal(const Expr* expr, const Token& name);
public:
	Resolver(class Interpreter* inInterpreter);
	~Resolver();

	void Resolve(const std::vector<StatPtr>& statements);
	void Resolve(const StatPtr& statement);
	void Resolve(const ExprPtr& expression);

	virtual bool DoVisitTernaryExpr(const Ternary* expr) override;
	virtual bool DoVisitBinaryExpr(const Binary* expr) override;
	virtual bool DoVisitGroupingExpr(const Grouping* expr) override;
	virtual bool DoVisitLiteralExpr(const Literal* expr) override;
	virtual bool DoVisitUnaryExpr(const Unary* expr) override;
	virtual bool DoVisitVariableExpr(const Variable* expr) override;
	virtual bool DoVisitAssignExpr(const Assign* expr) override;
	virtual bool DoVisitLogicalExpr(const Logical* expr) override;
	virtual bool DoVisitCallExpr(const Call* expr) override;
	virtual bool DoVisitLambdaExpr(const Lambda* expr) override;

	virtual bool DoVisitExpressionStat(const Expression* stat) override;
	virtual bool DoVisitPrintStat(const Print* stat) override;
	virtual bool DoVisitVarStat(const Var* stat) override;
	virtual bool DoVisitBlockStat(const Block* stat) override;
	virtual bool DoVisitIfStat(const If* stat) override;
	virtual bool DoVisitWhileStat(const While* stat) override;
	virtual bool DoVisitBreakStat(const Break* stat) override;
	virtual bool DoVisitFunctionStat(const Function* stat) override;
	virtual bool DoVisitReturnStat(const Return* stat) override;
};