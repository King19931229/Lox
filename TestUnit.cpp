#include "TestUnit.h"
#include "Scanner.h"
#include "Parser.h"
#include <cstdio>
#include <vector>
#include <string>

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