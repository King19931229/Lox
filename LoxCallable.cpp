#include "LoxCallable.h"

LoxLambda::LoxLambda(const Lambda* inDeclaration)
	: declaration(inDeclaration)
	, closure(nullptr)
{
	this->type = TYPE_CALLABLE;
}

ValuePtr LoxLambda::Create(const Lambda* declaration)
{
	return std::make_shared<LoxLambda>(declaration);
}

int LoxLambda::Arity() const
{
	return static_cast<int>(declaration->params.size());
}

LoxLambda::operator std::string() const
{
	return "<lambda> location: " + std::to_string(declaration->keyword.line) + ":" + std::to_string(declaration->keyword.column);
}

LoxFunction::LoxFunction(const Function* inDeclaration)
	: declaration(inDeclaration)
	, closure(nullptr)
{
	this->type = TYPE_CALLABLE;
}

ValuePtr LoxFunction::Create(const Function* declaration, EnvironmentPtr closure)
{
	auto function = std::make_shared<LoxFunction>(declaration);
	function->closure = closure;
	return function;
}

ValuePtr LoxFunction::Bound(ValuePtr instance)
{
	EnvironmentPtr boundEnv = std::make_shared<Environment>(closure, true);
	boundEnv->Define("this", instance, declaration->name.line, declaration->name.column);
	auto boundFunction = std::make_shared<LoxFunction>(declaration);
	boundFunction->closure = boundEnv;
	return boundFunction;
}

int LoxFunction::Arity() const
{
	return static_cast<int>(declaration->params.size());
}

LoxFunction::operator std::string() const
{
	return "<fn " + declaration->name.lexeme + ">";
}

LoxClass::LoxClass(const std::string& inName)
	: name(inName)
{
	this->type = TYPE_CALLABLE;
}

ValuePtr LoxClass::Create(const std::string& name)
{
	return std::make_shared<LoxClass>(name);
}

int LoxClass::Arity() const
{
	return 0;
}

LoxClass::operator std::string() const
{
	return "<class " + name + ">";
}

ValuePtr LoxClass::FindMethod(const std::string& methodName)
{
	auto it = methods.find(methodName);
	if (it != methods.end())
	{
		return it->second;
	}
	return nullptr;
}

LoxInstance::LoxInstance(ValuePtr inClass)
	: klass(inClass)
{
	this->type = TYPE_INSTANCE;
}

ValuePtr LoxInstance::Create(ValuePtr klass)
{
	return std::make_shared<LoxInstance>(klass);
}

LoxInstance::operator std::string() const	
{
	return "<instance of " + std::static_pointer_cast<LoxClass>(klass)->name + ">";
}

ValuePtr LoxInstance::Get(const Token& name, size_t line, size_t column)
{
	auto it = fields.find(name.lexeme);
	if (it != fields.end())
	{
		return it->second;
	}
	// check methods
	auto classPtr = std::static_pointer_cast<LoxClass>(klass);
	ValuePtr method = classPtr->FindMethod(name.lexeme);
	if (method)
	{
		// bind 'this'
		auto function = std::static_pointer_cast<LoxFunction>(method);
		return function->Bound(shared_from_this());
	}
	Lox::GetInstance().RuntimeError(line, column, ("Undefined property '" + name.lexeme + "'.").c_str());
	return NilValue::Create();
}

void LoxInstance::Set(const Token& name, ValuePtr value)
{
	fields[name.lexeme] = value;
}