#pragma once
#include "Chunk.h"

enum InterpretResult
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
};

class VM
{
protected:
	static constexpr uint32_t STACK_MAX = 256;
	Chunk* chunk = nullptr;
	uint8_t* ip = nullptr;

    VMValue* stacks = nullptr;
    size_t stackCapacity = STACK_MAX;
    VMValue* stackTop = nullptr;

	// Stack operations
	void ResetStack();
	void Push(VMValue value);
	InterpretResult Negate();
	VMValue Pop();
	VMValue Peek(int distance);

	bool IsNumber(VMValue value);
	bool IsFalsey(VMValue value);

	void RuntimeError(const char* format, ...);

public:
	// VM lifecycle
	void Init();
	void Free();

	// Execution
	InterpretResult Run();
	InterpretResult Interpret(Chunk* chunk);
	InterpretResult Interpret(const char* source);

	void Repl();
	void RunFile(const char* path);
};