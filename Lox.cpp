#include "Lox.h"
#include "Scanner.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <string>

Lox* Lox::instance = nullptr;

Lox::Lox()
{
	if (instance)
	{
		printf("Lox have more than 1 instance.");
		exit(0);
	}
}

void Lox::Run(int argc, char* argv[])
{

    if (argc > 1)
    {
        printf("Argument count error: %d, expected at most 1\n", argc);
        exit(64);
    }
    else if (argc == 1)
    {
        RunFile(argv[0]);
    }
    else
    {
        RunPrompt();
    }
}

void Lox::RunFile(const char* path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		printf("Could not open file: %s\n", path);
		return;
	}
	file.seekg(0, std::ios::end);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	if (!file.read(buffer.data(), size))
	{
		printf("Failed to read file: %s\n", path);
		return;
	}
	printf("File loaded (%zd bytes)\n", buffer.size());
	Run(buffer.data());

	// Indicate an error in the exit code.
	if (hadError)
	{
		exit(65);
	}
}

void Lox::RunPrompt()
{
	char line[1024];
	while (true)
	{
		printf("> ");
		if (!fgets(line, sizeof(line), stdin)) break;
		Run(line);
		hadError = false;
	}
}

void Lox::Run(const char* source)
{
	Scanner scanner(source);
	std::vector<Token> tokens = scanner.ScanTokens();
	for (const Token& token : tokens)
	{
		printf("[LOG] Token: '%s' | type: %d | line: %zd\n", token.lexeme.c_str(), static_cast<int>(token.type), token.line);
	}
	printf("[LOG] Total tokens: %zu\n", tokens.size());
}

void Lox::Error(size_t line, size_t column, const char* fmt, ...)
{
	char buffer[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	Report(line, column, "", buffer);
}

void Lox::Report(size_t line, size_t column, const char* where, const char* message)
{
	printf("[%zd:%zd] Error%s: %s\n", line, column, where, message);
	hadError = true;
}