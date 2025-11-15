#pragma once

class Lox
{
protected:
	bool hadError = false;
	bool hadSemanticError = false;
	bool hadRuntimeError = false;
	bool runForPrompt = false;
	bool ignoreError = false;
	static Lox* instance;
	class Resolver* resolver;
	class Interpreter* interpreter;
	Lox();
	~Lox();
public:
	static Lox& GetInstance()
	{
		if (!instance)
		{
			instance = new Lox();
		}
		return *instance;
	}
	void Run(int argc, char* argv[]);
protected:
	void RunFile(const char* path);
	void RunPrompt();
	void Run(const char* source);
	void Report(size_t line, size_t column, const char* where, const char* message);
public:
	void ResetError();
	void Error(size_t line, size_t column, const char* fmt, ...);
	void SemanticError(size_t line, size_t column, const char* fmt, ...);
	void SemanticError(const char* fmt, ...);
	void RuntimeError(size_t line, size_t column, const char* fmt, ...);
	void RuntimeError(const char* fmt, ...);

	bool HasRuntimeError() const { return hadRuntimeError; }
};