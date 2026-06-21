#pragma once
#include "Scanner.h"
#include "Chunk.h"
#include <vector>

class Compiler
{
	enum FunctionType
	{
		TYPE_SCRIPT,
		TYPE_FUNCTION,
	};
protected:
public:
	// Root compiler
	Compiler();
	~Compiler();

	VMValue Compile(const char* source);
	VMValue CompileFunction(FunctionType fnType, const std::string& name);

	enum VMFunctionType
	{
		VM_FUNC_SCRIPT,
		VM_FUNC_FUNCTION,
		VM_FUNC_NATIVE,
		VM_FUNC_CLASS,
		VM_FUNC_CLOSURE
	};
	
	friend class VM;
	struct VMFunctionBase : public Value
	{
		virtual int Arity() const = 0;
		virtual VMFunctionType GetType() const = 0;
		void Blacken(VM& vm) override;
	};

	// Lightweight placeholder for the top-level script callable.
	// Inherits directly from Value so no extra headers are needed.
	struct ScriptFunction : public VMFunctionBase
	{
		Chunk* chunk = nullptr;
		explicit ScriptFunction(Chunk* inChunk = nullptr)
			: chunk(inChunk)
		{
			this->type = TYPE_CALLABLE;
		}
		~ScriptFunction() override
		{
			if (chunk)
			{
				chunk->Free();
				delete chunk;
			}
		}
		int Arity() const override { return 0; }
		Chunk* GetChunk() const override { return chunk; }
		operator std::string() const override { return "<script>"; }
		VMFunctionType GetType() const override { return VM_FUNC_SCRIPT; }
		size_t Size() const override { return sizeof(*this); }
	};

	// Placeholder for a named function compiled by the VM compiler.
	struct VMFunctionValue : public VMFunctionBase
	{
		std::string name;
		int32_t arity = 0;
		int32_t upvalueCount = 0;
		Chunk* chunk = nullptr;
		explicit VMFunctionValue(const std::string& inName, Chunk* inChunk = nullptr)
			: name(inName)
			, chunk(inChunk)
		{
			this->type = TYPE_CALLABLE;
		}
		~VMFunctionValue() override
		{
			if (chunk)
			{
				chunk->Free();
				delete chunk;
			}
		}
		int Arity() const override { return arity; }
		Chunk* GetChunk() const override { return chunk; }
		operator std::string() const override { return "<fn " + name + ">"; }
		VMFunctionType GetType() const override { return VM_FUNC_FUNCTION; }
		size_t Size() const override { return sizeof(*this) + name.capacity(); }
	};

	typedef VMValue(*NativeFn)(int argCount, VMValue* args);

	// Placeholder for a native function callable. The VM will call the stored function pointer
	struct NativeFunctionValue : public VMFunctionBase
	{
		std::string name;
		int32_t arity = 0;
		NativeFn function;
		explicit NativeFunctionValue(const std::string& inName, NativeFn inFunction, int32_t inArity)
			: name(inName), function(inFunction), arity(inArity)
		{
			this->type = TYPE_CALLABLE;
		}
		int Arity() const override { return arity; }
		operator std::string() const override { return "<native fn " + name + ">"; }
		VMFunctionType GetType() const override { return VM_FUNC_NATIVE; }
		size_t Size() const override { return sizeof(*this) + name.capacity(); }
	};

	// Placeholder for a closure value, which wraps a function and its upvalues.
	// The VM will call the wrapped function and manage the upvalues as needed.
	struct VMClosureValue : public VMFunctionBase
	{
		VMValue function;
		std::vector<VMValue> upvalues;
		explicit VMClosureValue(VMValue inFunction, std::vector<VMValue> inUpvalues)
			: function(inFunction)
			, upvalues(std::move(inUpvalues))
		{
			this->type = TYPE_CALLABLE;
		}
		int Arity() const override
		{
			VMFunctionBase* functionValue = static_cast<VMFunctionBase*>(function.value);
			return functionValue->Arity();
		}
		Chunk* GetChunk() const override
		{
			return function.GetChunk();
		}
		void Blacken(VM& vm) override;
		operator std::string() const override
		{
			VMFunctionBase* functionValue = static_cast<VMFunctionBase*>(function.value);
			return "<closure " + functionValue->operator std::string() + ">";
		}
		VMFunctionType GetType() const override { return VM_FUNC_CLOSURE; }
		size_t Size() const override { return sizeof(*this) + upvalues.capacity() * sizeof(VMValue); }
	};

	struct VMClassValue : public Value
	{
		std::string name;
		VMValue superClass;
		explicit VMClassValue(const std::string& inName, VMValue inSuperClass = VMValue())
			: name(inName)
			, superClass(inSuperClass)
		{
			this->type = TYPE_CLASS;
		}
		virtual operator std::string() const override { return "<class " + name + ">"; }
		virtual size_t Size() const override  { return sizeof(*this) + name.capacity(); }
	};

	struct VMInstanceValue : public VMFunctionBase
	{
		VMValue classValue;
		std::unordered_map<std::string, VMValue> fields;
		explicit VMInstanceValue(VMValue inClass)
			: classValue(inClass)
		{
			this->type = TYPE_INSTANCE;
		}
		virtual operator std::string() const override
		{
			VMClassValue* classObj = static_cast<VMClassValue*>(classValue.value);
			return "<instance of " + classObj->name + ">";
		}
		virtual size_t Size() const override
		{
			size_t size = sizeof(*this);
			for (auto& pair : fields)
			{
				size += pair.first.capacity() + pair.second.value->Size();
			}
			return size;
		}
		void Blacken(VM& vm) override;

		virtual int Arity() const override { return 0; }
		virtual VMFunctionType GetType() const override { return VM_FUNC_CLASS; }
	};
private:
	enum Precedence
	{
		PREC_NONE,
		PREC_ASSIGNMENT,  // =
		PREC_QUESTION,    // ?:
		PREC_OR,          // or
		PREC_AND,         // and
		PREC_EQUALITY,    // == !=
		PREC_COMPARISON,  // < > <= >=
		PREC_TERM,        // + -
		PREC_FACTOR,      // * /
		PREC_UNARY,       // ! -
		PREC_CALL,        // . ()
		PREC_PRIMARY
	};

	typedef void (Compiler::* PrefixParseFn)(bool);
	typedef void (Compiler::* InfixParseFn)(bool);

	struct ParseRule
	{
		PrefixParseFn prefix;
		InfixParseFn infix;
		Precedence precedence;
	};

	// Shared parse state — owned by the root compiler, referenced by sub-compilers.
	// Keeping it in one place means all compilers in a compilation unit advance
	// through the same token stream automatically.
	struct ParseContext
	{
		struct Parser
		{
			Token current;
			Token previous;
			bool hadError  = false;
			bool panicMode = false;
		} parser;

		std::vector<Token>                  tokens;
		size_t                              nextToken = 0;
		std::unordered_map<uint32_t, bool>  globalFinals;
	};

	// Private constructor for function sub-compilers.
	// Shares the caller's ParseContext; creates own chunk internally.
	Compiler(Compiler* enclosing, ParseContext* sharedCtx);

	ParseContext  ownCtx;   // storage; only meaningful for the root compiler
	ParseContext* ctx;

	// The enclosing compiler, if any. Used to access the enclosing function's chunk for upvalue management.
	Compiler* enclosing;
	// The chunk being compiled. For the root compiler, this is the chunk of the top-level script function.
	Chunk* compilingChunk;

	// Reference aliases into *ctx so every method in the .cpp can keep its
	// existing "parser.xxx", "tokens[i]", "nextToken", "globalFinals" spelling.
	ParseContext::Parser&               parser;
	std::vector<Token>&                 tokens;
	size_t&                             nextToken;
	std::unordered_map<uint32_t, bool>& globalFinals;

	VMValue      function;
	FunctionType type;

	// Local variable tracking (adapted from C implementation)
	static constexpr uint32_t LOCAL_MAX = 0xFFFFFF;
	struct Local
	{
		Token name;
		int   depth   = -1;
		bool  isCaptured = false;
		bool  isFinal = false;
	};
	std::vector<Local> locals;

	struct UpValue
	{
		int32_t index;
		bool    isLocal;
		bool    isFinal;
	};
	std::vector<UpValue> upvalues;

	int scopeDepth = 0;

	uint32_t currentLoopStart = -1;
	uint32_t currentLoopContinue = -1;
	std::unordered_map<uint32_t, std::vector<uint32_t>> breakJumpPatches;

	void Init(FunctionType type, const std::string& name = "");

	// --- Core Parsing Flow ---
	void Advance();
	void Delclaration();
	void Statement();
	void VarDeclaration(bool isFinal);
	void FinalVarDeclaration();
	void FunctionDeclaration();
	void ClassDeclaration();
	void Function(FunctionType type, const std::string& name = "");
	void PrintStatement();
	void ExpressionStatement();
	void IfStatement();
	void WhileStatement();
	void BreakStatement();
	void ContinueStatement();
	void SwitchStatement();
	void ForStatement();
	void ReturnStatement();
	void Expression();
	void BeginScope();
	void Block();
	void EndScope();
	void ParsePrecedence(Precedence precedence);
	void Synchronize();
	VMValue EndCompiler();

	// --- Grammar Rules ---
	void Number(bool canAssign);
	void Literal(bool canAssign);
	void String(bool canAssign);
	void Grouping(bool canAssign);
	void Unary(bool canAssign);
	void Binary(bool);
	void Trinary(bool);
	void Equality(bool);
	void And(bool);
	void Or(bool);
	void NamedVariable(bool);
	void Call(bool);
	void Dot(bool canAssign);

	// --- Token Helpers ---
	Token ScanToken();
	Token Peek(int32_t offset);
	bool Check(TokenType type);
	bool Match(TokenType type);
	void Consume(TokenType type, const char* message);
	ParseRule* GetRule(TokenType type);

	// --- Code Generation ---
	void EmitByte(uint8_t byte);
	void EmitBytes() {}
	template<typename... Args>
	void EmitBytes(uint8_t byte, Args... args)
	{
		EmitByte(byte);
		EmitBytes(args...);
	}
	void EmitConstant(VMValue value);
	Chunk* CurrentChunk();

	// --- Variable Helpers ---
	uint32_t ParseVariable(const std::string& errorMessage, bool isFinal);
	uint32_t MakeConstant(VMValue value);
	uint32_t IdentifierConstant(const Token& name);
	void DefineVariable(uint32_t global, bool isFinal);
	void DeclareVariable(bool isFinal);
	void AddLocal(const Token& name, bool isFinal);
	void MarkInitialize();
	int32_t ResolveLocal(const Token& name);
	int32_t ResolveUpvalue(const Token& name);
	int32_t AddUpvalue(int32_t index, bool isLocal, bool isFinal);

	// --- Jump Helpers ---
	int32_t EmitJump(uint8_t instruction);
	void PatchJump(int32_t offset);
	void EmitLoop(int32_t loopStart);
	void PatchBreaks(int32_t loopStart);

	// --- Error Handling ---
	void Error(const char* message);
	void ErrorAt(Token* token, const char* message);
	void ErrorAtCurrent(const char* message);

	// --- Argument List Parsing ---
	uint8_t ArgumentList();
};
