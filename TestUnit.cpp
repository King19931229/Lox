#include "TestUnit.h"
#include "Scanner.h"
#include "Parser.h"
#include "Interpreter.h"
#include "Resolver.h"
#include "VM.h"
#include "Lox.h"
#include <cstdio>
#include <vector>
#include <string>
#include <utility> // for std::pair
#include <iostream> // 用于捕获 cout
#include <sstream>  // 用于捕获 cout
#ifdef _WIN32
#include <io.h>
#endif

#ifdef _WIN32
#include <windows.h> // 包含 Windows API 用于控制台颜色
#endif

void TestUnit::RunScannerTest()
{
	std::string source = R"lox(
		// Single line comment
		/*
		 * Multi-line comment (not supported in this scanner, but should be ignored if implemented)
		 */
		class Foo {
			fun bar(x, y) {
				var str = "Hello, \"Lox\"!\nTab:\tBackslash:\\";
				var num = 3.1415;
				var flag = true;
				if (x > y and flag or !flag) {
					print(str + " " + num);
				} else {
					print("fail");
				}
				for (var i = 0; i < 10; i = i + 1) {
					print(i);
				}
				while (num >= 0) {
					num = num - 1;
				}
				return nil;
			}
		}
		var obj = Foo();
		obj.bar(42, 24);
		// Edge cases
		var empty = "";
		var esc = "a\\nb\\tc\\\"d\\\\e";
		var num2 = 123.456e-2; // scientific notation, not supported but should be tokenized as number and dot
		var weird = _var123 + foo - __bar__;
	)lox";

	Scanner scanner(source);
	std::vector<Token> tokens = scanner.ScanTokens();

	printf("Token count: %zu\n", tokens.size());
	for (const Token& token : tokens)
	{
		printf("Token: %-20s | type: %s | line: %-2zd column: %-2zd\n", token.lexeme.c_str(), TokenTypeName[token.type], token.line, token.column);
	}
}

void TestUnit::RunExpressionParserTest()
{
	const std::vector<std::string> testSources = {
		// 1. 优先级和结合性测试
		"1 + 2 * 3 - 4 / 5",         // 预期: (1 + (2 * 3)) - (4 / 5)
		"1 - 2 - 3",                 // 预期: (1 - 2) - 3 (左结合)
		"-1 * (2 + 3)",              // 预期: (-1) * (2 + 3)
		"!true == false",            // 预期: (!true) == false

		// 2. 三元运算符 (右结合)
		"1 ? 2 : 3 ? 4 : 5",         // 预期: 1 ? 2 : (3 ? 4 : 5)
		"1 > 2 ? 3 + 4 : 5 * 6",     // 预期: (1 > 2) ? (3 + 4) : (5 * 6)

		// 3. 逗号运算符 (最低优先级)
		"1, 2 + 3, 4",               // 预期: ((1, (2 + 3)), 4)
		"1 ? 2, 3 : 4",              // 预期: (1 ? (2, 3) : 4)

		// 4. 复杂的括号和嵌套
		"((1 + 2) * (3 - 4)) / 5",
		"1 + (2 * (3 - (4 / 5)))",

		// 5. 错误处理：缺少左操作数
		"* 1 + 2",                   // 错误: '*' 缺少左操作数
		"1 + > 2",                   // 错误: '>' 缺少左操作数
		"== 3",                      // 错误: '==' 缺少左操作数

		// 6. 其他语法错误
		"1 + ",                      // 错误: 表达式末尾缺少操作数
		"1 ? 2",                     // 错误: '?' 缺少 ':'
		"(1 + 2",                    // 错误: 缺少 ')'
		"1 + (2 * 3",
	};

	for (const auto& source : testSources)
	{
		printf("--- Testing Expression: \"%s\" ---\n", source.c_str());
		Scanner scanner(source);
		std::vector<Token> tokens = scanner.ScanTokens();

		// (可选) 打印 tokens 以便调试
		for (const Token& token : tokens)
		{
			printf("  Token: %-15s | type: %s\n", token.lexeme.c_str(), TokenTypeName[token.type]);
		}

		Parser parser(tokens);
		parser.Parse(); // Parse() 会在内部调用错误报告
		printf("----------------------------------------\n\n");
	}
}

void TestUnit::RunExpressionInterpreterTest()
{
	const std::vector<std::pair<std::string, std::string>> testCases = {
		// Literals
		{ "123", "123" },
		{ "123.45", "123.449997" }, // std::to_string for float has this format
		{ "\"hello\"", "hello" },
		{ "true", "true" },
		{ "false", "false" },
		{ "nil", "nil" },

		// Unary
		{ "-10", "-10" },
		{ "-10.5", "-10.500000" },
		{ "!true", "false" },
		{ "!false", "true" },
		{ "!nil", "true" },
		{ "!0", "true" },
		{ "!1", "false" },
		{ "!\"\"", "true" },
		{ "!\"hello\"", "false" },

		// Arithmetic
		{ "1 + 2", "3" },
		{ "10 - 3.5", "6.500000" },
		{ "5 * 2.5", "12.500000" },
		{ "10 / 4", "2" }, // int / int -> int
		{ "10 / 4.0", "2.500000" },
		{ "\"hello\" + \" world\"", "hello world" },

		// Comparison
		{ "5 > 3", "true" },
		{ "5 < 3", "false" },
		{ "5 >= 5", "true" },
		{ "3.5 <= 3", "false" },

		// Equality
		{ "1 == 1", "true" },
		{ "1 != 2", "true" },
		{ "1 == 1.0", "true" }, // Note: This depends on your IsEqual implementation
		{ "nil == nil", "true" },
		{ "nil == false", "false" },
		{ "\"a\" == \"a\"", "true" },
		{ "\"a\" == \"b\"", "false" },

		// Ternary
		{ "true ? 1 : 2", "1" },
		{ "false ? 1 : 2", "2" },
		{ "5 > 3 ? \"yes\" : \"no\"", "yes" },

		// Comma
		{ "1, 2, 3", "3" },
		{ "(1, 2), 3", "3" },

		// Complex expressions
		{ "-(1 + 2) * 3", "-9" },
		{ "1 + 2 * 3 / 4", "2" }, // 1 + ((2*3)/4) = 1 + (6/4) = 1 + 1 = 2
		{ "1 + 2 * 3 / 4.0", "2.500000" }, // 1 + ((2*3)/4.0) = 1 + (6/4.0) = 1 + 1.5 = 2.5
		{ "(5 > 3 ? (1, 2) : 3) + 10", "12" }, // (2) + 10 = 12

		// Runtime error cases
		{ "5 / 0", "Division by zero." },
		{ "\"a\" - \"b\"", "Operands must be numbers for subtraction." },
		{ "-true", "Operand must be a number for unary minus." },

		// --- 新增的复杂测试用例 ---
		{ "(10.5 - 0.5) / (2 * 2) + (1, 2, 3)", "5.500000" }, // (10.0 / 4) + 3 = 2.5 + 3 = 5.5
		{ "\"result: \" + (true ? \"pass\" : \"fail\")", "result: pass" }
	};

	Interpreter interpreter;
	Resolver resolver(&interpreter);
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD saved_attributes = 0;
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	if (GetConsoleScreenBufferInfo(hConsole, &consoleInfo))
	{
		saved_attributes = consoleInfo.wAttributes;
	}
#endif

	for (const auto& test : testCases)
	{
		printf("--- Testing: \"%s\" ---\n", test.first.c_str());
		std::string resultString;

		Scanner scanner(test.first);
		auto tokens = scanner.ScanTokens();
		Parser parser(tokens);
		ExprPtr expression = parser.ParseExpr();

		// 只有在解析成功时才进行解释
		if (expression && !parser.HasError())
		{
			resolver.Resolve(expression);
			ValuePtr result = interpreter.InterpretExpr(expression);
			resultString = static_cast<std::string>(*result);
		}
		else
		{
			// 如果解析失败，我们将其标记为 "Parse Error"
			resultString = "Parse Error";
		}

		if (resultString == test.second)
		{
			printf("  [PASS] Expected: %s, Got: %s\n", test.second.c_str(), resultString.c_str());
		}
		else
		{
#ifdef _WIN32
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
#endif
			printf("  [FAIL] Expected: %s, Got: %s\n", test.second.c_str(), resultString.c_str());
#ifdef _WIN32
			if (saved_attributes != 0) SetConsoleTextAttribute(hConsole, saved_attributes);
#endif
		}
		printf("----------------------------------------\n\n");
	}
}

// 辅助函数：运行代码并捕获 stdout 输出
std::string RunWithCapture(const std::string& source)
{
	// 1. 备份原始的 cout 缓冲并重定向
	std::streambuf* oldCoutStreamBuf = std::cout.rdbuf();
	std::ostringstream strCout;
	std::cout.rdbuf(strCout.rdbuf());

	Lox::GetInstance().ResetError();

	// 2. 运行扫描、解析和解释流程
	Scanner scanner(source);
	auto tokens = scanner.ScanTokens();
	Parser parser(tokens);
	auto statements = parser.Parse();

	// 只有在解析成功时才进行解释
	if (!parser.HasError())
	{
		Interpreter interpreter;
		Resolver resolver(&interpreter);
		resolver.Resolve(statements);
		interpreter.Interpret(statements); // 假设 Interpret 会执行语句
	}
	else
	{
		// 如果解析失败，返回一个特定的错误信息
		strCout << "Parse Error";
	}

	// 3. 恢复原始的 cout 缓冲
	std::cout.rdbuf(oldCoutStreamBuf);

	// 4. 返回捕获到的输出
	return strCout.str();
}

// 新增辅助函数：转义字符串以便打印
std::string EscapeForPrinting(const std::string& str)
{
	std::string result;
	for (char c : str)
	{
		switch (c)
		{
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			case '\"': result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			default:
				result += c;
				break;
		}
	}
	return result;
}

struct VMRunResult
{
	std::string output;
	InterpretResult result = INTERPRET_OK;
};

class VMTestHarness : public VM
{
public:
	void Reset()
	{
		Free();
		Init();
		globalNameToSlot.clear();
		globalSlots.clear();
	}
};

VMRunResult RunVMWithCapture(const std::string& source)
{
	VMRunResult runResult;
	VMTestHarness vm;
	vm.Reset();
	// Capture std::cout like other helpers (Chunk::PrintValue now uses std::cout)
	std::streambuf* oldCout = std::cout.rdbuf();
	std::ostringstream oss;
	std::cout.rdbuf(oss.rdbuf());

	FILE* stderrCapture = nullptr;
	int savedStderrFd = -1;
	std::string stderrOutput;

#ifdef _WIN32
	if (tmpfile_s(&stderrCapture) == 0 && stderrCapture != nullptr)
	{
		savedStderrFd = _dup(_fileno(stderr));
		if (savedStderrFd != -1)
		{
			fflush(stderr);
			_dup2(_fileno(stderrCapture), _fileno(stderr));
		}
	}
#endif

	Lox::GetInstance().ResetError();
	runResult.result = vm.Interpret(source.c_str());

	std::cout.flush();
	fflush(stderr);

#ifdef _WIN32
	if (savedStderrFd != -1)
	{
		_dup2(savedStderrFd, _fileno(stderr));
		_close(savedStderrFd);
	}

	if (stderrCapture != nullptr)
	{
		std::ostringstream errStream;
		rewind(stderrCapture);
		char buffer[256];
		size_t bytesRead = 0;
		while ((bytesRead = std::fread(buffer, 1, sizeof(buffer), stderrCapture)) > 0)
		{
			errStream.write(buffer, (std::streamsize)bytesRead);
		}
		stderrOutput = errStream.str();
		fclose(stderrCapture);
	}
#endif

	std::cout.rdbuf(oldCout);
	runResult.output = oss.str() + stderrOutput;

	return runResult;
}

void TestUnit::RunStatementInterpreterTest()
{
	const std::vector<std::pair<std::string, std::string>> testCases = {
		// 1. Print 语句
		{ "print 123;", "123\n" },
		{ "print \"hello, world!\";", "hello, world!\n" },
		{ "print true;", "true\n" },
		{ "print nil;", "nil\n" },
		{ "print 1 + 2 * 3;", "7\n" },

		// 2. 表达式语句 (应该没有输出)
		{ "1 + 2;", "" },
		{ "false;", "" },

		// 3. 变量声明和赋值实现
		{ "var a = 10; a + 20;", "" },
		{ "var msg = \"test\"; print msg;", "test\n" },
		{ "var x = 5; x = x + 10; print x;", "15\n" },

		// 4. 多条语句
		{ "print 1; print 2; print 3;", "1\n2\n3\n" },
		{ "1 + 1; print \"result\"; 3*3;", "result\n" },

		// 5. 解析错误
		{ "print 123", "[1:7] : Expect ';' after '123'.\nParse Error" }, // 缺少分号

		// 6. 块作用域
		{ "{ print 1; print 2; }", "1\n2\n" },
		{ "var a = 5; { var a = 10; print a; } print a;", "10\n5\n" },
		// { "var a = 10; { var a = a + 20; print a; } print a;", "30\n10\n" },

		// 7. 变量声明与赋值
		{ "var a = 10; print a;", "10\n" },
		{ "var a = 1; a = 2; print a;", "2\n" },
		{ "var a = \"hello\"; var b = \" world\"; print a + b;", "hello world\n" },

		// 8. 条件语句
		{ "if (true) print \"yes\";", "yes\n" },
		{ "if (false) print \"yes\"; else print \"no\";", "no\n" },
		{ "var a = 1; if (a > 0) { print \"positive\"; }", "positive\n" },

		// 9. 循环语句
		{ "var i = 0; while (i < 3) { print i; i = i + 1; }", "0\n1\n2\n" },
		{ "for (var j = 0; j < 2; j = j + 1) { print j; }", "0\n1\n" },
		{ "var sum = 0; for (var k = 1; k <= 3; k = k + 1) { sum = sum + k; } print sum;", "6\n" },

		// 10. break
		{ "var i = 0; while (true) { if (i == 2) { break; } print i; i = i + 1; }", "0\n1\n" },
		{ "for (var i = 0; i < 2; i = i + 1) { print \"outer\"; for (var j = 0; j < 2; j = j + 1) { print \"inner\"; break; } }", "outer\ninner\nouter\ninner\n" },
		{ "var i = 0; while (i < 3) { print i; if (i == 1) { break; print 99; } i = i + 1; }", "0\n1\n" },

	};

#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD saved_attributes = 0;
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	if (GetConsoleScreenBufferInfo(hConsole, &consoleInfo))
	{
		saved_attributes = consoleInfo.wAttributes;
	}
#endif

	for (const auto& test : testCases)
	{
		printf("--- Testing Statement: \"%s\" ---\n", test.first.c_str());

		std::string capturedOutput = RunWithCapture(test.first);

		// 使用转义函数来美化输出
		std::string expectedEscaped = EscapeForPrinting(test.second);
		std::string gotEscaped = EscapeForPrinting(capturedOutput);

		if (capturedOutput == test.second)
		{
			printf("  [PASS] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
		}
		else
		{
#ifdef _WIN32
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
#endif
			printf("  [FAIL] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
#ifdef _WIN32
			if (saved_attributes != 0) SetConsoleTextAttribute(hConsole, saved_attributes);
#endif
		}
		printf("----------------------------------------\n\n");
	}
}

void TestUnit::RunFunctionInterpreterTest()
{
	const std::vector<std::pair<std::string, std::string>> testCases = {
		// Basic function call
		{ "fun sayHi() { print \"hi\"; } sayHi();", "hi\n" },
		// Function with parameters
		{ "fun echo(a) { print a; } echo(123);", "123\n" },
		// Scoping with functions
		{ "var a = \"global\"; fun showA() { print a; } showA();", "global\n" },
		{ "var a = \"global\"; fun showA() { var a = \"local\"; print a; } showA(); print a;", "local\nglobal\n" },
		// Recursive function (user request)
		{ "fun count(n) { if (n > 1) count(n - 1); print n; } count(3);", "1\n2\n3\n" },
		// Function with return value
		{ "fun add(x, y) { return x + y; } print add(2, 3);", "5\n" },
		// Function without return (should return nil)
		{ "fun noReturn() { print \"no return\"; } var result = noReturn(); print result;", "no return\nnil\n" },
		// recursive factorial function
		{ "fun factorial(n) { if (n <= 1) return 1; return n * factorial(n - 1); } print factorial(5);", "120\n" },
		// complex recursive function: Fibonacci
		{ "fun fib(n) { if (n <= 1) return n; return fib(n - 1) + fib(n - 2); } print fib(6);", "8\n" },
		// lambda function test
		{ "var add = fun(x, y) { return x + y; }; print add(10, 20);", "30\n" },
		// lambda capturing variable
		{ "var factor = 3; var multiply = fun(x) { return x * factor; }; print multiply(5);", "15\n" },
		// pass lambda to a function
		{ "fun applyFunc(f, value) { return f(value); } var square = fun(x) { return x * x; }; print applyFunc(square, 4);", "16\n" },
		// call lambda directly
		{ "var result = (fun(x, y) { return x - y; })(10, 4); print result;", "6\n" },
		// closure test with nested functions
		{ "fun outer(x) { fun inner(y) { return x + y; } return inner; } var add5 = outer(5); print add5(10);", "15\n" },
		// closure capturing variable from outer scope
		{ "var base = 10; fun makeAdder(n) { return fun(x) { return x + n + base; }; } var add3 = makeAdder(3); print add3(7);", "20\n" },
	};

#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD saved_attributes = 0;
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	if (GetConsoleScreenBufferInfo(hConsole, &consoleInfo))
	{
		saved_attributes = consoleInfo.wAttributes;
	}
#endif

	for (const auto& test : testCases)
	{
		printf("--- Testing Function: \"%s\" ---\n", test.first.c_str());

		std::string capturedOutput = RunWithCapture(test.first);

		// 使用转义函数来美化输出
		std::string expectedEscaped = EscapeForPrinting(test.second);
		std::string gotEscaped = EscapeForPrinting(capturedOutput);

		if (capturedOutput == test.second)
		{
			printf("  [PASS] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
		}
		else
		{
#ifdef _WIN32
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
#endif
			printf("  [FAIL] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
#ifdef _WIN32
			if (saved_attributes != 0) SetConsoleTextAttribute(hConsole, saved_attributes);
#endif
		}
		printf("----------------------------------------\n\n");
	}
}

void TestUnit::RunClassInterpreterTest()
{
	const std::vector<std::pair<std::string, std::string>> testCases = {
		// Basic class and method
		{ "class Foo{ func(){} } var f = Foo(); print(Foo); print(f);", "<class Foo>\n<instance of Foo>\n" },
		// Assigning and accessing fields
		{ "class Bar{ } var b = Bar(); b.x = 42; print(b.x);", "42\n" },
		{ "class Cat{ } var c = Cat(); c.meow = fun() { print \"meow\"; }; c.meow();", "meow\n" },
		// Method calling
		{ "class Dog{ bark() { print \"woof\"; } } var d = Dog(); d.bark();", "woof\n" },
		// This
		{ "class Counter{ increment() { this.count = this.count + 1; } getCount() { return this.count; } } var c = Counter(); c.count = 0; c.increment(); c.increment(); print(c.getCount());", "2\n" },
		// Constructor (init method)
		{ "class Point{ init(x, y) { this.x = x; this.y = y; } } var p = Point(3, 4); print(p.x); print(p.y);", "3\n4\n" },
		// Class static method
		{ "class Math{ class add(a, b) { return a + b; } } print(Math.add(5, 7));", "12\n" },
		// Class static method call another static method
		{ "class Math{ class add(a, b) { return a + b; } class addAndMultiply(x, y, z) { return Math.add(x, y) * z; } } print(Math.addAndMultiply(2, 3, 4));", "20\n" },
		// Getters
		{ "class Rectangle{ init(width, height) { this.width = width; this.height = height; } area { return this.width * this.height; } } var r = Rectangle(5, 10); print(r.area);", "50\n" },
		// Inheritance
		{ "class Animal{ speak() { print \"animal sound\"; } } class Cat < Animal {} var c = Cat(); c.speak();", "animal sound\n" },
		{ "class Animal{ speak() { print \"animal sound\"; } } class Dog < Animal{ speak() { print \"woof\"; } } var d = Dog(); d.speak();", "woof\n" },
		// Super
		{ "class A{ greet() { print \"Hello from A\"; } } class B < A{ greet() { super.greet(); print \"Hello from B\"; } } var b = B(); b.greet();", "Hello from A\nHello from B\n" },
		{ "class A{ greet() { print \"Hello from A\"; } } class B < A{ greet() { super.greet(); print \"Hello from B\"; } } class C < A{greet() { super.greet(); print \"Hello from C\"; } } var b = B(); var c = C(); b.greet(); c.greet();", "Hello from A\nHello from B\nHello from A\nHello from C\n" },
		// Root method calling
		{ "class Base{ func() { print \"Base func\"; } } class Derived < Base{ func() { print \"Derived func\"; } } var d = Derived(); d..func();", "Base func\n" },
		// inner() tests (chain-of-responsibility via RootGet)
		//		1) base calls inner(), derived overrides -> expect Base then Derived
		{ "class A{ m(s) { print \"A \" + s; inner(s); } } class B < A{ m(s) { print \"B \" + s; } } var b = B(); b..m(\"x\");", "A x\nB x\n" },
		//		2) three-level chain: A -> B -> C, passing a string argument through inner(s)
		{ "class A{ m(s) { print \"A \" + s; inner(s); } } class B < A{ m(s) { print \"B \" + s; inner(s); } } class C < B{ m(s) { print \"C \" + s; } } var c = C(); c..m(\"y\");", "A y\nB y\nC y\n" },
		//		3) middle class doesn't implement m, lower level does -> expect Base then Lower with arg
		{ "class A{ m(s) { print \"A \" + s; inner(s); } } class B < A{ } class C < B{ m(s) { print \"C \" + s; } } var c = C(); c..m(\"z\");", "A z\nC z\n" },
		//		4) derived doesn't override, only base exists -> expect only Base with arg
		{ "class A{ m(s) { print \"A \" + s; inner(s); } } class B < A{ } var b2 = B(); b2..m(\"w\");", "A w\n" },
	};

#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD saved_attributes = 0;
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	if (GetConsoleScreenBufferInfo(hConsole, &consoleInfo))
	{
		saved_attributes = consoleInfo.wAttributes;
	}
#endif

	for (const auto& test : testCases)
	{
		printf("--- Testing Function: \"%s\" ---\n", test.first.c_str());

		std::string capturedOutput = RunWithCapture(test.first);

		// 使用转义函数来美化输出
		std::string expectedEscaped = EscapeForPrinting(test.second);
		std::string gotEscaped = EscapeForPrinting(capturedOutput);

		if (capturedOutput == test.second)
		{
			printf("  [PASS] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
		}
		else
		{
#ifdef _WIN32
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
#endif
			printf("  [FAIL] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
#ifdef _WIN32
			if (saved_attributes != 0) SetConsoleTextAttribute(hConsole, saved_attributes);
#endif
		}
		printf("----------------------------------------\n\n");
	}
}

void TestUnit::RunVMTest()
{
	struct TestCase
	{
		std::string source;
		std::string expectedOutput;
		InterpretResult expectedResult = INTERPRET_OK;
	};

	const std::vector<TestCase> testCases = {
		// 基础常量与打印
		{ "print 1;", "1\n" },
		{ "print 1.5;", "1.500000\n" },
		{ "print \"hello\";", "hello\n" },
		{ "print true; print false; print nil;", "true\nfalse\nnil\n" },

		// 一元和二元运算
		{ "print -3;", "-3\n" },
		{ "print !false;", "true\n" },
		{ "print 1 + 2 * 3;", "7\n" },
		{ "print (1 + 2) * 3;", "9\n" },
		{ "print 10 - 3; print 8 / 4;", "7\n2\n" },
		{ "print 5 > 3; print 5 < 3; print 5 >= 5; print 3 <= 2;", "true\nfalse\ntrue\nfalse\n" },
		{ "print 1 == 1; print 1 != 2;", "true\ntrue\n" },
		{ "print \"a\" + \"b\";", "ab\n" },

		// 全局变量
		{ "var a = 10; print a;", "10\n" },
		{ "var a = 1; a = a + 2; print a;", "3\n" },
		{ "var first = \"L\"; var second = \"ox\"; print first + second;", "Lox\n" },

		// VM: while / for loops
		// VM: while / for loops (including aggressive break/nested cases)
		{ "var i = 0; while (i < 3) { print i; i = i + 1; }", "0\n1\n2\n" },
		{ "for (var j = 0; j < 2; j = j + 1) { print j; }", "0\n1\n" },
		{ "var sum = 0; for (var k = 1; k <= 3; k = k + 1) { sum = sum + k; } print sum;", "6\n" },
		// omitted clauses and simple breaks
		{ "var x = 0; for (; x < 2; ) { print x; x = x + 1; }", "0\n1\n" },
		{ "var c = 0; for (;;) { c = c + 1; if (c == 2) break; } print c;", "2\n" },
		{ "var a = 0; while (a < 3) { a = a + 1; } print a;", "3\n" },
		{ "var n = 0; while (n < 0) { print n; } print \"done\";", "done\n" },
		// nested loops and break behavior
		{ "for (var i = 0; i < 2; i = i + 1) { for (var j = 0; j < 2; j = j + 1) { print \"inner\"; break; } print \"outer\"; }", "inner\nouter\ninner\nouter\n" },
		{ "for (var j = 0; j < 3; j = j + 1) { for (var k = 0; k < 3; k = k + 1) { if (j == 1 and k == 1) break; print j; print k; if (k == 0) break; } }", "0\n0\n1\n0\n2\n0\n" },
		{ "var x = 0; while (x < 2) { var y = 0; while (y < 3) { if (y == 1) break; print x; print y; y = y + 1; } x = x + 1; }", "0\n0\n1\n0\n" },
		{ "for (var m = 0; m < 2; m = m + 1) { for (var n = 0; n < 4; n = n + 1) { if (n == 2) break; print m; print n; break; print \"unreachable\"; } print \"outer-end\"; }", "0\n0\nouter-end\n1\n0\nouter-end\n" },
		{ "for (;;) { print \"loop\"; break; print \"unreachable\"; }", "loop\n" },

		// 局部变量与作用域
		{ "{ var a = 5; print a; }", "5\n" },
		{ "var a = 5; { var a = 7; print a; } print a;", "7\n5\n" },
		{ "var a = 1; { a = 4; } print a;", "4\n" },
		{ "{ var a = 1; a = a + 4; print a; }", "5\n" },
		{ "var outer = 2; { var inner = 3; print outer + inner; }", "5\n" },
		// 变量声明时可以访问同名的外层变量
		{ "var a = 10; { var a = a + 5; { var a = a + 10; print a; } }", "25\n" },

		// 精简后的组合 if/else 与 and/or 一行用例（5 个代表性用例）
		{ "var a = 0; if (true or (a = 1)) print \"then\"; else print \"else\"; print a;", "then\n0\n" },
		{ "var a = 0; if (false or (a = 1)) print \"then\"; else print \"else\"; print a;", "then\n1\n" },
		{ "var b = 0; if (true and (b = 1)) print \"then\"; else print \"else\"; print b;", "then\n1\n" },
		{ "final var f = true; var x = 0; if (f or (x = 1)) print x; else print -1;", "0\n" },
		{ "var a = 0; if ((false or true) and (a = 3)) print \"ok\"; else print \"no\"; print a;", "ok\n3\n" },

		// 错误路径：运行时错误
		{ "print 1 / 0;", "Division by zero.", INTERPRET_RUNTIME_ERROR },

		// 错误路径：final 变量不能重新赋值
		{ "final var a = 1; a = 2;", "Cannot assign to a final variable.", INTERPRET_COMPILE_ERROR },
		{ "{ final var a = 1; a = 2; }", "Cannot assign to a final variable.", INTERPRET_COMPILE_ERROR },

		// switch: 匹配第一个 case 并 break
		{ "var x = 7; switch (x) { case 7: print \"seven\"; break; case 9: print \"nine\"; break; default: print \"other\"; }", "seven\n" },
		// switch: 匹配第二个 case
		{ "var x = 9; switch (x) { case 7: print \"seven\"; break; case 9: print \"nine\"; break; default: print \"other\"; }", "nine\n" },
		// switch: 未匹配任何 case，走 default
		{ "var x = 0; switch (x) { case 7: print \"seven\"; break; case 9: print \"nine\"; break; default: print \"other\"; }", "other\n" },
		// switch: 无 default，未匹配时跳过所有 case
		{ "var x = 1; switch (x) { case 2: print \"two\"; break; case 3: print \"three\"; break; } print \"done\";", "done\n" },
		// switch: fall-through（case 7 无 break，执行 case 7 和 case 9 的 body）
		{ "var x = 7; switch (x) { case 7: print \"seven\"; case 9: print \"nine\"; break; default: print \"other\"; }", "seven\nnine\n" },
		// switch: fall-through 一直到 default
		{ "var x = 7; switch (x) { case 7: print \"a\"; default: print \"b\"; }", "a\nb\n" },
		// switch: 空 switch（直接 {}）
		{ "var x = 1; switch (x) {} print \"ok\";", "ok\n" },
		// switch: 表达式为字符串
		{ "var s = \"hello\"; switch (s) { case \"hello\": print \"hi\"; break; default: print \"bye\"; }", "hi\n" },
		// switch: break 不影响外层 while
		{ "var i = 0; while (i < 2) { switch (i) { case 0: print \"zero\"; break; default: print \"nonzero\"; } i = i + 1; }", "zero\nnonzero\n" },
		// switch: 嵌套 switch
		{ "var x = 1; var y = 2; switch (x) { case 1: switch (y) { case 2: print \"1-2\"; break; default: print \"1-other\"; } break; default: print \"other\"; }", "1-2\n" },

		// continue: while 中跳过奇数
		{ "var i = 0; while (i < 5) { i = i + 1; if (i == 2 or i == 4) continue; print i; }", "1\n3\n5\n" },
		// continue: for 中跳过偶数（increment 仍然执行）
		{ "for (var i = 0; i < 6; i = i + 1) { if (i == 1 or i == 3 or i == 5) continue; print i; }", "0\n2\n4\n" },
		// continue: 嵌套循环 continue 只影响最内层
		{ "for (var i = 0; i < 2; i = i + 1) { for (var j = 0; j < 3; j = j + 1) { if (j == 1) continue; print i; print j; } }", "0\n0\n0\n2\n1\n0\n1\n2\n" },
		// continue: while + 计数器验证 increment 正常
		{ "var n = 0; var sum = 0; while (n < 5) { n = n + 1; if (n == 3) continue; sum = sum + n; } print sum;", "12\n" },
		// continue: for 中 continue 后不执行 continue 后面的语句
		{ "for (var i = 0; i < 3; i = i + 1) { if (i == 1) { continue; print \"unreachable\"; } print i; }", "0\n2\n" },
	};

#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD saved_attributes = 0;
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	if (GetConsoleScreenBufferInfo(hConsole, &consoleInfo))
	{
		saved_attributes = consoleInfo.wAttributes;
	}
#endif

	for (const auto& test : testCases)
	{
		printf("--- Testing VM: \"%s\" ---\n", test.source.c_str());

		VMRunResult runResult = RunVMWithCapture(test.source);
		std::string expectedEscaped = EscapeForPrinting(test.expectedOutput);
		std::string gotEscaped = EscapeForPrinting(runResult.output);

		bool passed = false;
		if (test.expectedResult == INTERPRET_OK)
		{
			passed = (runResult.result == INTERPRET_OK && runResult.output == test.expectedOutput);
		}
		else
		{
			passed = (runResult.result == test.expectedResult && runResult.output.find(test.expectedOutput) != std::string::npos);
		}

		if (passed)
		{
			printf("  [PASS] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
		}
		else
		{
#ifdef _WIN32
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
#endif
			printf("  [FAIL] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
			printf("  [INFO] Result code: %d\n", (int)runResult.result);
#ifdef _WIN32
			if (saved_attributes != 0) SetConsoleTextAttribute(hConsole, saved_attributes);
#endif
		}
		printf("----------------------------------------\n\n");
	}
}

// 辅助函数：运行解析器并捕获语义错误
std::string RunResolverWithCapture(const std::string& source)
{
	// 1. 备份原始的 cout 缓冲并重定向
	std::streambuf* oldCoutStreamBuf = std::cout.rdbuf();
	std::ostringstream strCout;
	std::cout.rdbuf(strCout.rdbuf());

	// 2. 重置 Lox 的错误状态
	Lox::GetInstance().ResetError();

	// 3. 运行扫描、解析和语义分析
	Scanner scanner(source);
	auto tokens = scanner.ScanTokens();
	Parser parser(tokens);
	auto statements = parser.Parse();

	// 只有在解析成功时才进行语义分析
	if (!parser.HasError())
	{
		Interpreter interpreter; // 解析器需要一个解释器实例
		Resolver resolver(&interpreter);
		resolver.Resolve(statements);
	}
	else
	{
		strCout << "[Parse error]";
	}

	// 4. 恢复原始的 cout 缓冲
	std::cout.rdbuf(oldCoutStreamBuf);

	// 5. 返回捕获到的错误字符串
	return strCout.str();
}

void TestUnit::RunResolverTest()
{
	const std::vector<std::pair<std::string, std::string>> testCases = {
		// --- Valid Cases (should produce no output) ---
		{ "var a = 1; print a;", "" },
		{ "{ var a = 1; print a; }", "" },
		{ "var a = 1; { var a = 2; print a; } print a;", "" },
		{ "fun f() { return 1; } f();", "" },
		{ "while(true) { break; }", "" },
		{ "fun f() { while(true) { break; } return; }", "" },
		{ "var a = 1; a = 2;", "" },
		{ "class C { m() { return this; } }", "" },

		// --- Invalid Cases (should produce semantic errors) ---
		// Variable Errors
		{ "{ var a = 1; var a = 2; }", "[1:18] SemanticError: Variable 'a' already defined in this scope.\n" },
		// { "var a = a;", "[1:8] SemanticError: Cannot read local variable 'a' in its own initializer.\n" },
		{ "fun f() { var a = a; }", "[1:19] SemanticError: Cannot read local variable 'a' in its own initializer.\n" },

		// Return Statement Errors
		{ "return;", "[1:1] SemanticError: 'return' statement not within a function.\n" },
		{ "fun f() {} return;", "[1:12] SemanticError: 'return' statement not within a function.\n" },

		// Break Statement Errors
		{ "break;", "[1:1] SemanticError: 'break' statement not within a loop.\n" },
		{ "fun f() { break; }", "[1:11] SemanticError: 'break' statement not within a loop.\n" },
		{ "if (true) { break; }", "[1:13] SemanticError: 'break' statement not within a loop.\n" },

		// This Errors
		{ "this;", "[1:1] SemanticError: 'this' cannot be used outside of a class.\n" },
		{ "fun f() { this; }", "[1:11] SemanticError: 'this' cannot be used outside of a class.\n" },

		// Class Errors
		{ "class C { init() { return 1; } }", "[1:20] SemanticError: Cannot return a value from an initializer.\n" },
		// Class static method use this
		{ "class C { class Method() { this; } }", "[1:28] SemanticError: 'this' cannot be used in a class method.\n" },
		// Class inherit from itself
		{ "class C < C { }", "[1:7] SemanticError: Class cannot inherit from itself.\n" },
	};

#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD saved_attributes = 0;
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	if (GetConsoleScreenBufferInfo(hConsole, &consoleInfo))
	{
		saved_attributes = consoleInfo.wAttributes;
	}
#endif

	for (const auto& test : testCases)
	{
		printf("--- Testing Resolver: \"%s\" ---\n", test.first.c_str());

		std::string capturedOutput = RunResolverWithCapture(test.first);

		std::string expectedEscaped = EscapeForPrinting(test.second);
		std::string gotEscaped = EscapeForPrinting(capturedOutput);

		if (capturedOutput == test.second)
		{
			printf("  [PASS] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
		}
		else
		{
#ifdef _WIN32
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
#endif
			printf("  [FAIL] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
#ifdef _WIN32
			if (saved_attributes != 0) SetConsoleTextAttribute(hConsole, saved_attributes);
#endif
		}
		printf("----------------------------------------\n\n");
	}
}
