// Lox.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "Lox.h"
#include "TestUnit.h"
#include "GenerateAST.h"

int main(int argc, char* argv[])
{
	TestUnit::RunExpressionParserTest();
	//Lox& lox = Lox::GetInstance();
	//lox.Run(argc - 1, argc > 1 ? &argv[1] : nullptr);

	GenerateAST generator;
	generator.DefineAST("G:/Lox/", "Expr",
	{
		"Ternary : Expr left, Token opLeft, Expr middle, Token opRight, Expr right",
		"Binary : Expr left, Token op, Expr right",
		"Grouping : Expr expression",
		"Literal : Lexeme value",
		"Unary : Token op, Expr right"
	});
	return 0;
}