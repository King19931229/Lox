#include "Chunk.h"
#include <cstdio>

// VMValueArray implementations
void VMValueArray::Init()
{
	capacity = 0;
	count = 0;
	values = nullptr;
}

void VMValueArray::Write(VMValue value)
{
	if (capacity < count + 1)
	{
		int32_t oldCapacity = capacity;
		capacity = GROW_CAPACITY(oldCapacity);
		values = GROW_ARRAY(VMValue, values, oldCapacity, capacity);
	}
	values[count++] = value;
}

void VMValueArray::Free()
{
	FREE_ARRAY(VMValue, values, capacity);
	Init();
}

// Chunk implementations
void Chunk::Init()
{
	capacity = 0;
	count = 0;
	code = nullptr;
	lines = nullptr;
	columns = nullptr;
	constants.Init();
}

void Chunk::Write(uint8_t byte, int32_t line, int32_t column)
{
	if (capacity < count + 1)
	{
		int32_t oldCapacity = capacity;
		capacity = GROW_CAPACITY(oldCapacity);
		code = GROW_ARRAY(uint8_t, code, oldCapacity, capacity);
		lines = GROW_ARRAY(int32_t, lines, oldCapacity, capacity);
		columns = GROW_ARRAY(int32_t, columns, oldCapacity, capacity);
	}
	code[count] = byte;
	lines[count] = line;
	columns[count] = column;
	++count;
}

int32_t Chunk::GetLine(int32_t offset)
{
	return lines[offset];
}

int32_t Chunk::GetColumn(int32_t offset)
{
	return columns[offset];
}

int32_t Chunk::AddCounstant(VMValue value)
{
	constants.Write(value);
	return constants.count - 1;
}

void Chunk::Free()
{
	constants.Free();
	FREE_ARRAY(uint8_t, code, capacity);
	FREE_ARRAY(int32_t, lines, capacity);
	FREE_ARRAY(int32_t, columns, capacity);
	Init();
}

int32_t Chunk::SimpleInstruction(const char* name, int32_t offset)
{
	printf("%s\n", name);
	return offset + 1;
}

void Chunk::PrintValue(VMValue value)
{
	printf("%g", value);
}

int32_t Chunk::ConstantInstruction(const char* name, int32_t offset)
{
	uint8_t constant = code[offset + 1];
	printf("%-16s %4d '", name, constant);
	PrintValue(constants.values[constant]);
	printf("'\n");
	return offset + 2;
}

int32_t Chunk::ConstantLongInstruction(const char* name, int32_t offset)
{
	uint32_t constant = (uint32_t)(code[offset + 1] << 16);
	constant |= (uint32_t)(code[offset + 2] << 8);
	constant |= (uint32_t)(code[offset + 3]);
	printf("%-16s %4d '", name, constant);
	PrintValue(constants.values[constant]);
	printf("'\n");
	return offset + 4;
}

int32_t Chunk::DisassembleInstruction(int32_t offset)
{
	printf("%04d ", offset);
	if (offset > 0 && lines[offset] == lines[offset - 1] && columns[offset] == columns[offset - 1])
	{
		printf("     | ");
	}
	else
	{
		printf("%4d:%d ", lines[offset], columns[offset]);
	}
	uint8_t instruction = code[offset];
	switch (instruction)
	{
		case OP_CONSTANT:
			return ConstantInstruction("OP_CONSTANT", offset);
		case OP_CONSTANT_LONG:
			return ConstantLongInstruction("OP_CONSTANT_LONG", offset);
		case OP_RETURN:
			return SimpleInstruction("OP_RETURN", offset);
		default:
			printf("Unknown opcode %d\n", instruction);
			return offset + 1;
	}
}

void Chunk::Disassemble(const char* name)
{
	printf("== %s ==\n", name);
	for (int32_t offset = 0; offset < count;)
	{
		offset = DisassembleInstruction(offset);
	}
}

void Chunk::WriteConstant(VMValue value, int32_t line, int32_t column)
{
	if (constants.count <= 255)
	{
		Write(OP_CONSTANT, line, column);
		int32_t index = AddCounstant(value);
		Write(index, line, column);
	}
	else
	{
		Write(OP_CONSTANT_LONG, line, column);
		int32_t index = AddCounstant(value);
		Write((index >> 16) & 0xFF, line, column);
		Write((index >> 8) & 0xFF, line, column);
		Write(index & 0xFF, line, column);
	}
}
