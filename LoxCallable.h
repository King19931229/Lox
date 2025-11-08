#pragma once
#include "Value.h"
#include "Stat.h"
#include "Environment.h"
#include <vector>

struct LoxCallable : public Value
{
	// return the number of parameters the function/method takes
	virtual int Arity() const = 0;
	virtual ValuePtr Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments) = 0;
};

struct LoxLambda : public LoxCallable
{
	const Lambda* declaration;
	Environment* closure;

	LoxLambda(const Lambda* inDeclaration)
		: declaration(inDeclaration)
		, closure(nullptr)
	{
		this->type = TYPE_CALLABLE;
	}

	~LoxLambda()
	{
		delete closure;
	}

	static ValuePtr Create(const Lambda* declaration)
	{
		return std::make_shared<LoxLambda>(declaration);
	}

	int Arity() const override
	{
		return static_cast<int>(declaration->params.size());
	}

	operator std::string() const override
	{
		return "<lambda> location: " + std::to_string(declaration->keyword.line) + ":" + std::to_string(declaration->keyword.column);
	}

	ValuePtr Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments) override;
};

struct LoxFunction : public LoxCallable
{
	const Function* declaration;
	Environment* closure;

	LoxFunction(const Function* inDeclaration)
		: declaration(inDeclaration)
		, closure(nullptr)
	{
		this->type = TYPE_CALLABLE;
	}

	~LoxFunction()
	{
		delete closure;
	}

	static ValuePtr Create(const Function* declaration)
	{
		return std::make_shared<LoxFunction>(declaration);
	}

	int Arity() const override
	{
		return static_cast<int>(declaration->params.size());
	}

	operator std::string() const override
	{
		return "<fn " + declaration->name.lexeme + ">";
	}

	ValuePtr Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments) override;
};