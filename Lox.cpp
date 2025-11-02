#include "Lox.h"
#include "Scanner.h"
#include "Parser.h"
#include "Interpreter.h"
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
	interpreter = new Interpreter();
}

Lox::~Lox()
{
	delete interpreter;
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
	runForPrompt = false;
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
	if (hadRuntimeError)
	{
		exit(70);
	}
}

void Lox::RunPrompt()
{
	runForPrompt = true;
	char line[1024];
	while (true)
	{
		printf("> ");
		if (!fgets(line, sizeof(line), stdin)) break;
		Run(line);
		hadError = false;
		hadRuntimeError = false;
	}
}

void Lox::Run(const char* source)
{
	Scanner scanner(source);
	std::vector<Token> tokens = scanner.ScanTokens();
	Parser parser(tokens);

	if (runForPrompt)
	{
		ignoreError = true;
		ExprPtr expr = parser.ParseExpr();
		ignoreError = false;
		if (hadError) return;
		if (expr)
		{
			ValuePtr result = interpreter->InterpretExpr(expr);
			if (hadRuntimeError) return;
			if (result)
			{
				std::cout << interpreter->Stringify(result) << std::endl;
			}
			return;
		}
	}

	parser.Reset();
	std::vector<StatPtr> stats = parser.Parse();
	if (hadError) return;
	interpreter->Interpret(stats);
}

void Lox::Error(size_t line, size_t column, const char* fmt, ...)
{
	if (ignoreError)
	{
		return;
	}
	char buffer[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	Report(line, column, "", buffer);
	hadError = true;
}

void Lox::RuntimeError(size_t line, size_t column, const char* fmt, ...)
{
	if (ignoreError)
	{
		return;
	}
	char buffer[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	Report(line, column, "RuntimeError", buffer);
	hadRuntimeError = true;
}

void Lox::RuntimeError(const char* fmt, ...)
{
	if (ignoreError)
	{
		return;
	}
	char buffer[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	Report(0, 0, "RuntimeError", buffer);
	hadRuntimeError = true;
}

void Lox::Report(size_t line, size_t column, const char* where, const char* message)
{
	if (line != 0 && column != 0)
	{
		printf("[%zd:%zd] %s: %s\n", line, column, where, message);
	}
	else
	{
		printf("%s: %s\n", where, message);
	}
}