#pragma once
#include "Value.h"
#include <unordered_map>
#include <memory> // 确保包含了 <memory>

typedef std::shared_ptr<class Environment> EnvironmentPtr;

class Environment : public std::enable_shared_from_this<Environment>
{
protected:
	std::unordered_map<std::string, ValuePtr> values;
	EnvironmentPtr enclosing = nullptr;
	const struct While* currentWhile = nullptr;
	ValuePtr returnValue = nullptr;
	bool isFunctionEnv = false;
public:
	Environment(EnvironmentPtr parent = nullptr, bool isFunction = false)
		: enclosing(parent), isFunctionEnv(isFunction)
	{
	}
	EnvironmentPtr Clone()
	{
		auto newEnv = std::make_shared<Environment>(enclosing, isFunctionEnv);
		newEnv->values = values;
		return newEnv;
	}
	EnvironmentPtr GetFunctionEnv()
	{
		EnvironmentPtr env = shared_from_this();
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
	EnvironmentPtr GetTopEnv()
	{
		EnvironmentPtr env = shared_from_this();
		while (env->enclosing)
		{
			env = env->enclosing;
		}
		return env;
	}
	void SetCurrentWhile(const While* whileStat)
	{
		auto env = GetFunctionEnv();
		if (!env)
		{
			env = GetTopEnv();
		}
		env->currentWhile = whileStat;
	}
	const While* GetCurrentWhile()
	{
		auto env = GetFunctionEnv();
		if (!env)
		{
			env = GetTopEnv();
		}
		return env->currentWhile;
	}
	void SetReturnValue(ValuePtr value)
	{
		auto funcEnv = GetFunctionEnv();
		if (funcEnv)
		{
			funcEnv->returnValue = value;
		}
	}
	bool HasReturnValue()
	{
		auto funcEnv = GetFunctionEnv();
		return funcEnv && funcEnv->returnValue != nullptr;
	}
	ValuePtr GetReturnValue()
	{
		auto funcEnv = GetFunctionEnv();
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
	ValuePtr GetAt(int distance, const std::string& name, size_t line = 0, size_t column = 0)
	{
		EnvironmentPtr env = shared_from_this();
		for (int i = 0; i < distance; ++i)
		{
			env = env->enclosing;
		}
		auto it = env->values.find(name);
		if (it != env->values.end())
		{
			return it->second;
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
    void AssignAt(int distance, const Token& name, ValuePtr value)
    {
        EnvironmentPtr env = shared_from_this();
        for (int i = 0; i < distance; ++i)
        {
            env = env->enclosing;
        }
        env->values[name.lexeme] = value;
    }
};