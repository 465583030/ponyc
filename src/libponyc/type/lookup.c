#include "lookup.h"
#include "assemble.h"
#include "reify.h"
#include "../ast/token.h"
#include "viewpoint.h"
#include <assert.h>

static ast_t* lookup_base(ast_t* orig, ast_t* type, const char* name);

static ast_t* lookup_nominal(ast_t* orig, ast_t* type, const char* name)
{
  assert(ast_id(type) == TK_NOMINAL);
  ast_t* def = ast_data(type);
  ast_t* typename = ast_child(def);
  ast_t* find = ast_get(def, name);

  if(find != NULL)
  {
    find = ast_dup(find);
    flatten_thistype(&find, orig);

    ast_t* typeparams = ast_sibling(typename);
    ast_t* typeargs = ast_childidx(type, 2);
    find = reify(find, typeparams, typeargs);
  } else {
    ast_error(type, "couldn't find '%s' in '%s'", name, ast_name(typename));
  }

  return find;
}

static ast_t* lookup_structural(ast_t* type, const char* name)
{
  assert(ast_id(type) == TK_STRUCTURAL);
  ast_t* find = ast_get(type, name);

  if(find == NULL)
    ast_error(type, "couldn't find '%s'", name);

  return find;
}

static ast_t* lookup_typeparam(ast_t* orig, ast_t* type, const char* name)
{
  ast_t* def = ast_data(type);
  ast_t* constraint = ast_childidx(def, 1);

  // lookup on the constraint instead
  return lookup_base(orig, constraint, name);
}

static ast_t* lookup_base(ast_t* orig, ast_t* type, const char* name)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      ast_error(type, "can't lookup by name on a union type");
      return NULL;

    case TK_ISECTTYPE:
      ast_error(type, "can't lookup by name on an intersection type");
      return NULL;

    case TK_TUPLETYPE:
      ast_error(type, "can't lookup by name on a tuple");
      return NULL;

    case TK_NOMINAL:
      return lookup_nominal(orig, type, name);

    case TK_STRUCTURAL:
      return lookup_structural(type, name);

    case TK_ARROW:
      return lookup_base(orig, ast_childidx(type, 1), name);

    case TK_TYPEPARAMREF:
      return lookup_typeparam(orig, type, name);

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* lookup(ast_t* type, const char* name)
{
  return lookup_base(type, type, name);
}

ast_t* lookup_tuple(ast_t* ast, int index)
{
  assert(ast_id(ast) == TK_TUPLETYPE);

  while(index > 1)
  {
    ast_t* right = ast_childidx(ast, 1);

    if(ast_id(right) != TK_TUPLETYPE)
      return NULL;

    index--;
    ast = right;
  }

  if(index == 0)
    return ast_child(ast);

  ast = ast_childidx(ast, 1);

  if(ast_id(ast) == TK_TUPLETYPE)
    return ast_child(ast);

  return ast;
}
