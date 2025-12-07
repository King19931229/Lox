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

LoxFunction::LoxFunction(const Function* inDeclaration, EnvironmentPtr inClosure, bool inIsInitializer)
	: declaration(inDeclaration)
	, closure(inClosure)
	, isInitializer(inIsInitializer)
{
	this->type = TYPE_CALLABLE;
}

ValuePtr LoxFunction::Create(const Function* declaration, EnvironmentPtr closure, bool isInitializer)
{
	auto function = std::make_shared<LoxFunction>(declaration, closure, isInitializer);
	return function;
}

ValuePtr LoxFunction::Bound(ValuePtr instance)
{
	EnvironmentPtr boundEnv = std::make_shared<Environment>(closure, true);
	boundEnv->Define("this", instance, declaration->name.line, declaration->name.column);
	auto boundFunction = std::make_shared<LoxFunction>(declaration, boundEnv, isInitializer);
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

LoxGetter::LoxGetter(const Getter* inDeclaration, EnvironmentPtr inClosure)
	: declaration(inDeclaration)
	, closure(inClosure)
{
	this->type = TYPE_CALLABLE;
}

ValuePtr LoxGetter::Create(const Getter* declaration, EnvironmentPtr closure)
{
	auto function = std::make_shared<LoxGetter>(declaration, closure);
	return function;
}

ValuePtr LoxGetter::Bound(ValuePtr instance)
{
	EnvironmentPtr boundEnv = std::make_shared<Environment>(closure, true);
	boundEnv->Define("this", instance, declaration->name.line, declaration->name.column);
	auto boundGetter = std::make_shared<LoxGetter>(declaration, boundEnv);
	return boundGetter;
}

int LoxGetter::Arity() const
{
	return 0;
}

LoxGetter::operator std::string() const
{
	return "<getter " + declaration->name.lexeme + ">";
}

LoxClass::LoxClass(const std::string& inName, ValuePtr inSuperClass)
	: name(inName)
	, superClass(inSuperClass)
{
	this->type = TYPE_CLASS;
}

ValuePtr LoxClass::Create(const std::string& name, ValuePtr superClass)
{
	auto loxClass = std::make_shared<LoxClass>(name, superClass);
	return loxClass	;
}

int LoxClass::Arity() const
{
	// look for "init" method
	if (ValuePtr initializer = FindMethod("init"))
	{
		LoxFunction* func = static_cast<LoxFunction*>(initializer.get());
		return func->Arity();
	}
	return 0;
}

LoxClass::operator std::string() const
{
	return "<class " + name + ">";
}

ValuePtr LoxClass::FindMethod(const std::string& methodName) const
{
	auto it = methods.find(methodName);
	if (it != methods.end())
	{
		return it->second;
	}
	if (superClass)
	{
		LoxClass* superClassPtr = static_cast<LoxClass*>(superClass.get());
		return superClassPtr->FindMethod(methodName);
	}
	return nullptr;
}

ValuePtr LoxClass::FindGetter(const std::string& getterName) const
{
	auto it = getters.find(getterName);
	if (it != getters.end())
	{
		return it->second;
	}
	return nullptr;
}

ValuePtr LoxClass::Get(const Token& name, size_t line, size_t column)
{
	auto it = classMethods.find(name.lexeme);
	if (it != classMethods.end())
	{
		return it->second;
	}
	Lox::GetInstance().RuntimeError(line, column, ("Undefined class method '" + name.lexeme + "'.").c_str());
	return NilValue::Create();
}

void LoxClass::Set(const Token& name, ValuePtr value)
{
	methods[name.lexeme] = value;
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

ValuePtr LoxInstance::Get(Interpreter* interpreter, const Token& name, size_t line, size_t column)
{
	// check fields
	auto it = fields.find(name.lexeme);
	if (it != fields.end())
	{
		return it->second;
	}
	// check getters
	auto classPtr = std::static_pointer_cast<LoxClass>(klass);
	ValuePtr getter = classPtr->FindGetter(name.lexeme);
	if (getter)
	{
		// bind 'this'
		auto function = std::static_pointer_cast<LoxGetter>(getter);
		ValuePtr boundGetter = function->Bound(shared_from_this());
		return static_cast<LoxGetter*>(boundGetter.get())->Call(interpreter, {});
	}
	// check methods
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