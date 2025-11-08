// Lox.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "Lox.h"
#include "TestUnit.h"
#include "GenerateAST.h"
#include "Interpreter.h"

int main(int argc, char* argv[])
{
	TestUnit::RunExpressionInterpreterTest();
	TestUnit::RunStatementInterpreterTest();
	TestUnit::RunFunctionInterpreterTest();
	//Lox& lox = Lox::GetInstance();
	//lox.Run(0, nullptr);
	//lox.Run(argc - 1, argc > 1 ? &argv[1] : nullptr);

	GenerateAST generator;
	generator.DefineAST("G:/Lox/", "Expr",
	{
		"Ternary : Expr left, Token opLeft, Expr middle, Token opRight, Expr right",
		"Binary : Expr left, Token op, Expr right",
		"Grouping : Expr expression",
		"Literal : Token value",
		"Unary : Token op, Expr right",
		"Variable : Token name",
		"Assign : Token name, Expr value",
		"Logical : Expr left, Token op, Expr right",
		"Call : Expr callee, Token paren, List<Expr> arguments",
		"Lambda: Token keyword, List<Token> params, List<Stat> body",
	});
	generator.DefineAST("G:/Lox/", "Stat",
	{
		"Expression : Expr expression",
		"Print : Expr expression",
		"Var : Token name, Expr initializer",
		"Block : List<Stat> statements",
		"If : Expr condition, Stat thenBranch, Stat elseBranch",
		"Function : Token name, List<Token> params, List<Stat> body",
		"Return: Token keyword, Expr value",
	});

	return 0;
}