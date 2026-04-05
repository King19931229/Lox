#pragma once
#include "Chunk.h"
#include <unordered_map>
#include <vector>

enum InterpretResult
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
};

struct CallFrame
{
	VMValue function;
	uint8_t* ip;
	VMValue* slots;
};

class VM
{
	friend struct VMValue;
protected:
	static VM* instance;

	static constexpr uint32_t FRAMES_MAX = 64;
	static constexpr uint32_t STACK_MAX = FRAMES_MAX * 255;

    VMValue* stacks = nullptr;
    size_t stackCapacity = 255;
    VMValue* stackTop = nullptr;

	VMValue* objects = nullptr;

	std::unordered_map<std::string, size_t> globalNameToSlot;
	std::vector<VMValue> globalSlots;

	CallFrame frames[FRAMES_MAX];
	uint32_t frameCount = 0;

	// Stack operations
	void ResetStack();
	void AdjustFrameSlots(VMValue* oldStacks, VMValue* newStacks);
	void Push(VMValue value);
	InterpretResult Negate();
	VMValue Pop();
	VMValue Peek(int32_t distance);
	void Free(VMValue* object);

	bool IsNumber(VMValue value);
	bool IsFalsey(VMValue value);
	bool IsString(VMValue value);

	void RuntimeError(const char* format, ...);

public:
	static VM& GetInstance()
	{
		if (!instance)
		{
			instance = new VM();
		}
		return *instance;
	}

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
