#include "Interpreter.h"
#include "Environment.h"
#include "LoxCallable.h"
#include <assert.h>

#define LOOP_CONTROL_FAST_RETURN if (loopControl != LOOP_NONE) { return true; }
#define ERROR_CONTROL_FAST_RETURN if (Lox::GetInstance().HasRuntimeError()) { return true; }

Interpreter::Interpreter()
	: globalEnvironment(new Environment())
	, environment(globalEnvironment)
{
	struct Clock : public LoxCallable
	{
		Clock()
		{
			this->type = TYPE_CALLABLE;
		}
		int Arity() const override
		{
			return 0;
		}
		ValuePtr Call(Interpreter* interpreter, const std::vector<ValuePtr>& arguments) override
		{
			return FloatValue::Create(static_cast<float>(time(nullptr)));
		}
		operator std::string() const override
		{
			return "<native fn>";
		}
		static ValuePtr Create()
		{
			return std::make_shared<Clock>();
		}
	};
	globalEnvironment->Define("clock", Clock::Create());
}

Interpreter::~Interpreter()
{
	assert(environment == globalEnvironment && "Environment stack not balanced on Interpreter destruction.");
}

ValuePtr Interpreter::InterpretExpr(const ExprPtr& expr)
{
	if (!expr) return NilValue::Create();
	return VisitExpr(expr.get());
}

void Interpreter::Interpret(const std::vector<StatPtr>& statements)
{
	for (const StatPtr& stat : statements)
	{
		Execute(stat);
	}
}

std::string Interpreter::Stringify(ValuePtr value)
{
	return static_cast<std::string>(*value);
}

bool Interpreter::Trueify(ValuePtr value)
{
	return static_cast<bool>(*value);
}

ValuePtr Interpreter::Evaluate(const ExprPtr& expr)
{
	if (!expr) return NilValue::Create();
	return VisitExpr(expr.get());
}

void Interpreter::Execute(const StatPtr& stat)
{
	if (stat)
	{
		VisitStat(stat.get());
	}
}

ValuePtr Interpreter::DoVisitTernaryExpr(const Ternary* expr)
{
	ValuePtr condition = Evaluate(expr->left);
	if (Trueify(condition))
	{
		return Evaluate(expr->middle);
	}
	else
	{
		return Evaluate(expr->right);
	}
}

ValuePtr Interpreter::DoVisitBinaryExpr(const Binary* expr)
{
	ValuePtr left = Evaluate(expr->left);
	ValuePtr right = Evaluate(expr->right);

	switch (expr->op.type)
	{
		// 算术运算符
		case TokenType::MINUS:         return left - right;
		case TokenType::SLASH:         return left / right;
		case TokenType::STAR:          return left * right;
		case TokenType::PLUS:          return left + right;

			// 比较运算符
		case TokenType::GREATER:       return left > right;
		case TokenType::GREATER_EQUAL: return left >= right;
		case TokenType::LESS:          return left < right;
		case TokenType::LESS_EQUAL:    return left <= right;

			// 相等性运算符
		case TokenType::BANG_EQUAL:    return left != right;
		case TokenType::EQUAL_EQUAL:   return left == right;

			// 逗号运算符
		case TokenType::COMMA:
			return right;
	}

	// 补上行/列信息用于更精确的运行时错误定位
	Lox::GetInstance().RuntimeError(expr->op.line, expr->op.column, "Unknown binary operator.");
	return ErrorValue::Create("Unknown binary operator.");
}

ValuePtr Interpreter::DoVisitGroupingExpr(const Grouping* expr)
{
	return Evaluate(expr->expression);
}

ValuePtr Interpreter::DoVisitLiteralExpr(const Literal* expr)
{
	const Token& token = expr->value;

	switch (token.type)
	{
		case TokenType::TRUE:   return BoolValue::Create(true);
		case TokenType::FALSE:  return BoolValue::Create(false);
		case TokenType::NIL:    return NilValue::Create();
		case TokenType::NUMBER:
		{
			const std::string& lexeme = token.lexeme;
			if (lexeme.find('.') != std::string::npos)
			{
				return FloatValue::Create(std::stof(lexeme));
			}
			else
			{
				return IntValue::Create(std::stoi(lexeme));
			}
		}
		case TokenType::STRING:
			return StringValue::Create(token.lexeme);
		default:
			// 补上行/列信息用于更精确的运行时错误定位
			Lox::GetInstance().RuntimeError(token.line, token.column, "Unexpected literal type.");
			return ErrorValue::Create("Unexpected literal type.");
	}
}

ValuePtr Interpreter::DoVisitUnaryExpr(const Unary* expr)
{
	ValuePtr right = Evaluate(expr->right);

	switch (expr->op.type)
	{
		case TokenType::BANG:
		{
			return BoolValue::Create(!Trueify(right));
		}
		case TokenType::MINUS: return -right;
	}

	Lox::GetInstance().RuntimeError(expr->op.line, expr->op.column, "Unknown unary operator.");
	return ErrorValue::Create("Unknown unary operator.");
}

ValuePtr Interpreter::LookUpVariable(const Token& name, const Expr* expr)
{
	auto it = locals.find(expr);
	if (it != locals.end())
	{
		int distance = it->second;
		return environment->GetAt(distance, name.lexeme, name.line, name.column);
	}
	else
	{
		return globalEnvironment->Get(name.lexeme, name.line, name.column);
	}
}

ValuePtr Interpreter::DoVisitVariableExpr(const Variable* expr)
{
	if (resolver)
	{
		return LookUpVariable(expr->name, expr);
	}
	else
	{
		return environment->Get(expr->name.lexeme);
	}
}

ValuePtr Interpreter::DoVisitAssignExpr(const Assign* expr)
{
	ValuePtr value = Evaluate(expr->value);
	if (resolver)
	{
		auto it = locals.find(expr);
		if (it != locals.end())
		{
			int distance = it->second;
			environment->AssignAt(distance, expr->name, value);
		}
		else
		{
			globalEnvironment->Assign(expr->name.lexeme, value, expr->name.line, expr->name.column);
		}
	}
	else
	{
		environment->Assign(expr->name.lexeme, value, expr->name.line, expr->name.column);
	}
	return value;
}

ValuePtr Interpreter::DoVisitLogicalExpr(const Logical* expr)
{
	ValuePtr left = Evaluate(expr->left);
	if (expr->op.type == TokenType::OR)
	{
		if (Trueify(left)) return left;
	}
	else // AND
	{
		if (!Trueify(left)) return left;
	}
	return Evaluate(expr->right);
}

ValuePtr Interpreter::DoVisitCallExpr(const Call* expr)
{
	ValuePtr callee = Evaluate(expr->callee);
	std::vector<ValuePtr> arguments;
	for (const ExprPtr& argumentExpr : expr->arguments)
	{
		arguments.push_back(Evaluate(argumentExpr));
	}

	LoxCallable* callable = dynamic_cast<LoxCallable*>(callee.get());
	if (!callable)
	{
		Lox::GetInstance().RuntimeError(expr->paren.line, expr->paren.column, "Can only call functions and classes.");
		return ErrorValue::Create("Can only call functions and classes.");
	}
	else
	{
		if (arguments.size() != callable->Arity())
		{
			Lox::GetInstance().RuntimeError(expr->paren.line, expr->paren.column, "Argument count mismatch, expected %d but got %d.", callable->Arity(), arguments.size());
			return ErrorValue::Create("Argument count mismatch.");
		}
		return callable->Call(this, arguments);
	}
}

ValuePtr Interpreter::DoVisitLambdaExpr(const Lambda* expr)
{
	ValuePtr lambda = LoxLambda::Create(expr);
	dynamic_cast<LoxLambda*>(lambda.get())->closure = environment;
	return lambda;
}

ValuePtr Interpreter::DoVisitGetExpr(const Get* expr)
{
	ValuePtr object = Evaluate(expr->object);
	if (object->type != TYPE_INSTANCE && object->type != TYPE_CLASS)
	{
		Lox::GetInstance().RuntimeError(expr->name.line, expr->name.column, "Only instances or class have properties.");
		return ErrorValue::Create("Only instances have properties.");
	}
	if (object->type == TYPE_INSTANCE)
	{
		LoxInstance* instance = static_cast<LoxInstance*>(object.get());
		return instance->Get(this, expr->name);
	}
	else if (object->type == TYPE_CLASS)
	{
		LoxClass* klass = static_cast<LoxClass*>(object.get());
		return klass->Get(expr->name);
	}
	return ErrorValue::Create("Unreachable code in DoVisitGetExpr.");
}

ValuePtr Interpreter::DoVisitSetExpr(const Set* expr)
{
	ValuePtr object = Evaluate(expr->object);
	if (object->type != TYPE_INSTANCE && object->type != TYPE_CLASS)
	{
		Lox::GetInstance().RuntimeError(expr->name.line, expr->name.column, "Only instances or class have properties.");
		return ErrorValue::Create("Only instances have properties.");
	}
	if (object->type == TYPE_INSTANCE)
	{
		LoxInstance* instance = static_cast<LoxInstance*>(object.get());
		ValuePtr value = Evaluate(expr->value);
		instance->Set(expr->name, value);
		return value;
	}
	else if (object->type == TYPE_CLASS)
	{
		LoxClass* klass = static_cast<LoxClass*>(object.get());
		ValuePtr value = Evaluate(expr->value);
		klass->Set(expr->name, value);
		return value;
	}
	return ErrorValue::Create("Unreachable code in DoVisitSetExpr.");
}

ValuePtr Interpreter::DoVisitThisExpr(const This* expr)
{
	return LookUpVariable(expr->keyword, expr);
}

ValuePtr Interpreter::DoVisitSuperExpr(const Super* expr)
{
	if (!locals.count(expr))
	{
		Lox::GetInstance().RuntimeError("'%s' method is not resolved.", expr->method.lexeme.c_str());
		return ErrorValue::Create("Method is not resolved.");
	}
	int distance = locals[expr];
	ValuePtr superClassValue = environment->GetAt(distance, expr->keyword.lexeme, expr->keyword.line, expr->keyword.column);
	LoxClass* superClass = superClassValue ? dynamic_cast<LoxClass*>(superClassValue.get()) : nullptr;
	if (!superClass)
	{
		Lox::GetInstance().RuntimeError(expr->keyword.line, expr->keyword.column, "Super class must be a class type.");
		return ErrorValue::Create("Super class must be a class type.");
	}
	ValuePtr methodValue = superClass->FindMethod(expr->method.lexeme);
	LoxFunction* method = methodValue ? dynamic_cast<LoxFunction*>(methodValue.get()) : nullptr;
	if (!method)
	{
		Lox::GetInstance().RuntimeError(expr->method.line, expr->method.column, "Method '%s' is not defined in super class.", expr->method.lexeme.c_str());
		return ErrorValue::Create("Method is not defined in super class.");
	}
	ValuePtr object = environment->GetAt(distance - 1, "this", expr->keyword.line, expr->keyword.column);
	if (object ->type != TYPE_INSTANCE)
	{
		Lox::GetInstance().RuntimeError(expr->keyword.line, expr->keyword.column, "'this' must be an instance.");
		return ErrorValue::Create("'this' must be an instance.");
	}
	return method->Bound(object);
};

bool Interpreter::DoVisitExpressionStat(const Expression* stat)
{
	ERROR_CONTROL_FAST_RETURN;
	LOOP_CONTROL_FAST_RETURN;

	Evaluate(stat->expression);
	return true;
}

bool Interpreter::DoVisitPrintStat(const Print* stat)
{
	ERROR_CONTROL_FAST_RETURN;
	LOOP_CONTROL_FAST_RETURN;

	Evaluate(stat->expression);
	std::cout << Stringify(Evaluate(stat->expression)) << std::endl;
	return true;
}

bool Interpreter::DoVisitVarStat(const Var* stat)
{
	ERROR_CONTROL_FAST_RETURN;
	LOOP_CONTROL_FAST_RETURN;

	if (stat->initializer)
	{
		environment->Define(stat->name.lexeme, Evaluate(stat->initializer));
	}
	else
	{
		environment->Define(stat->name.lexeme, ErrorValue::Create("Uninitialized variable."));
	}
	return true;
}

void Interpreter::ExecuteBlock(const std::vector<StatPtr>& statements, EnvironmentPtr newEnv)
{
	auto oldEnv = environment;
	environment = newEnv;
	for (StatPtr stat : statements)
	{
		if (environment->HasReturnValue())
		{
			break;
		}
		Execute(stat);
	}
	environment = oldEnv;
}

ValuePtr LoxFunction::Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments)
{
	return interpreter->CallFunction(this, arguments);
}

ValuePtr Interpreter::CallFunction(const LoxFunction* function, const std::vector<ValuePtr>& arguments)
{
	EnvironmentPtr functionEnv = std::make_shared<Environment>(function->closure, true);
	for (size_t i = 0; i < function->declaration->params.size(); ++i)
	{
		functionEnv->Define(function->declaration->params[i].lexeme, arguments[i], function->declaration->params[i].line, function->declaration->params[i].column);
	}
	ExecuteBlock(function->declaration->body, functionEnv);

	ValuePtr returnValue = nullptr;
	if (functionEnv->HasReturnValue())
	{
		returnValue = functionEnv->GetReturnValue();
	}
	else
	{
		returnValue = NilValue::Create();
	}
	return returnValue;
}

ValuePtr LoxLambda::Call(class Interpreter* interpreter, const std::vector<ValuePtr>& arguments)
{
	return interpreter->CallLambda(this, arguments);
}

ValuePtr Interpreter::CallLambda(const LoxLambda* lambda, const std::vector<ValuePtr>& arguments)
{
	EnvironmentPtr lambdaEnv = std::make_shared<Environment>(lambda->closure, true);
	for (size_t i = 0; i < lambda->declaration->params.size(); ++i)
	{
		lambdaEnv->Define(lambda->declaration->params[i].lexeme, arguments[i], lambda->declaration->params[i].line, lambda->declaration->params[i].column);
	}
	ExecuteBlock(lambda->declaration->body, lambdaEnv);
	ValuePtr returnValue = nullptr;
	if (lambdaEnv->HasReturnValue())
	{
		returnValue = lambdaEnv->GetReturnValue();
	}
	else
	{
		returnValue = NilValue::Create();
	}
	return returnValue;
}

bool Interpreter::DoVisitBlockStat(const Block* stat)
{
	ERROR_CONTROL_FAST_RETURN;
	LOOP_CONTROL_FAST_RETURN;

	EnvironmentPtr newEnv = std::make_shared<Environment>(environment);
	ExecuteBlock(stat->statements, newEnv);
	return true;
}

bool Interpreter::DoVisitFunctionStat(const Function* stat)
{
	ValuePtr function = LoxFunction::Create(stat, environment, false);
	environment->Define(stat->name.lexeme, function, stat->name.line, stat->name.column);
	return true;
}

bool Interpreter::DoVisitGetterStat(const Getter* stat)
{
	return true;
}

bool Interpreter::DoVisitReturnStat(const Return* stat)
{
	ERROR_CONTROL_FAST_RETURN;

	ValuePtr value = NilValue::Create();
	if (stat->value)
	{
		value = Evaluate(stat->value);
	}
	else // void return
	{
		value = NilValue::Create();
	}
	environment->SetReturnValue(value);
	return true;
}

ValuePtr LoxClass::Call(Interpreter* interpreter, const std::vector<ValuePtr>& arguments)
{
	ValuePtr instance = LoxInstance::Create(shared_from_this());
	if (ValuePtr initializer = FindMethod("init"))
	{
		LoxFunction* initFunction = static_cast<LoxFunction*>(initializer.get());
		ValuePtr boundedInit = initFunction->Bound(instance);
		static_cast<LoxFunction*>(boundedInit.get())->Call(interpreter, arguments);
	}
	return instance;
}

ValuePtr LoxGetter::Call(Interpreter* interpreter, const std::vector<ValuePtr>& arguments)
{
	if (arguments.size() != 0)
	{
		Lox::GetInstance().RuntimeError("Getter should not have arguments.");
		return ErrorValue::Create("Getter should not have arguments.");
	}
	return interpreter->CallGetter(this);
}

ValuePtr Interpreter::CallGetter(const LoxGetter* getter)
{
	EnvironmentPtr getterEnv = std::make_shared<Environment>(getter->closure, true);
	ExecuteBlock(getter->declaration->body, getterEnv);
	ValuePtr returnValue = nullptr;
	if (getterEnv->HasReturnValue())
	{
		returnValue = getterEnv->GetReturnValue();
	}
	else
	{
		returnValue = NilValue::Create();
	}
	return returnValue;
}

bool Interpreter::DoVisitClassStat(const Class* stat)
{
	ERROR_CONTROL_FAST_RETURN;
	environment->Define(stat->name.lexeme, nullptr, stat->name.line, stat->name.column);
	ValuePtr superclassValue = nullptr;
	if (stat->superclass)
	{
		superclassValue = Evaluate(stat->superclass);
		LoxClass* superClass = dynamic_cast<LoxClass*>(superclassValue.get());
		if (!superClass)
		{
			Lox::GetInstance().RuntimeError(
				stat->name.line,
				stat->name.column,
				"Superclass must be a class.");
			return true;
		}
	}

	ValuePtr klass = LoxClass::Create(stat->name.lexeme, superclassValue);

	// Create a new environment for methods to capture 'super' if there is a superclass
	EnvironmentPtr classDefEnv = environment;
	if (stat->superclass)
	{
		environment = std::make_shared<Environment>(classDefEnv);
		environment->Define("super", superclassValue, stat->name.line, stat->name.column);
	}

	auto& methods = static_cast<LoxClass*>(klass.get())->methods;
	for (StatPtr methodStat : stat->methods)
	{
		Function* methodFunction = dynamic_cast<Function*>(methodStat.get());
		if (methodFunction)
		{
			ValuePtr function = LoxFunction::Create(methodFunction, environment, methodFunction->name.lexeme == "init");
			methods[methodFunction->name.lexeme] = function;
		}
	}
	auto& getters = static_cast<LoxClass*>(klass.get())->getters;
	for (StatPtr getterStat : stat->getters)
	{
		Getter* getterFunction = dynamic_cast<Getter*>(getterStat.get());
		if (getterFunction)
		{
			ValuePtr function = LoxGetter::Create(getterFunction, environment);
			getters[getterFunction->name.lexeme] = function;
		}
	}

	auto& classMethods = static_cast<LoxClass*>(klass.get())->classMethods;
	for (StatPtr classMethodStat : stat->classMethods)
	{
		Function* classMethodFunction = dynamic_cast<Function*>(classMethodStat.get());
		if (classMethodFunction)
		{
			ValuePtr function = LoxFunction::Create(classMethodFunction, environment, false);
			classMethods[classMethodFunction->name.lexeme] = function;
		}
	}

	// Restore the previous environment
	if (stat->superclass)
	{
		environment = classDefEnv;
	}

	environment->Assign(stat->name.lexeme, klass, stat->name.line, stat->name.column);

	return true;
}

bool Interpreter::DoVisitIfStat(const If* stat)
{
	ERROR_CONTROL_FAST_RETURN;
	LOOP_CONTROL_FAST_RETURN;

	ValuePtr value = Evaluate(stat->condition);
	if (Trueify(value))
	{
		Execute(stat->thenBranch);
	}
	else
	{
		Execute(stat->elseBranch);
	}
	return true;
}

bool Interpreter::DoVisitWhileStat(const While* stat)
{
	while (Trueify(Evaluate(stat->condition)))
	{
		environment->SetCurrentWhile(stat);
		Execute(stat->body);
		if (loopControl == LOOP_BREAK)
		{
			loopControl = LOOP_NONE;
			break;
		}
	}
	environment->SetCurrentWhile(nullptr);
	return true;
}

bool Interpreter::DoVisitBreakStat(const Break* stat)
{
	if (!environment->GetCurrentWhile())
	{
		Lox::GetInstance().RuntimeError(stat->keyword.line, stat->keyword.column, "Break statement not within a loop.");
		return true;
	}
	loopControl = LOOP_BREAK;
	return true;
}

void Interpreter::Resolve(const Expr* expr, int depth)
{
	locals[expr] = depth;
}

void Interpreter::SetResolver(const class Resolver* resolver)
{
	this->resolver = resolver;
}