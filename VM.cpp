#include "VM.h"
#include "Compiler.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdarg>

// #define DEBUG_TRACE_EXECUTION

VM* VM::instance = nullptr;

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
	return value.value && (value.value->type == TYPE_INT || value.value->type == TYPE_FLOAT);
}

bool VM::IsFalsey(VMValue value)
{
	return !value.value || value.value->type == TYPE_NIL || (value.value->type == TYPE_BOOL && !static_cast<bool>(*value.value));
}

bool VM::IsString(VMValue value)
{
	return value.value && value.value->type == TYPE_STRING;
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

	(stackTop - 1)->value = ValNegate((stackTop - 1)->value);
	return INTERPRET_OK;
}

void VM::Init()
{
	chunk = nullptr;
	objects = nullptr;
	ResetStack();
}

void VM::Free()
{
	// Clean up all the VM allocated objects
	Value* object = objects;
	while (object != nullptr)
	{
		Value* next = object->next;
		delete object;
		object = next;
	}
	objects = nullptr;
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

		if (!(IsNumber(a) && IsNumber(b)))
		{
			RuntimeError("Operands must be numbers!");
			return INTERPRET_RUNTIME_ERROR;
		}

		switch (op)
		{
			case OP_ADD:
				Push(VMValue::Create(ValAdd(a.value, b.value)));
				break;
			case OP_SUBTRACT:
				Push(VMValue::Create(ValSub(a.value, b.value)));
				break;
			case OP_MULTIPLY:
				Push(VMValue::Create(ValMul(a.value, b.value)));
				break;
			case OP_DIVIDE:
				Push(VMValue::Create(ValDiv(a.value, b.value)));
				break;
			case OP_GERATER:
				Push(VMValue::Create(ValGreater(a.value, b.value)));
				break;
			case OP_LESS:
				Push(VMValue::Create(ValLess(a.value, b.value)));
				break;
			default:
				// Handle unknown operation (could throw an exception or abort)
				RuntimeError("Unknown binary operation!\n");
				return INTERPRET_RUNTIME_ERROR;
		}

		return INTERPRET_OK;
	};

	auto ADD_OP = [&]() {
		VMValue b = Pop();
		VMValue a = Pop();
		if (IsString(a) && IsString(b))
		{
			Push(VMValue::Create(ValAdd(a.value, b.value)));
			return INTERPRET_OK;
		}
		else if (IsNumber(a) && IsNumber(b))
		{
			Push(VMValue::Create(ValAdd(a.value, b.value)));
			return INTERPRET_OK;
		}
		else
		{
			RuntimeError("Operands must be two numbers or two strings for '+'.");
			return INTERPRET_RUNTIME_ERROR;
		}
	};

	auto NOT_OP = [&]() {
		VMValue value = Pop();
		Push(VMValue::Create(BoolValue::CreateRaw(IsFalsey(value))));
		return INTERPRET_OK;
	};

	const size_t INVALID_GLOBAL_SLOT = (size_t)-1;

	auto RESOLVE_OR_CREATE_GLOBAL_SLOT = [&](VMValue nameValue, size_t& outSlot) -> bool {
		if (!IsString(nameValue))
		{
			RuntimeError("Global variable name must be a string.");
			return false;
		}

		StringValue* stringValue = static_cast<StringValue*>(nameValue.value);
		size_t cachedSlot = stringValue->cachedGlobalSlot;
		if (cachedSlot != INVALID_GLOBAL_SLOT && cachedSlot < globalSlots.size())
		{
			outSlot = cachedSlot;
			return true;
		}

		auto it = globalNameToSlot.find(stringValue->value);
		if (it == globalNameToSlot.end())
		{
			size_t newSlot = globalSlots.size();
			globalNameToSlot[stringValue->value] = newSlot;
			globalSlots.push_back(VMValue());
			stringValue->cachedGlobalSlot = newSlot;
			outSlot = newSlot;
			return true;
		}

		outSlot = it->second;
		if (outSlot >= globalSlots.size())
		{
			globalSlots.resize(outSlot + 1);
		}
		stringValue->cachedGlobalSlot = outSlot;
		return true;
	};

	auto RESOLVE_EXISTING_GLOBAL_SLOT = [&](VMValue nameValue, size_t& outSlot) -> bool {
		if (!IsString(nameValue))
		{
			RuntimeError("Global variable name must be a string.");
			return false;
		}

		StringValue* stringValue = static_cast<StringValue*>(nameValue.value);
		size_t cachedSlot = stringValue->cachedGlobalSlot;
		if (cachedSlot != INVALID_GLOBAL_SLOT && cachedSlot < globalSlots.size())
		{
			outSlot = cachedSlot;
			return true;
		}

		auto it = globalNameToSlot.find(stringValue->value);
		if (it == globalNameToSlot.end())
		{
			RuntimeError("Undefined global variable '%s'.", stringValue->value.c_str());
			return false;
		}

		outSlot = it->second;
		if (outSlot >= globalSlots.size())
		{
			RuntimeError("Undefined global variable '%s'.", stringValue->value.c_str());
			return false;
		}

		stringValue->cachedGlobalSlot = outSlot;
		return true;
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
			case OP_CONSTANT_LONG:
			{
				VMValue value;
				if (opCode == OP_CONSTANT)
					value = READ_CONSTANT();
				else
					value = READ_LONG_CONSTANT();
				Push(value);
				break;
			}
			case OP_NIL:
			{
				Push(VMValue::Create(NilValue::CreateRaw()));
				break;
			}
			case OP_TRUE:
			{
				Push(VMValue::Create(BoolValue::CreateRaw(true)));
				break;
			}
			case OP_FALSE:
			{
				Push(VMValue::Create(BoolValue::CreateRaw(false)));
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
			{
				InterpretResult result = ADD_OP();
				if (result != INTERPRET_OK)
				{
					return result;
				}
				break;
			}
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
				Push(VMValue::Create(BoolValue::CreateRaw(IsEqual(a.value, b.value))));
				break;
			}
			case OP_PRINT:
			{
				VMValue value = Pop();
				chunk->PrintValue(value);
				printf("\n");
				break;
			}
			case OP_DEFINE_GLOBAL:
			case OP_DEFINE_GLOBAL_LONG:
			{
				VMValue nameValue;
				if (opCode == OP_DEFINE_GLOBAL)
					nameValue = READ_CONSTANT();
				else
					nameValue = READ_LONG_CONSTANT();

				size_t slot;
				if (!RESOLVE_OR_CREATE_GLOBAL_SLOT(nameValue, slot))
				{
					return INTERPRET_RUNTIME_ERROR;
				}

				globalSlots[slot] = Pop();
				break;
			}
			case OP_GET_GLOBAL:
			case OP_GET_GLOBAL_LONG:
			{
				VMValue nameValue;
				if (opCode == OP_GET_GLOBAL)
					nameValue = READ_CONSTANT();
				else
					nameValue = READ_LONG_CONSTANT();

				size_t slot;
				if (!RESOLVE_EXISTING_GLOBAL_SLOT(nameValue, slot))
				{
					return INTERPRET_RUNTIME_ERROR;
				}

				Push(globalSlots[slot]);
				break;
			}
			case OP_SET_GLOBAL:
			case OP_SET_GLOBAL_LONG:
			{
				VMValue nameValue;
				if (opCode == OP_SET_GLOBAL)
					nameValue = READ_CONSTANT();
				else
					nameValue = READ_LONG_CONSTANT();

				size_t slot;
				if (!RESOLVE_EXISTING_GLOBAL_SLOT(nameValue, slot))
				{
					return INTERPRET_RUNTIME_ERROR;
				}

				globalSlots[slot] = Peek(0);
				break;
			}
			case OP_RETURN:
			{
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