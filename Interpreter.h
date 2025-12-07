#pragma once
#include "Value.h"
#include "Expr.h"
#include "Stat.h"
#include "TokenType.h"
#include "Environment.h"
#include <vector>
#include <stdexcept>

class Interpreter : public ExprVisitor<ValuePtr>, public StatVisitor<bool>
{
public:
	Interpreter();
	~Interpreter();
	ValuePtr InterpretExpr(const ExprPtr& expr);
	void Interpret(const std::vector<StatPtr>& statements);
	void Resolve(const Expr* expr, int depth);
	std::string Stringify(ValuePtr value);
	bool Trueify(ValuePtr value);
	void SetResolver(const class Resolver* resolver);
protected:
	friend struct LoxLambda;
	friend struct LoxFunction;
	friend struct LoxGetter;

	EnvironmentPtr globalEnvironment = nullptr;
	EnvironmentPtr environment = nullptr;
	const class Resolver* resolver = nullptr;

	enum LoopControl
	{
		LOOP_CONTINUE,
		LOOP_BREAK,
		LOOP_NONE
	};
	LoopControl loopControl = LOOP_NONE;

	std::unordered_map<const Expr*, int> locals;

	ValuePtr CallFunction(const LoxFunction* function, const std::vector<ValuePtr>& arguments);
	ValuePtr CallLambda(const LoxLambda* lambda, const std::vector<ValuePtr>& arguments);
	ValuePtr CallGetter(const LoxGetter* getter);

	ValuePtr Evaluate(const ExprPtr& expr);
	void Execute(const StatPtr& stat);
	void ExecuteBlock(const std::vector<StatPtr>& statements, EnvironmentPtr newEnv);

	ValuePtr LookUpVariable(const Token& name, const Expr* expr);

	virtual ValuePtr DoVisitTernaryExpr(const Ternary* expr) override;
	virtual ValuePtr DoVisitBinaryExpr(const Binary* expr) override;
	virtual ValuePtr DoVisitGroupingExpr(const Grouping* expr) override;
	virtual ValuePtr DoVisitLiteralExpr(const Literal* expr) override;
	virtual ValuePtr DoVisitUnaryExpr(const Unary* expr) override;
	virtual ValuePtr DoVisitVariableExpr(const Variable* expr) override;
	virtual ValuePtr DoVisitAssignExpr(const Assign* expr) override;
	virtual ValuePtr DoVisitLogicalExpr(const Logical* expr) override;
	virtual ValuePtr DoVisitCallExpr(const Call* expr) override;
	virtual ValuePtr DoVisitLambdaExpr(const Lambda* expr) override;
	virtual ValuePtr DoVisitGetExpr(const Get* expr) override;
	virtual ValuePtr DoVisitSetExpr(const Set* expr) override;
	virtual ValuePtr DoVisitThisExpr(const This* expr) override;
	virtual ValuePtr DoVisitSuperExpr(const Super* expr) override;

	virtual bool DoVisitExpressionStat(const Expression* stat) override;
	virtual bool DoVisitPrintStat(const Print* stat) override;
	virtual bool DoVisitVarStat(const Var* stat) override;
	virtual bool DoVisitBlockStat(const Block* stat) override;
	virtual bool DoVisitIfStat(const If* stat) override;
	virtual bool DoVisitWhileStat(const While* stat) override;
	virtual bool DoVisitBreakStat(const Break* stat) override;
	virtual bool DoVisitFunctionStat(const Function* stat) override;
	virtual bool DoVisitGetterStat(const Getter* stat) override;
	virtual bool DoVisitReturnStat(const Return* stat) override;
	virtual bool DoVisitClassStat(const Class* stat) override;
};