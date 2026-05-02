#pragma once
#include <memory>
#include <string>
#include <stdexcept>
#include <iostream>
#include "Lox.h" // Lox runtime error reporting interface

enum ValueType
{
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_STRING,
	TYPE_BOOL,
	TYPE_NIL,
	TYPE_CALLABLE,
	TYPE_CLASS,
	TYPE_INSTANCE,
	// For upvalues captured by closures. Only useable in VM.
	TYPE_UPVALUE,
	TYPE_ERROR // Error type
};

struct Value;
typedef std::shared_ptr<Value> ValuePtr;

// Base Value structure
struct Value : public std::enable_shared_from_this<Value>
{
	ValueType type;
	virtual ~Value() = default;

	Value() : type(TYPE_ERROR) {}

	// Disable copy and move semantics to prevent accidental copying of Values
	Value& operator=(const Value& other) = delete;
	Value& operator=(Value&& other) = delete;
	Value(const Value& other) = delete;
	Value(Value&& other) = delete;

	// Invalid conversions report runtime errors and return default values.
	virtual operator int() const { Lox::GetInstance().RuntimeError("Invalid conversion to int."); return 0; }
	virtual operator float() const { Lox::GetInstance().RuntimeError("Invalid conversion to float."); return 0.0f; }
	virtual operator bool() const { Lox::GetInstance().RuntimeError("Invalid conversion to bool."); return false; }
	virtual operator std::string() const { Lox::GetInstance().RuntimeError("Invalid conversion to string."); return ""; }
};

// --- Concrete Value types ---

struct ErrorValue : public Value
{
	std::string message;
	static Value* CreateRaw(const std::string& inMessage)
	{
		auto val = new ErrorValue();
		val->type = TYPE_ERROR;
		val->message = inMessage;
		return val;
	}
	static ValuePtr Create(const std::string& inMessage)
	{
		return ValuePtr(CreateRaw(inMessage));
	}

	operator bool() const override { return false; } // Error values are falsey
	operator std::string() const override { return message; }
};

struct IntValue : public Value
{
	int value = 0;
	static Value* CreateRaw(int inValue)
	{
		auto val = new IntValue();
		val->type = TYPE_INT;
		val->value = inValue;
		return val;
	}
	static ValuePtr Create(int inValue)
	{
		return ValuePtr(CreateRaw(inValue));
	}

	operator int() const override { return value; }
	operator float() const override { return static_cast<float>(value); }
	operator bool() const override { return value != 0; }
	operator std::string() const override { return std::to_string(value); }
};

struct FloatValue : public Value
{
	float value = 0;
	static Value* CreateRaw(float inValue)
	{
		auto val = new FloatValue();
		val->type = TYPE_FLOAT;
		val->value = inValue;
		return val;
	}
	static ValuePtr Create(float inValue)
	{
		return ValuePtr(CreateRaw(inValue));
	}

	operator int() const override { return static_cast<int>(value); }
	operator float() const override { return value; }
	operator bool() const override { return value != 0.0f; }
	operator std::string() const override { return std::to_string(value); }
};

struct StringValue : public Value
{
	std::string value;
	// Cache for global variable slot index to optimize global variable access. Updated by the VM when a global variable is defined.
	size_t cachedGlobalSlot = (size_t)-1;
	static Value* CreateRaw(const std::string& inValue)
	{
		auto val = new StringValue();
		val->type = TYPE_STRING;
		val->value = inValue;
		return val;
	}
	static ValuePtr Create(const std::string& inValue)
	{
		return ValuePtr(CreateRaw(inValue));
	}

	StringValue& operator=(Value value)
	{		
		return *this;
	}

	operator bool() const override { return !value.empty(); }
	operator std::string() const override { return value; }
};

struct BoolValue : public Value
{
	bool value = false;
	static Value* CreateRaw(bool inValue)
	{
		auto val = new BoolValue();
		val->type = TYPE_BOOL;
		val->value = inValue;
		return val;
	}
	static ValuePtr Create(bool inValue)
	{
		return ValuePtr(CreateRaw(inValue));
	}

	operator bool() const override { return value; }
	operator std::string() const override { return value ? "true" : "false"; }
};

struct NilValue : public Value
{
	NilValue() { type = TYPE_NIL; }
	static Value* CreateRaw()
	{
		return new NilValue();
	}
	static ValuePtr Create()
	{
		return ValuePtr(CreateRaw());
	}

	operator bool() const override { return false; }
	operator std::string() const override { return "nil"; }
};

// --- Raw Value* helper functions (C++ forbids operator overloads on pure pointer types) ---

#define VAL_PROPAGATE_ERROR(left, right) \
    if ((left)->type == TYPE_ERROR) return ErrorValue::CreateRaw(static_cast<const ErrorValue*>(left)->message); \
    if ((right)->type == TYPE_ERROR) return ErrorValue::CreateRaw(static_cast<const ErrorValue*>(right)->message);

inline Value* ValNegate(const Value* val)
{
	if (val->type == TYPE_ERROR) return ErrorValue::CreateRaw(static_cast<const ErrorValue*>(val)->message);
	if (val->type == TYPE_INT)   return IntValue::CreateRaw(-static_cast<int>(*val));
	if (val->type == TYPE_FLOAT) return FloatValue::CreateRaw(-static_cast<float>(*val));
	Lox::GetInstance().RuntimeError("Operand must be a number for unary minus.");
	return ErrorValue::CreateRaw("Operand must be a number for unary minus.");
}

inline Value* ValAdd(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	if (left->type == TYPE_STRING && right->type == TYPE_STRING)
		return StringValue::CreateRaw(static_cast<std::string>(*left) + static_cast<std::string>(*right));
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT))
	{
		if (left->type == TYPE_INT && right->type == TYPE_INT)
			return IntValue::CreateRaw(static_cast<int>(*left) + static_cast<int>(*right));
		return FloatValue::CreateRaw(static_cast<float>(*left) + static_cast<float>(*right));
	}
	Lox::GetInstance().RuntimeError("Operands must be two numbers or two strings for '+'.");
	return ErrorValue::CreateRaw("Operands must be two numbers or two strings for '+'.");
}

inline Value* ValSub(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT))
	{
		if (left->type == TYPE_INT && right->type == TYPE_INT)
			return IntValue::CreateRaw(static_cast<int>(*left) - static_cast<int>(*right));
		return FloatValue::CreateRaw(static_cast<float>(*left) - static_cast<float>(*right));
	}
	Lox::GetInstance().RuntimeError("Operands must be numbers for subtraction.");
	return ErrorValue::CreateRaw("Operands must be numbers for subtraction.");
}

inline Value* ValMul(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT))
	{
		if (left->type == TYPE_INT && right->type == TYPE_INT)
			return IntValue::CreateRaw(static_cast<int>(*left) * static_cast<int>(*right));
		return FloatValue::CreateRaw(static_cast<float>(*left) * static_cast<float>(*right));
	}
	Lox::GetInstance().RuntimeError("Operands must be numbers for multiplication.");
	return ErrorValue::CreateRaw("Operands must be numbers for multiplication.");
}

inline Value* ValDiv(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	if ((left->type != TYPE_INT && left->type != TYPE_FLOAT) || (right->type != TYPE_INT && right->type != TYPE_FLOAT))
	{
		Lox::GetInstance().RuntimeError("Operands must be numbers for division.");
		return ErrorValue::CreateRaw("Operands must be numbers for division.");
	}
	if ((right->type == TYPE_INT && static_cast<int>(*right) == 0) ||
		(right->type == TYPE_FLOAT && static_cast<float>(*right) == 0.0f))
	{
		Lox::GetInstance().RuntimeError("Division by zero.");
		return ErrorValue::CreateRaw("Division by zero.");
	}
	if (left->type == TYPE_INT && right->type == TYPE_INT)
		return IntValue::CreateRaw(static_cast<int>(*left) / static_cast<int>(*right));
	return FloatValue::CreateRaw(static_cast<float>(*left) / static_cast<float>(*right));
}

inline Value* ValLess(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT))
		return BoolValue::CreateRaw(static_cast<float>(*left) < static_cast<float>(*right));
	Lox::GetInstance().RuntimeError("Operands must be numbers for comparison.");
	return ErrorValue::CreateRaw("Operands must be numbers for comparison.");
}

inline Value* ValGreater(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT))
		return BoolValue::CreateRaw(static_cast<float>(*left) > static_cast<float>(*right));
	Lox::GetInstance().RuntimeError("Operands must be numbers for comparison.");
	return ErrorValue::CreateRaw("Operands must be numbers for comparison.");
}

inline Value* ValLessEq(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT))
		return BoolValue::CreateRaw(static_cast<float>(*left) <= static_cast<float>(*right));
	Lox::GetInstance().RuntimeError("Operands must be numbers for comparison.");
	return ErrorValue::CreateRaw("Operands must be numbers for comparison.");
}

inline Value* ValGreaterEq(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT))
		return BoolValue::CreateRaw(static_cast<float>(*left) >= static_cast<float>(*right));
	Lox::GetInstance().RuntimeError("Operands must be numbers for comparison.");
	return ErrorValue::CreateRaw("Operands must be numbers for comparison.");
}

inline bool IsEqual(const Value* left, const Value* right)
{
	if ((left->type == TYPE_INT || left->type == TYPE_FLOAT) && (right->type == TYPE_INT || right->type == TYPE_FLOAT))
		return static_cast<float>(*left) == static_cast<float>(*right);
	if (left->type != right->type)
		return false;
	switch (left->type)
	{
		case TYPE_NIL:    return true;
		case TYPE_BOOL:   return static_cast<bool>(*left) == static_cast<bool>(*right);
		case TYPE_STRING: return static_cast<const StringValue*>(left)->value == static_cast<const StringValue*>(right)->value;
		case TYPE_CALLABLE:
			// For simplicity, we consider all callables unequal (unless you want to implement a more complex identity system)
			return false;
		default:          return false;
	}
}

inline Value* ValEqual(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	return BoolValue::CreateRaw(IsEqual(left, right));
}

inline Value* ValNotEqual(const Value* left, const Value* right)
{
	VAL_PROPAGATE_ERROR(left, right);
	return BoolValue::CreateRaw(!IsEqual(left, right));
}

// --- ValuePtr operator overloads (shared_ptr<Value> is a class type, so operators are valid) ---

inline bool IsEqual(const ValuePtr& left, const ValuePtr& right)
{
	return IsEqual(left.get(), right.get());
}

inline ValuePtr operator-(const ValuePtr& val)
{
	return ValuePtr(ValNegate(val.get()));
}

inline ValuePtr operator+(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValAdd(left.get(), right.get()));
}

inline ValuePtr operator-(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValSub(left.get(), right.get()));
}

inline ValuePtr operator*(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValMul(left.get(), right.get()));
}

inline ValuePtr operator/(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValDiv(left.get(), right.get()));
}

inline ValuePtr operator<(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValLess(left.get(), right.get()));
}

inline ValuePtr operator>(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValGreater(left.get(), right.get()));
}

inline ValuePtr operator<=(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValLessEq(left.get(), right.get()));
}

inline ValuePtr operator>=(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValGreaterEq(left.get(), right.get()));
}

inline ValuePtr operator==(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValEqual(left.get(), right.get()));
}

inline ValuePtr operator!=(const ValuePtr& left, const ValuePtr& right)
{
	if (left->type == TYPE_ERROR) return left;
	if (right->type == TYPE_ERROR) return right;
	return ValuePtr(ValNotEqual(left.get(), right.get()));
}
