#pragma once
#include "Value.h"
#include <cstdint>
#include <malloc.h>
#include <cstdlib>
#include <vector>
#include <type_traits>
#include <new>
#include <utility>

enum OpCode
{
	OP_CONSTANT,
	OP_CONSTANT_LONG,
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_NEGATE,
	OP_PRINT,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_NOT,
	OP_DEFINE_GLOBAL,
	OP_DEFINE_GLOBAL_LONG,
	OP_GET_GLOBAL,
	OP_GET_GLOBAL_LONG,
	OP_SET_GLOBAL,
	OP_SET_GLOBAL_LONG,
	OP_EQUAL,
	OP_GERATER,
	OP_LESS,
	OP_RETURN,
};

inline void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
	if (newSize == 0)
	{
		free(pointer);
		return nullptr;
	}
	void* result = realloc(pointer, newSize);
	if (result == nullptr)
	{
		// Handle allocation failure (could throw an exception or abort)
		exit(1);
	}
	return result;
}

template <typename T>
T* grow_array_impl(T* pointer, size_t old_count, size_t new_count) {

	if (std::is_trivially_copyable<T>::value)
	{
		return (T*)reallocate(pointer, sizeof(T) * old_count, sizeof(T) * new_count);
	}
	else
	{
		T* new_block = new T[new_count];
		if (pointer != nullptr) {
			for (size_t i = 0; i < old_count; ++i)
			{
				new_block[i] = std::move(pointer[i]);
			}
			delete[] pointer;
		}
		return new_block;
	}
}

template <typename T>
void free_array_impl(T* pointer, size_t old_count)
{
	if (std::is_trivially_copyable<T>::value)
	{
		reallocate(pointer, sizeof(T) * old_count, 0);
	}
	else
	{
		delete[] pointer;
	}
}

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)
#define GROW_ARRAY(type, pointer, oldCount, newCount) grow_array_impl(pointer, oldCount, newCount)
#define FREE_ARRAY(type, pointer, oldCount) free_array_impl(pointer, oldCount)

class VM;

struct VMValue
{
	Value* value;
	VMValue()
		: value(nullptr)
	{}
	VMValue(Value* inValue)
		: value(inValue)
	{}
	static VMValue Create(Value* value);
};

struct VMValueArray
{
	int32_t capacity;
	int32_t count;
	VMValue* values;

	void Init();
	void Write(VMValue value);
	void Free();
};

struct Chunk
{
	int32_t capacity;
	int32_t count;
	uint8_t* code;
	int32_t* lines;
	int32_t* columns;
	VMValueArray constants;

	void Init();
	void Write(uint8_t byte, int32_t line, int32_t column);

	int32_t GetLine(int32_t offset);
	int32_t GetColumn(int32_t offset);

	int32_t AddConstant(VMValue value);
	void Free();

	int32_t SimpleInstruction(const char* name, int32_t offset);
	void PrintValue(VMValue value);
	int32_t ConstantInstruction(const char* name, int32_t offset);
	int32_t ConstantLongInstruction(const char* name, int32_t offset);
	int32_t DisassembleInstruction(int32_t offset);
	void Disassemble(const char* name);

	void WriteConstant(VMValue value, int32_t line, int32_t column);
};
