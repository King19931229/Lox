#include "Interpreter.h"
#include "Environment.h"
#include <stdexcept>

Interpreter::Interpreter()
	: environment(new Environment())
{
}

Interpreter::~Interpreter()
{
	delete environment;
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

ValuePtr Interpreter::DoVisitVariableExpr(const Variable* expr)
{
	return environment->Get(expr->name.lexeme);
}

ValuePtr Interpreter::DoVisitAssignExpr(const Assign* expr)
{
	ValuePtr value = Evaluate(expr->value);
	environment->Assign(expr->name.lexeme, value, expr->name.line, expr->name.column);
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

bool Interpreter::DoVisitExpressionStat(const Expression* stat)
{
	Evaluate(stat->expression);
	return true;
}

bool Interpreter::DoVisitPrintStat(const Print* stat)
{
	Evaluate(stat->expression);
	std::cout << Stringify(Evaluate(stat->expression)) << std::endl;
	return true;
}

bool Interpreter::DoVisitVarStat(const Var* stat)
{
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
		Execute(stat);
	}
	environment = oldEnv;
}

bool Interpreter::DoVisitBlockStat(const Block* stat)
{
	Environment* newEnv = new Environment(environment);
	ExecuteBlock(stat->statements, newEnv);
	delete newEnv;
	return true;
}

bool Interpreter::DoVisitIfStat(const If* stat)
{
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