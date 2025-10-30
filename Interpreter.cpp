#include "Interpreter.h"
#include "Environment.h" // 从 .h 文件移动至此
#include <stdexcept>     // 从 .h 文件移动至此

ValuePtr Interpreter::InterpretExpr(const ExprPtr& expr)
{
	if (!expr) return NilValue::Create();
	return VisitExpr(expr.get());
}

void Interpreter::Interpret(const std::vector<StatPtr>& statements)
{
	for(const StatPtr& stat : statements)
	{
		VisitStat(stat.get());
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

	throw std::runtime_error("Interpreter error: Unknown binary operator.");
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
			throw std::runtime_error("Interpreter error: Unexpected literal type.");
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

	throw std::runtime_error("Interpreter error: Unknown unary operator.");
}

ValuePtr Interpreter::DoVisitVariableExpr(const Variable* Expr)
{
	return environment.Get(Expr->name.lexeme);
}

bool Interpreter::DoVisitExpressionStat(const Expression* Stat)
{
	Evaluate(Stat->expression);
	return true;
}

bool Interpreter::DoVisitPrintStat(const Print* Stat)
{
	Evaluate(Stat->expression);
	std::cout << Stringify(Evaluate(Stat->expression)) << std::endl;
	return true;
}

bool Interpreter::DoVisitVarStat(const Var* Stat)
{
	environment.Define(Stat->name.lexeme, Evaluate(Stat->initializer));
	return true;
}