// Lox.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "Lox.h"
#include "TestUnit.h"
#include "GenerateAST.h"
#include "Interpreter.h"

int main(int argc, char* argv[])
{
	//TestUnit::RunExpressionInterpreterTest();
	TestUnit::RunStatementInterpreterTest();
	//Lox& lox = Lox::GetInstance();
	//lox.Run(argc - 1, argc > 1 ? &argv[1] : nullptr);

	GenerateAST generator;
	generator.DefineAST("D:/Lox/", "Expr",
	{
		"Ternary : Expr left, Token opLeft, Expr middle, Token opRight, Expr right",
		"Binary : Expr left, Token op, Expr right",
		"Grouping : Expr expression",
		"Literal : Token value",
		"Unary : Token op, Expr right",
		"Variable : Token name"
	});
	generator.DefineAST("D:/Lox/", "Stat",
	{
		"Expression : Expr expression",
		"Print : Expr expression",
		"Var : Token name, Expr initializer",
	});

	return 0;
}