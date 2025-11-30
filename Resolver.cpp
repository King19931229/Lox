#include "Resolver.h"
#include "Interpreter.h"

Resolver::Resolver(class Interpreter* inInterpreter)
	: interpreter(inInterpreter)
{
	interpreter->SetResolver(this);
}

Resolver::~Resolver()
{
}

void Resolver::BeginScope()
{
	scopes.push_back(Scope());
}

void Resolver::EndScope()
{
	scopes.pop_back();
}

void Resolver::Declare(const Token& name)
{
	if (scopes.empty())
	{
		return;
	}
	Scope& scope = scopes.back();
	if (scope.count(name.lexeme))
	{
		Lox::GetInstance().SemanticError(name.line, name.column,
			"Variable '%s' already defined in this scope.", name.lexeme.c_str());
	}
	scope[name.lexeme] = false;
}

void Resolver::Define(const Token& name)
{
	if (scopes.empty())
	{
		return;
	}
	Scope& scope = *scopes.rbegin();
	scope[name.lexeme] = true;
}

void Resolver::Resolve(const std::vector<StatPtr>& statements)
{
	for (const auto& stat : statements)
	{
		Resolve(stat);
	}
}

void Resolver::Resolve(const StatPtr& statement)
{
	statement->Accept(*this);
}

void Resolver::Resolve(const ExprPtr& expression)
{
	expression->Accept(*this);
}

void Resolver::ResolveLocal(const Expr* expr, const Token& name)
{
	// Iterate from the innermost scope to the outermost.
	for (int32_t i = (int32_t)scopes.size() - 1; i >= 0; --i)
	{
		if (scopes[i].count(name.lexeme))
		{
			interpreter->Resolve(expr, (int32_t)scopes.size() - 1 - i);
			return;
		}
	}
}

void Resolver::ResolveFunction(const Stat* function, FunctionType type)
{
	auto enclosingFunction = currentFunctionType;
	currentFunctionType = type;
	BeginScope();
	const Function* funcStat = static_cast<const Function*>(function);
	for (const Token& param : funcStat->params)
	{
		Declare(param);
		Define(param);
	}
	Resolve(funcStat->body);
	EndScope();
	currentFunctionType = enclosingFunction;
}

void Resolver::ResolveGetter(const Stat* getter)
{
	// Getters have no parameters.
	auto enclosingFunction = currentFunctionType;
	currentFunctionType = FunctionType::METHOD;
	const Getter* getterStat = static_cast<const Getter*>(getter);
	BeginScope();
	Resolve(getterStat->body);
	EndScope();
	currentFunctionType = enclosingFunction;
}

bool Resolver::DoVisitVariableExpr(const Variable* expr)
{
	if (!scopes.empty())
	{
		Scope& currentScope = scopes.back();
		auto it = currentScope.find(expr->name.lexeme);
		if (it != currentScope.end() && it->second == false)
		{
			Lox::GetInstance().SemanticError(expr->name.line, expr->name.column,
				"Cannot read local variable '%s' in its own initializer.", expr->name.lexeme.c_str());
		}
	}

	ResolveLocal(expr, expr->name);

	return true;
}

bool Resolver::DoVisitAssignExpr(const Assign* expr)
{
	Resolve(expr->value);
	ResolveLocal(expr, expr->name);
	return true;
}

bool Resolver::DoVisitTernaryExpr(const Ternary* expr)
{
	Resolve(expr->left);
	Resolve(expr->middle);
	Resolve(expr->right);
	return true;
}

bool Resolver::DoVisitBinaryExpr(const Binary* expr)
{
	Resolve(expr->left);
	Resolve(expr->right);
	return true;
}

bool Resolver::DoVisitLogicalExpr(const Logical* expr)
{
	Resolve(expr->left);
	Resolve(expr->right);
	return true;
}

bool Resolver::DoVisitGroupingExpr(const Grouping* expr)
{
	Resolve(expr->expression);
	return true;
}

bool Resolver::DoVisitUnaryExpr(const Unary* expr)
{
	Resolve(expr->right);
	return true;
}

bool Resolver::DoVisitLiteralExpr(const Literal* expr)
{
	return true;
}

bool Resolver::DoVisitCallExpr(const Call* expr)
{
	Resolve(expr->callee);
	for (const ExprPtr& argument : expr->arguments)
	{
		Resolve(argument);
	}
	return true;
}

bool Resolver::DoVisitLambdaExpr(const Lambda* expr)
{
	auto enclosingFunction = currentFunctionType;
	currentFunctionType = FunctionType::FUNCTION;
	BeginScope();
	for (const Token& param : expr->params)
	{
		Declare(param);
		Define(param);
	}
	Resolve(expr->body);
	EndScope();
	currentFunctionType = enclosingFunction;
	return true;
}

bool Resolver::DoVisitGetExpr(const Get* expr)
{
	Resolve(expr->object);
	return true;
}

bool Resolver::DoVisitSetExpr(const Set* expr)
{
	Resolve(expr->value);
	Resolve(expr->object);
	return true;
}

bool Resolver::DoVisitThisExpr(const This* expr)
{
	if (currentClassType == ClassType::NONE)
	{
		Lox::GetInstance().SemanticError(expr->keyword.line, expr->keyword.column,
			"'this' cannot be used outside of a class.");
	}
	if (currentFunctionType == FunctionType::CLASS)
	{
		Lox::GetInstance().SemanticError(expr->keyword.line, expr->keyword.column,
			"'this' cannot be used in a class method.");
	}
	ResolveLocal(expr, expr->keyword);
	return true;
}

bool Resolver::DoVisitBlockStat(const Block* stat)
{
	BeginScope();
	Resolve(stat->statements);
	EndScope();
	return true;
}

bool Resolver::DoVisitVarStat(const Var* stat)
{
	Declare(stat->name);
	if (stat->initializer)
	{
		Resolve(stat->initializer);
	}
	Define(stat->name);
	return true;
}

bool Resolver::DoVisitPrintStat(const Print* stat)
{
	Resolve(stat->expression);
	return true;
}

bool Resolver::DoVisitExpressionStat(const Expression* stat)
{
	Resolve(stat->expression);
	return true;
}

bool Resolver::DoVisitIfStat(const If* stat)
{
	Resolve(stat->condition);
	Resolve(stat->thenBranch);
	if (stat->elseBranch)
	{
		Resolve(stat->elseBranch);
	}
	return true;
}

bool Resolver::DoVisitWhileStat(const While* stat)
{
	auto enclosingWhile = currentWhileType;
	Resolve(stat->condition);
	currentWhileType = WhileType::IN_WHILE;
	Resolve(stat->body);
	currentWhileType = enclosingWhile;
	return true;
}

bool Resolver::DoVisitFunctionStat(const Function* stat)
{
	Declare(stat->name);
	Define(stat->name);
	ResolveFunction(stat, FunctionType::FUNCTION);
	return true;
}

bool Resolver::DoVisitGetterStat(const Getter* stat)
{
	return true;
}

bool Resolver::DoVisitBreakStat(const Break* stat)
{
	if (currentWhileType == WhileType::NOT_IN_WHILE)
	{
		Lox::GetInstance().SemanticError(stat->keyword.line, stat->keyword.column,
			"'break' statement not within a loop.");
	}
	return true;
}

bool Resolver::DoVisitReturnStat(const Return* stat)
{
	if (currentFunctionType == FunctionType::NONE)
	{
		Lox::GetInstance().SemanticError(stat->keyword.line, stat->keyword.column,
			"'return' statement not within a function.");
	}
	if (currentFunctionType == FunctionType::INITIALIZER && stat->value)
	{
		Lox::GetInstance().SemanticError(stat->keyword.line, stat->keyword.column,
			"Cannot return a value from an initializer.");
	}
	if (stat->value)
	{
		Resolve(stat->value);
	}
	return true;
}

bool Resolver::DoVisitClassStat(const Class* stat)
{
	ClassType enclosingClass = currentClassType;
	currentClassType = ClassType::CLASS;
	Declare(stat->name);
	Define(stat->name);
	// Create a scope out of the method scopes to hold "this"
	scopes.push_back(Scope());
	Scope& scope = scopes.back();
	scope["this"] = true;
	for (const StatPtr& method : stat->methods)
	{
		Function* func = static_cast<Function*>(method.get());
		if (func->name.lexeme == "init")
		{
			ResolveFunction(method.get(), FunctionType::INITIALIZER);
		}
		else
		{
			ResolveFunction(method.get(), FunctionType::METHOD);
		}
	}
	for (const StatPtr& getter : stat->getters)
	{
		ResolveGetter(getter.get());
	}
	for (const StatPtr& method : stat->classMethods)
	{
		ResolveFunction(method.get(), FunctionType::CLASS);
	}
	scopes.pop_back();
	currentClassType = enclosingClass;
	return true;
}