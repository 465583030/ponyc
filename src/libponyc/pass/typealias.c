#include "typealias.h"
#include "../type/nominal.h"
#include "../ds/stringtab.h"
#include <assert.h>

typedef enum
{
  TYPEALIAS_INITIAL = 0,
  TYPEALIAS_IN_PROGRESS,
  TYPEALIAS_DONE
} typealias_state_t;

static bool typealias_applycap(ast_t* ast, ast_t* cap, ast_t* ephemeral)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      if(ast_id(cap) != TK_NONE)
      {
        ast_error(cap,
          "can't specify a capability for an alias to a type expression");
        return false;
      }

      if(ast_id(ephemeral) != TK_NONE)
      {
        ast_error(ephemeral,
          "can't specify ephemerality for an alias to a type expression");
        return false;
      }

      return true;
    }

    case TK_NOMINAL:
    {
      ast_t* a_cap = ast_childidx(ast, 3);
      ast_t* a_ephemeral = ast_sibling(a_cap);

      if(ast_id(cap) != TK_NONE)
        ast_replace(&a_cap, cap);

      if(ast_id(ephemeral) != TK_NONE)
        ast_replace(&a_ephemeral, ephemeral);

      return true;
    }

    case TK_STRUCTURAL:
    {
      ast_t* a_cap = ast_childidx(ast, 1);
      ast_t* a_ephemeral = ast_sibling(a_cap);

      if(ast_id(cap) != TK_NONE)
        ast_replace(&a_cap, cap);

      if(ast_id(ephemeral) != TK_NONE)
        ast_replace(&a_ephemeral, ephemeral);

      return true;
    }

    case TK_THISTYPE:
      return true;

    case TK_ARROW:
      return typealias_applycap(ast_childidx(ast, 1), cap, ephemeral);

    default: {}
  }

  assert(0);
  return false;
}

static bool typealias_alias(ast_t* ast)
{
  typealias_state_t state = (typealias_state_t)ast_data(ast);

  switch(state)
  {
    case TYPEALIAS_INITIAL:
      ast_setdata(ast, (void*)TYPEALIAS_IN_PROGRESS);
      break;

    case TYPEALIAS_IN_PROGRESS:
      ast_error(ast, "type aliases can't be recursive");
      return false;

    case TYPEALIAS_DONE:
      return true;

    default:
      assert(0);
      return false;
  }

  if(ast_visit(ast, NULL, pass_typealias) != AST_OK)
    return false;

  ast_setdata(ast, (void*)TYPEALIAS_DONE);
  return true;
}

static bool typealias_nominal(ast_t* ast)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = nominal_def(ast, ast);

  // look for type aliases
  if(ast_id(def) != TK_TYPE)
    return true;

  // TODO: type aliases can't have type parameters

  // make sure the alias is resolved
  ast_t* alias = ast_childidx(def, 3);

  if(!typealias_alias(alias))
    return false;

  // apply our cap and ephemeral to the result
  ast_t* cap = ast_childidx(ast, 3);
  ast_t* ephemeral = ast_sibling(cap);
  alias = ast_dup(ast_child(alias));

  if(!typealias_applycap(alias, cap, ephemeral))
  {
    ast_free_unattached(alias);
    return false;
  }

  // replace this with the alias
  ast_replace(&ast, alias);
  return true;
}

ast_result_t pass_typealias(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!typealias_nominal(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}
