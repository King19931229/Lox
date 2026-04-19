#include "Chunk.h"
#include "VM.h"
#include <cstdio>
#include <iostream>

VMValue VMValue::Create(Value* value, Chunk* chunk)
{
	if (!value) return VMValue(nullptr, nullptr);
	VM& vm = VM::GetInstance();
	VMValue* node = new VMValue(value, chunk);
	node->next = vm.objects;
	vm.objects = node;
	return VMValue(value, chunk);
}

static void PrintIndent(int32_t indent)
{
	for (int32_t i = 0; i < indent; ++i)
	{
		printf("    ");
	}
}

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

int32_t Chunk::AddConstant(VMValue value)
{
	for (int32_t i = 0; i < constants.count; ++i)
	{
		if (IsEqual(constants.values[i].value, value.value))
		{
			return i;
		}
	}
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

int32_t Chunk::ByteInstruction(const char* name, int32_t offset)
{
	uint8_t operand = code[offset + 1];
	printf("%-16s %4d\n", name, operand);
	return offset + 2;
}

int32_t Chunk::ThreeByteInstruction(const char* name, int32_t offset)
{
	uint32_t operand = (uint32_t)(code[offset + 1] << 16);
	operand |= (uint32_t)(code[offset + 2] << 8);
	operand |= (uint32_t)(code[offset + 3]);
	printf("%-16s %4d\n", name, operand);
	return offset + 4;
}

int32_t Chunk::JumpInstruction(const char* name, int32_t sign, int32_t offset)
{
    int32_t jump = (int32_t)((code[offset + 1] << 8) | code[offset + 2]);
    int32_t target = offset + 3 + sign * jump;
    printf("%-16s %4d -> %d\n", name, offset, target);
    return offset + 3;
}

void Chunk::PrintValue(VMValue value)
{
	if (!value.value)
	{
		printf("nil");
		return;
	}
	// Convert to std::string via Value's operator std::string()
	std::string s = static_cast<std::string>(*value.value);
	printf("%s", s.c_str());
}

void Chunk::PrintValueStdout(VMValue value)
{
	if (!value.value)
	{
		std::cout << "nil";
		return;
	}
	std::string s = static_cast<std::string>(*value.value);
	std::cout << s;
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

int32_t Chunk::DisassembleInstruction(int32_t offset, int32_t indent)
{
	PrintIndent(indent);
	printf("%04d ", offset);
	if (offset > 0 && lines[offset] == lines[offset - 1] && columns[offset] == columns[offset - 1])
	{
		printf("     |  ");
	}
	else
	{
		printf("%4d:%-3d", lines[offset], columns[offset]);
	}
	uint8_t instruction = code[offset];
	switch (instruction)
	{
		case OP_CONSTANT:
			return ConstantInstruction("OP_CONSTANT", offset);
		case OP_CONSTANT_LONG:
			return ConstantLongInstruction("OP_CONSTANT_LONG", offset);
		case OP_NIL:
			return SimpleInstruction("OP_NIL", offset);
		case OP_TRUE:
			return SimpleInstruction("OP_TRUE", offset);
		case OP_FALSE:
			return SimpleInstruction("OP_FALSE", offset);
		case OP_NEGATE:
			return SimpleInstruction("OP_NEGATE", offset);
		case OP_PRINT:
			return SimpleInstruction("OP_PRINT", offset);
		case OP_ADD:
			return SimpleInstruction("OP_ADD", offset);
		case OP_SUBTRACT:
			return SimpleInstruction("OP_SUBTRACT", offset);
		case OP_MULTIPLY:
			return SimpleInstruction("OP_MULTIPLY", offset);
		case OP_DIVIDE:
			return SimpleInstruction("OP_DIVIDE", offset);
		case OP_NOT:
			return SimpleInstruction("OP_NOT", offset);
		case OP_DEFINE_GLOBAL:
			return ConstantInstruction("OP_DEFINE_GLOBAL", offset);
		case OP_DEFINE_GLOBAL_LONG:
			return ConstantLongInstruction("OP_DEFINE_GLOBAL_LONG", offset);
		case OP_GET_GLOBAL:
			return ConstantInstruction("OP_GET_GLOBAL", offset);
		case OP_GET_GLOBAL_LONG:
			return ConstantLongInstruction("OP_GET_GLOBAL_LONG", offset);
		case OP_SET_GLOBAL:
			return ConstantInstruction("OP_SET_GLOBAL", offset);
		case OP_SET_GLOBAL_LONG:
			return ConstantLongInstruction("OP_SET_GLOBAL_LONG", offset);
		case OP_GET_LOCAL:
			return ByteInstruction("OP_GET_LOCAL", offset);
		case OP_GET_LOCAL_LONG:
			return ThreeByteInstruction("OP_GET_LOCAL_LONG", offset);
		case OP_SET_LOCAL:
			return ByteInstruction("OP_SET_LOCAL", offset);
		case OP_SET_LOCAL_LONG:
			return ThreeByteInstruction("OP_SET_LOCAL_LONG", offset);
		case OP_EQUAL:
			return SimpleInstruction("OP_EQUAL", offset);
		case OP_GREATER:
			return SimpleInstruction("OP_GREATER", offset);
		case OP_LESS:
			return SimpleInstruction("OP_LESS", offset);
		case OP_RETURN:
			return SimpleInstruction("OP_RETURN", offset);
		case OP_POP:
			return SimpleInstruction("OP_POP", offset);
		case OP_DUP:
			return SimpleInstruction("OP_DUP", offset);
		case OP_NOP:
			return SimpleInstruction("OP_NOP", offset);
		case OP_JUMP_IF_FALSE:
			return JumpInstruction("OP_JUMP_IF_FALSE", 1, offset);
		case OP_JUMP:
			return JumpInstruction("OP_JUMP", 1, offset);
		case OP_LOOP:
			return JumpInstruction("OP_LOOP", -1, offset);
		case OP_CALL:
			return ByteInstruction("OP_CALL", offset);
		default:
			printf("Unknown opcode %d\n", instruction);
			return offset + 1;
	}
}

void Chunk::DisassembleConstant(int32_t index, int32_t indent)
{
	PrintIndent(indent);
	printf("%4d '", index);
	PrintValue(constants.values[index]);
	printf("'");
	/*if (constants.values[index].chunk)
	{
		printf(" ->\n");
		std::string nestedName = constants.values[index].value ? static_cast<std::string>(*constants.values[index].value) : std::string("<chunk>");
		constants.values[index].chunk->Disassemble(nestedName.c_str(), indent + 1);
	}
	else*/
	{
		printf("\n");
	}
}

void Chunk::Disassemble(const char* name, int32_t indent)
{
	PrintIndent(indent);
	printf("==== %s ====\n", name);
	PrintIndent(indent);
	printf("== Constants ==\n");
	for (int32_t i = 0; i < constants.count; ++i)
	{
		DisassembleConstant(i, indent);
	}
	PrintIndent(indent);
	printf("== Instructions ==\n");
	for (int32_t offset = 0; offset < count;)
	{
		offset = DisassembleInstruction(offset, indent);
	}
}

void Chunk::WriteConstant(VMValue value, int32_t line, int32_t column)
{
	if (constants.count <= 255)
	{
		Write(OP_CONSTANT, line, column);
		int32_t index = AddConstant(value);
		Write(index, line, column);
	}
	else
	{
		Write(OP_CONSTANT_LONG, line, column);
		int32_t index = AddConstant(value);
		Write((index >> 16) & 0xFF, line, column);
		Write((index >> 8) & 0xFF, line, column);
		Write(index & 0xFF, line, column);
	}
}
