#pragma once
#include "Value.h"
#include <cstdint>
#include <cstdlib>
#include <type_traits>
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
	OP_GET_LOCAL,
	OP_GET_LOCAL_LONG,
	OP_SET_LOCAL,
	OP_SET_LOCAL_LONG,
	OP_POP,
	OP_DUP,
	OP_NOP,
	OP_GET_GLOBAL,
	OP_GET_GLOBAL_LONG,
	OP_SET_GLOBAL,
	OP_SET_GLOBAL_LONG,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_JUMP_IF_FALSE,
	OP_JUMP,
	OP_LOOP,
	OP_CALL,
	OP_INVOKE,
	OP_INVOKE_LONG,
	OP_ROOT_INVOKE,
	OP_ROOT_INVOKE_LONG,
	OP_INNER_INVOKE,
	OP_CLOSURE,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,
	OP_CLOSE_UPVALUE,
	OP_CLASS,
	OP_SET_PROPERTY,
	OP_GET_PROPERTY,
	OP_SET_PROPERTY_LONG,
	OP_GET_PROPERTY_LONG,
	OP_GET_INDEX,
	OP_SET_INDEX,
	OP_METHOD,
	OP_METHOD_LONG,
	OP_CLASS_METHOD,
	OP_CLASS_METHOD_LONG,
	OP_INHERIT,
	OP_GET_SUPER,
	OP_GET_SUPER_LONG,
	OP_SUPER_INVOKE,
	OP_SUPER_INVOKE_LONG,
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

struct Chunk;

struct VMValue
{
	ValueType type;
	union
	{
		bool boolean;
		int integer;
		float number;
		Value* object;
	};
	VMValue()
		: type(TYPE_ERROR)
		, object(nullptr)
	{}
	VMValue(Value* inObject)
		: type(inObject ? inObject->type : TYPE_ERROR)
		, object(inObject)
	{}
	VMValue(bool inBoolean)
		: type(TYPE_BOOL)
		, boolean(inBoolean)
	{}
	VMValue(int inInteger)
		: type(TYPE_INT)
		, integer(inInteger)
	{}
	VMValue(float inNumber)
		: type(TYPE_FLOAT)
		, number(inNumber)
	{}

	static VMValue Nil()
	{
		VMValue value;
		value.type = TYPE_NIL;
		return value;
	}

	bool IsObject() const
	{
		return type != TYPE_INT && type != TYPE_FLOAT && type != TYPE_BOOL && type != TYPE_NIL;
	}

	bool IsValid() const
	{
		return type != TYPE_ERROR || object != nullptr;
	}

	Chunk* GetChunk() const
	{
		return IsObject() && object ? object->GetChunk() : nullptr;
	}
};

bool IsEqual(const VMValue& left, const VMValue& right);
std::string VMValueToString(const VMValue& value);

struct VMValueArray
{
	int32_t capacity;
	int32_t count;
	VMValue* values;

	void Init();
	void Write(VMValue value);
	void Free();
};

struct InlineCache
{
	static constexpr uint32_t ENTRY_COUNT = 4;

	struct Entry
	{
		void* klass;
		uint32_t slotNum;
		uint32_t slot;
		VMValue method;
	} entries[ENTRY_COUNT];

	uint32_t writeLocation;

	InlineCache()
		: writeLocation(0)
	{
		for (uint32_t i = 0; i < ENTRY_COUNT; ++i)
		{
			entries[i].klass = nullptr;
			entries[i].slotNum = 0;
			entries[i].slot = -1;
			entries[i].method = VMValue();
		}
	}

	const Entry* Match(void* inKlass, uint32_t inSlotNum) const
	{
		if (!inKlass)
		{
			return nullptr;
		}
		for (uint32_t i = 0; i < ENTRY_COUNT; ++i)
		{
			uint32_t index = (ENTRY_COUNT + writeLocation - 1 - i) % ENTRY_COUNT;
			if (entries[index].klass == inKlass && entries[index].slotNum == inSlotNum)
			{
				return &entries[index];
			}
		}
		return nullptr;
	}

	void Update(void* inKlass, uint32_t inSlotNum, uint32_t inSlot, VMValue inMethod)
	{
		entries[writeLocation].klass = inKlass;
		entries[writeLocation].slotNum = inSlotNum;
		entries[writeLocation].slot = inSlot;
		entries[writeLocation].method = inMethod;
		writeLocation = (writeLocation + 1) % ENTRY_COUNT;
	}
};

struct InlineCacheArray
{
	int32_t capacity;
	int32_t count;
	InlineCache* caches;

	void Init();
	uint32_t Append();
	InlineCache& Get(uint32_t index);
	const InlineCache& Get(uint32_t index) const;
	void InvalidateAll();
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
	InlineCacheArray inlineCaches;

	Chunk()
	{
		Init();
	}
	~Chunk() = default;

	void Init();
	void Write(uint8_t byte, int32_t line, int32_t column);

	int32_t GetLine(int32_t offset);
	int32_t GetColumn(int32_t offset);

	inline int32_t GetSize() const { return count; }

	int32_t AddConstant(VMValue value);
	void Free();

	// Zero-operand instructions simply print the instruction name and return the offset of the next instruction.
	int32_t SimpleInstruction(const char* name, int32_t offset);
	// One-byte operand instructions print the instruction name and the operand, then return the offset of the next instruction.
	int32_t ByteInstruction(const char* name, int32_t offset);
	int32_t ThreeByteInstruction(const char* name, int32_t offset);
	int32_t JumpInstruction(const char* name, int32_t sign, int32_t offset);
	static void PrintValue(VMValue value);
	static void PrintValueStdout(VMValue value);
	int32_t ConstantInstruction(const char* name, int32_t offset);
	int32_t ConstantLongInstruction(const char* name, int32_t offset);
	int32_t PropertyInstruction(const char* name, int32_t offset);
	int32_t PropertyLongInstruction(const char* name, int32_t offset);
	int32_t ClosureInstruction(const char* name, int32_t offset, int32_t indent);
	int32_t InvokeInstruction(const char* name, int32_t offset);
	int32_t InvokeLongInstruction(const char* name, int32_t offset);
	int32_t RootInvokeInstruction(const char* name, int32_t offset);
	int32_t RootInvokeLongInstruction(const char* name, int32_t offset);

	uint32_t AppendInlineCache();
	InlineCache& GetInlineCache(uint32_t cacheIndex);
	const InlineCache& GetInlineCache(uint32_t cacheIndex) const;
	void InvalidateInlineCaches();
	int32_t DisassembleInstruction(int32_t offset, int32_t indent = 0);
	void DisassembleConstant(int32_t index, int32_t indent = 0);
	void Disassemble(const char* name, int32_t indent = 0);

	void WriteConstant(VMValue value, int32_t line, int32_t column);
};
