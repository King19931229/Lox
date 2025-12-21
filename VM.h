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
	static constexpr uint32_t STACK_MAX = 256;
	Chunk* chunk = nullptr;
	uint8_t* ip = nullptr;

    VMValue* stacks = nullptr;
    size_t stackCapacity = STACK_MAX;
    VMValue* stackTop = nullptr;

	// Stack operations
	void ResetStack();
	void Push(VMValue value);
	void Negate();
	VMValue Pop();

	// VM lifecycle
	void Init();
	void Free();

	// Execution
	InterpretResult Run();
	InterpretResult Interpret(Chunk* chunk);
};