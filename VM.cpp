#include "VM.h"
#include "Compiler.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#define DEBUG_TRACE_EXECUTION

void VM::ResetStack()
{
	stackTop = stacks;
}

void VM::Push(VMValue value)
{
	// allocate initial stack if needed
	if (stacks == nullptr)
	{
		stackCapacity = STACK_MAX;
		stacks = (VMValue*)reallocate(nullptr, 0, sizeof(VMValue) * stackCapacity);
		stackTop = stacks;
	}

	size_t count = (size_t)(stackTop - stacks);
	if (count + 1 > stackCapacity)
	{
		size_t oldCapacity = stackCapacity;
		size_t newCapacity = oldCapacity * 2;
		VMValue* newStacks = (VMValue*)reallocate(stacks, sizeof(VMValue) * oldCapacity, sizeof(VMValue) * newCapacity);
		stacks = newStacks;
		stackTop = stacks + count;
		stackCapacity = newCapacity;
	}

	*stackTop++ = value;
}

VMValue VM::Pop()
{
	if (stacks == nullptr || stackTop == stacks)
	{
		// Handle stack underflow
		printf("Stack underflow!\n");
		exit(1);
	}

	VMValue value = *--stackTop;
	size_t count = (size_t)(stackTop - stacks);
	if (stackCapacity > STACK_MAX && count <= stackCapacity / 4)
	{
		size_t oldCapacity = stackCapacity;
		size_t newCapacity = oldCapacity / 2;
		if (newCapacity < STACK_MAX) newCapacity = STACK_MAX;
		VMValue* newStacks = (VMValue*)reallocate(stacks, sizeof(VMValue) * oldCapacity, sizeof(VMValue) * newCapacity);
		stacks = newStacks;
		stackTop = stacks + count;
		stackCapacity = newCapacity;
	}

	return value;
}

void VM::Negate()
{
	if (stacks == nullptr || stackTop == stacks)
	{
		printf("Stack underflow!\n");
		exit(1);
	}
	*(stackTop - 1) = -*(stackTop - 1);
}

void VM::Init()
{
	chunk = nullptr;
	ResetStack();
}

void VM::Free()
{
}

InterpretResult VM::Run()
{
	auto READ_BYTE = [&]() -> uint8_t {
		return *ip++;
	};

	auto READ_CONSTANT = [&]() -> VMValue
	{
		uint8_t constantIndex = READ_BYTE();
		return chunk->constants.values[constantIndex];
	};

	auto READ_LONG_CONSTANT = [&]() -> VMValue
	{
		uint32_t constantIndex = (READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE();
		return chunk->constants.values[constantIndex];
	};

	auto BINARY_OP = [&](OpCode op) {
		VMValue b = Pop();
		VMValue a = Pop();
		switch (op)
		{
			case OP_ADD:
				Push(a + b);
				break;
			case OP_SUBTRACT:
				Push(a - b);
				break;
			case OP_MULTIPLY:
				Push(a * b);
				break;
			case OP_DIVIDE:
				Push(a / b);
				break;
			default:
				// Handle unknown operation (could throw an exception or abort)
				printf("Unknown binary operation!\n");
				exit(1);
		}
	};

	while (chunk->code)
	{
		if (ip >= chunk->code + chunk->count)
		{
			break;
		}

#ifdef DEBUG_TRACE_EXECUTION
		chunk->DisassembleInstruction((uint32_t)(ip - chunk->code));
#endif

		uint8_t opCode = READ_BYTE();
		switch (opCode)
		{
			case OP_CONSTANT:
			{
				VMValue value = READ_CONSTANT();
				Push(value);
				break;
			}
			case OP_CONSTANT_LONG:
			{
				VMValue value = READ_LONG_CONSTANT();
				Push(value);
				break;
			}
			case OP_NEGATE:
			{
				Negate();
				break;
			}
			case OP_ADD:
			case OP_SUBTRACT:
			case OP_MULTIPLY:
			case OP_DIVIDE:
			{
				BINARY_OP((OpCode)opCode);
				break;
			}
			case OP_RETURN:
			{
				VMValue value = Pop();
				chunk->PrintValue(value);
				printf("\n");
				return INTERPRET_OK;
			}
		}
	}

	return INTERPRET_RUNTIME_ERROR;
}

InterpretResult VM::Interpret(Chunk* inChunk)
{
	chunk = inChunk;
	ip = chunk->code;
	return Run();
}

InterpretResult VM::Interpret(const char* source)
{
	Chunk localChunk;
	localChunk.Init();

	Compiler compiler;

	if (!compiler.Compile(source, &localChunk))
	{
		localChunk.Free();
		return INTERPRET_COMPILE_ERROR;
	}

	chunk = &localChunk;
	ip = chunk->code;

	InterpretResult result = Run();

	localChunk.Free();
	return result;
}

void VM::Repl()
{
	char line[1024];
	for (;;)
	{
		printf("> ");
		if (!fgets(line, sizeof(line), stdin))
		{
			printf("\n");
			break;
		}
		Interpret(line);
	}
}

static bool ReadFile(const char* path, std::string& outSource)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return false;
	}
	std::ostringstream contents;
	contents << file.rdbuf();
	outSource = contents.str();
	return true;
}

void VM::RunFile(const char* path)
{
	std::string source;
	if (!ReadFile(path, source))
	{
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}
	InterpretResult result = Interpret(source.c_str());
	if (result == INTERPRET_COMPILE_ERROR) exit(65);
	if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}