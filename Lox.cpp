#include "Lox.h"
#include "Scanner.h"
#include "Parser.h"
#include "Resolver.h"
#include "Interpreter.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <memory>

Lox* Lox::instance = nullptr;

// 辅助函数：使用可变参数格式化字符串，返回 std::string
// 这个函数会安全地处理缓冲区大小，避免溢出
std::string FormatString(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	// 创建一个 va_list 的副本，因为 vsnprintf 可能会修改它
	va_list args_copy;
	va_copy(args_copy, args);
	// 第一次调用 vsnprintf 获取格式化后所需的字符串长度 (不包括末尾的 '\0')
	int len = vsnprintf(nullptr, 0, fmt, args_copy);
	va_end(args_copy);

	if (len < 0)
	{
		va_end(args);
		// 如果发生编码错误，返回一个错误信息
		return "Format error";
	}

	// 创建一个足够大的缓冲区来存储格式化后的字符串
	// 使用 std::unique_ptr 来自动管理内存
	auto buffer = std::make_unique<char[]>(len + 1);
	// 第二次调用 vsnprintf，将结果写入缓冲区
	vsnprintf(buffer.get(), len + 1, fmt, args);
	va_end(args);

	return std::string(buffer.get());
}

Lox::Lox()
{
	if (instance)
	{
		std::cout << "Lox have more than 1 instance.";
		exit(0);
	}
	interpreter = new Interpreter();
	resolver = new Resolver(interpreter);
}

Lox::~Lox()
{
	delete resolver;
	delete interpreter;
}

void Lox::Run(int argc, char* argv[])
{
    if (argc > 2) // 修正：argc 包含程序名，所以 > 2 表示多于一个参数
    {
        std::cout << FormatString("Argument count error: %d, expected at most 1\n", argc - 1);
        exit(64);
    }
    else if (argc == 2) // 修正：argc == 2 表示有一个文件路径参数
    {
        RunFile(argv[1]);
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
		std::cout << FormatString("Could not open file: %s\n", path);
		return;
	}
	file.seekg(0, std::ios::end);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	if (!file.read(buffer.data(), size))
	{
		std::cout << FormatString("Failed to read file: %s\n", path);
		return;
	}
	std::cout << FormatString("File loaded (%zd bytes)\n", buffer.size());
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
		std::cout << "> ";
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
			resolver->Resolve(expr);
			if (hadSemanticError) return;

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

	resolver->Resolve(stats);
	if (hadSemanticError) return;

	interpreter->Interpret(stats);
}

void Lox::ResetError()
{
	hadError = false;
	hadSemanticError = false;
	hadRuntimeError = false;
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

void Lox::SemanticError(size_t line, size_t column, const char* fmt, ...)
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
	Report(line, column, "SemanticError", buffer);
	hadSemanticError = true;
}

void Lox::SemanticError(const char* fmt, ...)
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
	Report(0, 0, "SemanticError", buffer);
	hadSemanticError = true;
}

void Lox::Report(size_t line, size_t column, const char* where, const char* message)
{
	if (line != 0 && column != 0)
	{
		std::cout << FormatString("[%zd:%zd] %s: %s\n", line, column, where, message);
	}
	else
	{
		std::cout << FormatString("%s: %s\n", where, message);
	}
}