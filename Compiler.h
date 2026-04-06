#pragma once
#include "Scanner.h"
#include "Chunk.h"
#include <vector>

class Compiler
{
protected:
public:
	Compiler(Chunk* chunk = nullptr);  // Root compiler: takes external chunk
	~Compiler();
	VMValue Compile(const char* source);
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

	enum FunctionType
	{
		TYPE_SCRIPT,
		TYPE_FUNCTION,
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
		std::unordered_map<uint32_t, bool>  globalConstants;
	};

	// Lightweight placeholder for the top-level script callable.
	// Inherits directly from Value so no extra headers are needed.
	struct ScriptFunction : public Value
	{
		ScriptFunction() { this->type = TYPE_CALLABLE; }
		operator std::string() const override { return "<script>"; }
	};

	// Placeholder for a named function compiled by the VM compiler.
	struct VMFunctionValue : public Value
	{
		std::string name;
		explicit VMFunctionValue(const std::string& inName) : name(inName) { this->type = TYPE_CALLABLE; }
		operator std::string() const override { return "<fn " + name + ">"; }
	};

	// Private constructor for function sub-compilers.
	// Shares the caller's ParseContext; creates own chunk internally.
	Compiler(ParseContext* sharedCtx, FunctionType fnType, const std::string& name);

	ParseContext  ownCtx;   // storage; only meaningful for the root compiler
	ParseContext* ctx;

	Chunk* ownedChunk = nullptr;  // non-null only for sub-compilers that own their chunk
	bool   ownsChunk  = false;    // true if this compiler is responsible for freeing ownedChunk      // always valid — == &ownCtx for root, parent ptr for subs

	// Reference aliases into *ctx so every method in the .cpp can keep its
	// existing "parser.xxx", "tokens[i]", "nextToken", "globalConstants" spelling.
	ParseContext::Parser&               parser;
	std::vector<Token>&                 tokens;
	size_t&                             nextToken;
	std::unordered_map<uint32_t, bool>& globalConstants;

	VMValue      function;
	FunctionType type;

	// Local variable tracking (adapted from C implementation)
	static constexpr uint32_t LOCAL_MAX = 0xFFFFFF;
	struct Local
	{
		Token name;
		int   depth    = -1;
		bool  constant = false;
	};
	std::vector<Local> locals;
	int scopeDepth = 0;

	uint32_t currentLoopStart = -1;
	uint32_t currentLoopContinue = -1;
	std::unordered_map<uint32_t, std::vector<uint32_t>> breakJumpPatches;

	void Init(FunctionType type, const std::string& name = "");

	// --- Core Parsing Flow ---
	void Advance();
	void Delclaration();
	void Statement();
	void VarDeclaration(bool constant);
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
	uint32_t ParseVariable(const std::string& errorMessage, bool constant);
	uint32_t MakeConstant(VMValue value);
	uint32_t IdentifierConstant(const Token& name);
	void DefineVariable(uint32_t global, bool constant);
	void DeclareVariable(bool constant);
	void AddLocal(const Token& name, bool constant);
	void MarkInitialize();
	int ResolveLocal(const Token& name);

	// --- Jump Helpers ---
	int32_t EmitJump(uint8_t instruction);
	void PatchJump(int32_t offset);
	void EmitLoop(int32_t loopStart);
	void PatchBreaks(int32_t loopStart);

	// --- Error Handling ---
	void Error(const char* message);
	void ErrorAt(Token* token, const char* message);
	void ErrorAtCurrent(const char* message);
};
