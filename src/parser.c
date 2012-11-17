#include "parser.h"
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

struct parser_t
{
  lexer_t* lexer;
  token_t* t;
  ast_t* ast;
  errorlist_t* errors;
};

typedef ast_t* (*rule_t)( parser_t* );

typedef struct alt_t
{
  token_id id;
  rule_t f;
} alt_t;

static token_id current( parser_t* parser )
{
  return parser->t->id;
}

static ast_t* ast_new( parser_t* parser, token_id id )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = calloc( 1, sizeof(token_t) );
  ast->t->id = id;
  ast->t->line = parser->t->line;
  ast->t->pos = parser->t->pos;

  return ast;
}

static ast_t* ast_token( parser_t* parser )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = parser->t;
  parser->t = lexer_next( parser->lexer );

  return ast;
}

static ast_t* ast_expect( parser_t* parser, token_id id )
{
  if( current( parser ) == id )
  {
    return ast_token( parser );
  }

  error_new( parser->errors, parser->t->line, parser->t->pos, "Expected %d, got %d", id, current( parser ) );
  return NULL;
}

static void ast_free( ast_t* ast )
{
  if( ast == NULL ) { return; }
  if( ast->t != NULL ) { token_free( ast->t ); }

  for( int i = 0; i < AST_SLOTS; i++ )
  {
    ast_free( ast->child[i] );
  }

  ast_free( ast->sibling );
}

static bool accept( parser_t* parser, token_id id, ast_t* ast, int slot )
{
  if( current( parser ) != id ) { return false; }

  if( (ast != NULL) && (slot >= 0) )
  {
    assert( slot < AST_SLOTS );
    ast_t* child = ast_token( parser );
    ast->child[slot] = child;
  } else {
    token_free( parser->t );
    parser->t = lexer_next( parser->lexer );
  }

  return true;
}

static bool expect( parser_t* parser, token_id id, ast_t* ast, int slot )
{
  if( accept( parser, id, ast, slot ) ) { return true; }
  error_new( parser->errors, parser->t->line, parser->t->pos, "Expected %d, got %d", id, current( parser ) );
  return false;
}

static void rule( parser_t* parser, rule_t f, ast_t* ast, int slot )
{
  assert( ast != NULL );
  assert( slot >= 0 );
  assert( slot < AST_SLOTS );

  ast_t* child = f( parser );
  ast->child[slot] = child;
}

static void rulelist( parser_t* parser, rule_t f, token_id sep, ast_t* ast, int slot )
{
  assert( ast != NULL );
  assert( slot >= 0 );
  assert( slot < AST_SLOTS );

  ast_t* last = NULL;

  while( true )
  {
    ast_t* child = f( parser );
    if( child == NULL ) { return; }

    if( last != NULL )
    {
      last->sibling = child;
    } else {
      ast->child[slot] = child;
    }

    if( !accept( parser, sep, ast, -1 ) ) { return; }
    last = child;
  }
}

static ast_t* rulealt( parser_t* parser, const alt_t* alt )
{
  while( alt->f != NULL )
  {
    if( current( parser ) == alt->id )
    {
      return alt->f( parser );
    }

    alt++;
  }

  return NULL;
}

static void rulealtlist( parser_t* parser, const alt_t* alt, ast_t* ast, int slot )
{
  assert( ast != NULL );
  assert( slot >= 0 );
  assert( slot < AST_SLOTS );

  ast_t* last = NULL;

  while( true )
  {
    ast_t* child = rulealt( parser, alt );
    if( child == NULL ) { return; }

    if( last != NULL )
    {
      last->sibling = child;
    } else {
      ast->child[slot] = child;
    }

    last = child;
  }
}

static ast_t* annotation( parser_t* parser )
{
  // (BANG | UNIQ | READONLY | RECEIVER)?
  static const alt_t alt[] =
  {
    { TK_BANG, ast_token },
    { TK_UNIQ, ast_token },
    { TK_READONLY, ast_token },
    { TK_RECEIVER, ast_token },
    { 0, NULL }
  };

  return rulealt( parser, alt );
}

// forward declarations
static ast_t* lambda( parser_t* parser );
static ast_t* typeclass( parser_t* parser );
static ast_t* unary( parser_t* parser );
static ast_t* expr( parser_t* parser );
static ast_t* arg( parser_t* parser );
static ast_t* args( parser_t* parser );

static ast_t* atom( parser_t* parser )
{
  // THIS | TRUE | FALSE | INT | STRING | ID | typeclass
  static const alt_t alt[] =
  {
    { TK_THIS, ast_token },
    { TK_TRUE, ast_token },
    { TK_FALSE, ast_token },
    { TK_INT, ast_token },
    { TK_FLOAT, ast_token },
    { TK_STRING, ast_token },
    { TK_ID, ast_token },
    { TK_TYPEID, typeclass },
    { 0, NULL }
  };

  ast_t* ast = rulealt( parser, alt );

  if( ast == NULL )
  {
    error_new( parser->errors, parser->t->line, parser->t->pos, "Expected atom" );
  }

  return ast;
}

static ast_t* command( parser_t* parser )
{
  // (LPAREN expr RPAREN | LBRACKET arg (COMMA arg)* RBRACKET | atom) ((CALL ID)? args)*
  ast_t* ast;

  if( accept( parser, TK_LPAREN, NULL, -1 ) )
  {
    ast = expr( parser );
    expect( parser, TK_RPAREN, NULL, -1 );
  } else if( current( parser ) == TK_LBRACKET ) {
    ast = ast_token( parser );
    ast->t->id = TK_LIST;
    rulelist( parser, arg, TK_COMMA, ast, 0 );
    expect( parser, TK_RBRACKET, ast, -1 );
  } else {
    ast = atom( parser );
  }

  if( ast != NULL )
  {
    while( true )
    {
      switch( current( parser ) )
      {
      case TK_CALL:
        {
          ast_t* a = ast_token( parser );
          a->child[0] = ast;
          ast = a;

          expect( parser, TK_ID, ast, 1 );
          rule( parser, args, ast, 2 );
        }
        break;

      case TK_LPAREN:
        {
          ast_t* a = ast_new( parser, TK_CALL );
          a->child[0] = ast;
          ast = a;

          rule( parser, args, ast, 2 );
        }
        break;

      default:
        return ast;
      }
    }
  }

  return ast;
}

static ast_t* unop( parser_t* parser )
{
  ast_t* ast = ast_token( parser );
  rule( parser, unary, ast, 0 );
  return ast;
}

static ast_t* unary( parser_t* parser )
{
  // unop unary | lambda | command
  static const alt_t alt[] =
  {
    { TK_PARTIAL, unop },
    { TK_MINUS, unop },
    { TK_BANG, unop },
    { TK_LAMBDA, lambda },
    { 0, NULL }
  };

  ast_t* ast = rulealt( parser, alt );

  if( ast == NULL )
  {
    ast = command( parser );

    if( ast == NULL )
    {
      error_new( parser->errors, parser->t->line, parser->t->pos, "Expected unary" );
    }
  }

  return ast;
}

static ast_t* expr( parser_t* parser )
{
  // unary (binop unary)*
  ast_t* ast = unary( parser );

  if( ast != NULL )
  {
    while( true )
    {
      switch( current( parser ) )
      {
      case TK_PLUS:
      case TK_MINUS:
      case TK_MULTIPLY:
      case TK_DIVIDE:
      case TK_MOD:
      case TK_LSHIFT:
      case TK_RSHIFT:
      case TK_LT:
      case TK_LE:
      case TK_GE:
      case TK_GT:
      case TK_EQ:
      case TK_NOTEQ:
      case TK_STEQ:
      case TK_NSTEQ:
      case TK_OR:
      case TK_AND:
      case TK_XOR:
        {
          ast_t* binop = ast_token( parser );
          binop->child[0] = ast;
          ast = binop;

          rule( parser, unary, ast, 1 );
        }
        break;

      default:
        return ast;
      }
    }
  }

  return NULL;
}

// forward declarations
static ast_t* block( parser_t* parser );
static ast_t* oftype( parser_t* parser );

static ast_t* arg( parser_t* parser )
{
  // expr oftype (ASSIGN expr)?
  ast_t* ast = ast_new( parser, TK_ARG );

  rule( parser, expr, ast, 0 );
  rule( parser, oftype, ast, 1 );

  if( accept( parser, TK_ASSIGN, ast, -1 ) )
  {
    rule( parser, expr, ast, 2 );
  }

  return ast;
}

static ast_t* args( parser_t* parser )
{
  // LPAREN (arg (COMMA arg)*)? RPAREN
  ast_t* ast = ast_new( parser, TK_ARGS );
  expect( parser, TK_LPAREN, ast, -1 );

  if( !accept( parser, TK_RPAREN, ast, -1 ) )
  {
    rulelist( parser, arg, TK_COMMA, ast, 0 );
    expect( parser, TK_RPAREN, ast, -1 );
  }

  return ast;
}

static ast_t* conditional( parser_t* parser )
{
  // IF expr block (ELSE (conditional | block))?
  ast_t* ast = ast_expect( parser, TK_IF );

  rule( parser, expr, ast, 0 );
  rule( parser, block, ast, 1 );

  if( accept( parser, TK_ELSE, ast, -1 ) )
  {
    if( current( parser ) == TK_IF )
    {
      rule( parser, conditional, ast, 2 );
    } else {
      rule( parser, block, ast, 2 );
    }
  }

  return ast;
}

static ast_t* forvar( parser_t* parser )
{
  // ID oftype
  ast_t* ast = ast_new( parser, TK_VAR );

  expect( parser, TK_ID, ast, 0 );
  rule( parser, oftype, ast, 1 );

  return ast;
}

static ast_t* forloop( parser_t* parser )
{
  // FOR forvar (COMMA forvar)* IN expr block
  ast_t* ast = ast_expect( parser, TK_FOR );

  rulelist( parser, forvar, TK_COMMA, ast, 0 );
  expect( parser, TK_IN, ast, -1 );
  rule( parser, expr, ast, 1 );
  rule( parser, block, ast, 2 );

  return ast;
}

static ast_t* whileloop( parser_t* parser )
{
  // WHILE expr block
  ast_t* ast = ast_expect( parser, TK_WHILE );

  rule( parser, expr, ast, 0 );
  rule( parser, block, ast, 1 );

  return ast;
}

static ast_t* doloop( parser_t* parser )
{
  // DO block WHILE expr
  ast_t* ast = ast_expect( parser, TK_DO );

  rule( parser, block, ast, 1 );
  expect( parser, TK_WHILE, ast, -1 );
  rule( parser, expr, ast, 0 );

  return ast;
}

static ast_t* casevar( parser_t* parser )
{
  // AS forvar | expr (AS forvar)?
  ast_t* ast = ast_new( parser, TK_CASEVAR );

  if( accept( parser, TK_AS, ast, -1 ) )
  {
    rule( parser, forvar, ast, 0 );
  } else {
    rule( parser, expr, ast, 1 );

    if( accept( parser, TK_AS, ast, -1 ) )
    {
      rule( parser, forvar, ast, 0 );
    }
  }

  return ast;
}

static ast_t* caseblock( parser_t* parser )
{
  // CASE (casevar (COMMA casevar)*)? (IF expr)? block
  ast_t* ast = ast_expect( parser, TK_CASE );

  if( (current( parser ) != TK_IF)
    && (current( parser ) != TK_LBRACE)
    )
  {
    rulelist( parser, casevar, TK_COMMA, ast, 0 );
  }

  if( accept( parser, TK_IF, ast, -1 ) )
  {
    rule( parser, expr, ast, 1 );
  }

  rule( parser, block, ast, 2 );

  return ast;
}

static ast_t* match( parser_t* parser )
{
  // MATCH expr (COMMA expr)* LBRACE caseblock* RBRACE
  static const alt_t alt[] =
  {
    { TK_CASE, caseblock },
    { 0, NULL }
  };

  ast_t* ast = ast_expect( parser, TK_MATCH );

  rulelist( parser, expr, TK_COMMA, ast, 0 );
  expect( parser, TK_LBRACE, ast, -1 );
  rulealtlist( parser, alt, ast, 1 );
  expect( parser, TK_RBRACE, ast, -1 );

  return ast;
}

static ast_t* catchblock( parser_t* parser )
{
  // CATCH block
  ast_t* ast = ast_expect( parser, TK_CATCH );
  rule( parser, block, ast, 0 );
  return ast;
}

static ast_t* always( parser_t* parser )
{
  // ALWAYS block
  ast_t* ast = ast_expect( parser, TK_ALWAYS );
  rule( parser, block, ast, 0 );
  return ast;
}

static ast_t* lvalue( parser_t* parser )
{
  // (VAR ID oftype) | command
  ast_t* ast;

  if( current( parser ) == TK_VAR )
  {
    ast = ast_token( parser );
    expect( parser, TK_ID, ast, 0 );
    rule( parser, oftype, ast, 1 );
  } else {
    ast = command( parser );
  }

  return ast;
}

static ast_t* assignment( parser_t* parser )
{
  // lvalue (COMMA lvalue)* (ASSIGN expr (COMMA expr)*)?
  ast_t* ast = ast_new( parser, TK_ASSIGN );
  rulelist( parser, lvalue, TK_COMMA, ast, 0 );

  if( accept( parser, TK_ASSIGN, ast, -1 ) )
  {
    rulelist( parser, expr, TK_COMMA, ast, 1 );
  }

  return ast;
}

static ast_t* block( parser_t* parser )
{
  // LBRACE (block | conditional | forloop | whileloop | doloop | match | assignment
  // | RETURN | BREAK | CONTINUE | THROW)* catchblock? always? RBRACE
  static const alt_t alt[] =
  {
    { TK_LBRACE, block },
    { TK_IF, conditional },
    { TK_FOR, forloop },
    { TK_WHILE, whileloop },
    { TK_DO, doloop },
    { TK_MATCH, match },
    { TK_RETURN, ast_token },
    { TK_BREAK, ast_token },
    { TK_CONTINUE, ast_token },
    { TK_THROW, ast_token },

    { TK_VAR, assignment },
    { TK_THIS, assignment },
    { TK_TRUE, assignment },
    { TK_FALSE, assignment },
    { TK_INT, assignment },
    { TK_FLOAT, assignment },
    { TK_STRING, assignment },
    { TK_ID, assignment },
    { TK_TYPEID, assignment },

    { 0, NULL }
  };

  ast_t* ast = ast_new( parser, TK_BLOCK );
  expect( parser, TK_LBRACE, ast, -1 );
  rulealtlist( parser, alt, ast, 0 );

  if( current( parser ) == TK_CATCH )
  {
    rule( parser, catchblock, ast, 1 );
  }

  if( current( parser ) == TK_ALWAYS )
  {
    rule( parser, always, ast, 2 );
  }

  expect( parser, TK_RBRACE, ast, -1 );
  return ast;
}

static ast_t* formalargs( parser_t* parser )
{
  // (LBRACKET arg (COMMA arg)* RBRACKET)?
  ast_t* ast = ast_new( parser, TK_FORMALARGS );

  if( accept( parser, TK_LBRACKET, ast, -1 ) )
  {
    rulelist( parser, arg, TK_COMMA, ast, 0 );
    expect( parser, TK_RBRACKET, ast, -1 );
  }

  return ast;
}

static ast_t* typelambda( parser_t* parser )
{
  // LAMBDA annotation args (RESULTS args) THROWS?
  ast_t* ast = ast_expect( parser, TK_LAMBDA );

  rule( parser, annotation, ast, 0 );
  rule( parser, args, ast, 1 );

  if( accept( parser, TK_RESULTS, ast, -1 ) )
  {
    rule( parser, args, ast, 2 );
  }

  accept( parser, TK_THROWS, ast, 3 );
  return ast;
}

static ast_t* lambda( parser_t* parser )
{
  // LAMBDA annotation args (RESULTS args) THROWS? (IS block)?
  ast_t* ast = typelambda( parser );

  if( accept( parser, TK_IS, ast, -1 ) )
  {
    rule( parser, block, ast, 4 );
  }

  return ast;
}

static ast_t* typeclass( parser_t* parser )
{
  // TYPEID (PACKAGE TYPEID)? annotation formalargs
  ast_t* ast = ast_new( parser, TK_TYPECLASS );

  expect( parser, TK_TYPEID, ast, 0 );

  if( accept( parser, TK_PACKAGE, ast, -1 ) )
  {
    expect( parser, TK_TYPEID, ast, 1 );
  }

  rule( parser, annotation, ast, 2 );
  rule( parser, formalargs, ast, 3 );
  return ast;
}

static ast_t* partialtype( parser_t* parser )
{
  // PARTIAL typeclass
  ast_t* ast = ast_expect( parser, TK_PARTIAL );
  rule( parser, typeclass, ast, 0 );
  return ast;
}

static ast_t* typeelement( parser_t* parser )
{
  // partialtype | typeclass | typelambda
  static const alt_t alt[] =
  {
    { TK_PARTIAL, partialtype },
    { TK_TYPEID, typeclass },
    { TK_LAMBDA, typelambda },
    { 0, NULL }
  };

  ast_t* ast = rulealt( parser, alt );

  if( ast == NULL )
  {
    error_new( parser->errors, parser->t->line, parser->t->pos, "Expected partial type, type or lambda" );
  }

  return ast;
}

static ast_t* oftype( parser_t* parser )
{
  // (OFTYPE typeelement (OR typeelement)*)?
  ast_t* ast = ast_new( parser, TK_OFTYPE );

  if( accept( parser, TK_OFTYPE, ast, -1 ) )
  {
    rulelist( parser, typeelement, TK_OR, ast, 0 );
  }

  return ast;
}

static ast_t* field( parser_t* parser )
{
  // VAR ID oftype (ASSIGN expr)?
  ast_t* ast = ast_new( parser, TK_FIELD );

  expect( parser, TK_VAR, ast, -1 );
  expect( parser, TK_ID, ast, 0 );
  rule( parser, oftype, ast, 1 );

  if( accept( parser, TK_ASSIGN, ast, -1 ) )
  {
    rule( parser, expr, ast, 2 );
  }

  return ast;
}

static ast_t* delegate( parser_t* parser )
{
  // DELEGATE ID oftype
  ast_t* ast = ast_expect( parser, TK_DELEGATE );
  expect( parser, TK_ID, ast, 0 );
  rule( parser, oftype, ast, 1 );
  return ast;
}

static ast_t* constructor( parser_t* parser )
{
  // NEW ID? formalargs args THROWS? block?
  ast_t* ast = ast_expect( parser, TK_NEW );

  accept( parser, TK_ID, ast, 0 );
  rule( parser, formalargs, ast, 1 );
  rule( parser, args, ast, 2 );
  accept( parser, TK_THROWS, ast, 3 );

  if( current( parser ) == TK_LBRACE )
  {
    rule( parser, block, ast, 4 );
  }

  return ast;
}

static ast_t* ambient( parser_t* parser )
{
  // AMBIENT ID? formalargs args THROWS? block?
  ast_t* ast = ast_expect( parser, TK_AMBIENT );

  accept( parser, TK_ID, ast, 0 );
  rule( parser, formalargs, ast, 1 );
  rule( parser, args, ast, 2 );
  accept( parser, TK_THROWS, ast, 3 );

  if( current( parser ) == TK_LBRACE )
  {
    rule( parser, block, ast, 4 );
  }

  return ast;
}

static ast_t* function( parser_t* parser )
{
  // FUNCTION annotation ID? formalargs args (RESULTS args)? THROWS? block?
  ast_t* ast = ast_expect( parser, TK_FUNCTION );

  rule( parser, annotation, ast, 0 );
  accept( parser, TK_ID, ast, 1 );
  rule( parser, formalargs, ast, 2 );
  rule( parser, args, ast, 3 );

  if( accept( parser, TK_RESULTS, ast, -1 ) )
  {
    rule( parser, args, ast, 4 );
  }

  accept( parser, TK_THROWS, ast, 5 );

  if( current( parser ) == TK_LBRACE )
  {
    rule( parser, block, ast, 6 );
  }

  return ast;
}

static ast_t* message( parser_t* parser )
{
  // MESSAGE ID? formalargs args block?
  ast_t* ast = ast_expect( parser, TK_MESSAGE );

  accept( parser, TK_ID, ast, 0 );
  rule( parser, formalargs, ast, 1 );
  rule( parser, args, ast, 2 );

  if( current( parser ) == TK_LBRACE )
  {
    rule( parser, block, ast, 3 );
  }

  return ast;
}

static ast_t* typebody( parser_t* parser )
{
  // LBRACE (field | delegate | constructor | ambient | function | message)* RBRACE
  static const alt_t alt[] =
  {
    { TK_VAR, field },
    { TK_DELEGATE, delegate },
    { TK_NEW, constructor },
    { TK_AMBIENT, ambient },
    { TK_FUNCTION, function },
    { TK_MESSAGE, message },
    { 0, NULL }
  };

  ast_t* ast = ast_new( parser, TK_TYPEBODY );

  expect( parser, TK_LBRACE, ast, -1 );
  rulealtlist( parser, alt, ast, 0 );
  expect( parser, TK_RBRACE, ast, -1 );

  return ast;
}

static ast_t* is( parser_t* parser )
{
  // (IS typeclass (COMMA typeclass)*)?
  ast_t* ast = ast_new( parser, TK_IS );

  if( accept( parser, TK_IS, ast, -1 ) )
  {
    rulelist( parser, typeclass, TK_COMMA, ast, 0 );
  }

  return ast;
}

static ast_t* trait( parser_t* parser )
{
  // TRAIT TYPEID formalargs is typebody
  ast_t* ast = ast_expect( parser, TK_TRAIT );

  expect( parser, TK_TYPEID, ast, 0 );
  rule( parser, formalargs, ast, 1 );
  rule( parser, is, ast, 2 );
  rule( parser, typebody, ast, 3 );

  return ast;
}

static ast_t* object( parser_t* parser )
{
  // OBJECT TYPEID formalargs is typebody
  ast_t* ast = ast_expect( parser, TK_OBJECT );

  expect( parser, TK_TYPEID, ast, 0 );
  rule( parser, formalargs, ast, 1 );
  rule( parser, is, ast, 2 );
  rule( parser, typebody, ast, 3 );

  return ast;
}

static ast_t* actor( parser_t* parser )
{
  // ACTOR TYPEID formalargs is typebody
  ast_t* ast = ast_expect( parser, TK_ACTOR );

  expect( parser, TK_TYPEID, ast, 0 );
  rule( parser, formalargs, ast, 1 );
  rule( parser, is, ast, 2 );
  rule( parser, typebody, ast, 3 );

  return ast;
}

static ast_t* type( parser_t* parser )
{
  // TYPE TYPEID oftype is
  ast_t* ast = ast_expect( parser, TK_TYPE );

  expect( parser, TK_TYPEID, ast, 0 );
  rule( parser, oftype, ast, 1 );
  rule( parser, is, ast, 2 );

  return ast;
}

static ast_t* map( parser_t* parser )
{
  // ID ASSIGN ID
  ast_t* ast = ast_new( parser, TK_MAP );

  expect( parser, TK_ID, ast, 0 );
  expect( parser, TK_ASSIGN, ast, -1 );
  expect( parser, TK_ID, ast, 1 );

  return ast;
}

static ast_t* declaremap( parser_t* parser )
{
  // (LBRACE map (COMMA map)* RBRACE)?
  ast_t* ast = ast_new( parser, TK_DECLAREMAP );

  if( accept( parser, TK_LBRACE, ast, -1 ) )
  {
    rulelist( parser, map, TK_COMMA, ast, 0 );
    expect( parser, TK_RBRACE, ast, -1 );
  }

  return ast;
}

static ast_t* declare( parser_t* parser )
{
  // DECLARE typeclass is declaremap
  ast_t* ast = ast_expect( parser, TK_DECLARE );

  rule( parser, typeclass, ast, 0 );
  rule( parser, is, ast, 1 );
  rule( parser, declaremap, ast, 2 );

  return ast;
}

static ast_t* use( parser_t* parser )
{
  // USE (TYPEID ASSIGN)? STRING
  ast_t* ast = ast_expect( parser, TK_USE );

  if( accept( parser, TK_TYPEID, ast, 0 ) )
  {
    expect( parser, TK_ASSIGN, ast, -1 );
  }

  expect( parser, TK_STRING, ast, 1 );
  return ast;
}

static ast_t* module( parser_t* parser )
{
  // (use | declare | type | trait | object | actor)*
  static const alt_t alt[] =
  {
    { TK_USE, use },
    { TK_DECLARE, declare },
    { TK_TYPE, type },
    { TK_TRAIT, trait },
    { TK_OBJECT, object },
    { TK_ACTOR, actor },
    { 0, NULL }
  };

  ast_t* ast = ast_new( parser, TK_MODULE );
  rulealtlist( parser, alt, ast, 0 );
  expect( parser, TK_EOF, ast, -1 );

  return ast;
}

parser_t* parser_open( const char* file )
{
  // open the lexer
  lexer_t* lexer = lexer_open( file );
  if( lexer == NULL ) { return NULL; }

  // create a parser and attach the lexer
  parser_t* parser = calloc( 1, sizeof(parser_t) );
  parser->lexer = lexer;
  parser->t = lexer_next( lexer );
  parser->errors = errorlist_new();
  parser->ast = module( parser );

  errorlist_t* e = lexer_errors( lexer );

  if( e->count > 0 )
  {
    parser->errors->count += e->count;
    e->tail->next = parser->errors->head;
    parser->errors->head = e->head;
    e->head = NULL;
    e->tail = NULL;
  }

  errorlist_free( e );
  lexer_close( lexer );
  parser->lexer = NULL;

  return parser;
}

ast_t* parser_ast( parser_t* parser )
{
  return parser->ast;
}

errorlist_t* parser_errors( parser_t* parser )
{
  return parser->errors;
}

void parser_close( parser_t* parser )
{
  if( parser == NULL ) { return; }
  ast_free( parser->ast );
  errorlist_free( parser->errors );
  free( parser );
}
