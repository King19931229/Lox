#include "Parser.h"
#include "Lox.h"

Parser::Parser(const std::vector<Token>& inTokens)
	: tokens(inTokens)
	, current(0)
	, error(false)
{
}

Parser::~Parser()
{
}

Token Parser::Consume(TokenType type, const std::string& errorMessage)
{
	if (Check(type))
	{
		return Advance();
	}
	return Error(Peek(), errorMessage);
}

Token Parser::Error(const Token& token, const std::string& errorMessage)
{
	if (token.type == TokenType::END_OF_FILE)
	{
		Lox::GetInstance().Error(token.line, token.column, "%s", errorMessage.c_str());
	}
	else
	{
		Lox::GetInstance().Error(token.line, token.column, "at '%s': %s", token.lexeme.c_str(), errorMessage.c_str());
	}
	error = true;
	Synchronize();
	return Token();
}

void Parser::Synchronize()
{
	// Consume tokens until reaching a statement boundary
	if (!IsAtEnd()) Advance();
	while (!IsAtEnd())
	{
		if (Previous().type == TokenType::SEMICOLON) return;
		switch (Peek().type)
		{
			case TokenType::CLASS:
			case TokenType::FUN:
			case TokenType::VAR:
			case TokenType::FOR:
			case TokenType::IF:
			case TokenType::WHILE:
			case TokenType::PRINT:
			case TokenType::RETURN:
			case TokenType::BREAK:
				return;
			default:
				break;
		}
		Advance();
	}
}

ExprPtr Parser::Assignment()
{
	ExprPtr expr = Or();
	if (Match(TokenType::EQUAL))
	{
		Token equal = Previous();
		ExprPtr value = Assignment();

		if (Variable* var =	dynamic_cast<Variable*>(expr.get()))
		{
			expr = Assign::Create(var->name, value);
			return expr;
		}
		else if (Get* get = dynamic_cast<Get*>(expr.get()))
		{
			expr = Set::Create(get->object, get->name, value);
			return expr;
		}
		Lox::GetInstance().RuntimeError(equal.line, equal.column, "Invalid assignment target.");
		return nullptr;
	}
	return expr;
}

ExprPtr Parser::Or()
{
	ExprPtr expr = And();
	while (Match(TokenType::OR))
	{
		Token orOp = Previous();
		ExprPtr otherExpr = And();
		expr = Logical::Create(expr, orOp, otherExpr);
	}
	return expr;
}

ExprPtr Parser::And()
{
	ExprPtr expr = Comma();
	while (Match(TokenType::AND))
	{
		Token andOp = Previous();
		ExprPtr otherExpr = Comma();
		expr = Logical::Create(expr, andOp, otherExpr);
	}
	return expr;
}

ExprPtr Parser::Comma()
{
	ExprPtr expr = Ternary();

	while (Match(TokenType::COMMA))
	{
		Token op = Previous();
		ExprPtr right = Ternary();
		expr = Binary::Create(expr, op, right);
	}

	return expr;
}

ExprPtr Parser::Ternary()
{
	if (Match(TokenType::QUESTION))
	{
		Error(Previous(), "Expect expression before '?'.");
		return nullptr;
	}

	ExprPtr expr = Equality();

	if (Match(TokenType::QUESTION))
	{
		Token opLeft = Previous();
		ExprPtr middle = Or();
		Consume(TokenType::COLON, "Expect ':' after expression.");
		Token opRight = Previous();
		ExprPtr right = Ternary();
		expr = Ternary::Create(expr, opLeft, middle, opRight, right);
	}

	return expr;
}

ExprPtr Parser::Equality()
{
	if (Match(TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL))
	{
		Error(Previous(), "Expect expression before equality operator.");
		return nullptr;
	}

	ExprPtr expr = Comparsion();

	while (Match(TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL))
	{
		Token op = Previous();
		ExprPtr right = Comparsion();
		expr = Binary::Create(expr, op, right);
	}
	
	return expr;
}

ExprPtr Parser::Comparsion()
{
	if (Match(TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL))
	{
		Error(Previous(), "Expect expression before comparison operator.");
		return nullptr;
	}

	ExprPtr expr = Term();

	while (Match(TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL))
	{
		Token op = Previous();
		ExprPtr right = Term();
		expr = Binary::Create(expr, op, right);
	}

	return expr;
}

ExprPtr Parser::Term()
{
	ExprPtr expr = Factor();

	while (Match(TokenType::MINUS, TokenType::PLUS))
	{
		Token op = Previous();
		ExprPtr right = Factor();
		expr = Binary::Create(expr, op, right);
	}

	return expr;
}

ExprPtr Parser::Factor()
{
	if (Match(TokenType::SLASH, TokenType::STAR))
	{
		Error(Previous(), "Expect expression before factor operator.");
		return nullptr;
	}

	ExprPtr expr = Unary();

	while (Match(TokenType::SLASH, TokenType::STAR))
	{
		Token op = Previous();
		ExprPtr right = Unary();
		expr = Binary::Create(expr, op, right);
	}

	return expr;
}

ExprPtr Parser::Unary()
{
	if (Match(TokenType::BANG, TokenType::MINUS))
	{
		Token op = Previous();
		ExprPtr right = Unary();
		return Unary::Create(op, right);
	}

	return Call();
}

std::vector<ExprPtr> Parser::FinishArguments(ExprPtr expr)
{
	if (Binary* binary = dynamic_cast<Binary*>(expr.get()))
	{
		if (binary->op.type == TokenType::COMMA)
		{
			std::vector<ExprPtr> args;
			auto leftArgs = FinishArguments(binary->left);
			args.insert(args.end(), leftArgs.begin(), leftArgs.end());
			auto rightArgs = FinishArguments(binary->right);
			args.insert(args.end(), rightArgs.begin(), rightArgs.end());
			return args;
			return args;
		}
	}
	return { expr };
}

ExprPtr Parser::FinishCall(const ExprPtr& callee)
{
	std::vector<ExprPtr> arguments;
	if (!Check(TokenType::RIGHT_PAREN))
	{
		do
		{
			std::vector<ExprPtr> newArgs = FinishArguments(Expression());
			arguments.insert(arguments.end(), newArgs.begin(), newArgs.end());
			if (arguments.size() >= 255)
			{
				Error(Peek(), "Can't have more than 255 arguments.");
			}
		} while (Match(TokenType::COMMA));
	}
	Token paren = Consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
	return Call::Create(callee, paren, arguments);
}

ExprPtr Parser::Call()
{
	ExprPtr expr = Primary();
	// Handle continued calls
	while (true)
	{
		if (Match(TokenType::LEFT_PAREN))
		{
			expr = FinishCall(expr);
		}
		else if (Match(TokenType::DOT))
		{
			Token name = Consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
			expr = Get::Create(expr, name);
		}
		else
		{
			break;
		}
	}
	return expr;
}

ExprPtr Parser::Expression()
{
	return Assignment();
}

ExprPtr Parser::Primary()
{
	if (Match(TokenType::FALSE, TokenType::TRUE, TokenType::NIL, TokenType::NUMBER, TokenType::STRING))
	{
		return Literal::Create(Previous());
	}

	if (Match(TokenType::THIS))
	{
		return This::Create(Previous());
	}

	if (Match(TokenType::LEFT_PAREN))
	{
		ExprPtr expr = Expression();
		Consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
		return expr; // Should wrap in Grouping, omitted for brevity
	}

	if (Match(TokenType::IDENTIFIER))
	{
		return Variable::Create(Previous());
	}

	// Function expression (anonymous function)
	if (Match(TokenType::FUN))
	{
		Token keyword = Previous();
		Consume(TokenType::LEFT_PAREN, "Expect '(' after 'fun'.");
		std::vector<Token> parameters;
		if (!Check(TokenType::RIGHT_PAREN))
		{
			do
			{
				if (parameters.size() >= 255)
				{
					Error(Peek(), "Can't have more than 255 parameters.");
				}
				Consume(TokenType::IDENTIFIER, "Expect parameter name.");
				parameters.push_back(Previous());
			} while (Match(TokenType::COMMA));
		}
		Consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
		Consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");
		StatPtr body = BlockStatement();
		return Lambda::Create(keyword, parameters, dynamic_cast<Block*>(body.get())->statements);
	}

	Error(Peek(), "Expect expression.");
	return nullptr;
}

ExprPtr Parser::ParseExpr()
{
	return Expression();
}

std::vector<StatPtr> Parser::Parse()
{
	std::vector<StatPtr> statments;
	while (!IsAtEnd())
	{
		statments.push_back(Declaration());
	}
	return statments;
}

StatPtr Parser::Statment()
{
	if (Match(TokenType::PRINT))
	{
		return PrintStatement();
	}
	else if (Match(TokenType::LEFT_BRACE))
	{
		return BlockStatement();
	}
	else if (Match(TokenType::IF))
	{
		return IfStatement();
	}
	else if(Match(TokenType::RETURN))
	{
		return ReturnStatement();
	}
	else if (Match(TokenType::WHILE))
	{
		return WhileStatement();
	}
	else if (Match(TokenType::FOR))
	{
		return ForStatment();
	}
	else if (Match(TokenType::BREAK))
	{
		return BreakStatement();
	}
	return ExpressionStatment();
}

StatPtr Parser::Declaration()
{
	if (Match(TokenType::VAR))
	{
		return VarDeclaration();
	}
	else if (Match(TokenType::FUN))
	{
		return FunDeclaration("function");
	}
	else if (Match(TokenType::CLASS))
	{
		return ClassDeclaration();
	}
	else
	{
		StatPtr statment = Statment();
		return statment;
	}
}

StatPtr Parser::VarDeclaration()
{
	Consume(TokenType::IDENTIFIER, "Expect variable name.");
	Token name = Previous();
	ExprPtr initializer = nullptr;
	if (Match(TokenType::EQUAL))
	{
		initializer = Assignment();
	}
	Consume(TokenType::SEMICOLON, "Expect ';' after '" + Previous().lexeme + "'.");
	return Var::Create(name, initializer);
}

StatPtr Parser::FunDeclaration(const std::string& kind)
{
	Consume(TokenType::IDENTIFIER, "Expect " + kind + " name.");
	Token name = Previous();
	Consume(TokenType::LEFT_PAREN, "Expect '(' after function name.");
	std::vector<Token> parameters;
	if (!Check(TokenType::RIGHT_PAREN))
	{
		do
		{
			if (parameters.size() >= 255)
			{
				Error(Peek(), "Can't have more than 255 parameters.");
			}
			Consume(TokenType::IDENTIFIER, "Expect parameter name.");
			parameters.push_back(Previous());
		} while (Match(TokenType::COMMA));
	}
	Consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
	Consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");
	StatPtr body = BlockStatement();
	return Function::Create(name, parameters, dynamic_cast<Block*>(body.get())->statements);
}

StatPtr Parser::ClassDeclaration()
{
	Token name = Consume(TokenType::IDENTIFIER, "Expect class name.");
	Consume(TokenType::LEFT_BRACE, "Expect '{' before class body.");
	std::vector<StatPtr> methods;
	while (!Check(TokenType::RIGHT_BRACE) && !IsAtEnd())
	{
		methods.push_back(FunDeclaration("method"));
	}
	Consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
	return Class::Create(name, methods);
}

StatPtr Parser::PrintStatement()
{
	ExprPtr expr = Assignment();
	Consume(TokenType::SEMICOLON, "Expect ';' after '" + Previous().lexeme + "'.");
	return Print::Create(expr);
}

StatPtr Parser::BlockStatement()
{
	std::vector<StatPtr> statements;
	while (!Check(TokenType::RIGHT_BRACE) && !IsAtEnd())
	{
		statements.push_back(Declaration());
	}
	Consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
	return Block::Create(statements);
}

StatPtr Parser::IfStatement()
{
	ExprPtr condition = Expression();
	StatPtr thenBranch = Declaration();
	StatPtr elseBranch = nullptr;
	if (Match(TokenType::ELSE))
	{
		elseBranch = Declaration();
	}
	return If::Create(condition, thenBranch, elseBranch);
}

StatPtr Parser::WhileStatement()
{
	ExprPtr condition = Expression();
	StatPtr body = Statment();
	return While::Create(condition, body);
}

StatPtr Parser::ReturnStatement()
{
	Token keyword = Previous();
	ExprPtr value = nullptr;
	if (!Check(TokenType::SEMICOLON))
	{
		value = Expression();
	}
	Consume(TokenType::SEMICOLON, "Expect ';' after '" + Previous().lexeme + "'.");
	return Return::Create(keyword, value);
}

StatPtr Parser::ExpressionStatment()
{
	ExprPtr expr = Assignment();
	Consume(TokenType::SEMICOLON, "Expect ';' after '" + Previous().lexeme + "'.");
	return Expression::Create(expr);
}

StatPtr Parser::ForStatment()
{
	Consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");

	StatPtr initializer;
	if (Match(TokenType::SEMICOLON))
	{
		initializer = nullptr;
	}
	else if (Match(TokenType::VAR))
	{
		initializer = VarDeclaration();
	}
	else
	{
		initializer = ExpressionStatment();
	}

	ExprPtr condition = nullptr;
	if (!Check(TokenType::SEMICOLON))
	{
		condition = Expression();
	}
	Consume(TokenType::SEMICOLON, "Expect ';' after '" + Previous().lexeme + "'.");

	ExprPtr increment = nullptr;
	if (!Check(TokenType::RIGHT_PAREN))
	{
		increment = Expression();
	}
	Consume(TokenType::RIGHT_PAREN, "Expect ')' after for clauses.");

	StatPtr body = Statment();

	if (increment)
	{
		std::vector<StatPtr> statements;
		statements.push_back(body);
		statements.push_back(Expression::Create(increment));
		body = Block::Create(statements);
	}

	if (!condition)
	{
		condition = Literal::Create(Token(TokenType::TRUE, "true", 0, 0));
	}

	body = While::Create(condition, body);

	if (initializer)
	{
		std::vector<StatPtr> statements;
		statements.push_back(initializer);
		statements.push_back(body);
		body = Block::Create(statements);
	}

	return body;
}

StatPtr Parser::BreakStatement()
{
	Token keyword = Previous();
	Consume(TokenType::SEMICOLON, "Expect ';' after '" + keyword.lexeme + "'.");
	return Break::Create(keyword);
}