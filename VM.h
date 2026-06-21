#pragma once
#include "Chunk.h"
#include "Compiler.h"
#include <cstdarg>
#include <unordered_map>
#include <vector>

struct VMValue; // Forward declaration

enum InterpretResult
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
};

struct CallFrame
{
	VMValue closure;
	uint8_t* ip;
	VMValue* slots;

	inline Chunk* GetChunk()
	{
		return static_cast<Compiler::VMClosureValue*>(closure.value)->function.GetChunk();
	}
	inline std::vector<VMValue>& GetUpvalues()
	{
		return static_cast<Compiler::VMClosureValue*>(closure.value)->upvalues;
	}
};

class VM
{
public:
	friend struct VMValue;
	friend class Compiler;
	Value* objects = nullptr;
protected:
	static VM* instance;

	static constexpr uint32_t FRAMES_MAX = 64;
	static constexpr uint32_t STACK_MAX = FRAMES_MAX * 255;
	static constexpr size_t INITIAL_GC_THRESHOLD = 1024 * 1024;
	static constexpr size_t GC_HEAP_GROW_FACTOR = 2;

	struct UpvalueValue : public Value
	{
		VMValue* location = nullptr;
		VMValue closed;
		UpvalueValue* nextUpvalue = nullptr;
		UpvalueValue(VMValue* inLocation)
			: location(inLocation)
		{
			type = TYPE_UPVALUE;
		}
		void Blacken(VM& vm) override;
		size_t Size() const override { return sizeof(*this); }
	};

	VMValue* stacks = nullptr;
	size_t stackCapacity = 0;
	VMValue* stackTop = nullptr;
	UpvalueValue* openUpvalues = nullptr;

	Value** grayStack = nullptr;
	size_t grayStackCount = 0;
	size_t grayStackCapacity = 0;

	size_t bytesAllocated = 0;
	size_t nextGC = INITIAL_GC_THRESHOLD;

	bool currentMarkValue = true;

	std::unordered_map<std::string, size_t> globalNameToSlot;
	std::vector<VMValue> globalSlots;
	std::vector<Compiler*> compilerRoots;

	CallFrame frames[FRAMES_MAX];
	uint32_t frameCount = 0;

	// Stack operations
	void ResetStack();
	void AdjustFrameSlots(VMValue* oldStacks, VMValue* newStacks);
	void Push(VMValue value);
	InterpretResult Negate(const uint8_t* instructionIp = nullptr);
	VMValue Pop();
	VMValue Peek(int32_t distance);
	VMValue CaptureUpvalue(VMValue* local);
	void CloseUpvalues(VMValue* last);

	void FreeValue(Value* object);
	Value* AllocValue(Value* value);

	// Resolve a global variable slot by name, creating a new slot if it doesn't exist. Returns true on success.
	bool ResolveOrCreateGlobalSlot(VMValue nameValue, size_t& outSlot, const uint8_t* instructionIp = nullptr);
	// Resolve a global variable slot by name, returning false if it doesn't exist. Returns true on success.
	bool ResolveExistingGlobalSlot(VMValue nameValue, size_t& outSlot, const uint8_t* instructionIp = nullptr);

	bool IsNumber(VMValue value);
	bool IsFalsey(VMValue value);
	bool IsString(VMValue value);

	void RuntimeError(const char* format, ...);
	void RuntimeError(const uint8_t* instructionIp, const char* format, ...);
	void RuntimeErrorImpl(const uint8_t* instructionIp, const char* format, va_list args);

	void MarkRoots();
	void MarkCompilerRoots();
	void PushCompilerRoot(Compiler* compiler);
	void PopCompilerRoot(Compiler* compiler);
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
	void Reset();
	void Free();
	static VMValue Create(Value* value);

	// Execution
	InterpretResult Run();
	// Call a function value with given argument count. Returns true on success.
	bool Call(VMValue callee, int argCount, const uint8_t* instructionIp = nullptr);
	InterpretResult Interpret(VMValue function);
	InterpretResult Interpret(const char* source);

	void MarkValue(VMValue value);
	void TraceReferences();
	void Sweep();
	void CollectGarbage();

	void DefineNative(const std::string& name, Compiler::NativeFn function, int32_t arity);

	void Repl();
	void RunFile(const char* path);
};
