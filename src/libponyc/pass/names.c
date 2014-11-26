#include "names.h"
#include "../type/assemble.h"
#include "../type/alias.h"
#include "../type/cap.h"
#include "../pkg/package.h"
#include <assert.h>

static void names_applycap_index(ast_t* ast, ast_t* cap, ast_t* ephemeral,
  int index)
{
  ast_t* a_cap = ast_childidx(ast, index);
  ast_t* a_ephemeral = ast_sibling(a_cap);

  if(ast_id(cap) != TK_NONE)
    ast_replace(&a_cap, cap);

  if(ast_id(ephemeral) != TK_NONE)
    ast_replace(&a_ephemeral, ephemeral);
}

static bool names_applycap(ast_t* ast, ast_t* cap, ast_t* ephemeral)
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
      names_applycap_index(ast, cap, ephemeral, 3);
      return true;

    case TK_ARROW:
      return names_applycap(ast_childidx(ast, 1), cap, ephemeral);

    default: {}
  }

  assert(0);
  return false;
}

static bool names_resolvealias(ast_t* def, ast_t* type)
{
  ast_state_t state = (ast_state_t)((uint64_t)ast_data(def));

  switch(state)
  {
    case AST_STATE_INITIAL:
      ast_setdata(def, (void*)AST_STATE_INPROGRESS);
      break;

    case AST_STATE_INPROGRESS:
      ast_error(def, "type aliases can't be recursive");
      return false;

    case AST_STATE_DONE:
      return true;

    default:
      assert(0);
      return false;
  }

  if(ast_visit(&type, NULL, pass_names, NULL) != AST_OK)
    return false;

  ast_setdata(def, (void*)AST_STATE_DONE);
  return true;
}

static bool names_typealias(ast_t** astp, ast_t* def)
{
  ast_t* ast = *astp;

  // type aliases can't have type arguments
  ast_t* typeargs = ast_childidx(ast, 2);

  if(ast_id(typeargs) != TK_NONE)
  {
    ast_error(typeargs, "type aliases can't have type arguments");
    return false;
  }

  // make sure the alias is resolved
  ast_t* alias = ast_childidx(def, 1);

  if(!names_resolvealias(def, alias))
    return false;

  // apply our cap and ephemeral to the result
  ast_t* cap = ast_childidx(ast, 3);
  ast_t* ephemeral = ast_sibling(cap);
  alias = ast_dup(alias);

  if(!names_applycap(alias, cap, ephemeral))
  {
    ast_free_unattached(alias);
    return false;
  }

  // replace this with the alias
  ast_replace(astp, alias);
  return true;
}

static bool names_typeparam(ast_t** astp, ast_t* def)
{
  ast_t* ast = *astp;
  ast_t* package = ast_child(ast);
  ast_t* type = ast_sibling(package);
  ast_t* typeargs = ast_sibling(type);
  ast_t* cap = ast_sibling(typeargs);
  ast_t* ephemeral = ast_sibling(cap);

  assert(ast_id(package) == TK_NONE);

  if(ast_id(typeargs) != TK_NONE)
  {
    ast_error(typeargs, "can't qualify a type parameter with type arguments");
    return false;
  }

  if(ast_id(cap) != TK_NONE)
  {
    ast_error(cap, "can't specify a capability on a type parameter");
    return false;
  }

  // Change to a typeparamref.
  ast_t* typeparamref = ast_from(ast, TK_TYPEPARAMREF);
  ast_add(typeparamref, ephemeral);
  ast_add(typeparamref, cap);
  ast_add(typeparamref, type);

  ast_setdata(typeparamref, def);
  ast_replace(astp, typeparamref);

  return true;
}

static bool names_type(typecheck_t* t, ast_t** astp, ast_t* def)
{
  ast_t* ast = *astp;
  ast_t* package = ast_child(ast);
  ast_t* cap = ast_childidx(ast, 3);
  ast_t* defcap;

  // A nominal constraint without a capability is set to tag, otherwise to
  // the default capability for the type.
  if(ast_id(cap) == TK_NONE)
  {
    if(ast_id(def) == TK_PRIMITIVE)
      defcap = ast_from(cap, TK_VAL);
    else if((t != NULL) && (t->frame->constraint != NULL))
      defcap = ast_from(cap, TK_TAG);
    else
      defcap = ast_childidx(def, 2);

    ast_replace(&cap, defcap);
  }

  // Keep the actual package id.
  ast_append(ast, package);
  ast_replace(&package, package_id(def));

  // Store our definition for later use.
  ast_setdata(ast, def);
  return true;
}

bool names_nominal(typecheck_t* t, ast_t* scope, ast_t** astp)
{
  ast_t* ast = *astp;

  if(ast_data(ast) != NULL)
    return true;

  AST_GET_CHILDREN(ast, package_id, type_id);
  bool local_package;

  // find our actual package
  if(ast_id(package_id) != TK_NONE)
  {
    const char* name = ast_name(package_id);

    if(name[0] == '$')
      scope = ast_get(ast_nearest(scope, TK_PROGRAM), name, NULL);
    else
      scope = ast_get(scope, name, NULL);

    if((scope == NULL) || (ast_id(scope) != TK_PACKAGE))
    {
      ast_error(package_id, "can't find package '%s'", name);
      return false;
    }

    local_package = scope == ast_nearest(ast, TK_PACKAGE);
  } else {
    local_package = true;
  }

  // find our definition
  const char* name = ast_name(type_id);
  ast_t* def = ast_get(scope, name, NULL);

  if(def == NULL)
  {
    ast_error(type_id, "can't find definition of '%s'", name);
    return false;
  }

  if(!local_package && (name[0] == '_'))
  {
    ast_error(type_id, "can't access a private type from another package");
    return false;
  }

  switch(ast_id(def))
  {
    case TK_TYPE:
      return names_typealias(astp, def);

    case TK_TYPEPARAM:
      return names_typeparam(astp, def);

    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      return names_type(t, astp, def);

    default: {}
  }

  ast_error(type_id, "definition of '%s' is not a type", name);
  return false;
}

static bool names_arrow(ast_t* ast)
{
  ast_t* left = ast_child(ast);

  switch(ast_id(left))
  {
    case TK_THISTYPE:
    case TK_TYPEPARAMREF:
      return true;

    default: {}
  }

  ast_error(left, "only type parameters and 'this' can be viewpoints");
  return false;
}

static bool names_async(ast_t* ast)
{
  ast_t* params = ast_childidx(ast, 3);
  ast_t* param = ast_child(params);
  bool ok = true;

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, id, type, def);

    if(!sendable(type))
    {
      ast_error(type, "asynchronous methods must have sendable parameters");
      ok = false;
    }

    if(borrowed_type(type))
    {
      ast_error(type, "asynchronous methods cannot have borrowed parameters");
      ok = false;
    }

    param = ast_sibling(param);
  }

  return ok;
}

ast_result_t pass_names(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!names_nominal(t, ast, astp))
        return AST_ERROR;
      break;

    case TK_ARROW:
      if(!names_arrow(ast))
        return AST_ERROR;
      break;

    case TK_NEW:
    {
      if((ast_id(t->frame->type) == TK_ACTOR) && !names_async(ast))
        return AST_ERROR;
      break;
    }

    case TK_BE:
      if(!names_async(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}

ast_result_t flatten_typeparamref(ast_t* ast)
{
  // Get the lowest capability that could fulfill the constraint.
  ast_t* def = (ast_t*)ast_data(ast);

  AST_GET_CHILDREN(def, id, constraint, default_type);
  token_id constraint_cap = cap_for_type(constraint);
  token_id typeparam_cap = cap_typeparam(constraint_cap);

  // Set the typeparamref cap.
  AST_GET_CHILDREN(ast, t_id, t_cap, t_ephemeral);
  ast_setid(t_cap, typeparam_cap);

  return AST_OK;
}

ast_result_t flatten_tuple(typecheck_t* t, ast_t* ast)
{
  if(t->frame->constraint != NULL)
  {
    ast_error(ast, "tuple types can't be used as constraints");
    return AST_ERROR;
  }

  return AST_OK;
}

ast_result_t pass_flatten(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
      if(!flatten_union(astp))
        return AST_ERROR;
      break;

    case TK_ISECTTYPE:
      if(!flatten_isect(astp))
        return AST_ERROR;
      break;

    case TK_TUPLETYPE:
      return flatten_tuple(t, ast);

    case TK_TYPEPARAMREF:
      return flatten_typeparamref(ast);

    default: {}
  }

  return AST_OK;
}
