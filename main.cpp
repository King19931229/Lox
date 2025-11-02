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
	//Lox& lox = Lox::GetInstance();
	//lox.Run(0, nullptr);
	//lox.Run(argc - 1, argc > 1 ? &argv[1] : nullptr);

	/*GenerateAST generator;
	generator.DefineAST("G:/Lox/", "Expr",
	{
		"Ternary : Expr left, Token opLeft, Expr middle, Token opRight, Expr right",
		"Binary : Expr left, Token op, Expr right",
		"Grouping : Expr expression",
		"Literal : Token value",
		"Unary : Token op, Expr right",
		"Variable : Token name",
		"Assign : Token name, Expr value",
	});
	generator.DefineAST("G:/Lox/", "Stat",
	{
		"Expression : Expr expression",
		"Print : Expr expression",
		"Var : Token name, Expr initializer",
		"Block : List<Stat> statements",
	});*/

	return 0;
}