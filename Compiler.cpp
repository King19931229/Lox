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

    Advance();

    while (!Match(END_OF_FILE))
    {
        Delclaration();
    }

    EndCompiler();
    return !parser.hadError;
}

// --- Core Parsing Flow ---

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
        // ���ѵ�ĩβ��ScanToken �����ڱ� ERROR����ֱ���˳���������ѭ��
        if (currentToken >= tokens.size())
        {
            break;
        }
        ErrorAtCurrent(parser.current.lexeme.c_str());
    }
}

void Compiler::Delclaration()
{
    if (Match(VAR))
    {
        VarDeclaration();
    }
    else
    {
        Statement();
    }

    if (parser.panicMode)
    {
        Synchronize();
    }
}

void Compiler::Statement()
{
    if (Match(PRINT))
    {
        PrintStatement();
    }
}

void Compiler::VarDeclaration()
{
    uint32_t global = ParseVariable("Expect variable name.");
    if (Match(EQUAL))
    {
        Expression();
    }
    else
    {
        EmitByte(OP_NIL);
    }
    Consume(SEMICOLON, "Expect ';' after variable declaration.");
    DefineVariable(global);
}

void Compiler::PrintStatement()
{
    Expression();
    Consume(SEMICOLON, "Expect ';' after value.");
    EmitByte(OP_PRINT);
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

void Compiler::Synchronize()
{
    parser.panicMode = false;
    while (parser.current.type != END_OF_FILE)
    {
        if (parser.previous.type == SEMICOLON)
        {
            return;
        }
        switch (parser.current.type)
        {
            case CLASS:
            case FUN:
            case VAR:
            case FOR:
            case IF:
            case WHILE:
            case PRINT:
            case RETURN:
                return;
            default:
                break;
        }
        Advance();
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

// --- Grammar Rules ---

void Compiler::Number()
{
    const std::string& lexeme = parser.previous.lexeme;
    VMValue value;
    if (lexeme.find('.') != std::string::npos)
    {
        value = VMValue::Create(FloatValue::CreateRaw(std::stof(lexeme)));
    }
    else
    {
        value = VMValue::Create(IntValue::CreateRaw(std::stoi(lexeme)));
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

void Compiler::String()
{
    const std::string& lexeme = parser.previous.lexeme;
    EmitConstant(VMValue::Create(StringValue::CreateRaw(lexeme)));
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

// --- Token Helpers ---

Token Compiler::ScanToken()
{
    if (currentToken >= tokens.size())
    {
        return Token(ERROR, "", 0, 0);
    }
    return tokens[currentToken++];
}

Token Compiler::Peek(int32_t offset)
{
    // currentToken points to the next token to be processed
    // so we need to subtract 1 to get the current token
    return tokens[currentToken + offset - 1];
}

bool Compiler::Check(TokenType type)
{
    return parser.current.type == type;
}

bool Compiler::Match(TokenType type)
{
    if (Check(type))
    {
        Advance();
        return true;
    }
    return false;
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

Compiler::ParseRule* Compiler::GetRule(TokenType type)
{
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
        rules[STRING]        = { &Compiler::String,   nullptr,            PREC_NONE };
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

// --- Code Generation ---

void Compiler::EmitByte(uint8_t byte)
{
    compilingChunk->Write(byte, (int32_t)parser.previous.line, (int32_t)parser.previous.column);
}

void Compiler::EmitConstant(VMValue value)
{
    uint32_t constantIndex = MakeConstant(value);
    if (constantIndex <= 0xFF)
    {
        EmitBytes(OP_CONSTANT, (uint8_t)constantIndex);
    }
    else
    {
        EmitBytes(OP_CONSTANT_LONG,
            (uint8_t)((constantIndex >> 16) & 0xFF),
            (uint8_t)((constantIndex >> 8) & 0xFF),
            (uint8_t)(constantIndex & 0xFF));
    }
}

// --- Variable Helpers ---

uint32_t Compiler::ParseVariable(const std::string& errorMessage)
{
    Consume(IDENTIFIER, errorMessage.c_str());
    return IdentifierConstant(parser.previous);
}

uint32_t Compiler::MakeConstant(VMValue value)
{
    int32_t constantIndex = compilingChunk->AddConstant(value);
    if (constantIndex > 0xFFFFFF)
    {
        Error("Too many constants in one chunk.");
        return 0;
    }
    return (uint32_t)constantIndex;
}

uint32_t Compiler::IdentifierConstant(const Token& name)
{
    return MakeConstant(VMValue::Create(StringValue::CreateRaw(name.lexeme)));
}

void Compiler::DefineVariable(uint32_t global)
{
    if (global <= 0xFF)
    {
        EmitBytes(OP_DEFINE_GLOBAL, (uint8_t)global);
    }
    else
    {
        EmitBytes(OP_DEFINE_GLOBAL_LONG,
            (uint8_t)((global >> 16) & 0xFF),
            (uint8_t)((global >> 8) & 0xFF),
            (uint8_t)(global & 0xFF));
    }
}

// --- Error Handling ---

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

void Compiler::ErrorAtCurrent(const char* message)
{
    ErrorAt(&parser.current, message);
}