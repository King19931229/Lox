#pragma once
#include "Value.h"
#include <unordered_map>

class Environment
{
protected:
	std::unordered_map<std::string, ValuePtr> values;
	Environment* enclosing = nullptr;
	const struct While* currentWhile = nullptr;
	ValuePtr returnValue = nullptr;
	bool isFunctionEnv = false;
public:
	Environment(Environment* parent = nullptr, bool isFunction = false)
		: enclosing(parent), isFunctionEnv(isFunction)
	{
	}
	Environment* Clone()
	{
		Environment* newEnv = new Environment(enclosing, isFunctionEnv);
		newEnv->values = values;
		return newEnv;
	}
	Environment* GetFunctionEnv()
	{
		Environment* env = this;
		while (env)
		{
			if (env->isFunctionEnv)
			{
				return env;
			}
			env = env->enclosing;
		}
		return nullptr;
	}
	Environment* GetTopEnv()
	{
		Environment* env = this;
		while (env->enclosing)
		{
			env = env->enclosing;
		}
		return env;
	}
	void SetCurrentWhile(const While* whileStat)
	{
		Environment* env = GetFunctionEnv();
		if (!env)
		{
			env = GetTopEnv();
		}
		env->currentWhile = whileStat;
	}
	const While* GetCurrentWhile()
	{
		Environment* env = GetFunctionEnv();
		if (!env)
		{
			env = GetTopEnv();
		}
		return env->currentWhile;
	}
	void SetReturnValue(ValuePtr value)
	{
		Environment* funcEnv = GetFunctionEnv();
		if (funcEnv)
		{
			funcEnv->returnValue = value;
		}
	}
	bool HasReturnValue()
	{
		Environment* funcEnv = GetFunctionEnv();
		return funcEnv && funcEnv->returnValue != nullptr;
	}
	ValuePtr GetReturnValue()
	{
		Environment* funcEnv = GetFunctionEnv();
		if (funcEnv)
		{
			return funcEnv->returnValue;
		}
		return nullptr;
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