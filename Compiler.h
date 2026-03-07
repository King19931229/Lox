#pragma once
#include "Scanner.h"
#include "Chunk.h"

class Compiler
{
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

	typedef void (Compiler::* ParseFn)();

	struct ParseRule
	{
		ParseFn prefix;
		ParseFn infix;
		Precedence precedence;
	};

	struct Parser
	{
		Token current;
		Token previous;
		bool hadError = false;
		bool panicMode = false;
	} parser;

	std::vector<Token> tokens;
	size_t currentToken = 0;
	Chunk* compilingChunk = nullptr;

	// --- Core Parsing Flow ---
	void Advance();
	void Delclaration();
	void Statement();
	void VarDeclaration();
	void PrintStatement();
	void Expression();
	void ParsePrecedence(Precedence precedence);
	void Synchronize();
	void EndCompiler();

	// --- Grammar Rules ---
	void Number();
	void Literal();
	void String();
	void Grouping();
	void Unary();
	void Binary();
	void Trinary();
	void Equality();

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

	// --- Error Handling ---
	void Error(const char* message);
	void ErrorAt(Token* token, const char* message);
	void ErrorAtCurrent(const char* message);
};