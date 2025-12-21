#pragma once
#include <cstdint>
#include <malloc.h>
#include <cstdlib>

enum OpCode
{
	OP_CONSTANT,
	OP_CONSTANT_LONG,
	OP_NEGATE,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_RETURN,
};

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)
#define GROW_ARRAY(type, pointer, oldCount, newCount) (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))
#define FREE_ARRAY(type, pointer, oldCount) reallocate(pointer, sizeof(type) * (oldCount), 0)

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

typedef double VMValue;
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

	int32_t AddCounstant(VMValue value);
	void Free();

	int32_t SimpleInstruction(const char* name, int32_t offset);
	void PrintValue(VMValue value);
	int32_t ConstantInstruction(const char* name, int32_t offset);
	int32_t ConstantLongInstruction(const char* name, int32_t offset);
	int32_t DisassembleInstruction(int32_t offset);
	void Disassemble(const char* name);

	void WriteConstant(VMValue value, int32_t line, int32_t column);
};
