#pragma once
#include "Scanner.h"
#include "Chunk.h"

class Compiler
{
public:
    bool Compile(const char* source, Chunk* outChunk);
    void Advance();
    void Error(const char* message);

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

    typedef void (Compiler::*ParseFn)();

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
protected:
    Token ScanToken();
    void Consume(TokenType type, const char* message);
    void ErrorAt(Token* token, const char* message);
    void ErrorAtCurrent(const char* message);

    void EmitByte(uint8_t byte);
    // base-case overload to stop recursion
    void EmitBytes() {}
    template<typename... Args>
    void EmitBytes(uint8_t byte, Args... args)
    {
        EmitByte(byte);
        EmitBytes(args...);
    }
    void EmitConstant(VMValue value);

    void EndCompiler();

	ParseRule* GetRule(TokenType type);

	void Expression();
    void ParsePrecedence(Precedence precedence);
    void Number();
    void Grouping();
	void Unary();
    void Binary();
	void Trinary();
};