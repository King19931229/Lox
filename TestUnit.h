#pragma once

struct TestUnit
{
	static void RunScannerTest();
	static void RunExpressionParserTest();
	static void RunResolverTest();
	static void RunExpressionInterpreterTest();
	static void RunStatementInterpreterTest();
	static void RunFunctionInterpreterTest();
	static void RunClassInterpreterTest();
};