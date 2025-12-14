#pragma once
#include "Chunk.h"

enum InterpretResult
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
};

struct VM
{
	void Init();
	void Free();
	InterpretResult Interpret(Chunk* chunk);
};