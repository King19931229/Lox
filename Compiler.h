#pragma once
#include "Scanner.h"
#include "Chunk.h"
#include <vector>

class Compiler
{
protected:
public:
	bool Compile(const char* source, Chunk* outChunk);
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

	struct Parser
	{
		Token current;
		Token previous;
		bool hadError = false;
		bool panicMode = false;
	} parser;

	// Local variable tracking (adapted from C implementation)
	static constexpr uint32_t LOCAL_MAX = 0xFFFFFF;
	struct Local
	{
		Token name;
		int depth = -1;
	};

	std::vector<Local> locals;
	int scopeDepth = 0;

	std::vector<Token> tokens;
	size_t nextToken = 0;
	Chunk* compilingChunk = nullptr;

	void Init();

	// --- Core Parsing Flow ---
	void Advance();
	void Delclaration();
	void Statement();
	void VarDeclaration();
	void PrintStatement();
	void ExpressionStatement();
	void Expression();
	void BeginScope();
	void Block();
	void EndScope();
	void ParsePrecedence(Precedence precedence);
	void Synchronize();
	void EndCompiler();

	// --- Grammar Rules ---
	void Number(bool canAssign);
	void Literal(bool canAssign);
	void String(bool canAssign);
	void Grouping(bool canAssign);
	void Unary(bool canAssign);
	void Binary();
	void Trinary();
	void Equality();
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

	// --- Variable Helpers ---
	uint32_t ParseVariable(const std::string& errorMessage);
	uint32_t MakeConstant(VMValue value);
	uint32_t IdentifierConstant(const Token& name);
	void DefineVariable(uint32_t global);
	void DeclareVariable();
	void AddLocal(const Token& name);
	int ResolveLocal(const Token& name);

	// --- Error Handling ---
	void Error(const char* message);
	void ErrorAt(Token* token, const char* message);
	void ErrorAtCurrent(const char* message);
};