#include "VM.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdarg>
#include <iostream>
#include <chrono>

#define DEBUG_TRACE_EXECUTION
#define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC
// #define USE_LOCAL_IP

namespace
{
	constexpr size_t INVALID_GLOBAL_SLOT = (size_t)-1;
}

static VMValue clock(int argCount, VMValue* args)
{
	static const auto startTime = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime);
	return VMValue(elapsed.count());
}

VM* VM::instance = nullptr;

void VM::UpvalueValue::Blacken(VM& vm)
{
	if (location != nullptr)
	{
		vm.MarkValue(*location);
	}
}

void VM::InnerValue::Blacken(VM& vm)
{
	vm.MarkValue(closure);
	vm.MarkValue(nextInner);
}

void VM::PushCompilerRoot(Compiler* compiler)
{
	if (compiler == nullptr)
	{
		return;
	}
	compilerRoots.push_back(compiler);
}

void VM::PopCompilerRoot(Compiler* compiler)
{
	if (compiler == nullptr || compilerRoots.empty())
	{
		return;
	}

	for (auto it = compilerRoots.rbegin(); it != compilerRoots.rend(); ++it)
	{
		if (*it == compiler)
		{
			compilerRoots.erase((it + 1).base());
			return;
		}
	}
}

void VM::ResetStack()
{
	stackTop = stacks;
}

void VM::AdjustFrameSlots(VMValue* oldStacks, VMValue* newStacks)
{
	if (oldStacks == nullptr || newStacks == nullptr || oldStacks == newStacks)
	{
		return;
	}

	// Rebase cached frame pointers after the stack buffer moves.
	for (uint32_t i = 0; i < frameCount; ++i)
	{
		if (frames[i].slots != nullptr)
		{
			frames[i].slots = newStacks + (frames[i].slots - oldStacks);
		}
	}

	UpvalueValue* upvalue = openUpvalues;
	while (upvalue != nullptr)
	{
		upvalue->location = newStacks + (upvalue->location - oldStacks);
		upvalue = upvalue->nextUpvalue;
	}
}

void VM::Push(VMValue value)
{
	// Allocate the stack lazily so empty VMs do not pay the upfront cost.
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
		VMValue* oldStacks = stacks;
		VMValue* newStacks = GROW_ARRAY(VMValue, stacks, oldCapacity, newCapacity);
		// Growing the stack can move the buffer, so every frame slot pointer must be rebased.
		stacks = newStacks;
		AdjustFrameSlots(oldStacks, newStacks);
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
		VMValue* oldStacks = stacks;
		VMValue* newStacks = GROW_ARRAY(VMValue, stacks, oldCapacity, newCapacity);
		stacks = newStacks;
		AdjustFrameSlots(oldStacks, newStacks);
		stackTop = stacks + count;
		stackCapacity = newCapacity;
	}

	return value;
}

VMValue VM::Peek(int32_t distance)
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
	return value.type == TYPE_INT || value.type == TYPE_FLOAT;
}

bool VM::IsFalsey(VMValue value)
{
	return value.type == TYPE_NIL || value.type == TYPE_ERROR ||
		(value.type == TYPE_BOOL && !value.boolean);
}

bool VM::IsString(VMValue value)
{
	return value.type == TYPE_STRING && value.object;
}

bool VM::ResolveOrCreateGlobalSlot(VMValue nameValue, size_t& outSlot, const uint8_t* instructionIp)
{
	if (!IsString(nameValue))
	{
		RuntimeError(instructionIp, "Global variable name must be a string.");
		return false;
	}

	StringValue* stringValue = static_cast<StringValue*>(nameValue.object);
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
}

bool VM::ResolveExistingGlobalSlot(VMValue nameValue, size_t& outSlot, const uint8_t* instructionIp)
{
	if (!IsString(nameValue))
	{
		RuntimeError(instructionIp, "Global variable name must be a string.");
		return false;
	}

	StringValue* stringValue = static_cast<StringValue*>(nameValue.object);
	size_t cachedSlot = stringValue->cachedGlobalSlot;
	if (cachedSlot != INVALID_GLOBAL_SLOT && cachedSlot < globalSlots.size())
	{
		outSlot = cachedSlot;
		return true;
	}

	auto it = globalNameToSlot.find(stringValue->value);
	if (it == globalNameToSlot.end())
	{
		RuntimeError(instructionIp, "Undefined global variable '%s'.", stringValue->value.c_str());
		return false;
	}

	outSlot = it->second;
	if (outSlot >= globalSlots.size())
	{
		RuntimeError(instructionIp, "Undefined global variable '%s'.", stringValue->value.c_str());
		return false;
	}

	stringValue->cachedGlobalSlot = outSlot;
	return true;
}

void VM::RuntimeErrorImpl(const uint8_t* instructionIp, const char* format, va_list args)
{
	int32_t line = 0;
	int32_t column = 0;
	const uint8_t* resolvedInstructionIp = instructionIp;
	if (resolvedInstructionIp == nullptr && frameCount > 0)
	{
		resolvedInstructionIp = frames[frameCount - 1].ip;
	}

	if (frameCount > 0 && resolvedInstructionIp != nullptr)
	{
		CallFrame& currentFrame = frames[frameCount - 1];
		Chunk* chunk = currentFrame.GetChunk();
		if (chunk != nullptr && chunk->code != nullptr && chunk->count > 0 && resolvedInstructionIp > chunk->code)
		{
			size_t instruction = (size_t)(resolvedInstructionIp - chunk->code - 1);
			if (instruction >= (size_t)chunk->count)
			{
				instruction = (size_t)(chunk->count - 1);
			}
			line = chunk->lines[instruction];
			column = chunk->columns[instruction];
		}
	}
	fprintf(stderr, "VM RuntimeError [%d:%d]: ", line, column);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");

	while (openUpvalues != nullptr)
	{
		UpvalueValue* upvalue = openUpvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		openUpvalues = upvalue->nextUpvalue;
		upvalue->nextUpvalue = nullptr;
	}

	ResetStack();
	frameCount = 0;
}

void VM::RuntimeError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	RuntimeErrorImpl(nullptr, format, args);
	va_end(args);
}

void VM::RuntimeError(const uint8_t* instructionIp, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	RuntimeErrorImpl(instructionIp, format, args);
	va_end(args);
}

InterpretResult VM::Negate(const uint8_t* instructionIp)
{
	if (stacks == nullptr || stackTop == stacks)
	{
		printf("Stack underflow!\n");
		exit(1);
	}

	if (!IsNumber(Peek(0)))
	{
		RuntimeError(instructionIp, "Operand must be a number!");
		return INTERPRET_RUNTIME_ERROR;
	}

	VMValue top = *(stackTop - 1);
	*(stackTop - 1) = top.type == TYPE_INT ? VMValue(-top.integer) : VMValue(-top.number);
	return INTERPRET_OK;
}

void VM::Init()
{
	objects = nullptr;
	openUpvalues = nullptr;
	frameCount = 0;
	bytesAllocated = 0;
	nextGC = INITIAL_GC_THRESHOLD;
	ResetStack();
	DefineNative("clock", clock, 0);
}

void VM::Reset()
{
	Free();
	globalNameToSlot.clear();
	globalSlots.clear();
	compilerRoots.clear();
	Init();
}

void VM::Free()
{
	while (objects != nullptr)
	{
		FreeValue(objects);
	}

	if (stacks != nullptr)
	{
		FREE_ARRAY(VMValue, stacks, stackCapacity);
		stacks = nullptr;
	}
	stackTop = nullptr;
	stackCapacity = 0;

	if (grayStack != nullptr)
	{
		FREE_ARRAY(Value*, grayStack, grayStackCapacity);
		grayStack = nullptr;
	}
	grayStackCapacity = 0;
	grayStackCount = 0;
	bytesAllocated = 0;
}

void VM::FreeValue(Value* object)
{
	if (object == nullptr)
	{
		return;
	}

	// Remove from the intrusive linked list before deleting
	if (objects == object)
	{
		objects = object->nextGCValue;
	}
	else
	{
		for (Value* curr = objects; curr != nullptr; curr = curr->nextGCValue)
		{
			if (curr->nextGCValue == object)
			{
				curr->nextGCValue = object->nextGCValue;
				break;
			}
		}
	}

	size_t objectSize = object->Size();
	if (objectSize <= bytesAllocated)
	{
		bytesAllocated -= objectSize;
	}
	else
	{
		bytesAllocated = 0;
	}

#ifdef DEBUG_LOG_GC
	printf("  Freed object %p of type %s (%zu bytes, %zu remaining)\n",
		(void*)object,
		ValueTypeToString(object->type),
		objectSize,
		bytesAllocated);
#endif

	delete object;
}

Value* VM::AllocValue(Value* value)
{
	if (!value) return nullptr;
	size_t objectSize = value->Size();

#ifdef DEBUG_STRESS_GC
	CollectGarbage();
#endif
	if (bytesAllocated + objectSize > nextGC)
	{
		CollectGarbage();
	}

	value->nextGCValue = objects;
	value->markedValue = !currentMarkValue;
	objects = value;
	bytesAllocated += objectSize;

#ifdef DEBUG_LOG_GC
	printf("  Allocated object %p of type %s (%zu bytes, %zu total)\n",
		(void*)value,
		ValueTypeToString(value->type),
		objectSize,
		bytesAllocated);
#endif

	return value;
}

VMValue VM::Create(Value* value)
{
	if (value == nullptr)
	{
		return VMValue();
	}
	switch (value->type)
	{
		case TYPE_INT:
		{
			int result = static_cast<IntValue*>(value)->value;
			delete value;
			return VMValue(result);
		}
		case TYPE_FLOAT:
		{
			float result = static_cast<FloatValue*>(value)->value;
			delete value;
			return VMValue(result);
		}
		case TYPE_BOOL:
		{
			bool result = static_cast<BoolValue*>(value)->value;
			delete value;
			return VMValue(result);
		}
		case TYPE_NIL:
			delete value;
			return VMValue::Nil();
		default:
			break;
	}
	Value* object = GetInstance().AllocValue(value);
	return object ? VMValue(object) : VMValue();
}

VMValue VM::CaptureUpvalue(VMValue* local)
{
	UpvalueValue* upvalue = openUpvalues;
	UpvalueValue* previousUpvalue = nullptr;

	while (upvalue && upvalue->location > local)
	{
		previousUpvalue = upvalue;
		upvalue = upvalue->nextUpvalue;
	}

	if (upvalue && upvalue->location == local)
	{
		return VMValue(upvalue);
	}

	UpvalueValue* uv = new UpvalueValue(local);
	uv->location = local;
	AllocValue(uv);
	uv->nextUpvalue = upvalue;

	if (previousUpvalue)
	{
		previousUpvalue->nextUpvalue = uv;
	}
	else
	{
		openUpvalues = uv;
	}

	return VMValue(uv);
}

void VM::CloseUpvalues(VMValue* last)
{
	while (openUpvalues != nullptr)
	{
		UpvalueValue* upvalue = openUpvalues;
		if (upvalue->location < last)
		{
			break;
		}
		// Copy the value from the location to the closed value.
		upvalue->closed = *upvalue->location;
		// Set the location to the closed value.
		upvalue->location = &upvalue->closed;
		openUpvalues = upvalue->nextUpvalue;
		upvalue->nextUpvalue = nullptr;
	}
}

InterpretResult VM::Run()
{
#ifdef USE_LOCAL_IP
	uint8_t* ip = frames[frameCount - 1].ip;
#define IP ip
#else
#define IP frames[frameCount - 1].ip
#endif

	auto READ_BYTE = [&]() -> uint8_t {
		uint8_t byte = *IP++;
		return byte;
	};

	auto READ_SHORT = [&]() -> uint16_t {
		uint16_t value = (*IP << 8) | *(IP + 1);
		IP += 2;
		return value;
	};

	auto READ_THREE_BYTE = [&]() -> uint32_t {
		uint32_t value = (*IP << 16) | (*(IP + 1) << 8) | *(IP + 2);
		IP += 3;
		return value;
	};

	auto READ_CONSTANT = [&]() -> VMValue
	{
		uint8_t constantIndex = READ_BYTE();
		return frames[frameCount - 1].GetChunk()->constants.values[constantIndex];
	};

	auto READ_LONG_CONSTANT = [&]() -> VMValue
	{
		uint32_t constantIndex = (READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE();
		return frames[frameCount - 1].GetChunk()->constants.values[constantIndex];
	};

	auto READ_LOCAL_SLOT = [&]() -> uint32_t {
		return (uint32_t)READ_BYTE();
	};

	auto READ_LONG_LOCAL_SLOT = [&]() -> uint32_t {
		return ((uint32_t)READ_BYTE() << 16) | ((uint32_t)READ_BYTE() << 8) | (uint32_t)READ_BYTE();
	};

	auto BINARY_OP = [&](OpCode op) {
		VMValue b = Pop();
		VMValue a = Pop();

		if (!(IsNumber(a) && IsNumber(b)))
		{
			RuntimeError(IP, "Operands must be numbers!");
			return INTERPRET_RUNTIME_ERROR;
		}

		bool bothIntegers = a.type == TYPE_INT && b.type == TYPE_INT;
		float aNumber = a.type == TYPE_INT ? (float)a.integer : a.number;
		float bNumber = b.type == TYPE_INT ? (float)b.integer : b.number;
		switch (op)
		{
			case OP_SUBTRACT:
				Push(bothIntegers ? VMValue(a.integer - b.integer) : VMValue(aNumber - bNumber));
				break;
			case OP_MULTIPLY:
				Push(bothIntegers ? VMValue(a.integer * b.integer) : VMValue(aNumber * bNumber));
				break;
			case OP_DIVIDE:
			{
				if (bNumber == 0.0f)
				{
					RuntimeError(IP, "Division by zero.");
					return INTERPRET_RUNTIME_ERROR;
				}
				Push(bothIntegers ? VMValue(a.integer / b.integer) : VMValue(aNumber / bNumber));
				break;
			}
			case OP_GREATER:
				Push(VMValue(aNumber > bNumber));
				break;
			case OP_LESS:
				Push(VMValue(aNumber < bNumber));
				break;
			default:
				RuntimeError(IP, "Unknown binary operation!\n");
				return INTERPRET_RUNTIME_ERROR;
		}

		return INTERPRET_OK;
	};

	auto ADD_OP = [&]() {
		VMValue b = Pop();
		VMValue a = Pop();
		if (IsString(a) && IsString(b))
		{
			std::string result = static_cast<StringValue*>(a.object)->value +
				static_cast<StringValue*>(b.object)->value;
			Push(VM::Create(StringValue::CreateRaw(result)));
		}
		else if (IsNumber(a) && IsNumber(b))
		{
			if (a.type == TYPE_INT && b.type == TYPE_INT)
			{
				Push(VMValue(a.integer + b.integer));
			}
			else
			{
				float aNumber = a.type == TYPE_INT ? (float)a.integer : a.number;
				float bNumber = b.type == TYPE_INT ? (float)b.integer : b.number;
				Push(VMValue(aNumber + bNumber));
			}
		}
		else
		{
			RuntimeError(IP, "Operands must be two numbers or two strings for '+'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		return INTERPRET_OK;
	};

	auto NOT_OP = [&]() {
		VMValue value = Pop();
		Push(VMValue(IsFalsey(value)));
		return INTERPRET_OK;
	};

	while (frames[frameCount - 1].GetChunk()->code)
	{
		if (IP >= frames[frameCount - 1].GetChunk()->code + frames[frameCount - 1].GetChunk()->count)
		{
			break;
		}

#ifdef DEBUG_TRACE_EXECUTION
			printf("> ");
			frames[frameCount - 1].GetChunk()->DisassembleInstruction((uint32_t)(IP - frames[frameCount - 1].GetChunk()->code), frameCount - 1);
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
				Push(VMValue::Nil());
				break;
			}
			case OP_TRUE:
			{
				Push(VMValue(true));
				break;
			}
			case OP_FALSE:
			{
				Push(VMValue(false));
				break;
			}
			case OP_NEGATE:
			{
				InterpretResult result = Negate(IP);
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
			case OP_GREATER:
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
				Push(VMValue(IsEqual(a, b)));
				break;
			}
			case OP_PRINT:
			{
				VMValue value = Pop();
				frames[frameCount - 1].GetChunk()->PrintValueStdout(value);
				std::cout << std::endl;
				break;
			}
			case OP_POP:
			{
				Pop();
				break;
			}
			case OP_DUP:
			{
				Push(Peek(0));
				break;
			}
			case OP_NOP:
			{
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
				if (!ResolveOrCreateGlobalSlot(nameValue, slot, IP))
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
				if (!ResolveExistingGlobalSlot(nameValue, slot, IP))
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
				if (!ResolveExistingGlobalSlot(nameValue, slot, IP))
				{
					return INTERPRET_RUNTIME_ERROR;
				}

				globalSlots[slot] = Peek(0);
				break;
			}
			case OP_GET_LOCAL:
			case OP_GET_LOCAL_LONG:
			{
				uint32_t slot = (opCode == OP_GET_LOCAL) ? READ_LOCAL_SLOT() : READ_LONG_LOCAL_SLOT();
				// Local slots are addressed relative to the current frame's base slot.
				if (frames[frameCount - 1].slots == nullptr || slot >= (uint32_t)(stackTop - frames[frameCount - 1].slots))
				{
					RuntimeError(IP, "Local slot %d out of range.", slot);
					return INTERPRET_RUNTIME_ERROR;
				}
				Push(frames[frameCount - 1].slots[slot]);
				break;
			}
			case OP_SET_LOCAL:
			case OP_SET_LOCAL_LONG:
			{
				uint32_t slot = (opCode == OP_SET_LOCAL) ? READ_LOCAL_SLOT() : READ_LONG_LOCAL_SLOT();
				// Writing through the frame base updates the live local variable in place.
				if (frames[frameCount - 1].slots == nullptr || slot >= (uint32_t)(stackTop - frames[frameCount - 1].slots))
				{
					RuntimeError(IP, "Local slot out of range.");
					return INTERPRET_RUNTIME_ERROR;
				}
				frames[frameCount - 1].slots[slot] = Peek(0);
				break;
			}
			case OP_JUMP_IF_FALSE:
			{
				uint16_t offset = READ_SHORT();
				if (IsFalsey(Peek(0)))
				{
					IP += offset;
				}
				break;
			}
			case OP_JUMP:
			{
				uint16_t offset = READ_SHORT();
				IP += offset;
				break;
			}
			case OP_LOOP:
			{
				uint16_t offset = READ_SHORT();
				IP -= offset;
				break;
			}
			case OP_CALL:
			{
				uint8_t argCount = READ_BYTE();
				// The callee sits below its arguments on the stack.
				VMValue callee = Peek(argCount);
				// Update the instruction pointer before calling so the callee can return to the correct place.
				frames[frameCount - 1].ip = IP;
				if (!Call(callee, argCount, IP))
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				IP = frames[frameCount - 1].ip;
				break;
			}
			case OP_ROOT_INVOKE:
			case OP_ROOT_INVOKE_LONG:
			{
				VMValue nameValue = (opCode == OP_ROOT_INVOKE) ? READ_CONSTANT() : READ_LONG_CONSTANT();
				if (nameValue.type != TYPE_STRING || nameValue.object == nullptr)
				{
					RuntimeError(IP, "Method name must be a string.");
					return INTERPRET_RUNTIME_ERROR;
				}

				uint8_t argCountValue = READ_BYTE();
				VMValue receiver = stackTop[-argCountValue - 1];
				if (receiver.type != TYPE_INSTANCE || receiver.object == nullptr)
				{
					RuntimeError(IP, "Only instances have methods.");
					return INTERPRET_RUNTIME_ERROR;
				}

				const std::string& methodName = static_cast<StringValue*>(nameValue.object)->value;
				Compiler::VMInstanceValue* instance = static_cast<Compiler::VMInstanceValue*>(receiver.object);
				Compiler::VMClassValue* klass = static_cast<Compiler::VMClassValue*>(instance->classValue.object);
				std::vector<VMValue> methods;
				Compiler::VMClassValue* current = klass;
				while (current)
				{
					VMValue method = current->FindDirectMethod(methodName);
					if (method.object)
					{
						methods.push_back(method);
					}
					current = current->superClass.object ? static_cast<Compiler::VMClassValue*>(current->superClass.object) : nullptr;
				}

				if (methods.empty())
				{
					RuntimeError(IP, "Undefined method '%s'.", methodName.c_str());
					return INTERPRET_RUNTIME_ERROR;
				}

				// methods is derived-to-base. Build the inner chain in the
				// opposite direction so the base method sees the next derived method.
				VMValue nextInner;
				for (size_t i = 0; i + 1 < methods.size(); ++i)
				{
					nextInner = VM::Create(new InnerValue(methods[i], nextInner));
					Push(nextInner);
				}
				stackTop -= (int32_t)(methods.size() - 1);

				frames[frameCount - 1].ip = IP;
				if (!Invoke(receiver, methods.back(), argCountValue, IP))
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				frames[frameCount - 1].inner = nextInner;
				IP = frames[frameCount - 1].ip;
				break;
			}
			case OP_INNER_INVOKE:
			{
				uint8_t argCountValue = READ_BYTE();
				CallFrame* caller = &frames[frameCount - 1];
				VMValue inner = caller->inner;
				if (!inner.IsValid())
				{
					RuntimeError(IP, "inner() can only be used during a root invocation.");
					return INTERPRET_RUNTIME_ERROR;
				}
				if (inner.type != TYPE_INNER_VALUE || inner.object == nullptr)
				{
					RuntimeError(IP, "Inner value expected for invocation.");
					return INTERPRET_RUNTIME_ERROR;
				}
				VMValue instance = stackTop[-argCountValue - 1];
				InnerValue* innerValue = static_cast<InnerValue*>(inner.object);
				caller->ip = IP;
				if (!Invoke(instance, innerValue->closure, argCountValue, IP))
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				frames[frameCount - 1].inner = innerValue->nextInner;
				IP = frames[frameCount - 1].ip;
				break;
			}
			case OP_RETURN:
			{
				CallFrame* frame = &frames[frameCount - 1];
				VMValue returnValue = Pop();
				// Close all open upvalues owned by this frame before unwinding.
				CloseUpvalues(frame->slots);
				if (frameCount == 1)
				{
					ResetStack();
					frameCount = 0;
					return INTERPRET_OK;
				}
				// Restore the stack to the callee slot so the caller's locals stay intact.
				stackTop = frame->slots;
				*stackTop++ = returnValue;
				--frameCount;
				// Update the instruction pointer to the caller frame's IP so execution continues from there.
				IP = frames[frameCount - 1].ip;
				break;
			}
			case OP_CLOSURE:
			{
				VMValue functionValue = Pop();
				if (functionValue.type != TYPE_CALLABLE || functionValue.object == nullptr)
				{
					RuntimeError(IP, "Can only create closures from function values.");
					return INTERPRET_RUNTIME_ERROR;
				}
				std::vector<VMValue> upvalues;
				uint8_t upvalueCount = READ_BYTE();
				for (int32_t i = 0; i < upvalueCount; ++i)
				{
					uint8_t isLocal = READ_BYTE();
					uint32_t index = (uint32_t)READ_BYTE();
					if (isLocal)
					{
						if (frames[frameCount - 1].slots == nullptr || index >= (uint32_t)(stackTop - frames[frameCount - 1].slots))
						{
							RuntimeError(IP, "Local slot index out of range for closure.");
							return INTERPRET_RUNTIME_ERROR;
						}
						// Capture the local variable by creating an upvalue that points to the variable's slot on the stack.
						// This is a open upvalue that will be closed when the variable goes out of scope.
						VMValue capturedValue = CaptureUpvalue(&frames[frameCount - 1].slots[index]);
						upvalues.push_back(capturedValue);
					}
					else
					{
						if (index >= frames[frameCount - 1].GetUpvalues().size())
						{
							RuntimeError(IP, "Upvalue index out of range for closure.");
							return INTERPRET_RUNTIME_ERROR;
						}
						upvalues.push_back(frames[frameCount - 1].GetUpvalues()[index]);
					}
				}
				VMValue closure = VM::Create(new Compiler::VMClosureValue(functionValue, upvalues));
				Push(closure);
				break;
			}
			case OP_GET_UPVALUE:
			{
				uint8_t index = READ_BYTE();
				CallFrame* frame = &frames[frameCount - 1];
				if (index >= frame->GetUpvalues().size())
				{
					RuntimeError(IP, "Upvalue index out of range.");
					return INTERPRET_RUNTIME_ERROR;
				}
				VMValue value = frame->GetUpvalues()[index];
				UpvalueValue* upvalue = static_cast<UpvalueValue*>(value.object);
				Push(*upvalue->location);
				break;
			}
			case OP_SET_UPVALUE:
			{
				uint8_t index = READ_BYTE();
				CallFrame* frame = &frames[frameCount - 1];
				if (index >= frame->GetUpvalues().size())
				{
					RuntimeError(IP, "Upvalue index out of range.");
					return INTERPRET_RUNTIME_ERROR;
				}
				VMValue newValue = Peek(0);
				UpvalueValue* upvalue = static_cast<UpvalueValue*>(frame->GetUpvalues()[index].object);
				*upvalue->location = newValue;
				break;
			}
			case OP_CLOSE_UPVALUE:
			{
				CallFrame* frame = &frames[frameCount - 1];
				CloseUpvalues(stackTop - 1);
				Pop();
				break;
			}
			case OP_CLASS:
			{
				VMValue nameValue = READ_CONSTANT();
				if (nameValue.type != TYPE_STRING || nameValue.object == nullptr)
				{
					RuntimeError(IP, "Class name must be a string.");
					return INTERPRET_RUNTIME_ERROR;
				}
				VMValue classValue = VM::Create(new Compiler::VMClassValue(static_cast<StringValue*>(nameValue.object)->value));
				Push(classValue);
				break;
			}
			case OP_INVOKE:
			case OP_INVOKE_LONG:
			{
				VMValue nameValue = (opCode == OP_INVOKE) ? READ_CONSTANT() : READ_LONG_CONSTANT();
				if (nameValue.type != TYPE_STRING || nameValue.object == nullptr)
				{
					RuntimeError(IP, "Method name must be a string.");
					return INTERPRET_RUNTIME_ERROR;
				}

				uint8_t argCountValue = READ_BYTE();
				uint32_t cacheIndex = (opCode == OP_INVOKE) ? READ_BYTE() : READ_THREE_BYTE();

				VMValue object = stackTop[-argCountValue - 1];
				if (object.type != TYPE_INSTANCE || object.object == nullptr)
				{
					RuntimeError(IP, "Only instances have methods.");
					return INTERPRET_RUNTIME_ERROR;
				}

				Compiler::VMInstanceValue* instance = static_cast<Compiler::VMInstanceValue*>(object.object);
				const std::string& propertyName = static_cast<StringValue*>(nameValue.object)->value;
				// Keep the caller IP up to date before either call path can push a frame.
				frames[frameCount - 1].ip = IP;
				if (!InvokeFromClass(instance->classValue, object, propertyName, argCountValue, cacheIndex, IP))
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				IP = frames[frameCount - 1].ip;
				break;
			}
			case OP_GET_PROPERTY:
			case OP_GET_PROPERTY_LONG:
			{
				Chunk* chunk = frames[frameCount - 1].GetChunk();
				uint32_t constantIndex;
				uint32_t cacheIndex;
				if (opCode == OP_GET_PROPERTY)
				{
					constantIndex = READ_BYTE();
					cacheIndex = READ_BYTE();
				}
				else
				{
					constantIndex = READ_THREE_BYTE();
					cacheIndex = READ_THREE_BYTE();
				}

				VMValue object = Peek(0);
				if (object.type != TYPE_INSTANCE || object.object == nullptr)
				{
					RuntimeError(IP, "Only instances have properties.");
					return INTERPRET_RUNTIME_ERROR;
				}
				Compiler::VMInstanceValue* instance = static_cast<Compiler::VMInstanceValue*>(object.object);
				Compiler::VMClassValue* klass = static_cast<Compiler::VMClassValue*>(instance->classValue.object);

				VMValue nameValue = chunk->constants.values[constantIndex];
				if (nameValue.type != TYPE_STRING || nameValue.object == nullptr)
				{
					RuntimeError(IP, "Property name must be a string.");
					return INTERPRET_RUNTIME_ERROR;
				}
				const std::string& propertyName = static_cast<StringValue*>(nameValue.object)->value;

				InlineCache& cache = chunk->GetInlineCache(cacheIndex);
				uint32_t slot = Compiler::VMClassValue::INVALID_SLOT;
				VMValue method;
				const InlineCache::Entry* entry = cache.Match(klass, klass->slotNum);
				if (entry)
				{
					slot = entry->slot;
					method = entry->method;
				}
				else
				{
					slot = klass->GetSlot(propertyName);
					method = klass->FindMethod(propertyName);
					cache.Update(klass, klass->slotNum, slot, method);
				}

				VMValue valueToGet = (slot != Compiler::VMClassValue::INVALID_SLOT) ? instance->GetField(slot) : VMValue();
				if (valueToGet.IsValid())
				{
					// Pop the instance
					Pop();
					Push(valueToGet);
				}
				else
				{
					if (method.object)
					{
						VMValue boundMethod = VM::Create(new Compiler::BoundMethodValue(object, method));
						Pop();
						Push(boundMethod);
					}
					else
					{
						RuntimeError(IP, "Undefined property '%s'.", propertyName.c_str());
						return INTERPRET_RUNTIME_ERROR;
					}
				}
				break;
			}
			case OP_SET_PROPERTY:
			case OP_SET_PROPERTY_LONG:
			{
				Chunk* chunk = frames[frameCount - 1].GetChunk();
				uint32_t constantIndex;
				uint32_t cacheIndex;
				if (opCode == OP_SET_PROPERTY)
				{
					constantIndex = READ_BYTE();
					cacheIndex = READ_BYTE();
				}
				else
				{
					constantIndex = READ_THREE_BYTE();
					cacheIndex = READ_THREE_BYTE();
				}

				VMValue nameValue = chunk->constants.values[constantIndex];
				if (nameValue.type != TYPE_STRING || nameValue.object == nullptr)
				{
					RuntimeError(IP, "Property name must be a string.");
					return INTERPRET_RUNTIME_ERROR;
				}
				VMValue valueToSet = Pop();
				VMValue object = Pop();
				if (object.type != TYPE_INSTANCE || object.object == nullptr)
				{
					RuntimeError(IP, "Only instances have properties.");
					return INTERPRET_RUNTIME_ERROR;
				}
				Compiler::VMInstanceValue* instance = static_cast<Compiler::VMInstanceValue*>(object.object);
				Compiler::VMClassValue* klass = static_cast<Compiler::VMClassValue*>(instance->classValue.object);
				const std::string& propertyName = static_cast<StringValue*>(nameValue.object)->value;

				InlineCache& cache = chunk->GetInlineCache(cacheIndex);
				const InlineCache::Entry* entry = cache.Match(klass, klass->slotNum);
				uint32_t slot;
				if (entry)
				{
					slot = entry->slot;
				}
				else
				{
					slot = klass->GetOrCreateSlot(propertyName);
					cache.Update(klass, klass->slotNum, slot, VMValue());
				}
				instance->SetField(slot, valueToSet);
				Push(valueToSet);
				break;
			}
			case OP_GET_INDEX:
			{
				VMValue nameValue = Pop();
				if (nameValue.type != TYPE_STRING || nameValue.object == nullptr)
				{
					RuntimeError(IP, "Property name must be a string.");
					return INTERPRET_RUNTIME_ERROR;
				}
				VMValue object = Pop();
				if (object.type != TYPE_INSTANCE || object.object == nullptr)
				{
					RuntimeError(IP, "Only instances can be indexed.");
					return INTERPRET_RUNTIME_ERROR;
				}
				Compiler::VMInstanceValue* instance = static_cast<Compiler::VMInstanceValue*>(object.object);
				Compiler::VMClassValue* klass = static_cast<Compiler::VMClassValue*>(instance->classValue.object);
				const std::string& propertyName = static_cast<StringValue*>(nameValue.object)->value;
				uint32_t slot = klass->GetSlot(propertyName);
				if (slot == Compiler::VMClassValue::INVALID_SLOT)
				{
					RuntimeError(IP, "Undefined property '%s'.", propertyName.c_str());
					return INTERPRET_RUNTIME_ERROR;
				}
				VMValue valueToGet = instance->GetField(slot);
				if (!valueToGet.IsValid())
				{
					RuntimeError(IP, "Undefined property '%s'.", propertyName.c_str());
					return INTERPRET_RUNTIME_ERROR;
				}
				Push(valueToGet);
				break;
			}
			case OP_SET_INDEX:
			{
				VMValue valueToSet = Pop();
				VMValue nameValue = Pop();
				if (nameValue.type != TYPE_STRING || nameValue.object == nullptr)
				{
					RuntimeError(IP, "Property name must be a string.");
					return INTERPRET_RUNTIME_ERROR;
				}
				VMValue object = Pop();
				if (object.type != TYPE_INSTANCE || object.object == nullptr)
				{
					RuntimeError(IP, "Only instances have properties.");
					return INTERPRET_RUNTIME_ERROR;
				}
				Compiler::VMInstanceValue* instance = static_cast<Compiler::VMInstanceValue*>(object.object);
				Compiler::VMClassValue* klass = static_cast<Compiler::VMClassValue*>(instance->classValue.object);
				const std::string& propertyName = static_cast<StringValue*>(nameValue.object)->value;
				uint32_t slot = klass->GetOrCreateSlot(propertyName);
				instance->SetField(slot, valueToSet);
				Push(valueToSet);
				break;
			}
			case OP_METHOD:
			case OP_METHOD_LONG:
			{
				VMValue nameValue;
				if (opCode == OP_METHOD)
					nameValue = READ_CONSTANT();
				else
					nameValue = READ_LONG_CONSTANT();
				VMValue methodValue = Pop();
				VMValue classValue = Peek(0);
				if (classValue.type != TYPE_CLASS || classValue.object == nullptr)
				{
					RuntimeError(IP, "Only classes can have methods.");
					return INTERPRET_RUNTIME_ERROR;
				}
				Compiler::VMClassValue* klass = static_cast<Compiler::VMClassValue*>(classValue.object);
				if (methodValue.type != TYPE_CALLABLE || methodValue.object == nullptr)
				{
					RuntimeError(IP, "Method must be a callable.");
					return INTERPRET_RUNTIME_ERROR;
				}
				Compiler::VMFunctionBase* functionValue = static_cast<Compiler::VMFunctionBase*>(methodValue.object);
				if (functionValue->GetType() != Compiler::VM_FUNC_CLOSURE)
				{
					RuntimeError(IP, "Method must be a closure.");
					return INTERPRET_RUNTIME_ERROR;
				}
				klass->methods[static_cast<StringValue*>(nameValue.object)->value] = methodValue;
				break;
			}
			case OP_INHERIT:
			{
				VMValue classValue = Pop();
				VMValue superclassValue = Peek(0);
				if (classValue.type != TYPE_CLASS || classValue.object == nullptr)
				{
					RuntimeError(IP, "Can only inherit from a class.");
					return INTERPRET_RUNTIME_ERROR;
				}
				if (superclassValue.type != TYPE_CLASS || superclassValue.object == nullptr)
				{
					RuntimeError(IP, "Superclass must be a class.");
					return INTERPRET_RUNTIME_ERROR;
				}
				static_cast<Compiler::VMClassValue*>(classValue.object)->superClass = superclassValue;
				break;
			}
			case OP_GET_SUPER:
			case OP_GET_SUPER_LONG:
			{
				VMValue nameValue;
				if (opCode == OP_GET_SUPER)
					nameValue = READ_CONSTANT();
				else
					nameValue = READ_LONG_CONSTANT();

				uint32_t cacheIndex = (opCode == OP_GET_SUPER) ? READ_BYTE() : READ_THREE_BYTE();

				VMValue superclassValue = Pop();
				if (superclassValue.type != TYPE_CLASS || superclassValue.object == nullptr)
				{
					RuntimeError(IP, "Superclass must be a class.");
					return INTERPRET_RUNTIME_ERROR;
				}

				VMValue instance = Pop();
				if (instance.type != TYPE_INSTANCE || instance.object == nullptr)
				{
					RuntimeError(IP, "Only instances have methods.");
					return INTERPRET_RUNTIME_ERROR;
				}

				Compiler::VMClassValue* klass = static_cast<Compiler::VMClassValue*>(superclassValue.object);
				const std::string& methodName = static_cast<StringValue*>(nameValue.object)->value;

				InlineCache& cache = frames[frameCount - 1].GetChunk()->GetInlineCache(cacheIndex);
				uint32_t slot = Compiler::VMClassValue::INVALID_SLOT;

				VMValue method;
				const InlineCache::Entry* entry = cache.Match(klass, klass->slotNum);
				if (entry)
				{
					slot = entry->slot;
					method = entry->method;
				}
				else
				{
					slot = klass->GetSlot(methodName);
					method = klass->FindMethod(methodName);
					if (slot == Compiler::VMClassValue::INVALID_SLOT && !method.object)
					{
						RuntimeError(IP, "Undefined method '%s' in superclass.", methodName.c_str());
						return INTERPRET_RUNTIME_ERROR;
					}
					cache.Update(klass, klass->slotNum, slot, method);
				}
				if (!method.object)
				{					
					return INTERPRET_RUNTIME_ERROR;
				}
				VMValue boundMethod = VM::Create(new Compiler::BoundMethodValue(instance, method));
				Push(boundMethod);
				break;
			}
			case OP_SUPER_INVOKE:
			case OP_SUPER_INVOKE_LONG:
			{
				VMValue nameValue;
				if (opCode == OP_SUPER_INVOKE)
					nameValue = READ_CONSTANT();
				else
					nameValue = READ_LONG_CONSTANT();
				uint8_t argCountValue = READ_BYTE();
				uint32_t cacheIndex = (opCode == OP_SUPER_INVOKE) ? READ_BYTE() : READ_THREE_BYTE();

				VMValue superclassValue = Pop();
				if (superclassValue.type != TYPE_CLASS || superclassValue.object == nullptr)
				{
					RuntimeError(IP, "Superclass must be a class.");
					return INTERPRET_RUNTIME_ERROR;
				}

				VMValue instance = stackTop[-argCountValue - 1];
				if (instance.type != TYPE_INSTANCE || instance.object == nullptr)
				{
					RuntimeError(IP, "Only instances have methods.");
					return INTERPRET_RUNTIME_ERROR;
				}

				const std::string& methodName = static_cast<StringValue*>(nameValue.object)->value;
				frames[frameCount - 1].ip = IP;
				if (!InvokeFromClass(superclassValue, instance, methodName, argCountValue, cacheIndex, IP))
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				IP = frames[frameCount - 1].ip;
				break;
			}
		}
	}

	return INTERPRET_RUNTIME_ERROR;
}

InterpretResult VM::Interpret(VMValue function)
{
	frameCount = 0;
	Push(function);
	VMValue closure = VM::Create(new Compiler::VMClosureValue(function, {}));
	Pop();
	Push(closure);
	if (!Call(closure, 0))
	{
		return INTERPRET_RUNTIME_ERROR;
	}
	return Run();
}

bool VM::Call(VMValue callee, int argCount, const uint8_t* instructionIp)
{
	if ((callee.type != TYPE_CLASS && callee.type != TYPE_CALLABLE && callee.type != TYPE_BOUND_METHOD) ||
		callee.object == nullptr)
	{
		RuntimeError(instructionIp, "Can't call a non-function value.");
		return false;
	}

	if (callee.type == TYPE_CLASS)
	{
		Compiler::VMClassValue* classValue = static_cast<Compiler::VMClassValue*>(callee.object);
		VMValue instance = VM::Create(new Compiler::VMInstanceValue(classValue));
		// Replace the callee on the stack with the new instance
		stackTop[-argCount - 1] = instance;
		auto initIt = classValue->methods.find("init");
		if (initIt != classValue->methods.end())
		{
			VMValue initMethod = initIt->second;
			VMValue boundMethod = VM::Create(new Compiler::BoundMethodValue(instance, initMethod));
			stackTop[-argCount - 1] = boundMethod;
			if (!Call(boundMethod, argCount, instructionIp))
			{
				return false;
			}
		}
		else if (argCount > 0)
		{
			RuntimeError(instructionIp, "Expected 0 arguments but got %d.", argCount);
			return false;
		}
		return true;
	}

	VMValue closure;
	if (callee.type == TYPE_BOUND_METHOD)
	{
		Compiler::BoundMethodValue* boundMethod = static_cast<Compiler::BoundMethodValue*>(callee.object);
		closure = boundMethod->method;
	}
	else
	{
		closure = callee;
	}

	Compiler::VMClosureValue* closureValue = static_cast<Compiler::VMClosureValue*>(closure.object);
	VMValue function = closureValue->function;

	if (function.type != TYPE_CALLABLE || function.object == nullptr)
	{
		RuntimeError(instructionIp, "Can't call a non-function value.");
		return false;
	}

	Compiler::VMFunctionBase* functionValue = static_cast<Compiler::VMFunctionBase*>(function.object);
	if (functionValue->GetType() != Compiler::VM_FUNC_NATIVE && !function.GetChunk())
	{
		RuntimeError(instructionIp, "Can't call a non-function value.");
		return false;
	}

	int expectedArgCount = functionValue->Arity();
	if (argCount != expectedArgCount)
	{
		RuntimeError(instructionIp, "Expected %d arguments but got %d.", expectedArgCount, argCount);
		return false;
	}

	if (functionValue->GetType() == Compiler::VM_FUNC_NATIVE)
	{
		Compiler::NativeFunctionValue* nativeFunction = static_cast<Compiler::NativeFunctionValue*>(function.object);
		VMValue result = nativeFunction->function(argCount, stackTop - argCount);
		// Pop arguments and the callee
		stackTop -= argCount + 1;
		// Push the native function result onto the stack so it can be used by caller frames.
		Push(result);
	}
	else
	{
		if (!function.GetChunk())
		{
			RuntimeError(instructionIp, "Can only call functions with bytecode.");
			return false;
		}

		if (frameCount >= FRAMES_MAX)
		{
			RuntimeError(instructionIp, "Stack overflow: too many nested calls.");
			return false;
		}

		CallFrame newFrame;
		newFrame.closure = closure;
		newFrame.ip = newFrame.GetChunk()->code;
		newFrame.inner = VMValue();
		// Frame slots start at the callee slot, so locals can index from that base.
		newFrame.slots = stackTop - argCount - 1;
		// If the callee is a bound method, the receiver is stored in slot 0 of the new frame.
		if (callee.type == TYPE_BOUND_METHOD)
		{
			newFrame.slots[0] = static_cast<Compiler::BoundMethodValue*>(callee.object)->receiver;
		}
		frames[frameCount++] = newFrame;
	}

	return true;
}

bool VM::Invoke(VMValue receiver, VMValue method, int argCount, const uint8_t* instructionIp)
{
	Compiler::VMClosureValue* closureValue = static_cast<Compiler::VMClosureValue*>(method.object);
	int expectedArgCount = closureValue->Arity();
	if (argCount != expectedArgCount)
	{
		RuntimeError(instructionIp, "Expected %d arguments but got %d.", expectedArgCount, argCount);
		return false;
	}

	if (frameCount >= FRAMES_MAX)
	{
		RuntimeError(instructionIp, "Stack overflow: too many nested calls.");
		return false;
	}

	CallFrame newFrame;
	newFrame.closure = method;
	newFrame.ip = newFrame.GetChunk()->code;
	newFrame.slots = stackTop - argCount - 1;
	newFrame.slots[0] = receiver;
	newFrame.inner = VMValue();
	frames[frameCount++] = newFrame;
	return true;
}

bool VM::InvokeFromClass(VMValue classValue, VMValue receiver, const std::string& methodName, int argCount, uint32_t cacheIndex, const uint8_t* instructionIp)
{
	Compiler::VMClassValue* klass = static_cast<Compiler::VMClassValue*>(classValue.object);
	Compiler::VMInstanceValue* instance = static_cast<Compiler::VMInstanceValue*>(receiver.object);
	InlineCache& cache = frames[frameCount - 1].GetChunk()->GetInlineCache(cacheIndex);

	uint32_t slot = Compiler::VMClassValue::INVALID_SLOT;
	VMValue method;
	const InlineCache::Entry* entry = cache.Match(klass, klass->slotNum);
	if (entry)
	{
		slot = entry->slot;
		method = entry->method;
	}
	else
	{
		slot = klass->GetSlot(methodName);
		method = klass->FindMethod(methodName);
		if (slot == Compiler::VMClassValue::INVALID_SLOT && !method.object)
		{
			if (classValue.object == instance->classValue.object)
			{
				RuntimeError(instructionIp, "Undefined method '%s'.", methodName.c_str());
			}
			else
			{
				RuntimeError(instructionIp, "Undefined method '%s' in superclass.", methodName.c_str());
			}
			return false;
		}
		cache.Update(klass, klass->slotNum, slot, method);
	}

	VMValue callee;
	if (classValue.object == instance->classValue.object && slot != Compiler::VMClassValue::INVALID_SLOT)
	{
		callee = instance->GetField(slot);
	}
	if (!callee.IsValid() && !method.IsValid())
	{
		if (classValue.object == instance->classValue.object)
		{
			RuntimeError(instructionIp, "Undefined method '%s'.", methodName.c_str());
		}
		else
		{
			RuntimeError(instructionIp, "Undefined method '%s' in superclass.", methodName.c_str());
		}
		return false;
	}

	return callee.IsValid()
		? Call(callee, argCount, instructionIp)
		: Invoke(receiver, method, argCount, instructionIp);
}

void VM::DefineNative(const std::string& name, Compiler::NativeFn function, int32_t arity)
{
	size_t slot = -1;
	if (ResolveOrCreateGlobalSlot(VM::Create(StringValue::CreateRaw(name)), slot))
	{
		VMValue nativeValue = VM::Create(new Compiler::NativeFunctionValue(name, function, arity));
		Push(nativeValue);
		VMValue closure = VM::Create(new Compiler::VMClosureValue(nativeValue, {}));
		Pop();
		globalSlots[slot] = closure;
	}
	else
	{
		RuntimeError("Failed to define native function '%s'.", name.c_str());
	}
}

InterpretResult VM::Interpret(const char* source)
{
	Compiler compiler;

	VMValue compiledFunction = compiler.Compile(source);
	if (compiledFunction.type != TYPE_CALLABLE || compiledFunction.object == nullptr)
	{
		return INTERPRET_COMPILE_ERROR;
	}

	// Push the compiled function onto the stack so it is reachable by the GC while the initial call frame is being set up.
	Push(compiledFunction);
	// Wrap the script function in a VMClosureValue so Call() always receives a closure.
	VMValue scriptClosure = VM::Create(new Compiler::VMClosureValue(compiledFunction, {}));
	// Remove the compiled function from the stack since it's now referenced by the closure
	Pop();

	// Slot 0 is reserved by the compiler for the implicit "function" object.	
	Push(scriptClosure);

	frameCount = 0;
	if (!Call(scriptClosure, 0))
	{
		return INTERPRET_RUNTIME_ERROR;
	}

	InterpretResult result = Run();

	return result;
}

void VM::MarkValue(VMValue value)
{
	if (!value.IsObject() || value.object == nullptr || value.object->markedValue == currentMarkValue)
	{
		return;
	}
#ifdef DEBUG_LOG_GC
	printf("  Mark object %p of type %s\n", (void*)value.object, ValueTypeToString(value.object->type));
#endif
	value.object->markedValue = currentMarkValue;
	if (grayStackCount + 1 > grayStackCapacity)
	{
		size_t oldCapacity = grayStackCapacity;
		size_t newCapacity = oldCapacity == 0 ? 8 : oldCapacity * 2;
		grayStack = GROW_ARRAY(Value*, grayStack, oldCapacity, newCapacity);
		grayStackCapacity = newCapacity;
	}
	grayStack[grayStackCount++] = value.object;
}

void VM::TraceReferences()
{
	while (grayStackCount > 0)
	{
		Value* value = grayStack[--grayStackCount];
#ifdef DEBUG_LOG_GC
		printf("  Blacken object %p of type %s\n", (void*)value, ValueTypeToString(value->type));
#endif
		value->Blacken(*this);
	}
}

void VM::Sweep()
{
	for (Value* object = objects; object != nullptr; )
	{
		if (object->markedValue != currentMarkValue)
		{
			Value* unreached = object;
			object = object->nextGCValue;
			FreeValue(unreached);
		}
		else
		{
			object = object->nextGCValue;
		}
	}
	currentMarkValue = !currentMarkValue;
}

void VM::MarkRoots()
{
	for (VMValue* slot = stacks; slot < stackTop; ++slot)
	{
		MarkValue(*slot);
	}
	for (uint32_t i = 0; i < frameCount; ++i)
	{
		CallFrame& frame = frames[i];
		MarkValue(frame.closure);
		MarkValue(frame.inner);
	}
	for (UpvalueValue* upvalue = openUpvalues; upvalue != nullptr; upvalue = upvalue->nextUpvalue)
	{
		MarkValue(VMValue(upvalue));
	}
	for (VMValue& global : globalSlots)
	{
		MarkValue(global);
	}
	MarkCompilerRoots();
}

void VM::MarkCompilerRoots()
{
	for (Compiler* compiler : compilerRoots)
	{
		if (compiler != nullptr)
		{
			MarkValue(compiler->function);
		}
	}
}

void VM::InvalidateInlineCaches()
{
	for (Value* object = objects; object != nullptr; object = object->nextGCValue)
	{
		if (object->type != TYPE_CALLABLE)
		{
			continue;
		}

		Compiler::VMFunctionBase* functionValue = static_cast<Compiler::VMFunctionBase*>(object);
		Chunk* chunk = functionValue->GetChunk();
		if (chunk != nullptr)
		{
			chunk->InvalidateInlineCaches();
		}
	}
}

void VM::CollectGarbage()
{
#ifdef DEBUG_LOG_GC
	printf("GC Begin\n");
#endif
	MarkRoots();
	TraceReferences();
	Sweep();
	// Inline caches keep raw class identities, so a GC cycle invalidates them.
	InvalidateInlineCaches();
	nextGC = bytesAllocated * GC_HEAP_GROW_FACTOR;
	if (nextGC < INITIAL_GC_THRESHOLD)
	{
		nextGC = INITIAL_GC_THRESHOLD;
	}
#ifdef DEBUG_LOG_GC
	printf("GC End (%zu bytes allocated, next at %zu)\n", bytesAllocated, nextGC);
#endif
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
