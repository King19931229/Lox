#pragma once
#include "Scanner.h"
#include "Chunk.h"

class Compiler
{
public:
    bool Compile(const char* source, Chunk* outChunk);
    void Advance();
    void Error(const char* message);

protected:
    Token ScanToken();
    void Consume(TokenType type, const char* message);
    void ErrorAt(Token* token, const char* message);
    void ErrorAtCurrent(const char* message);

private:
    struct Parser
    {
        Token current;
        Token previous;
        bool hadError = false;
        bool panicMode = false;
    } parser;
    std::vector<Token> tokens;
    size_t currentToken = 0;
};