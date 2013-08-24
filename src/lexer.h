#ifndef LEXER_H
#define LEXER_H

#include "error.h"
#include "source.h"
#include <stddef.h>

typedef struct lexer_t lexer_t;

typedef enum
{
  TK_NONE,

  // primitives
  TK_STRING,
  TK_INT,
  TK_FLOAT,
  TK_ID,

  // symbols
  TK_LBRACE,
  TK_RBRACE,
  TK_LPAREN,
  TK_RPAREN,
  TK_LBRACKET,
  TK_RBRACKET,
  TK_COMMA,
  TK_ARROW,

  TK_DOT,
  TK_COLON,
  TK_SEMI,
  TK_BACKSLASH,
  TK_EQUALS,

  TK_PLUS,
  TK_MINUS,
  TK_MULTIPLY,
  TK_DIVIDE,
  TK_MOD,

  TK_LSHIFT,
  TK_RSHIFT,

  TK_LT,
  TK_LE,
  TK_GE,
  TK_GT,

  TK_EQ,
  TK_NEQ,

  TK_PIPE,
  TK_AMP,

  // keywords
  TK_USE,
  TK_AS,
  TK_ALIAS,
  TK_TRAIT,
  TK_CLASS,
  TK_ACTOR,
  TK_IS,
  TK_ISO,
  TK_VAR,
  TK_VAL,
  TK_TAG,
  TK_FUN,
  TK_MSG,
  TK_RETURN,
  TK_BREAK,
  TK_CONTINUE,
  TK_IF,
  TK_THEN,
  TK_ELSE,
  TK_END,
  TK_FOR,
  TK_IN,
  TK_WHILE,
  TK_DO,
  TK_MATCH,
  TK_THIS,
  TK_NOT,
  TK_AND,
  TK_OR,
  TK_XOR,
  TK_PRIVATE,
  TK_PACKAGE,
  TK_INFER,

  // abstract
  TK_PROGRAM,
  TK_MODULE,
  TK_TYPEID,
  TK_ADT,
  TK_PARTIALTYPE,
  TK_OBJECTTYPE,
  TK_FUNCTIONTYPE,
  TK_FORMALPARAMS,
  TK_MODE,
  TK_PARAMS,
  TK_CALL,
  TK_FORMALARGS,
  TK_ARGS,
  TK_SEQ,

  TK_EOF
} token_id;

typedef struct token_t
{
  token_id id;
  size_t line;
  size_t pos;

  union
  {
    const char* string;
    double real;
    __int128_t integer;
  };
} token_t;

lexer_t* lexer_open( source_t* source );
void lexer_close( lexer_t* lexer );
token_t* lexer_next( lexer_t* lexer );

token_t* token_new( token_id id, size_t line, size_t pos );
const char* token_string( token_t* token );
void token_free( token_t* token );

#endif
