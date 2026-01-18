#include "VM.h"
#include "Compiler.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdarg>

// #define DEBUG_TRACE_EXECUTION

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
		stacks = GROW_ARRAY(VMValue, (VMValue*)nullptr, 0, stackCapacity);
		stackTop = stacks;
	}

	size_t count = (size_t)(stackTop - stacks);
	if (count + 1 > stackCapacity)
	{
		size_t oldCapacity = stackCapacity;
		size_t newCapacity = oldCapacity * 2;
		VMValue* newStacks = GROW_ARRAY(VMValue, stacks, oldCapacity, newCapacity);
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
		VMValue* newStacks = GROW_ARRAY(VMValue, stacks, oldCapacity, newCapacity);
		stacks = newStacks;
		stackTop = stacks + count;
		stackCapacity = newCapacity;
	}

	return value;
}

VMValue VM::Peek(int distance)
{
	if (stacks == nullptr || stackTop - distance - 1 < stacks)
	{
		// Handle stack underflow
		printf("Stack underflow!\n");
		exit(1);
	}
	return *(stackTop - distance - 1);
}

bool VM::IsNumber(VMValue value)
{
	return value->type == TYPE_INT || value->type == TYPE_FLOAT;
}

bool VM::IsFalsey(VMValue value)
{
	return value->type == TYPE_NIL || (value->type == TYPE_BOOL && !static_cast<bool>(*value));
}

void VM::RuntimeError(const char* format, ...)
{
	// compute current instruction index
	size_t instruction = (size_t)(ip - chunk->code - 1);
	int32_t line = chunk->lines[instruction];
	int32_t column = chunk->columns[instruction];
	fprintf(stderr, "VM RuntimeError [%d:%d]: ", line, column);

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	ResetStack();
}

InterpretResult VM::Negate()
{
	if (stacks == nullptr || stackTop == stacks)
	{
		printf("Stack underflow!\n");
		exit(1);
	}

	if (!IsNumber(Peek(0)))
	{
		RuntimeError("Operand must be a number!");
		return INTERPRET_RUNTIME_ERROR;
	}

	*(stackTop - 1) = -*(stackTop - 1);
	return INTERPRET_OK;
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

		if (!IsNumber(a) || !IsNumber(b))
		{
			RuntimeError("Operands must be numbers!");
			return INTERPRET_RUNTIME_ERROR;
		}

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
			case OP_GERATER:
				Push(a > b);
				break;
			case OP_LESS:
				Push(a < b);
				break;
			default:
				// Handle unknown operation (could throw an exception or abort)
				RuntimeError("Unknown binary operation!\n");
				return INTERPRET_RUNTIME_ERROR;
		}

		return INTERPRET_OK;
	};

	auto NOT_OP = [&]() {
		VMValue value = Pop();
		Push(BoolValue::Create(IsFalsey(value)));
		return INTERPRET_OK;
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
			case OP_NIL:
			{
				Push(NilValue::Create());
				break;
			}
			case OP_TRUE:
			{
				Push(BoolValue::Create(true));
				break;
			}
			case OP_FALSE:
			{
				Push(BoolValue::Create(false));
				break;
			}
			case OP_NEGATE:
			{
				InterpretResult result = Negate();
				if (result != INTERPRET_OK)
				{
					return result;
				}
				break;
			}
			case OP_ADD:
			case OP_SUBTRACT:
			case OP_MULTIPLY:
			case OP_DIVIDE:
			case OP_GERATER:
			case OP_LESS:
			{
				InterpretResult result = BINARY_OP((OpCode)opCode);
				if (result != INTERPRET_OK)
				{
					return result;
				}
				break;
			}
			case OP_NOT:
			{
				InterpretResult result = NOT_OP();
				if (result != INTERPRET_OK)
				{
					return result;
				}
				break;
			}
			case OP_EQUAL:
			{
				VMValue b = Pop();
				VMValue a = Pop();
				Push(BoolValue::Create(IsEqual(a, b)));
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