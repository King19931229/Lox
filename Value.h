#pragma once
#include <memory>
#include <string>
#include <stdexcept>
#include <iostream>
#include "Lox.h" // ���� Lox ���󱨸�ӿ�

enum ValueType
{
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_STRING,
	TYPE_BOOL,
	TYPE_NIL,
	TYPE_ERROR // ��������������
};

struct Value;
typedef std::shared_ptr<Value> ValuePtr;

// ���� Value �ṹ��
struct Value
{
	ValueType type;
	virtual ~Value() = default;

	// ����ת�����ڶ��ڲ�֧�ֵĲ������� ErrorValue���������׳��쳣
	virtual operator int() const { Lox::GetInstance().RuntimeError("Invalid conversion to int."); return 0; }
	virtual operator float() const { Lox::GetInstance().RuntimeError("Invalid conversion to float."); return 0.0f; }
	virtual operator bool() const { Lox::GetInstance().RuntimeError("Invalid conversion to bool."); return false; }
	virtual operator std::string() const { Lox::GetInstance().RuntimeError("Invalid conversion to string."); return ""; }
};

// --- ���� Value ���Ͷ��� ---

struct ErrorValue : public Value
{
	std::string message;
	static ValuePtr Create(const std::string& inMessage)
	{
		auto val = std::make_shared<ErrorValue>();
		val->type = TYPE_ERROR;
		val->message = inMessage;
		return val;
	}

	operator bool() const override { return false; } // ����ֵ�� falsey
	operator std::string() const override { return message; }
};

struct IntValue : public Value
{
	int value = 0;
	static ValuePtr Create(int inValue)
	{
		auto val = std::make_shared<IntValue>();
		val->type = TYPE_INT;
		val->value = inValue;
		return val;
	}

	operator int() const override { return value; }
	operator float() const override { return static_cast<float>(value); }
	operator bool() const override { return value != 0; }
	operator std::string() const override { return std::to_string(value); }
};

struct FloatValue : public Value
{
	float value = 0;
	static ValuePtr Create(float inValue)
	{
		auto val = std::make_shared<FloatValue>();
		val->type = TYPE_FLOAT;
		val->value = inValue;
		return val;
	}

	operator int() const override { return static_cast<int>(value); }
	operator float() const override { return value; }
	operator bool() const override { return value != 0.0f; }
	operator std::string() const override { return std::to_string(value); }
};

struct StringValue : public Value
{
	std::string value;
	static ValuePtr Create(const std::string& inValue)
	{
		auto val = std::make_shared<StringValue>();
		val->type = TYPE_STRING;
		val->value = inValue;
		return val;
	}

	operator bool() const override { return !value.empty(); }
	operator std::string() const override { return value; }
};

struct BoolValue : public Value
{
	bool value = false;
	static ValuePtr Create(bool inValue)
	{
		auto val = std::make_shared<BoolValue>();
		val->type = TYPE_BOOL;
		val->value = inValue;
		return val;
	}

	operator bool() const override { return value; }
	operator std::string() const override { return value ? "true" : "false"; }
};

struct NilValue : public Value
{
	NilValue() { type = TYPE_NIL; }
	static ValuePtr Create()
	{
		return std::make_shared<NilValue>();
	}

	operator bool() const override { return false; }
	operator std::string() const override { return "nil"; }
};

// --- ���������� ---

// �����꣬�����ڲ���ǰ��鲢���ݴ���
#define PROPAGATE_ERROR(left, right) \
    if (left->type == TYPE_ERROR) return left; \
    if (right->type == TYPE_ERROR) return right;

inline ValuePtr operator-(const ValuePtr& val)
{
	if (val->type == TYPE_ERROR) return val;
	if (val->type == TYPE_INT) return IntValue::Create(-static_cast<int>(*val));
	if (val->type == TYPE_FLOAT) return FloatValue::Create(-static_cast<float>(*val));

	Lox::GetInstance().RuntimeError("Operand must be a number for unary minus.");
	return ErrorValue::Create("Operand must be a number for unary minus.");
}

inline ValuePtr operator!(const ValuePtr& val)
{
	if (val->type == TYPE_ERROR) return val;
	return BoolValue::Create(!static_cast<bool>(*val));
}

#define DEFINE_ARITHMETIC_OPERATOR(op, op_name) \
inline ValuePtr operator op(const ValuePtr& left, const ValuePtr& right) \
{ \
    PROPAGATE_ERROR(left, right); \
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT)) \
    { \
        if (left->type == TYPE_INT && right->type == TYPE_INT) \
            return IntValue::Create(static_cast<int>(*left) op static_cast<int>(*right)); \
        return FloatValue::Create(static_cast<float>(*left) op static_cast<float>(*right)); \
    } \
	Lox::GetInstance().RuntimeError("Operands must be numbers for " op_name "."); \
	return ErrorValue::Create("Operands must be numbers for " op_name "."); \
}

DEFINE_ARITHMETIC_OPERATOR(-, "subtraction")
DEFINE_ARITHMETIC_OPERATOR(*, "multiplication")

inline ValuePtr operator/(const ValuePtr& left, const ValuePtr& right)
{
	PROPAGATE_ERROR(left, right);
	if ((left->type != TYPE_INT && left->type != TYPE_FLOAT) || (right->type != TYPE_INT && right->type != TYPE_FLOAT))
	{
		Lox::GetInstance().RuntimeError("Operands must be numbers for division.");
		return ErrorValue::Create("Operands must be numbers for division.");
	}
	if ((right->type == TYPE_INT && static_cast<int>(*right) == 0) || (right->type == TYPE_FLOAT && static_cast<float>(*right) == 0.0f))
	{
		Lox::GetInstance().RuntimeError("Division by zero.");
		return ErrorValue::Create("Division by zero.");
	}
	if (left->type == TYPE_INT && right->type == TYPE_INT)
		return IntValue::Create(static_cast<int>(*left) / static_cast<int>(*right));
	return FloatValue::Create(static_cast<float>(*left) / static_cast<float>(*right));
}

inline ValuePtr operator+(const ValuePtr& left, const ValuePtr& right)
{
	PROPAGATE_ERROR(left, right);
	if (left->type == TYPE_STRING && right->type == TYPE_STRING)
		return StringValue::Create(static_cast<std::string>(*left) + static_cast<std::string>(*right));
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT))
	{
		if (left->type == TYPE_INT && right->type == TYPE_INT)
			return IntValue::Create(static_cast<int>(*left) + static_cast<int>(*right));
		return FloatValue::Create(static_cast<float>(*left) + static_cast<float>(*right));
	}
	Lox::GetInstance().RuntimeError("Operands must be two numbers or two strings for '+'.");
	return ErrorValue::Create("Operands must be two numbers or two strings for '+'.");
}

#define DEFINE_COMPARISON_OPERATOR(op, op_name) \
inline ValuePtr operator op(const ValuePtr& left, const ValuePtr& right) \
{ \
    PROPAGATE_ERROR(left, right); \
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT)) \
		return BoolValue::Create(static_cast<float>(*left) op static_cast<float>(*right)); \
	Lox::GetInstance().RuntimeError("Operands must be numbers for " op_name "."); \
	return ErrorValue::Create("Operands must be numbers for " op_name "."); \
}

DEFINE_COMPARISON_OPERATOR(< , "comparison")
DEFINE_COMPARISON_OPERATOR(>, "comparison")
DEFINE_COMPARISON_OPERATOR(<=, "comparison")
DEFINE_COMPARISON_OPERATOR(>=, "comparison")

inline bool IsEqual(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type != right->type)
	{
		return false;
	}
	switch (left->type)
	{
		case TYPE_NIL:    return true;
		case TYPE_INT:    return static_cast<int>(*left) == static_cast<int>(*right);
		case TYPE_FLOAT:  return static_cast<float>(*left) == static_cast<float>(*right);
		case TYPE_BOOL:   return static_cast<bool>(*left) == static_cast<bool>(*right);
		case TYPE_STRING: return static_cast<std::string>(*left) == static_cast<std::string>(*right);
		default:          return false;
	}
}

inline ValuePtr operator==(const ValuePtr& left, const ValuePtr& right)
{
	PROPAGATE_ERROR(left, right);
	return BoolValue::Create(IsEqual(left, right));
}

inline ValuePtr operator!=(const ValuePtr& left, const ValuePtr& right)
{
	PROPAGATE_ERROR(left, right);
	return BoolValue::Create(!IsEqual(left, right));
}