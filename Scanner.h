#pragma once
#include "TokenType.h"

#include <string>
#include <vector>
#include <unordered_map>

class Scanner
{
public:
	explicit Scanner(const std::string& inSource);
	std::vector<Token> tokens;
	std::vector<Token> ScanTokens();
	static std::unordered_map<std::string, TokenType> keywords;
protected:
	std::string source;
	size_t start = 0;
	size_t current = 0;
	size_t line = 1;
	size_t column = 1;
	size_t startColumn = 1;
	bool IsAtEnd();
	bool IsDigit(char c);
	bool IsAlpha(char c);
	char Advance();
	char Peek();
	char PeekNext();
	bool Match(char expected);
	void String();
	void Number();
	void Identifier();
	void AddToken(TokenType tokenType);
	void AddToken(TokenType tokenType, const std::string& lexeme);
	void ScanToken();
};