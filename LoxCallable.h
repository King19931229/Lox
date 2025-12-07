#pragma once
#include "Value.h"
#include "Expr.h"
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
	EnvironmentPtr closure;

	LoxLambda(const Lambda* inDeclaration);
	static ValuePtr Create(const Lambda* declaration);
	int Arity() const override;
	operator std::string() const override;
	ValuePtr Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments) override;
};

struct LoxFunction : public LoxCallable
{
	const Function* declaration;
	EnvironmentPtr closure;
	bool isInitializer;

	LoxFunction(const Function* inDeclaration, EnvironmentPtr inClosure, bool inIsInitializer);
	static ValuePtr Create(const Function* declaration, EnvironmentPtr closure, bool isInitializer);
	ValuePtr Bound(ValuePtr instance);
	int Arity() const override;
	operator std::string() const override;
	ValuePtr Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments) override;
};

struct LoxGetter : public LoxCallable
{
	const Getter* declaration;
	EnvironmentPtr closure;

	LoxGetter(const Getter* inDeclaration, EnvironmentPtr inClosure);
	static ValuePtr Create(const Getter* declaration, EnvironmentPtr closure);
	ValuePtr Bound(ValuePtr instance);
	int Arity() const override;
	operator std::string() const override;
	ValuePtr Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments) override;
};

struct LoxClass : public LoxCallable
{
	std::string name;
	std::unordered_map<std::string, ValuePtr> methods;
	std::unordered_map<std::string, ValuePtr> getters;
	std::unordered_map<std::string, ValuePtr> classMethods;
	ValuePtr superClass;

	LoxClass(const std::string& inName, ValuePtr inSuperClass);
	static ValuePtr Create(const std::string& name, ValuePtr superClass);
	int Arity() const override;
	operator std::string() const override;
	ValuePtr FindMethod(const std::string& methodName) const;
	ValuePtr FindGetter(const std::string& getterName) const;
	ValuePtr Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments) override;
	ValuePtr Get(const Token& name, size_t line = 0, size_t column = 0);
	void Set(const Token& name, ValuePtr value);
};

struct LoxInstance : public Value
{
	ValuePtr klass;
	std::unordered_map<std::string, ValuePtr> fields;

	LoxInstance(ValuePtr inClass);
	static ValuePtr Create(ValuePtr klass);
	operator std::string() const override;
	ValuePtr Get(class Interpreter* interpreter, const Token& name, size_t line = 0, size_t column = 0);
	void Set(const Token& name, ValuePtr value);
};