#include "scope.h"
#include "../ast/token.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include "../ds/stringtab.h"
#include <assert.h>

static bool is_type_id(const char* s)
{
  int i = 0;

  if(s[i] == '_')
    i++;

  return (s[i] >= 'A') && (s[i] <= 'Z');
}

/**
 * Insert a String->AST mapping into the specified scope. The string is the
 * string representation of the token of the name ast.
 */
static bool set_scope(ast_t* scope, ast_t* name, ast_t* value)
{
  assert(ast_id(name) == TK_ID);
  const char* s = ast_name(name);

  sym_status_t status = SYM_NONE;
  bool type = false;

  switch(ast_id(value))
  {
    case TK_ID:
    {
      ast_t* idseq = ast_parent(value);
      ast_t* decl = ast_parent(idseq);

      switch(ast_id(decl))
      {
        case TK_VAR:
        case TK_LET:
          status = SYM_UNDEFINED;
          break;

        case TK_AS:
          status = SYM_DEFINED;
          break;

        default:
          assert(0);
          return false;
      }

      break;
    }

    case TK_TYPE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    case TK_TYPEPARAM:
      type = true;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      status = SYM_DEFINED;
      break;

    case TK_PACKAGE:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      break;

    default:
      assert(0);
      return false;
  }

  if(type)
  {
    if(!is_type_id(s))
    {
      ast_error(name, "type name '%s' must start A-Z or _(A-Z)", s);
      return false;
    }
  } else {
    if(is_type_id(s))
    {
      ast_error(name, "identifier '%s' can't start A-Z or _(A-Z)", s);
      return false;
    }
  }

  if(!ast_set(scope, s, value, status))
  {
    ast_error(name, "can't reuse name '%s'", s);

    ast_t* prev = ast_get(scope, s, NULL);

    if(prev != NULL)
      ast_error(prev, "previous use of '%s'", s);

    return false;
  }

  return true;
}

/**
 * Import a package, either with a qualifying name or by merging it into the
 * current scope.
 */
static ast_t* use_package(ast_t* ast, ast_t* name, const char* path)
{
  ast_t* package = package_load(ast, path);

  if(package == ast)
    return package;

  if(package == NULL)
  {
    ast_error(ast, "can't load package '%s'", path);
    return NULL;
  }

  if(name && ast_id(name) == TK_ID)
  {
    if(!set_scope(ast, name, package))
      return NULL;

    return package;
  }

  assert((name == NULL) || (ast_id(name) == TK_NONE));

  if(!ast_merge(ast, package))
  {
    ast_error(ast, "can't merge symbols from '%s'", path);
    return NULL;
  }

  return package;
}

static bool scope_package(ast_t* ast)
{
  return use_package(ast, NULL, stringtab("builtin")) != NULL;
}

static bool scope_method(ast_t* ast)
{
  if(!set_scope(ast_parent(ast), ast_childidx(ast, 1), ast))
    return false;

  // If this isn't a constructor, we accept SYM_DEFINED for our fields.
  if(ast_id(ast) != TK_NEW)
    return true;

  ast_t* members = ast_parent(ast);
  ast_t* type = ast_parent(members);

  switch(ast_id(type))
  {
    case TK_PRIMITIVE:
    case TK_TRAIT:
      return true;

    case TK_CLASS:
    case TK_ACTOR:
      break;

    default:
      assert(0);
      return false;
  }

  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        AST_GET_CHILDREN(member, id, type, expr);

        // If this field has an initialiser, we accept SYM_DEFINED for it.
        if(ast_id(expr) != TK_NONE)
          break;

        // Mark this field as SYM_UNDEFINED.
        ast_setstatus(ast, ast_name(id), SYM_UNDEFINED);
        break;
      }

      default:
        return true;
    }

    member = ast_sibling(member);
  }

  return true;
}

static bool scope_use(ast_t* ast)
{
  ast_t* path = ast_child(ast);
  ast_t* name = ast_sibling(path);

  return use_package(ast, name, ast_name(path)) != NULL;
}

static bool scope_idseq(ast_t* ast)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    // each ID resolves to itself
    if(!set_scope(ast_parent(ast), child, child))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

ast_result_t pass_scope(ast_t** astp)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_PACKAGE:
      if(!scope_package(ast))
        return AST_ERROR;
      break;

    case TK_USE:
      if(!scope_use(ast))
        return AST_FATAL;
      break;

    case TK_TYPE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      if(!set_scope(ast_nearest(ast, TK_PACKAGE), ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      if(!set_scope(ast, ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      if(!scope_method(ast))
        return AST_ERROR;
      break;

    case TK_TYPEPARAM:
      if(!set_scope(ast, ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_IDSEQ:
      if(!scope_idseq(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}
