#pragma once

struct TestUnit
{
	static void RunScannerTest();
	static void RunExpressionParserTest();
	static void RunExpressionInterpreterTest();
	static void RunStatementInterpreterTest();
	static void RunFunctionInterpreterTest();
	static void RunResolverTest();
};