#include "Compiler.h"

#define DEBUG_PRINT_CODE

bool Compiler::Compile(const char* source, Chunk* outChunk)
{
	Scanner scanner(source);
	tokens = scanner.ScanTokens();
	if (tokens.empty() || tokens[tokens.size() - 1].type != END_OF_FILE)
	{
		return false;
	}
	compilingChunk = outChunk;
	// Initialize parser state, move the lookahead to the first token.
	Advance();
	Expression();
	Consume(END_OF_FILE, "Expect end of expression.");
	EndCompiler();
	return !parser.hadError;
}

void Compiler::Advance()
{
	parser.previous = parser.current;
	while (true)
	{
		parser.current = ScanToken();
		if (parser.current.type != ERROR)
		{
			break;
		}
		ErrorAtCurrent(parser.current.lexeme.c_str());
	}
}

void Compiler::Error(const char* message)
{
	ErrorAt(&parser.previous, message);
}

void Compiler::ErrorAt(Token* token, const char* message)
{
	if (parser.panicMode)
	{
		return;
	}
	parser.panicMode = true;
	fprintf(stderr, "[%zu:%zu] Error", token->line, token->column);
	if (token->type == END_OF_FILE)
	{
		fprintf(stderr, " at end");
	}
	else if (token->type == ERROR)
	{
		// Nothing.
	}
	else
	{
		fprintf(stderr, " at '%s'", token->lexeme.c_str());
	}
	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}

Compiler::ParseRule* Compiler::GetRule(TokenType type)
{
	// Use a lazily-initialized static vector and assign entries by index.
	// This emulates designated initializers in a C++14-compliant way and
	// is robust against changes in enum ordering as long as token names
	// remain consistent.
	static std::vector<ParseRule> rules;
	if (rules.empty())
	{
		rules.resize((size_t)ERROR + 1);

		rules[LEFT_PAREN]    = { &Compiler::Grouping, nullptr,            PREC_NONE };
		rules[RIGHT_PAREN]   = { nullptr,             nullptr,            PREC_NONE };
		rules[LEFT_BRACE]    = { nullptr,             nullptr,            PREC_NONE };
		rules[RIGHT_BRACE]   = { nullptr,             nullptr,            PREC_NONE };
		rules[COMMA]         = { nullptr,             nullptr,            PREC_NONE };
		rules[DOT]           = { nullptr,             nullptr,            PREC_NONE };
		rules[DOTDOT]        = { nullptr,             nullptr,            PREC_NONE };
		rules[MINUS]         = { &Compiler::Unary,    &Compiler::Binary,  PREC_TERM };
		rules[PLUS]          = { nullptr,             &Compiler::Binary,  PREC_TERM };
		rules[SEMICOLON]     = { nullptr,             nullptr,            PREC_NONE };
		rules[SLASH]         = { nullptr,             &Compiler::Binary,  PREC_FACTOR };
		rules[STAR]          = { nullptr,             &Compiler::Binary,  PREC_FACTOR };
		rules[BANG]          = { &Compiler::Unary,    nullptr,            PREC_NONE };
		rules[QUESTION]      = { nullptr,             &Compiler::Trinary, PREC_QUESTION };
		rules[COLON]         = { nullptr,             nullptr,            PREC_NONE };
		rules[BANG_EQUAL]    = { nullptr,             &Compiler::Equality,PREC_EQUALITY };
		rules[EQUAL]         = { nullptr,             nullptr,            PREC_NONE };
		rules[EQUAL_EQUAL]   = { nullptr,             &Compiler::Equality,PREC_EQUALITY };
		rules[GREATER]       = { nullptr,             &Compiler::Binary,  PREC_COMPARISON };
		rules[GREATER_EQUAL] = { nullptr,             &Compiler::Binary,  PREC_COMPARISON };
		rules[LESS]          = { nullptr,             &Compiler::Binary,  PREC_COMPARISON };
		rules[LESS_EQUAL]    = { nullptr,             &Compiler::Binary,  PREC_COMPARISON };
		rules[IDENTIFIER]    = { nullptr,             nullptr,            PREC_NONE };
		rules[STRING]        = { nullptr,             nullptr,            PREC_NONE };
		rules[NUMBER]        = { &Compiler::Number,   nullptr,            PREC_NONE };
		rules[AND]           = { nullptr,             nullptr,            PREC_NONE };
		rules[CLASS]         = { nullptr,             nullptr,            PREC_NONE };
		rules[ELSE]          = { nullptr,             nullptr,            PREC_NONE };
		rules[FALSE]         = { &Compiler::Literal,  nullptr,            PREC_NONE };
		rules[FUN]           = { nullptr,             nullptr,            PREC_NONE };
		rules[FOR]           = { nullptr,             nullptr,            PREC_NONE };
		rules[IF]            = { nullptr,             nullptr,            PREC_NONE };
		rules[NIL]           = { &Compiler::Literal,  nullptr,            PREC_NONE };
		rules[OR]            = { nullptr,             nullptr,            PREC_NONE };
		rules[PRINT]         = { nullptr,             nullptr,            PREC_NONE };
		rules[RETURN]        = { nullptr,             nullptr,            PREC_NONE };
		rules[SUPER]         = { nullptr,             nullptr,            PREC_NONE };
		rules[THIS]          = { nullptr,             nullptr,            PREC_NONE };
		rules[TRUE]          = { &Compiler::Literal,  nullptr,            PREC_NONE };
		rules[VAR]           = { nullptr,             nullptr,            PREC_NONE };
		rules[WHILE]         = { nullptr,             nullptr,            PREC_NONE };
		rules[BREAK]         = { nullptr,             nullptr,            PREC_NONE };
		rules[END_OF_FILE]   = { nullptr,             nullptr,            PREC_NONE };
		rules[ERROR]         = { nullptr,             nullptr,            PREC_NONE };
	}

	return &rules[type];
}


Token Compiler::ScanToken()
{
	if (currentToken >= tokens.size())
	{
		return Token(ERROR, "", 0, 0);
	}
	return tokens[currentToken++];
}

void Compiler::Consume(TokenType type, const char* message)
{
	if (parser.current.type == type)
	{
		if (parser.current.type != END_OF_FILE)
		{
			Advance();
		}
		return;
	}
	ErrorAt(&parser.current, message);
}

void Compiler::ErrorAtCurrent(const char* message)
{
	ErrorAt(&parser.current, message);
}

Token Compiler::Peek(int32_t offset)
{
	// currentToken points to the next token to be processed
	// so we need to subtract 1 to get the current token
	return tokens[currentToken + offset - 1];
}

void Compiler::EmitByte(uint8_t byte)
{
	compilingChunk->Write(byte, (int32_t)parser.previous.line, (int32_t)parser.previous.column);
}

void Compiler::EmitConstant(VMValue value)
{
	int32_t constantIndex = compilingChunk->AddConstant(value);
	if (constantIndex <= 0xFF)
	{
		EmitBytes(OP_CONSTANT, (uint8_t)constantIndex);
	}
	else if (constantIndex <= 0xFFFFFF)
	{
		EmitBytes(OP_CONSTANT_LONG,
			(uint8_t)((constantIndex >> 16) & 0xFF),
			(uint8_t)((constantIndex >> 8) & 0xFF),
			(uint8_t)(constantIndex & 0xFF));
	}
	else
	{
		Error("Too many constants in one chunk.");
	}
}

void Compiler::EndCompiler()
{
	EmitByte(OP_RETURN);
#ifdef DEBUG_PRINT_CODE	
	if (!parser.hadError)
	{
		compilingChunk->Disassemble("code");
	}
#endif // DEBUG_PRINT_CODE
}

void Compiler::Expression()
{
	ParsePrecedence(PREC_ASSIGNMENT);
}

void Compiler::ParsePrecedence(Precedence precedence)
{
	Advance();

	ParseFn prefixRule = GetRule(parser.previous.type)->prefix;
	if (prefixRule == nullptr)
	{
		Error("Expect expression.");
		return;
	}

	// Call the prefix parse function as a member function pointer.
	(this->*prefixRule)();

	while (precedence <= GetRule(parser.current.type)->precedence)
	{
		Advance();
		ParseFn infixRule = GetRule(parser.previous.type)->infix;
		// Call the infix parse function as a member function pointer.
		(this->*infixRule)();
	}
}

void Compiler::Number()
{
	const std::string& lexeme = parser.previous.lexeme;	
	VMValue value;
	if (lexeme.find('.') != std::string::npos)
	{
		value = FloatValue::Create(std::stof(lexeme));
	}
	else
	{
		value = IntValue::Create(std::stoi(lexeme));
	}
	EmitConstant(value);
}

void Compiler::Literal()
{
	switch (parser.previous.type)
	{
		case FALSE: EmitByte(OP_FALSE); break;
		case TRUE:  EmitByte(OP_TRUE);  break;
		case NIL:   EmitByte(OP_NIL);   break;
		default:
			Error("Unknown literal.");
			break;
	}
}

void Compiler::Grouping()
{
	Expression();
	Consume(RIGHT_PAREN, "Expect ')' after expression.");
}

void Compiler::Unary()
{
	TokenType operatorType = parser.previous.type;
	ParsePrecedence(PREC_UNARY);
	switch (operatorType)
	{
		case MINUS:
		{
			EmitByte(OP_NEGATE);
			break;
		}
		case BANG:
		{
			EmitByte(OP_NOT);
			break;
		}
		default:
			Error("Unknown unary operator.");
			break;
	}
}

void Compiler::Binary()
{
	TokenType operatorType = parser.previous.type;

	ParseRule* rule = GetRule(operatorType);
	ParsePrecedence((Precedence)(rule->precedence + 1));

	switch (operatorType)
	{
		case PLUS:          EmitByte(OP_ADD);       break;
		case MINUS:         EmitByte(OP_SUBTRACT);  break;
		case STAR:          EmitByte(OP_MULTIPLY);  break;
		case SLASH:         EmitByte(OP_DIVIDE);    break;
		case GREATER:       EmitByte(OP_GERATER);   break;
		case GREATER_EQUAL: EmitByte(OP_GERATER); EmitByte(OP_NOT); break;
		case LESS:          EmitByte(OP_LESS);      break;
		case LESS_EQUAL:    EmitByte(OP_LESS);    EmitByte(OP_NOT); break;
		default:
			// Unreachable.
			break;
	}
}

void Compiler::Trinary()
{
	TokenType operatorType = parser.previous.type;

	ParseRule* rule = GetRule(operatorType);
	ParsePrecedence((Precedence)(rule->precedence));

	switch (operatorType)
	{
		case QUESTION: break;
		default:
			// Unreachable.
			break;
	}

	Consume(COLON, "Expect ':' in trinary operator.");
	ParsePrecedence((Precedence)(rule->precedence));
}

void Compiler::Equality()
{
	TokenType operatorType = parser.previous.type;
	ParseRule* rule = GetRule(operatorType);
	ParsePrecedence((Precedence)(rule->precedence + 1));
	switch (operatorType)
	{
		case EQUAL_EQUAL:  EmitByte(OP_EQUAL); break;
		case BANG_EQUAL:   EmitByte(OP_EQUAL); EmitByte(OP_NOT); break;
		default:
			// Unreachable.
			break;
	}
}