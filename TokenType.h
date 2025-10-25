#pragma once
#include <string>

// X-Macro technique to define token types and their string representations
// Each token is defined with a name and a string representation
// The string representation is mainly for debugging and error messages
// The actual string values are not used in the code logic
// but can be useful for printing or logging purposes

#define TOKEN_TYPE_LIST \
	TOKEN(LEFT_PAREN,     "(")      /* ( */ \
	TOKEN(RIGHT_PAREN,    ")")      /* ) */ \
	TOKEN(LEFT_BRACE,     "{")      /* { */ \
	TOKEN(RIGHT_BRACE,    "}")      /* } */ \
	TOKEN(COMMA,          ",")      /* , */ \
	TOKEN(DOT,            ".")      /* . */ \
	TOKEN(MINUS,          "-")      /* - */ \
	TOKEN(PLUS,           "+")      /* + */ \
	TOKEN(SEMICOLON,      ";")      /* ; */ \
	TOKEN(SLASH,          "/")      /* / */ \
	TOKEN(STAR,           "*")      /* * */ \
	TOKEN(BANG,           "!")      /* ! */ \
	TOKEN(QUESTION,	      "?")      /* ? */ \
	TOKEN(COLON,		  ":")      /* : */ \
	TOKEN(BANG_EQUAL,     "!=")     /* != */ \
	TOKEN(EQUAL,          "=")      /* = */ \
	TOKEN(EQUAL_EQUAL,    "==")     /* == */ \
	TOKEN(GREATER,        ">")      /* > */ \
	TOKEN(GREATER_EQUAL,  ">=")     /* >= */ \
	TOKEN(LESS,           "<")      /* < */ \
	TOKEN(LESS_EQUAL,     "<=")     /* <= */ \
	TOKEN(IDENTIFIER,     "IDENTIFIER") /* identifier */ \
	TOKEN(STRING,         "STRING")      /* string */ \
	TOKEN(NUMBER,         "NUMBER")      /* number */ \
	TOKEN(AND,            "and")         /* and */ \
	TOKEN(CLASS,          "class")       /* class */ \
	TOKEN(ELSE,           "else")        /* else */ \
	TOKEN(FALSE,          "false")       /* false */ \
	TOKEN(FUN,            "fun")         /* fun */ \
	TOKEN(FOR,            "for")         /* for */ \
	TOKEN(IF,             "if")          /* if */ \
	TOKEN(NIL,            "nil")         /* nil */ \
	TOKEN(OR,             "or")          /* or */ \
	TOKEN(PRINT,          "print")       /* print */ \
	TOKEN(RETURN,         "return")      /* return */ \
	TOKEN(SUPER,          "super")       /* super */ \
	TOKEN(THIS,           "this")        /* this */ \
	TOKEN(TRUE,           "true")        /* true */ \
	TOKEN(VAR,            "var")         /* var */ \
	TOKEN(WHILE,          "while")       /* while */ \
	TOKEN(END_OF_FILE,    "EOF")         /* end of file */ \
	TOKEN(ERROR,          "ERROR")       /* error */

enum TokenType
{
#define TOKEN(name, str) name,     // str
	TOKEN_TYPE_LIST
#undef TOKEN
};

static const char* const TokenTypeName[] =
{
#define TOKEN(name, str) #name,
	TOKEN_TYPE_LIST
#undef TOKEN
};

#undef TOKEN_TYPE_LIST

typedef std::string Lexeme;

struct Token
{
	Lexeme lexeme;
	size_t line;
	size_t column;
	TokenType type;

	Token()
	{
		line = 0;
		column = 0;
		type = ERROR;
	}

	Token(TokenType inType, Lexeme inLexeme, size_t inIine, size_t inColumn)
	{
		lexeme = inLexeme;
		type = inType;
		line = inIine;
		column = inColumn;
	}
};