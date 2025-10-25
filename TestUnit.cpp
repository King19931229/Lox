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
		// 1. ���ȼ��ͽ���Բ���
		"1 + 2 * 3 - 4 / 5",         // Ԥ��: (1 + (2 * 3)) - (4 / 5)
		"1 - 2 - 3",                 // Ԥ��: (1 - 2) - 3 (����)
		"-1 * (2 + 3)",              // Ԥ��: (-1) * (2 + 3)
		"!true == false",            // Ԥ��: (!true) == false

		// 2. ��Ԫ����� (�ҽ��)
		"1 ? 2 : 3 ? 4 : 5",         // Ԥ��: 1 ? 2 : (3 ? 4 : 5)
		"1 > 2 ? 3 + 4 : 5 * 6",     // Ԥ��: (1 > 2) ? (3 + 4) : (5 * 6)

		// 3. ��������� (������ȼ�)
		"1, 2 + 3, 4",               // Ԥ��: ((1, (2 + 3)), 4)
		"1 ? 2, 3 : 4",              // Ԥ��: (1 ? (2, 3) : 4)

		// 4. ���ӵ����ź�Ƕ��
		"((1 + 2) * (3 - 4)) / 5",
		"1 + (2 * (3 - (4 / 5)))",

		// 5. ������ȱ���������
		"* 1 + 2",                   // ����: '*' ȱ���������
		"1 + > 2",                   // ����: '>' ȱ���������
		"== 3",                      // ����: '==' ȱ���������

		// 6. �����﷨����
		"1 + ",                      // ����: ���ʽĩβȱ�ٲ�����
		"1 ? 2",                     // ����: '?' ȱ�� ':'
		"(1 + 2",                    // ����: ȱ�� ')'
		"1 + (2 * 3",
	};

	for (const auto& source : testSources)
	{
		printf("--- Testing Expression: \"%s\" ---\n", source.c_str());
		Scanner scanner(source);
		std::vector<Token> tokens = scanner.ScanTokens();

		// (��ѡ) ��ӡ tokens �Ա����
		for (const Token& token : tokens)
		{
		 	printf("  Token: %-15s | type: %s\n", token.lexeme.c_str(), TokenTypeName[token.type]);
		}

		Parser parser(tokens);
		parser.Parse(); // Parse() �����ڲ����ô��󱨸�
		printf("----------------------------------------\n\n");
	}
}