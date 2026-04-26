#include "Compiler.h"
#include "LoxCallable.h"
#include "VM.h"

#define DEBUG_PRINT_CODE

Compiler::Compiler()
	: ctx(&ownCtx)
	, enclosing(nullptr)
	, compilingChunk(nullptr)
	, parser(ownCtx.parser)
	, tokens(ownCtx.tokens)
	, nextToken(ownCtx.nextToken)
	, globalFinals(ownCtx.globalFinals)
{
	compilingChunk = new Chunk();
	compilingChunk->Init();
	function = VMValue(nullptr, compilingChunk);
	type = TYPE_SCRIPT;
}

Compiler::Compiler(Compiler* inEnclosing, ParseContext* sharedCtx)
	: ctx(sharedCtx)
	, enclosing(inEnclosing)
	, compilingChunk(nullptr)
	, parser(sharedCtx->parser)
	, tokens(sharedCtx->tokens)
	, nextToken(sharedCtx->nextToken)
	, globalFinals(sharedCtx->globalFinals)
{
	compilingChunk = new Chunk();
	compilingChunk->Init();
	function = VMValue(nullptr, compilingChunk);
}

Compiler::~Compiler()
{
	if (compilingChunk)
	{
		compilingChunk->Free();
		delete compilingChunk;
	}
}

Chunk* Compiler::CurrentChunk()
{
	return function.chunk;
}

void Compiler::Init(FunctionType inType, const std::string& name)
{
	type = inType;
	if (inType == TYPE_SCRIPT)
	{
		function.value = new Compiler::ScriptFunction();
	}
	else
	{
		function.value = new Compiler::VMFunctionValue(name);
	}
	locals.clear();
	scopeDepth = 0;
	currentLoopStart = -1;
	currentLoopContinue = -1;
	// The first local slot is reserved for internal use
	Local local;
	local.depth = 0;
	locals.push_back(local);
}

VMValue Compiler::Compile(const char* source)
{
	Scanner scanner(source);
	tokens = scanner.ScanTokens();
	if (tokens.empty() || tokens[tokens.size() - 1].type != END_OF_FILE)
	{
		return VMValue();
	}

	Init(TYPE_SCRIPT);

	Advance();

	while (!Match(END_OF_FILE))
	{
		Delclaration();
	}

	VMValue fn = EndCompiler();
	return !parser.hadError ? fn : VMValue();
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
		if (nextToken >= tokens.size())
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
		VarDeclaration(false);
	}
	else if (Match(FINAL))
	{
		FinalVarDeclaration();
	}
	else if (Match(FUN))
	{
		FunctionDeclaration();
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

void Compiler::BeginScope()
{
	scopeDepth++;
}

void Compiler::Block()
{
	while (!Check(RIGHT_BRACE) && !Check(END_OF_FILE))
	{
		Delclaration();
	}
	Consume(RIGHT_BRACE, "Expect '}' after block.");
}

void Compiler::EndScope()
{
	scopeDepth--;
	while (!locals.empty() && locals.back().depth > scopeDepth)
	{
		// Drop locals that go out of scope so their stack slots are reclaimed.
		EmitByte(OP_POP);
		locals.pop_back();
	}
}

void Compiler::Statement()
{
	if (Match(PRINT))
	{
		PrintStatement();
	}
	else if (Match(IF))
	{
		IfStatement();
	}
	else if (Match(WHILE))
	{
		WhileStatement();
	}
	else if (Match(BREAK))
	{
		BreakStatement();
	}
	else if (Match(CONTINUE))
	{
		ContinueStatement();
	}
	else if (Match(SWITCH))
	{
		SwitchStatement();
	}
	else if (Match(FOR))
	{
		ForStatement();
	}
	else if (Match(LEFT_BRACE))
	{
		BeginScope();
		Block();	
		EndScope();
	}
	else if (Match(RETURN))
	{
		ReturnStatement();
	}
	else
	{
		ExpressionStatement();
	}
}

void Compiler::VarDeclaration(bool isFinal)
{
	uint32_t global = ParseVariable("Expect variable name.", isFinal);
	if (Match(EQUAL))
	{
		Expression();
	}
	else
	{
		// Uninitialized variables default to nil.
		EmitByte(OP_NIL);
	}
	Consume(SEMICOLON, "Expect ';' after variable declaration.");
	DefineVariable(global, isFinal);
}

void Compiler::FinalVarDeclaration()
{
	Consume(VAR, "Expect 'var' keyword for final variable declaration.");
	VarDeclaration(true);
}

void Compiler::FunctionDeclaration()
{
	uint32_t global = ParseVariable("Expect function name.", false);
	std::string name = parser.previous.lexeme;
	Function(TYPE_FUNCTION, name);
	DefineVariable(global, false);
}

VMValue Compiler::CompileFunction(FunctionType fnType, const std::string& name)
{
	Init(fnType, name);

	BeginScope();

	VMFunctionValue* fnValue = static_cast<VMFunctionValue*>(function.value);

	// Token consumption here uses *this* (the enclosing compiler) — since
	// sub shares ctx, nextToken advances for both simultaneously.
	Consume(LEFT_PAREN, "Expect '(' after function name.");

	while (!Check(RIGHT_PAREN) && !Check(END_OF_FILE))
	{
		fnValue->arity++;
		if (fnValue->arity > 255)
		{
			ErrorAtCurrent("Can't have more than 255 parameters.");
		}
		int32_t parameter = ParseVariable("Expect parameter name.", false);
		DefineVariable(parameter, false);
		if (!Match(COMMA))
		{
			break;
		}
	}

	Consume(RIGHT_PAREN, "Expect ')' after parameters. (Currently no parameters are supported)");
	Consume(LEFT_BRACE, "Expect '{' before function body.");
	Block();

	fnValue->upvalueCount = (int32_t)upvalues.size();

	VMValue fn = EndCompiler();
	return !parser.hadError ? fn : VMValue();
}

void Compiler::Function(FunctionType fnType, const std::string& name)
{
	// The sub-compiler creates its own chunk internally and shares this compiler's
	// ParseContext so both advance through the same token stream.
	Compiler sub(this, ctx);
	VMValue fn = sub.CompileFunction(fnType, name);
	EmitConstant(fn);
	EmitByte(OP_CLOSURE);
}

void Compiler::IfStatement()
{
	Consume(LEFT_PAREN, "Expect '(' after 'if'.");
	Expression();
	Consume(RIGHT_PAREN, "Expect ')' after condition.");
	int32_t thenJump = EmitJump(OP_JUMP_IF_FALSE);
	// The condition value is only needed for the branch test.
	EmitByte(OP_POP);
	Statement();
	int32_t elseJump = EmitJump(OP_JUMP);
	PatchJump(thenJump);
	// Remove the condition before entering the else branch.
	EmitByte(OP_POP);
	if (Match(ELSE))
	{
		Statement();
	}
	PatchJump(elseJump);
}

void Compiler::WhileStatement()
{
	Consume(LEFT_PAREN, "Expect '(' after 'if'.");
	int32_t outterLoopStart = currentLoopStart;
	int32_t outterLoopContinue = currentLoopContinue;
	int32_t loopStart = (int32_t)CurrentChunk()->GetSize();
	currentLoopStart = loopStart;
	currentLoopContinue = loopStart;
	Expression();
	Consume(RIGHT_PAREN, "Expect ')' after condition.");
	int32_t exitJump = EmitJump(OP_JUMP_IF_FALSE);
	// Keep the loop condition only long enough to decide whether to enter the body.
	EmitByte(OP_POP);
	Statement();
	EmitLoop(loopStart);
	PatchJump(exitJump);
	// Discard the false condition when the loop terminates.
	EmitByte(OP_POP);
	// Patch any pending break jumps to jump here (after the loop).
	PatchBreaks(loopStart);
	currentLoopStart = outterLoopStart;
	currentLoopContinue = outterLoopContinue;
}

void Compiler::BreakStatement()
{
	Consume(SEMICOLON, "Expect ';' after 'break'.");
	if (currentLoopStart == -1)
	{
		Error("Can't use 'break' outside of a loop.");
		return;
	}
	int32_t breakJump = EmitJump(OP_JUMP);
	breakJumpPatches[currentLoopStart].push_back(breakJump);
}

void Compiler::ContinueStatement()
{
	Consume(SEMICOLON, "Expect ';' after 'continue'.");
	if (currentLoopContinue == (uint32_t)-1)
	{
		Error("Can't use 'continue' outside of a loop.");
		return;
	}
	EmitLoop((int32_t)currentLoopContinue);
}

void Compiler::SwitchStatement()
{
	int32_t outterLoopStart = currentLoopStart;
	int32_t loopStart = (int32_t)CurrentChunk()->GetSize();
	currentLoopStart = loopStart;

	Consume(LEFT_PAREN, "Expect '(' after 'switch'.");
	Expression();
	Consume(RIGHT_PAREN, "Expect ')' after switch expression.");

	Consume(LEFT_BRACE, "Expect '{' after switch expression.");

	bool defaultFound = false;
	int32_t lastThroughJump = -1;

	while (!Match(RIGHT_BRACE) && !Check(END_OF_FILE))
	{
		if (Match(CASE))
		{
			// Duplicate the switch expression so each case can compare against it.
			EmitByte(OP_DUP);
			Expression();
			Consume(COLON, "Expect ':' after case value.");
			EmitByte(OP_EQUAL);
			int32_t caseJump = EmitJump(OP_JUMP_IF_FALSE);
			// Failed comparison consumes the boolean result.
			EmitByte(OP_POP);
			if (lastThroughJump != -1)
			{
				PatchJump(lastThroughJump);
				lastThroughJump = -1;
			}
			while (!Check(CASE) && !Check(DEFAULT) && !Check(RIGHT_BRACE) && !Check(END_OF_FILE))
			{
				Statement();
			}
			lastThroughJump = EmitJump(OP_JUMP);
			PatchJump(caseJump);
			// Remove the boolean result before executing the matched case body.
			EmitByte(OP_POP);
		}
		else if (Match(DEFAULT))
		{
			Consume(COLON, "Expect ':' after 'default'.");
			if (defaultFound)
			{
				Error("Multiple 'default' labels in one switch.");
				return;
			}
			if (lastThroughJump != -1)
			{
				PatchJump(lastThroughJump);
				lastThroughJump = -1;
			}
			while (!Check(CASE) && !Check(DEFAULT) && !Check(RIGHT_BRACE) && !Check(END_OF_FILE))
			{
				Statement();
			}
			defaultFound = true;
			lastThroughJump = EmitJump(OP_JUMP);
		}
		else
		{
			Error("Expect 'case' or 'default' in switch statement.");
			return;
		}
	}

	if (lastThroughJump != -1)
	{
		PatchJump(lastThroughJump);
	}
	// Patch break jumps BEFORE emitting the switch-expression pop,
	// so break also cleans up the switch value from the stack.
	PatchBreaks(loopStart);
	// Pop the original switch expression after all cases have finished.
	EmitByte(OP_POP);
	currentLoopStart = outterLoopStart;
}

void Compiler::ForStatement()
{
	Consume(LEFT_PAREN, "Expect '(' after 'for'.");
	BeginScope();
	// Initializer.
	if (Match(SEMICOLON))
	{
		// No initializer.
	}
	else if (Match(VAR))
	{
		VarDeclaration(false);
	}
	else
	{
		ExpressionStatement();
	}
	// Condition.
	int32_t outterLoopStart = currentLoopStart;
	int32_t outterLoopContinue = currentLoopContinue;
	int32_t loopStart = (int32_t)CurrentChunk()->GetSize();
	currentLoopStart = loopStart;
	if (Match(SEMICOLON))
	{
		// No condition.
		EmitByte(OP_TRUE);
	}
	else
	{
		Expression();
		Consume(SEMICOLON, "Expect ';' after loop condition.");
	}
	int32_t exitJump = EmitJump(OP_JUMP_IF_FALSE);
	// Drop the condition after the branch decision.
	EmitByte(OP_POP);
	int32_t bodyJump = EmitJump(OP_JUMP);
	// Increment.
	int32_t incrementStart = (int32_t)CurrentChunk()->GetSize();
	if (Match(RIGHT_PAREN))
	{
		// No increment.
	}
	else
	{
		Expression();
		// Pop the increment expression result so that it doesn't interfere with the loop body.
		EmitByte(OP_POP);
		Consume(RIGHT_PAREN, "Expect ')' after for clauses.");
	}
	// Jump back to the loop condition before compiling the body.
	EmitLoop(loopStart);
	PatchJump(bodyJump);
	currentLoopContinue = (uint32_t)incrementStart;
	Statement();
	// The increment section runs after the body, then loops back to the condition.
	EmitLoop(incrementStart);
	PatchJump(exitJump);
	// Remove the false condition when the loop exits.
	EmitByte(OP_POP);
	// Patch any pending break jumps to jump here (after the loop).
	PatchBreaks(loopStart);
	currentLoopStart = outterLoopStart;
	currentLoopContinue = outterLoopContinue;
	EndScope();
}

void Compiler::ReturnStatement()
{
	if (type == TYPE_SCRIPT)
	{
		Error("Can't return from top-level code.");
	}
	if (Match(SEMICOLON))
	{
		// A bare return becomes an implicit nil return value.
		EmitByte(OP_NIL);
	}
	else
	{
		Expression();
		Consume(SEMICOLON, "Expect ';' after return value.");
	}
	// Return always leaves through OP_RETURN with the value on top of the stack.
	EmitByte(OP_RETURN);
}

void Compiler::PrintStatement()
{
	Expression();
	Consume(SEMICOLON, "Expect ';' after value.");
	EmitByte(OP_PRINT);
}

void Compiler::ExpressionStatement()
{
	Expression();
	Consume(SEMICOLON, "Expect ';' after expression.");
	// Expression statements don't leave a value on the stack, so pop it off.
	EmitByte(OP_POP);
}

void Compiler::Expression()
{
	ParsePrecedence(PREC_ASSIGNMENT);
}

void Compiler::ParsePrecedence(Precedence precedence)
{
	Advance();

	auto prefixFunc = GetRule(parser.previous.type)->prefix;
	if (prefixFunc == nullptr)
	{
		Error("Expect expression.");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	// Call the prefix parse function as a member function pointer.
	(this->*prefixFunc)(canAssign);

	while (precedence <= GetRule(parser.current.type)->precedence)
	{
		Advance();
		auto infixFunc = GetRule(parser.previous.type)->infix;
		// Call the infix parse function as a member function pointer.
		(this->*infixFunc)();
	}

	if (canAssign && Match(EQUAL))
	{
		Error("Invalid assignment target.");
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

VMValue Compiler::EndCompiler()
{
	if (CurrentChunk()->GetSize() == 0 || CurrentChunk()->code[CurrentChunk()->GetSize() - 1] != OP_RETURN)
	{
		EmitByte(OP_NIL);
		EmitByte(OP_RETURN);
	}
#ifdef DEBUG_PRINT_CODE	
	if (!parser.hadError)
	{
		std::string disassemblyName = static_cast<std::string>(*function.value);
		CurrentChunk()->Disassemble(disassemblyName.c_str());
	}
#endif // DEBUG_PRINT_CODE

	// Let the VM take ownership of the compiled function's chunk and value, which are heap-allocated.
	function = VM::Create(function.value, function.chunk);
	// The compiler is done with the chunk and function value, but the VM now owns them, so don't free them in the destructor.
	compilingChunk = nullptr;

	return function;
}

// --- Grammar Rules ---

void Compiler::Number(bool /*canAssign*/)
{
	const std::string& lexeme = parser.previous.lexeme;
	VMValue value;
	if (lexeme.find('.') != std::string::npos)
	{
		value = VM::Create(FloatValue::CreateRaw(std::stof(lexeme)));
	}
	else
	{
		value = VM::Create(IntValue::CreateRaw(std::stoi(lexeme)));
	}
	EmitConstant(value);
}

void Compiler::Literal(bool /*canAssign*/)
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

void Compiler::String(bool /*canAssign*/)
{
	const std::string& lexeme = parser.previous.lexeme;
	EmitConstant(VM::Create(StringValue::CreateRaw(lexeme)));
}

void Compiler::Grouping(bool /*canAssign*/)
{
	Expression();
	Consume(RIGHT_PAREN, "Expect ')' after expression.");
}

void Compiler::Unary(bool /*canAssign*/)
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
		case GREATER:       EmitByte(OP_GREATER);   break;
		case GREATER_EQUAL: EmitByte(OP_LESS);    EmitByte(OP_NOT); break;
		case LESS:          EmitByte(OP_LESS);      break;
		case LESS_EQUAL:    EmitByte(OP_GREATER); EmitByte(OP_NOT); break;
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

void Compiler::And()
{
	// Short-circuit: if the left side is false, skip the right side.
	int32_t andJump = EmitJump(OP_JUMP_IF_FALSE);
	// The left operand is no longer needed once the branch is decided.
	EmitByte(OP_POP);
	ParsePrecedence(Compiler::PREC_AND);
	PatchJump(andJump);
}

void Compiler::Or()
{
	/*
	EmitByte(OP_NOT);
	int32_t orJump = EmitJump(OP_JUMP_IF_FALSE);
	EmitByte(OP_POP);
	ParsePrecedence(Compiler::PREC_OR);
	int32_t endJump = EmitJump(OP_JUMP);
	PatchJump(orJump);
	EmitByte(OP_NOT);
	PatchJump(endJump);
	*/
	// Short-circuit: if the left side is true, skip evaluating the right side.
	int32_t orJump = EmitJump(OP_JUMP_IF_FALSE);
	int32_t endJump = EmitJump(OP_JUMP);
	PatchJump(orJump);
	// Drop the left operand before evaluating the right-hand expression.
	EmitByte(OP_POP);
	ParsePrecedence(Compiler::PREC_OR);
	PatchJump(endJump);
}

void Compiler::NamedVariable(bool canAssign)
{
	OpCode getOp = OP_NIL, setOp = OP_NIL;
	int index = ResolveLocal(parser.previous);
	bool isFinal = false;
	uint32_t arg = 0;

	if (index != -1)
	{
		arg = (uint32_t)index;
		isFinal = locals[index].isFinal;
		getOp = arg <= 0xFF ? OP_GET_LOCAL : OP_GET_LOCAL_LONG;
		setOp = arg <= 0xFF ? OP_SET_LOCAL : OP_SET_LOCAL_LONG;
	}
	else
	{
		index = ResolveUpvalue(parser.previous);
		if (index != -1)
		{
			isFinal = upvalues[index].isFinal;
			getOp = OP_GET_UPVALUE;
			setOp = OP_SET_UPVALUE;
		}
		else
		{
			arg = IdentifierConstant(parser.previous);
			isFinal = globalFinals[arg];
			getOp = arg <= 0xFF ? OP_GET_GLOBAL : OP_GET_GLOBAL_LONG;
			setOp = arg <= 0xFF ? OP_SET_GLOBAL : OP_SET_GLOBAL_LONG;
		}
	}

	if (canAssign && Match(EQUAL))
	{
		if (isFinal)
		{
			Error("Cannot assign to a final variable.");
		}

		Expression();
		if (arg <= 0xFF)
		{
			EmitBytes(setOp, (uint8_t)arg);
		}
		else
		{
			EmitBytes(setOp,
				(uint8_t)((arg >> 16) & 0xFF),
				(uint8_t)((arg >> 8) & 0xFF),
				(uint8_t)(arg & 0xFF));
		}
	}
	else
	{
		if (arg <= 0xFF)
		{
			EmitBytes(getOp, (uint8_t)arg);
		}
		else
		{
			EmitBytes(getOp,
				(uint8_t)((arg >> 16) & 0xFF),
				(uint8_t)((arg >> 8) & 0xFF),
				(uint8_t)(arg & 0xFF));
		}
	}
}

uint8_t Compiler::ArgumentList()
{
	uint8_t argCount = 0;
	if (!Check(RIGHT_PAREN))
	{
		do
		{
			if (argCount == 255)
			{
				ErrorAtCurrent("Can't have more than 255 arguments.");
				return argCount;
			}
			argCount += 1;
			Expression();
		} while (Match(COMMA));
	}
	Consume(RIGHT_PAREN, "Expect ')' after arguments.");
	return argCount;
}

void Compiler::Call()
{
	uint8_t argCount = ArgumentList();
	EmitBytes(OP_CALL, argCount);
}

// --- Token Helpers ---

Token Compiler::ScanToken()
{
	if (nextToken >= tokens.size())
	{
		return Token(ERROR, "", 0, 0);
	}
	return tokens[nextToken++];
}

Token Compiler::Peek(int32_t offset)
{
	// nextToken points to the next token to be processed
	// so we need to subtract 1 to get the current token
	return tokens[nextToken + offset - 1];
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

		rules[LEFT_PAREN]    = { &Compiler::Grouping, &Compiler::Call,    PREC_CALL };
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
		rules[IDENTIFIER]    = { &Compiler::NamedVariable, nullptr,       PREC_NONE };
		rules[STRING]        = { &Compiler::String,   nullptr,            PREC_NONE };
		rules[NUMBER]        = { &Compiler::Number,   nullptr,            PREC_NONE };
		rules[AND]           = { nullptr,             &Compiler::And,     PREC_AND };
		rules[CLASS]         = { nullptr,             nullptr,            PREC_NONE };
		rules[ELSE]          = { nullptr,             nullptr,            PREC_NONE };
		rules[FALSE]         = { &Compiler::Literal,  nullptr,            PREC_NONE };
		rules[FUN]           = { nullptr,             nullptr,            PREC_NONE };
		rules[FOR]           = { nullptr,             nullptr,            PREC_NONE };
		rules[IF]            = { nullptr,             nullptr,            PREC_NONE };
		rules[NIL]           = { &Compiler::Literal,  nullptr,            PREC_NONE };
		rules[OR]            = { nullptr,             &Compiler::Or,      PREC_OR };
		rules[PRINT]         = { nullptr,             nullptr,            PREC_NONE };
		rules[RETURN]        = { nullptr,             nullptr,            PREC_NONE };
		rules[SUPER]         = { nullptr,             nullptr,            PREC_NONE };
		rules[THIS]          = { nullptr,             nullptr,            PREC_NONE };
		rules[TRUE]          = { &Compiler::Literal,  nullptr,            PREC_NONE };
		rules[VAR]           = { nullptr,             nullptr,            PREC_NONE };
		rules[WHILE]         = { nullptr,             nullptr,            PREC_NONE };
		rules[BREAK]         = { nullptr,             nullptr,            PREC_NONE };
		rules[CONTINUE]      = { nullptr,             nullptr,            PREC_NONE };
		rules[SWITCH]        = { nullptr,             nullptr,            PREC_NONE };
		rules[CASE]          = { nullptr,             nullptr,            PREC_NONE };
		rules[DEFAULT]       = { nullptr,             nullptr,            PREC_NONE };
		rules[END_OF_FILE]   = { nullptr,             nullptr,            PREC_NONE };
		rules[ERROR]         = { nullptr,             nullptr,            PREC_NONE };
	}

	return &rules[type];
}

// --- Code Generation ---

void Compiler::EmitByte(uint8_t byte)
{
	CurrentChunk()->Write(byte, (int32_t)parser.previous.line, (int32_t)parser.previous.column);
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

uint32_t Compiler::ParseVariable(const std::string& errorMessage, bool isFinal)
{
	Consume(IDENTIFIER, errorMessage.c_str());
	DeclareVariable(isFinal);
	if (scopeDepth > 0)
	{
		return UINT8_MAX;
	}
	return IdentifierConstant(parser.previous);
}

uint32_t Compiler::MakeConstant(VMValue value)
{
	int32_t constantIndex = CurrentChunk()->AddConstant(value);
	if (constantIndex > 0xFFFFFF)
	{
		Error("Too many constants in one chunk.");
		return 0;
	}
	return (uint32_t)constantIndex;
}

uint32_t Compiler::IdentifierConstant(const Token& name)
{
	return MakeConstant(VM::Create(StringValue::CreateRaw(name.lexeme)));
}

void Compiler::DefineVariable(uint32_t global, bool isFinal)
{
	// Define a local variable. Mark it defined at this scope depth and emit no bytecode.
	if (scopeDepth > 0)
	{
		// Local variable. Mark it defined at this scope depth and emit no bytecode.
		MarkInitialize();
		return;
	}

	// Define a global variable. Emit bytecode to define it at the top level.
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

	globalFinals[global] = isFinal;
}

void Compiler::DeclareVariable(bool isFinal)
{
	if (scopeDepth == 0)
	{
		return;
	}
	const Token& name = parser.previous;
	AddLocal(name, isFinal);
}

void Compiler::AddLocal(const Token& name, bool isFinal)
{
	if (locals.size() >= LOCAL_MAX)
	{
		Error("Too many local variables in function.");
		return;
	}
	for (int32_t index = (int32_t)locals.size() - 1; index >= 0; --index)
	{
		const Local& local = locals[(size_t)index];
		if (local.depth != -1 && local.depth < scopeDepth)
		{
			break;
		}
		if (local.name.lexeme == name.lexeme)
		{
			Error("Variable with this name already declared in this scope.");
		}
	}
	locals.push_back(Local{});
	locals.back().name = name;
	locals.back().isFinal = isFinal;
	locals.back().depth = -1;
}

void Compiler::MarkInitialize()
{
	if (scopeDepth == 0)
	{
		return;
	}
	if (!locals.empty())
	{
		locals.back().depth = scopeDepth;
	}
}

int32_t Compiler::ResolveLocal(const Token& name)
{
	for (int32_t index = (int32_t)locals.size() - 1; index >= 0; --index)
	{
		if (locals[(size_t)index].name.lexeme == name.lexeme)
		{
			if (locals[(size_t)index].depth == -1)
			{
				// This error is only relevant if we allow referencing a local variable in its own initializer, which we currently don't. 
				// If we add that feature in the future, we can uncomment this error to prevent it.
				// ErrorAt(&parser.previous, "Cannot read local variable in its own initializer.");
				// Skip this inner local because it's declared but not yet initialized.
				// This lets the initializer expression resolve an outer variable with the same name.
				continue;
			}
			return index;
		}
	}
	return -1;
}

int32_t Compiler::ResolveUpvalue(const Token& name)
{
	if (enclosing == nullptr)
	{
		return -1;
	}
	int32_t localIndex = enclosing->ResolveLocal(name);
	if (localIndex != -1)
	{
		return AddUpvalue(localIndex, true, enclosing->locals[localIndex].isFinal);
	}
	return -1;
}

int32_t Compiler::AddUpvalue(int32_t index, bool isLocal, bool isFinal)
{
	for (size_t i = 0; i < upvalues.size(); ++i)
	{
		if (upvalues[i].index == index && upvalues[i].isLocal == isLocal)
		{
			return (int32_t)i;
		}
	}

	if (upvalues.size() == UINT8_MAX)
	{
		Error("Too many closure variables in function.");
		return 0;
	}

	upvalues.push_back(UpValue{ index, isLocal, isFinal });
	return (int32_t)upvalues.size() - 1;
}

int32_t Compiler::EmitJump(uint8_t instruction)
{
	// Reserve two bytes for the jump distance and patch them once the target is known.
	EmitByte(instruction);
	EmitByte(0xFF);
	EmitByte(0xFF);
	return (int32_t)CurrentChunk()->GetSize() - 2;
}

void Compiler::PatchJump(int32_t offset)
{
	int32_t jump = CurrentChunk()->GetSize() - offset - 2;
	if (jump > 0xFFFFFF)
	{
		Error("Too much code to jump over.");
	}
	// Write the final forward jump distance into the reserved operand bytes.
	CurrentChunk()->code[offset] = (uint8_t)((jump >> 8) & 0xFF);
	CurrentChunk()->code[offset + 1] = (uint8_t)((jump >> 0) & 0xFF);
}

void Compiler::EmitLoop(int32_t loopStart)
{
	// Emit a backward jump to the loop header.
	EmitByte(OP_LOOP);
	int32_t offset = (int32_t)CurrentChunk()->GetSize() - loopStart + 2;
	if (offset > 0xFFFF)
	{
		Error("Loop body too large.");
	}
	EmitByte((uint8_t)((offset >> 8) & 0xFF));
	EmitByte((uint8_t)(offset & 0xFF));
}

void Compiler::PatchBreaks(int32_t loopStart)
{
	auto it = breakJumpPatches.find(loopStart);
	if (it != breakJumpPatches.end())
	{
		for (uint32_t offset : it->second)
		{
			PatchJump(offset);
		}
		breakJumpPatches.erase(it);
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
