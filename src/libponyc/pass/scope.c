#include "scope.h"
#include "../ast/token.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include "../pkg/use.h"
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
 * Insert a name->AST mapping into the specified scope.
 */
static bool set_scope(ast_t* scope, ast_t* name, ast_t* value, bool dups)
{
  assert(ast_id(name) == TK_ID);
  const char* s = ast_name(name);

  sym_status_t status = SYM_NONE;
  bool is_type = false;
  bool is_id = false;

  switch(ast_id(value))
  {
    case TK_ID:
    {
      if(ast_enclosing_pattern(value) != NULL)
        status = SYM_DEFINED;
      else
        status = SYM_UNDEFINED;

      is_id = true;
      break;
    }

    case TK_FFIDECL:
      break;

    case TK_TYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    case TK_TYPEPARAM:
      is_type = true;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      status = SYM_DEFINED;
      is_id = true;
      break;

    case TK_PACKAGE:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      is_id = true;
      break;

    default:
      assert(0);
      return false;
  }

  if(is_type && !is_type_id(s))
  {
    ast_error(name, "type name '%s' must start A-Z or _(A-Z)", s);
    return false;
  }

  if(is_id && is_type_id(s))
  {
    ast_error(name, "identifier '%s' can't start A-Z or _(A-Z)", s);
    return false;
  }

  if(!ast_set(scope, s, value, status) && !dups)
  {
    ast_error(name, "can't reuse name '%s'", s);

    ast_t* prev = ast_get(scope, s, NULL);

    if(prev != NULL)
      ast_error(prev, "previous use of '%s'", s);

    return false;
  }

  return true;
}

bool use_package(ast_t* ast, const char* path, ast_t* name, pass_opt_t* options)
{
  assert(ast != NULL);
  assert(path != NULL);

  ast_t* package = package_load(ast, path, options);

  if(package == ast)  // Stop builtin recursing
    return true;

  if(package == NULL)
  {
    ast_error(ast, "can't load package '%s'", path);
    return false;
  }

  if(name != NULL && ast_id(name) == TK_ID) // We have an alias
    return set_scope(ast, name, package, false);

  // We do not have an alias
  if(!ast_merge(ast, package))
  {
    ast_error(ast, "can't merge symbols from '%s'", path);
    return false;
  }

  return true;
}

static bool scope_module(ast_t* ast, pass_opt_t* options)
{
  // Every module has an implicit use "builtin" command
  return use_package(ast, stringtab("builtin"), NULL, options);
}

static void set_fields_undefined(ast_t* ast)
{
  assert(ast_id(ast) == TK_NEW);

  ast_t* members = ast_parent(ast);
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

      default: {}
    }

    member = ast_sibling(member);
  }
}

static bool scope_method(ast_t* ast, bool dups)
{
  ast_t* id = ast_childidx(ast, 1);

  if(!set_scope(ast_parent(ast), id, ast, dups))
    return false;

  if(ast_id(ast) == TK_NEW)
    set_fields_undefined(ast);

  return true;
}

static bool scope_idseq(ast_t* ast, bool dups)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    // Each ID resolves to itself.
    if(!set_scope(ast_parent(ast), child, child, dups))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

static ast_result_t do_scope(ast_t** astp, pass_opt_t* options, bool dups)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_MODULE:
      if(!scope_module(ast, options))
        return AST_FATAL;
      break;

    case TK_USE:
      if(!use_command(ast, options))
        return AST_FATAL;
      break;

    case TK_TYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    case TK_FFIDECL:
      if(!set_scope(ast_nearest(ast, TK_PACKAGE), ast_child(ast), ast, dups))
        return AST_ERROR;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      if(!set_scope(ast, ast_child(ast), ast, dups))
        return AST_ERROR;
      break;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      if(!scope_method(ast, dups))
        return AST_ERROR;
      break;

    case TK_TYPEPARAM:
      if(!set_scope(ast, ast_child(ast), ast, dups))
        return AST_ERROR;
      break;

    case TK_IDSEQ:
      if(!scope_idseq(ast, dups))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}

ast_result_t pass_scope(ast_t** astp, pass_opt_t* options)
{
  return do_scope(astp, options, false);
}

ast_result_t pass_scope2(ast_t** astp, pass_opt_t* options)
{
  return do_scope(astp, options, true);
}
