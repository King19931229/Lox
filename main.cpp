// Lox.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "Lox.h"
#include "TestUnit.h"
#include "GenerateAST.h"
#include "Interpreter.h"
#include "Chunk.h"
#include "VM.h"

int LegacyMain(int argc, char* argv[])
{
	/*
	TestUnit::RunExpressionInterpreterTest();
	TestUnit::RunResolverTest();
	TestUnit::RunStatementInterpreterTest();
	TestUnit::RunFunctionInterpreterTest();
	TestUnit::RunClassInterpreterTest();
	*/

	Lox& lox = Lox::GetInstance();
	lox.Run(0, nullptr);
	lox.Run(argc - 1, argc > 1 ? &argv[1] : nullptr);

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
		"Logical : Expr left, Token op, Expr right",
		"Call : Expr callee, Token paren, List<Expr> arguments",
		"Lambda: Token keyword, List<Token> params, List<Stat> body",
		"Get: Expr object, Token name",
		"Set : Expr object, Token name, Expr value",
		"RootGet: Expr object, Token name",
		"This: Token keyword",
		"Super: Token keyword, Token method",
	});
	generator.DefineAST("G:/Lox/", "Stat",
	{
		"Expression : Expr expression",
		"Print : Expr expression",
		"Var : Token name, Expr initializer",
		"Block : List<Stat> statements",
		"If : Expr condition, Stat thenBranch, Stat elseBranch",
		"While : Expr condition, Stat body",
		"Break : Token keyword",
		"Function : Token name, List<Token> params, List<Stat> body",
		"Getter: Token name, List<Stat> body",
		"Return: Token keyword, Expr value",
		"Class: Token name, Expr superclass, List<Stat> methods, List<Stat> getters, List<Stat> classMethods",
	});*/

	return 0;
}

void TestChunk()
{
	VM vm;

	// 1 * 2 + 3
	{
		Chunk chunk;
		vm.Init();
		chunk.Init();

		uint32_t line = 1;
		uint32_t column = 1;

		uint32_t index = chunk.AddConstant(1);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(2);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		chunk.Write(OP_MULTIPLY, line, column);

		index = chunk.AddConstant(3);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		chunk.Write(OP_ADD, line, column);

		chunk.Write(OP_RETURN, line, column);
		vm.Interpret(&chunk);
		chunk.Free();
	}
	// 1 + 2 * 3
	{
		Chunk chunk;
		vm.Init();
		chunk.Init();

		uint32_t line = 1;
		uint32_t column = 1;

		uint32_t index = chunk.AddConstant(1);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(2);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(3);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		chunk.Write(OP_MULTIPLY, line, column);
		chunk.Write(OP_ADD, line, column);

		chunk.Write(OP_RETURN, line, column);
		vm.Interpret(&chunk);
		chunk.Free();
	}
	// 3 - 2 - 1
	{
		Chunk chunk;
		vm.Init();
		chunk.Init();

		uint32_t line = 1;
		uint32_t column = 1;

		uint32_t index = chunk.AddConstant(3);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(2);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		chunk.Write(OP_SUBTRACT, line, column);

		index = chunk.AddConstant(1);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		chunk.Write(OP_SUBTRACT, line, column);

		chunk.Write(OP_RETURN, line, column);
		vm.Interpret(&chunk);
		chunk.Free();
	}
	// (1 + 2 * 3) - (4 / -5)
	{
		Chunk chunk;
		vm.Init();
		chunk.Init();

		uint32_t line = 1;
		uint32_t column = 1;

		uint32_t index = chunk.AddConstant(1);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(2);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(3);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		chunk.Write(OP_MULTIPLY, line, column);
		chunk.Write(OP_ADD, line, column);

		index = chunk.AddConstant(4);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(5);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		chunk.Write(OP_NEGATE, line, column);
		chunk.Write(OP_DIVIDE, line, column);

		chunk.Write(OP_SUBTRACT, line, column);

		chunk.Write(OP_RETURN, line, column);
		vm.Interpret(&chunk);
		chunk.Free();
	}
	// 4 - 3 * -2 (alternative)
	{
		Chunk chunk;
		vm.Init();
		chunk.Init();

		uint32_t line = 1;
		uint32_t column = 1;

		uint32_t index = chunk.AddConstant(4);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(3);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(0);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(2);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		chunk.Write(OP_SUBTRACT, line, column);
		chunk.Write(OP_MULTIPLY, line, column);
		chunk.Write(OP_SUBTRACT, line, column);

		chunk.Write(OP_RETURN, line, column);
		vm.Interpret(&chunk);
		chunk.Free();
	}
	// 4 - 3 * -2
	{
		Chunk chunk;
		vm.Init();
		chunk.Init();

		uint32_t line = 1;
		uint32_t column = 1;

		uint32_t index = chunk.AddConstant(4);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(3);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		index = chunk.AddConstant(2);
		chunk.Write(OP_CONSTANT, line, column);
		chunk.Write(index, line, column);

		chunk.Write(OP_NEGATE, line, column);
		chunk.Write(OP_MULTIPLY, line, column);
		chunk.Write(OP_SUBTRACT, line, column);

		chunk.Write(OP_RETURN, line, column);
		vm.Interpret(&chunk);
		chunk.Free();
	}

	vm.Free();
}

int main(int argc, char* argv[])
{
	VM vm;
	vm.Init();
    //vm.Interpret("1+2/3");
	//vm.Interpret("1*2-3");
	//vm.Interpret("1+2+3");
	//vm.Interpret("(-1 + 2) * 3 - -4");
	vm.Interpret("1?2:3");
	vm.Interpret("1?2:3?4:5");
	vm.Interpret("1?2?3:4:5");
	vm.Free();
	return 0;
}	