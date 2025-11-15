#include "Scanner.h"
#include "Lox.h"

std::unordered_map<std::string, TokenType> Scanner::keywords = {
	{"and",    AND},
	{"class",  CLASS},
	{"else",   ELSE},
	{"false",  FALSE},
	{"for",    FOR},
	{"fun",    FUN},
	{"if",     IF},
	{"nil",    NIL},
	{"or",     OR},
	{"print",  PRINT},
	{"return", RETURN},
	{"super",  SUPER},
	{"this",   THIS},
	{"true",   TRUE},
	{"var",    VAR},
	{"while",  WHILE},
	{"break",  BREAK},
};

Scanner::Scanner(const std::string& inSource)
	: source(inSource)
{
}

std::vector<Token> Scanner::ScanTokens()
{
	tokens.clear();
	start = 0;
	current = 0;
	line = 1;
	column = 1;

	while (!IsAtEnd())
	{
		start = current;
		startColumn = column;
		ScanToken();
	}

	tokens.push_back(Token(TokenType::END_OF_FILE, "", line, startColumn));
	return tokens;
}

bool Scanner::IsAtEnd()
{
	return current >= source.size();
}

bool Scanner::IsDigit(char c)
{
	return c >= '0' && c <= '9';
}

bool Scanner::IsAlpha(char c)
{
	return (c >= 'a' && c <= 'z') ||
	       (c >= 'A' && c <= 'Z');
}

char Scanner::Advance()
{
	if (!IsAtEnd()) {
		if (source.at(current) == '\n') {
			line++;
			column = 1;
		} else {
			column++;
		}
	}
	current++;
	return source.at(current - 1);
}

char Scanner::Peek()
{
	if (IsAtEnd()) return '\0';
	return source.at(current);
}

char Scanner::PeekNext()
{
	if (current + 1 >= source.size()) return '\0';
	return source.at(current + 1);
}

bool Scanner::Match(char expected)
{
	if (IsAtEnd()) return false;
	if (source.at(current) != expected) return false;
	Advance();
	return true;
}

void Scanner::AddToken(TokenType tokenType)
{
	tokens.push_back(Token(tokenType, source.substr(start, current - start), line, startColumn));
}

void Scanner::AddToken(TokenType tokenType, const std::string& lexeme)
{
	tokens.push_back(Token(tokenType, lexeme, line, startColumn));
}

void Scanner::String()
{
	std::string str;
	while (Peek() != '"' && !IsAtEnd())
	{
		char c = Advance();
		if (c == '\\')
		{
			if (IsAtEnd())
			{
				break;
			}
			char next = Advance();
			switch (next)
			{
				case '"':  str += '"';  break;
				case '\\': str += '\\'; break;
				case 'n':  str += '\n'; break;
				case 'r':  str += '\r'; break;
				case 't':  str += '\t'; break;
				default:
				{
					Lox::GetInstance().Error(line, column, "Unknown escape: \\%c", next);
					break;
				}
			}
		}
		else
		{
			str += c;
		}
	}

	if (IsAtEnd())
	{
		Lox::GetInstance().Error(line, column, "Unterminated string.");
		return;
	}

	// Consume "
	Advance();

	AddToken(STRING, str);
}

void Scanner::Number()
{
	// Deal with the integer part
    while (IsDigit(Peek()))
    {
        Advance();
    }

	// Deal with the fractional part
    if (Peek() == '.' && IsDigit(PeekNext()))
    {
        Advance(); // consume '.'
        while (IsDigit(Peek()))
        {
            Advance();
        }
    }

	// Deal with the exponent part
    if (Peek() == 'e' || Peek() == 'E')
    {
        Advance(); // consume 'e' or 'E'
        if (Peek() == '+' || Peek() == '-')
        {
            Advance();
        }
        if (!IsDigit(Peek()))
        {
            Lox::GetInstance().Error(line, column, "Malformed number: exponent has no digits.");
            return;
        }
        while (IsDigit(Peek()))
        {
            Advance();
        }
    }

    AddToken(NUMBER);
}

void Scanner::Identifier()
{
	while (IsAlpha(Peek()) || IsDigit(Peek()) || Peek() == '_')
	{
		Advance();
	}
	std::string text = source.substr(start, current - start);
	TokenType type = IDENTIFIER;

	auto it = keywords.find(text);
	if (it != keywords.end())
	{
		type = it->second;
	}

	AddToken(type, text);
}

void Scanner::ScanToken()
{
	char c = Advance();
	switch (c)
	{
		case '(': AddToken(LEFT_PAREN); break;
		case ')': AddToken(RIGHT_PAREN); break;
		case '{': AddToken(LEFT_BRACE); break;
		case '}': AddToken(RIGHT_BRACE); break;
		case ',': AddToken(COMMA); break;
		case '.':
		{
			if (IsDigit(Peek()))
			{
				Number();
				break;
			}
			AddToken(DOT);
			break;
		}
		case '-': AddToken(MINUS); break;
		case '+': AddToken(PLUS); break;
		case ';': AddToken(SEMICOLON); break;
		case '*': AddToken(STAR); break;
		case '?': AddToken(QUESTION); break;
		case ':': AddToken(COLON); break;
		case '!':
			AddToken(Match('=') ? BANG_EQUAL : BANG);
			break;
		case '=':
			AddToken(Match('=') ? EQUAL_EQUAL : EQUAL);
			break;
		case '<':
			AddToken(Match('=') ? LESS_EQUAL : LESS);
			break;
		case '>':
			AddToken(Match('=') ? GREATER_EQUAL : GREATER);
			break;
			// Ignore whitespace.
		case ' ':
		case '\r':
		case '\t':
			break;
		case '/':
			if (Match('/'))
			{
				while (Peek() != '\n' && !IsAtEnd())
				{
					Advance();
				}
			}
			else if (Match('*'))
			{
				size_t commentCount = 1;
				while (!IsAtEnd())
				{
					if (Peek() == '/' && PeekNext() == '*')
					{
						Advance();
						Advance();
						++commentCount;
					}
					else if (Peek() == '*' && PeekNext() == '/')
					{
						Advance();
						Advance();
						if (--commentCount == 0)
						{
							break;
						}
					}
					else if (Peek() == '\n')
					{
						Advance();
					}
					else
					{
						Advance();
					}
				}
				if (commentCount != 0)
				{
					Lox::GetInstance().Error(line, column, "Unterminated multi-line comment.");
					return;
				}
			}
			else
			{
				AddToken(SLASH);
			}
			break;
		case '\n':
			break;
		case '"':
			String();
			break;
		default:
			if (IsDigit(c))
			{
				Number();
				break;
			}
			if (c == '_' || IsAlpha(c))
			{
				Identifier();
				break;
			}
			else
			{
				Lox::GetInstance().Error(line, column, "Unexpected character: %c", c);
				break;
			}
	}
}