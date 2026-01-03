#include "Compiler.h"

bool Compiler::Compile(const char* source, Chunk* outChunk)
{
	Scanner scanner(source);
	tokens = scanner.ScanTokens();
	if (tokens.empty() || tokens[tokens.size() - 1].type != END_OF_FILE)
	{
		return false;
	}
	Advance();
	Consume(END_OF_FILE, "Expect end of expression.");
	return true;
}

void Compiler::Advance()
{
	parser.previous = parser.current;
	while (true)
	{
		parser.current = ScanToken();
		if (parser.current.type != ERROR)
		{
			break;
		}
		ErrorAtCurrent(parser.current.lexeme.c_str());
	}
}

void Compiler::Error(const char* message)
{
	ErrorAt(&parser.previous, message);
}

Token Compiler::ScanToken()
{
	if (currentToken >= tokens.size())
	{
		return Token(ERROR, "", 0, 0);
	}
	return tokens[currentToken++];
}

void Compiler::Consume(TokenType type, const char* message)
{
	if (parser.current.type == type)
	{
		Advance();
		return;
	}
	ErrorAt(&parser.current, message);
}

void Compiler::ErrorAtCurrent(const char* message)
{
	ErrorAt(&parser.current, message);
}

void Compiler::ErrorAt(Token* token, const char* message)
{
	if (parser.panicMode)
	{
		return;
	}
	parser.panicMode = true;
	fprintf(stderr, "[%zu:%zu] Error", token->line, token->column);
	if (token->type == END_OF_FILE)
	{
		fprintf(stderr, " at end");
	}
	else if (token->type == ERROR)
	{
		// Nothing.
	}
	else
	{
		fprintf(stderr, " at '%s'", token->lexeme.c_str());
	}
	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}
