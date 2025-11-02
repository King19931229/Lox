#pragma once
#include "Value.h"
#include <unordered_map>

class Environment
{
protected:
	std::unordered_map<std::string, ValuePtr> values;
	Environment* enclosing = nullptr;
public:
	Environment(Environment* parent = nullptr)
		: enclosing(parent)
	{
	}
	void Define(const std::string& name, ValuePtr value, size_t line = 0, size_t column = 0)
	{
		if (values.find(name) != values.end())
		{
			Lox::GetInstance().RuntimeError(line, column, "Variable '%s' already defined in this scope.", name.c_str());
		}
		else
		{
			values.insert({ name, value });
		}
	}
	ValuePtr Get(const std::string& name, size_t line = 0, size_t column = 0)
	{
		if (values.find(name) != values.end())
		{
			return values[name];
		}
		if (enclosing)
		{
			return enclosing->Get(name, line, column);
		}
		Lox::GetInstance().RuntimeError(line, column, "Undefined variable '%s'.", name.c_str());
		return NilValue::Create();
	}
	void Assign(const std::string& name, ValuePtr value, size_t line = 0, size_t column = 0)
	{
		if (values.find(name) != values.end())
		{
			values[name] = value;
			return;
		}
		if (enclosing)
		{
			enclosing->Assign(name, value, line, column);
			return;
		}
		Lox::GetInstance().RuntimeError(line, column, "Undefined variable '%s'.", name.c_str());
	}
};