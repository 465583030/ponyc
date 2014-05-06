#include "ast.h"
#include "symtab.h"
#include "types.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct ast_t
{
  token_t* t;
  table_t* symtab;
  void* data;

  struct ast_t* parent;
  struct ast_t* child;
  struct ast_t* sibling;
};

static const char in[] = " ";
static const size_t in_len = 1;

void print( ast_t* ast, size_t indent, size_t width );

size_t length( ast_t* ast, size_t indent )
{
  size_t len = (indent * in_len) + strlen( token_string( ast->t ) );
  ast_t* child = ast->child;

  if( child != NULL ) { len += 2; }

  while( child != NULL )
  {
    len += 1 + length( child, indent );
    child = child->sibling;
  }

  return len;
}

void print_compact( ast_t* ast, size_t indent )
{
  for( size_t i = 0; i < indent; i++ ) { printf( in ); }
  ast_t* child = ast->child;
  bool parens = child != NULL;

  if( parens ) { printf( "(" ); }
  printf( "%s", token_string( ast->t ) );

  while( child != NULL )
  {
    printf( " " );
    print_compact( child, 0 );
    child = child->sibling;
  }

  if( parens ) { printf( ")" ); }
}

void print_extended( ast_t* ast, size_t indent, size_t width )
{
  for( size_t i = 0; i < indent; i++ ) { printf( in ); }
  ast_t* child = ast->child;
  bool parens = child != NULL;

  if( parens ) { printf( "(" ); }
  printf( "%s\n", token_string( ast->t ) );

  while( child != NULL )
  {
    print( child, indent + 1, width );
    child = child->sibling;
  }

  if( parens )
  {
    for( size_t i = 0; i <= indent; i++ ) { printf( in ); }
    printf( ")" );
  }
}

void print( ast_t* ast, size_t indent, size_t width )
{
  size_t len = length( ast, indent );

  if( len <= width )
  {
    print_compact( ast, indent );
  } else {
    print_extended( ast, indent, width );
  }

  printf( "\n" );
}

ast_t* ast_new( token_id id, size_t line, size_t pos, void* data )
{
  ast_t* ast = ast_token( token_new( id, line, pos ) );
  ast->data = data;
  return ast;
}

ast_t* ast_newid( token_id id )
{
  return ast_token( token_new( id, 0, 0 ) );
}

ast_t* ast_token( token_t* t )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = t;
  return ast;
}

void ast_attach( ast_t* ast, void* data )
{
  ast->data = data;
}

void ast_scope( ast_t* ast )
{
  ast->symtab = symtab_new();
}

token_id ast_id( ast_t* ast )
{
  return ast->t->id;
}

size_t ast_line( ast_t* ast )
{
  return ast->t->line;
}

size_t ast_pos( ast_t* ast )
{
  return ast->t->pos;
}

void* ast_data( ast_t* ast )
{
  return ast->data;
}

const char* ast_name( ast_t* ast )
{
  return token_string( ast->t );
}

ast_t* ast_nearest( ast_t* ast, token_id id )
{
  while( (ast != NULL) && (ast->t->id != id) )
  {
    ast = ast->parent;
  }

  return ast;
}

ast_t* ast_parent( ast_t* ast )
{
  return ast->parent;
}

ast_t* ast_child( ast_t* ast )
{
  return ast->child;
}

ast_t* ast_childidx( ast_t* ast, size_t idx )
{
  ast_t* child = ast->child;

  while( (child != NULL) && (idx > 0) )
  {
    child = child->sibling;
    idx--;
  }

  return child;
}

ast_t* ast_sibling( ast_t* ast )
{
  return ast->sibling;
}

size_t ast_index( ast_t* ast )
{
  ast_t* child = ast->parent->child;
  size_t idx = 0;

  while( child != ast )
  {
    child = child->sibling;
    idx++;
  }

  return idx;
}

size_t ast_childcount( ast_t* ast )
{
  ast_t* child = ast->child;
  size_t count = 0;

  while( child != NULL )
  {
    child = child->sibling;
    count++;
  }

  return count;
}

void* ast_get( ast_t* ast, const char* name )
{
  /* searches all parent scopes, but not the program scope, because the name
   * space for paths is separate from the name space for all other IDs.
   * if called directly on the program scope, searches it.
   */
  do
  {
    if( ast->symtab != NULL )
    {
      void* value = symtab_get( ast->symtab, name );

      if( value != NULL )
      {
        return value;
      }
    }

    ast = ast->parent;
  } while( (ast != NULL) && (ast->t->id != TK_PROGRAM) );

  return NULL;
}

bool ast_set( ast_t* ast, const char* name, void* value )
{
  while( ast->symtab == NULL )
  {
    ast = ast->parent;
  }

  return (ast_get( ast, name ) == NULL)
    && symtab_add( ast->symtab, name, value );
}

bool ast_merge( ast_t* dst, ast_t* src )
{
  while( dst->symtab == NULL )
  {
    dst = dst->parent;
  }

  return table_merge( dst->symtab, src->symtab );
}

void ast_add( ast_t* parent, ast_t* child )
{
  assert( parent != child );
  assert( parent->child != child );
  child->parent = parent;
  child->sibling = parent->child;
  parent->child = child;
}

ast_t* ast_pop( ast_t* parent )
{
  ast_t* child = parent->child;

  if( child != NULL )
  {
    parent->child = child->sibling;
    child->parent = NULL;
    child->sibling = NULL;
  }

  return child;
}

void ast_append( ast_t* parent, ast_t* child )
{
  assert( parent != child );
  child->parent = parent;

  if( parent->child == NULL )
  {
    parent->child = child;
    return;
  }

  ast_t* ast = parent->child;
  while( ast->sibling != NULL ) { ast = ast->sibling; }
  ast->sibling = child;
}

void ast_reverse( ast_t* ast )
{
  if( ast == NULL ) { return; }

  ast_t* cur = ast->child;
  ast_t* next;
  ast_t* last = NULL;

  while( cur != NULL )
  {
    ast_reverse( cur );
    next = cur->sibling;
    cur->sibling = last;
    last = cur;
    cur = next;
  }

  ast->child = last;
}

void ast_print( ast_t* ast, size_t width )
{
  print( ast, 0, width );
  printf( "\n" );
}

void ast_free( ast_t* ast )
{
  if( ast == NULL ) { return; }

  ast_t* child = ast->child;
  ast_t* next;

  while( child != NULL )
  {
    next = child->sibling;
    ast_free( child );
    child = next;
  }

  switch( ast->t->id )
  {
    case TK_MODULE:
      source_close( ast->data );
      break;

    default:
      break;
  }

  token_free( ast->t );
  table_free( ast->symtab );
}

void ast_error( ast_t* ast, const char* fmt, ... )
{
  ast_t* module = ast_nearest( ast, TK_MODULE );
  source_t* source = (module != NULL) ? module->data : NULL;

  va_list ap;
  va_start( ap, fmt );
  errorv( source, ast->t->line, ast->t->pos, fmt, ap );
  va_end( ap );
}

bool ast_visit( ast_t* ast, ast_visit_t pre, ast_visit_t post )
{
  bool ret = true;
  if( pre != NULL ) { ret &= pre( ast ); }

  ast_t* child = ast->child;

  while( child != NULL )
  {
    ret &= ast_visit( child, pre, post );
    child = child->sibling;
  }

  if( post != NULL ) { ret &= post( ast ); }

  return ret;
}
