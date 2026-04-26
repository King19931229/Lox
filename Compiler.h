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
		VM_FUNC_CLOSURE
	};
	
	friend class VM;
	struct VMFunctionBase : public Value
	{
		virtual int Arity() const = 0;
		virtual VMFunctionType GetType() const = 0;
	};

	// Lightweight placeholder for the top-level script callable.
	// Inherits directly from Value so no extra headers are needed.
	struct ScriptFunction : public VMFunctionBase
	{
		explicit ScriptFunction() { this->type = TYPE_CALLABLE; }
		int Arity() const override { return 0; }
		operator std::string() const override { return "<script>"; }
		VMFunctionType GetType() const override { return VM_FUNC_SCRIPT; }
	};

	// Placeholder for a named function compiled by the VM compiler.
	struct VMFunctionValue : public VMFunctionBase
	{
		std::string name;
		int32_t arity = 0;
		int32_t upvalueCount = 0;
		explicit VMFunctionValue(const std::string& inName) : name(inName) { this->type = TYPE_CALLABLE; }
		int Arity() const override { return arity; }
		operator std::string() const override { return "<fn " + name + ">"; }
		VMFunctionType GetType() const override { return VM_FUNC_FUNCTION; }
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
	};

	// Placeholder for a closure value, which wraps a function and its upvalues.
	// The VM will call the wrapped function and manage the upvalues as needed.
	struct VMClosureValue : public VMFunctionBase
	{
		VMValue function;
		std::vector<VMValue> upvalues;
		explicit VMClosureValue(VMValue inFunction, std::vector<VMValue> inUpvalues)
			: function(inFunction), upvalues(std::move(inUpvalues))
		{
			this->type = TYPE_CALLABLE;
		}
		int Arity() const override
		{
			VMFunctionBase* functionValue = static_cast<VMFunctionBase*>(function.value);
			return functionValue->Arity();
		}
		operator std::string() const override
		{
			VMFunctionBase* functionValue = static_cast<VMFunctionBase*>(function.value);
			return "<closure " + functionValue->operator std::string() + ">";
		}
		VMFunctionType GetType() const override { return VM_FUNC_CLOSURE; }
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
	typedef void (Compiler::* InfixParseFn)();

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
	void Binary();
	void Trinary();
	void Equality();
	void And();
	void Or();
	void NamedVariable(bool canAssign);
	void Call();

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
