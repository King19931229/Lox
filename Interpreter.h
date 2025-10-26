#pragma once
#include "Value.h"
#include "Expr.h"
#include "TokenType.h"
#include <stdexcept>

class Interpreter : public Visitor<ValuePtr>
{
public:
	ValuePtr Interpret(const ExprPtr& expr)
	{
			if (!expr) return NilValue::Create();
			return Visit(expr.get());
	}
protected:
	ValuePtr Evaluate(const ExprPtr& expr)
	{
		if (!expr) return NilValue::Create();
		return Visit(expr.get());
	}

	virtual ValuePtr DoVisitTernaryExpr(const Ternary* expr) override
	{
		ValuePtr condition = Evaluate(expr->left);
		if (static_cast<bool>(*condition))
		{
			return Evaluate(expr->middle);
		}
		else
		{
			return Evaluate(expr->right);
		}
	}

	virtual ValuePtr DoVisitBinaryExpr(const Binary* expr) override
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

	virtual ValuePtr DoVisitGroupingExpr(const Grouping* expr) override
	{
		return Evaluate(expr->expression);
	}

	virtual ValuePtr DoVisitLiteralExpr(const Literal* expr) override
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

	virtual ValuePtr DoVisitUnaryExpr(const Unary* expr) override
	{
		ValuePtr right = Evaluate(expr->right);

		switch (expr->op.type)
		{
			case TokenType::BANG:  return !right;
			case TokenType::MINUS: return -right;
		}

		throw std::runtime_error("Interpreter error: Unknown unary operator.");
	}
};