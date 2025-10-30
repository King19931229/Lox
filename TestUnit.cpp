#include "TestUnit.h"
#include "Scanner.h"
#include "Parser.h"
#include "Interpreter.h"
#include <cstdio>
#include <vector>
#include <string>
#include <utility> // for std::pair
#include <iostream> // 用于捕获 cout
#include <sstream>  // 用于捕获 cout

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
		{ "123.45", "123.450000" }, // std::to_string for float has this format
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
		{ "!0", "false" },
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
			printf("  [FAIL] Expected: %s, Got: %s\n", test.second.c_str(), resultString.c_str());
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

	// 2. 运行扫描、解析和解释流程
	Scanner scanner(source);
	auto tokens = scanner.ScanTokens();
	Parser parser(tokens);
	auto statements = parser.Parse();

	// 只有在解析成功时才进行解释
	if (!parser.HasError())
	{
		Interpreter interpreter;
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

		// 4. 多条语句
		{ "print 1; print 2; print 3;", "1\n2\n3\n" },
		{ "1 + 1; print \"result\"; 3*3;", "result\n" },

		// 5. 解析错误
		{ "print 123", "Parse Error" }, // 缺少分号

		// --- 以下为占位符，当你的解释器支持这些功能时可以取消注释 ---
		// 6. 块作用域
		// { "{ print 1; print 2; }", "1\n2\n" },

		// 7. 变量声明与赋值
		// { "var a = 10; print a;", "10\n" },
		// { "var a = 1; a = 2; print a;", "2\n" },
		// { "var a = \"hello\"; var b = \" world\"; print a + b;", "hello world\n" },

		// 8. 条件语句
		// { "if (true) print \"yes\";", "yes\n" },
		// { "if (false) print \"yes\"; else print \"no\";", "no\n" },
		// { "var a = 1; if (a > 0) { print \"positive\"; }", "positive\n" },
	};

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
			printf("  [FAIL] Expected: '%s', Got: '%s'\n", expectedEscaped.c_str(), gotEscaped.c_str());
		}
		printf("----------------------------------------\n\n");
	}
}