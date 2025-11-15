#include "Interpreter.h"
#include "Environment.h"
#include "LoxCallable.h"
#include <assert.h>

#define LOOP_CONTRAL_FAST_RETURN if (loopControl != LOOP_NONE) { return true; }
#define ERROR_CONTRAL_FAST_RETURN if (Lox::GetInstance().HasRuntimeError()) { return true; }

Interpreter::Interpreter()
	: globalEnvironment(new Environment())
	, environment(globalEnvironment)
{
	struct Clock : public LoxCallable
	{
		Clock()
		{
			this->type = TYPE_CALLABLE;
		}
		int Arity() const override
		{
			return 0;
		}
		ValuePtr Call(Interpreter* interpreter, const std::vector<ValuePtr>& arguments) override
		{
			return FloatValue::Create(static_cast<float>(time(nullptr)));
		}
		operator std::string() const override
		{
			return "<native fn>";
		}
		static ValuePtr Create()
		{
			return std::make_shared<Clock>();
		}
	};
	globalEnvironment->Define("clock", Clock::Create());
}

Interpreter::~Interpreter()
{
	assert(environment == globalEnvironment && "Environment stack not balanced on Interpreter destruction.");
	delete globalEnvironment;
}

ValuePtr Interpreter::InterpretExpr(const ExprPtr& expr)
{
	if (!expr) return NilValue::Create();
	return VisitExpr(expr.get());
}

void Interpreter::Interpret(const std::vector<StatPtr>& statements)
{
	for(const StatPtr& stat : statements)
	{
		Execute(stat);
	}
}

std::string Interpreter::Stringify(ValuePtr value)
{
	return static_cast<std::string>(*value);
}

bool Interpreter::Trueify(ValuePtr value)
{
	return static_cast<bool>(*value);
}

ValuePtr Interpreter::Evaluate(const ExprPtr& expr)
{
	if (!expr) return NilValue::Create();
	return VisitExpr(expr.get());
}

void Interpreter::Execute(const StatPtr& stat)
{
	if (stat)
	{
		VisitStat(stat.get());
	}
}

ValuePtr Interpreter::DoVisitTernaryExpr(const Ternary* expr)
{
	ValuePtr condition = Evaluate(expr->left);
	if (Trueify(condition))
	{
		return Evaluate(expr->middle);
	}
	else
	{
		return Evaluate(expr->right);
	}
}

ValuePtr Interpreter::DoVisitBinaryExpr(const Binary* expr)
{
	ValuePtr left = Evaluate(expr->left);
	ValuePtr right = Evaluate(expr->right);

	switch (expr->op.type)
	{
			// 算术运算符
		case TokenType::MINUS:         return left - right;
		case TokenType::SLASH:         return left / right;
		case TokenType::STAR:          return left * right;
		case TokenType::PLUS:          return left + right;

			// 比较运算符
		case TokenType::GREATER:       return left > right;
		case TokenType::GREATER_EQUAL: return left >= right;
		case TokenType::LESS:          return left < right;
		case TokenType::LESS_EQUAL:    return left <= right;

			// 相等性运算符
		case TokenType::BANG_EQUAL:    return left != right;
		case TokenType::EQUAL_EQUAL:   return left == right;

			// 逗号运算符
		case TokenType::COMMA:
			return right;
	}

	// 补上行/列信息用于更精确的运行时错误定位
	Lox::GetInstance().RuntimeError(expr->op.line, expr->op.column, "Unknown binary operator.");
	return ErrorValue::Create("Unknown binary operator.");
}

ValuePtr Interpreter::DoVisitGroupingExpr(const Grouping* expr)
{
	return Evaluate(expr->expression);
}

ValuePtr Interpreter::DoVisitLiteralExpr(const Literal* expr)
{
	const Token& token = expr->value;

	switch (token.type)
	{
		case TokenType::TRUE:   return BoolValue::Create(true);
		case TokenType::FALSE:  return BoolValue::Create(false);
		case TokenType::NIL:    return NilValue::Create();
		case TokenType::NUMBER:
		{
			const std::string& lexeme = token.lexeme;
			if (lexeme.find('.') != std::string::npos)
			{
				return FloatValue::Create(std::stof(lexeme));
			}
			else
			{
				return IntValue::Create(std::stoi(lexeme));
			}
		}
		case TokenType::STRING:
			return StringValue::Create(token.lexeme);
		default:
			// 补上行/列信息用于更精确的运行时错误定位
			Lox::GetInstance().RuntimeError(token.line, token.column, "Unexpected literal type.");
			return ErrorValue::Create("Unexpected literal type.");
	}
}

ValuePtr Interpreter::DoVisitUnaryExpr(const Unary* expr)
{
	ValuePtr right = Evaluate(expr->right);

	switch (expr->op.type)
	{
		case TokenType::BANG:  return !right;
		case TokenType::MINUS: return -right;
	}

	Lox::GetInstance().RuntimeError(expr->op.line, expr->op.column, "Unknown unary operator.");
	return ErrorValue::Create("Unknown unary operator.");
}

ValuePtr Interpreter::LookUpVariable(const Token& name, const Expr* expr)
{
	auto it = locals.find(expr);
	if (it != locals.end())
	{
		int distance = it->second;
		return environment->GetAt(distance, name.lexeme, name.line, name.column);
	}
	else
	{
		return globalEnvironment->Get(name.lexeme, name.line, name.column);
	}
}

ValuePtr Interpreter::DoVisitVariableExpr(const Variable* expr)
{
	if (resolver)
	{
		return LookUpVariable(expr->name, expr);
	}
	else
	{
		return environment->Get(expr->name.lexeme);
	}
}

ValuePtr Interpreter::DoVisitAssignExpr(const Assign* expr)
{
	ValuePtr value = Evaluate(expr->value);
	if (resolver)
	{
		auto it = locals.find(expr);
		if (it != locals.end())
		{
			int distance = it->second;
			environment->AssignAt(distance, expr->name, value);
		}
		else
		{
			globalEnvironment->Assign(expr->name.lexeme, value, expr->name.line, expr->name.column);
		}
	}
	else
	{
		environment->Assign(expr->name.lexeme, value, expr->name.line, expr->name.column);
	}
	return value;
}

ValuePtr Interpreter::DoVisitLogicalExpr(const Logical* expr)
{
	ValuePtr left = Evaluate(expr->left);
	if (expr->op.type == TokenType::OR)
	{
		if (Trueify(left)) return left;
	}
	else // AND
	{
		if (!Trueify(left)) return left;
	}
	return Evaluate(expr->right);
}

ValuePtr Interpreter::DoVisitCallExpr(const Call* expr)
{
	ValuePtr callee = Evaluate(expr->callee);
	std::vector<ValuePtr> arguments;
	for (const ExprPtr& argumentExpr : expr->arguments)
	{
		arguments.push_back(Evaluate(argumentExpr));
	}
	
	LoxCallable* function = dynamic_cast<LoxCallable*>(callee.get());
	if (!function)
	{
		Lox::GetInstance().RuntimeError(expr->paren.line, expr->paren.column, "Can only call functions and classes.");
		return ErrorValue::Create("Can only call functions and classes.");
	}
	else
	{
		if (arguments.size() != function->Arity())
		{
			Lox::GetInstance().RuntimeError(expr->paren.line, expr->paren.column, "Argument count mismatch, expected %d but got %d.", function->Arity(), arguments.size());
			return ErrorValue::Create("Argument count mismatch.");
		}
		return function->Call(this, arguments);
	}
}

ValuePtr Interpreter::DoVisitLambdaExpr(const Lambda* expr)
{
	ValuePtr lambda = LoxLambda::Create(expr);
	dynamic_cast<LoxLambda*>(lambda.get())->closure = environment->Clone();
	return lambda;
}

bool Interpreter::DoVisitExpressionStat(const Expression* stat)
{
	ERROR_CONTRAL_FAST_RETURN;
	LOOP_CONTRAL_FAST_RETURN;

	Evaluate(stat->expression);
	return true;
}

bool Interpreter::DoVisitPrintStat(const Print* stat)
{
	ERROR_CONTRAL_FAST_RETURN;
	LOOP_CONTRAL_FAST_RETURN;

	Evaluate(stat->expression);
	std::cout << Stringify(Evaluate(stat->expression)) << std::endl;
	return true;
}

bool Interpreter::DoVisitVarStat(const Var* stat)
{
	ERROR_CONTRAL_FAST_RETURN;
	LOOP_CONTRAL_FAST_RETURN;

	if (stat->initializer)
	{
		environment->Define(stat->name.lexeme, Evaluate(stat->initializer));
	}
	else
	{
		environment->Define(stat->name.lexeme, ErrorValue::Create("Uninitialized variable."));
	}
	return true;
}

void Interpreter::ExecuteBlock(const std::vector<StatPtr>& statements, Environment* newEnv)
{
	Environment* oldEnv = environment;
	environment = newEnv;
	for (StatPtr stat : statements)
	{
		if (environment->HasReturnValue())
		{
			break;
		}
		Execute(stat);
	}
	environment = oldEnv;
}

ValuePtr LoxFunction::Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments)
{
	return interpreter->CallFunction(this, arguments);
}

ValuePtr Interpreter::CallFunction(const LoxFunction* function, const std::vector<ValuePtr>& arguments)
{
	Environment* functionEnv = new Environment(function->closure, true);
	for (size_t i = 0; i < function->declaration->params.size(); ++i)
	{
		functionEnv->Define(function->declaration->params[i].lexeme, arguments[i], function->declaration->params[i].line, function->declaration->params[i].column);
	}
	ExecuteBlock(function->declaration->body, functionEnv);

	ValuePtr returnValue = nullptr;
	if (functionEnv->HasReturnValue())
	{
		returnValue = functionEnv->GetReturnValue();
	}
	else
	{
		returnValue = NilValue::Create();
	}

	delete functionEnv;
	return returnValue;
}

ValuePtr LoxLambda::Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments)
{
	return interpreter->CallLambda(this, arguments);
}

ValuePtr Interpreter::CallLambda(const LoxLambda* lambda, const std::vector<ValuePtr>& arguments)
{
	Environment* lambdaEnv = new Environment(lambda->closure, true);
	for (size_t i = 0; i < lambda->declaration->params.size(); ++i)
	{
		lambdaEnv->Define(lambda->declaration->params[i].lexeme, arguments[i], lambda->declaration->params[i].line, lambda->declaration->params[i].column);
	}
	ExecuteBlock(lambda->declaration->body, lambdaEnv);
	ValuePtr returnValue = nullptr;
	if (lambdaEnv->HasReturnValue())
	{
		returnValue = lambdaEnv->GetReturnValue();
	}
	else
	{
		returnValue = NilValue::Create();
	}
	delete lambdaEnv;
	return returnValue;
}

bool Interpreter::DoVisitBlockStat(const Block* stat)
{
	ERROR_CONTRAL_FAST_RETURN;
	LOOP_CONTRAL_FAST_RETURN;

	Environment* newEnv = new Environment(environment);
	ExecuteBlock(stat->statements, newEnv);
	delete newEnv;
	return true;
}

bool Interpreter::DoVisitFunctionStat(const Function* stat)
{
	ValuePtr function = LoxFunction::Create(stat);
	environment->Define(stat->name.lexeme, function, stat->name.line, stat->name.column);
	dynamic_cast<LoxFunction*>(function.get())->closure = environment->Clone();
	return true;
}

bool Interpreter::DoVisitReturnStat(const Return* stat)
{
	ERROR_CONTRAL_FAST_RETURN;

	ValuePtr value = NilValue::Create();
	if (stat->value)
	{
		value = Evaluate(stat->value);
	}
	else // void return
	{
		value = NilValue::Create();
	}
	environment->SetReturnValue(value);
	return true;
}

bool Interpreter::DoVisitIfStat(const If* stat)
{
	ERROR_CONTRAL_FAST_RETURN;
	LOOP_CONTRAL_FAST_RETURN;

	ValuePtr value = Evaluate(stat->condition);
	if (Trueify(value))
	{
		Execute(stat->thenBranch);
	}
	else
	{
		Execute(stat->elseBranch);
	}
	return true;
}

bool Interpreter::DoVisitWhileStat(const While* stat)
{
	while (Trueify(Evaluate(stat->condition)))
	{
		environment->SetCurrentWhile(stat);
		Execute(stat->body);
		if (loopControl == LOOP_BREAK)
		{
			loopControl = LOOP_NONE;
			break;
		}
	}
	environment->SetCurrentWhile(nullptr);
	return true;
}

bool Interpreter::DoVisitBreakStat(const Break* stat)
{
	if (!environment->GetCurrentWhile())
	{
		Lox::GetInstance().RuntimeError(stat->keyword.line, stat->keyword.column, "Break statement not within a loop.");
		return true;
	}
	loopControl = LOOP_BREAK;
	return true;
 }

void Interpreter::Resolve(const Expr* expr, int depth)
{
	locals[expr] = depth;
}

void Interpreter::SetResolver(const class Resolver* resolver)
{
	this->resolver = resolver;
}