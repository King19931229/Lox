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

	void Define(const std::string& name, ValuePtr value)
	{
		values[name] = value;
	}
	ValuePtr Get(const std::string& name)
	{
		if (values.find(name) != values.end())
		{
			return values[name];
		}
		Lox::GetInstance().RuntimeError("Undefined variable '%s'.", name.c_str());
		return NilValue::Create();
	}
};