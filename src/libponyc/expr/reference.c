#include "reference.h"
#include "literal.h"
#include "postfix.h"
#include "../pass/expr.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/alias.h"
#include "../type/viewpoint.h"
#include "../ast/token.h"
#include <assert.h>

/**
 * Make sure the definition of something occurs before its use. This is for
 * both fields and local variable.
 */
static bool def_before_use(ast_t* def, ast_t* use, const char* name)
{
  if((ast_line(def) > ast_line(use)) ||
     ((ast_line(def) == ast_line(use)) &&
      (ast_pos(def) > ast_pos(use))))
  {
    ast_error(use, "declaration of '%s' appears after use", name);
    ast_error(def, "declaration of '%s' appears here", name);
    return false;
  }

  return true;
}

static bool is_assigned_to(ast_t* ast)
{
  ast_t* parent = ast_parent(ast);

  switch(ast_id(parent))
  {
    case TK_ASSIGN:
    {
      // Has to be the left hand side of an assignment. Left and right sides
      // are swapped, so we must be the second child.
      if(ast_childidx(parent, 1) != ast)
        return false;

      // The result of that assignment can't be used.
      return !is_result_needed(parent);
    }

    case TK_SEQ:
    {
      // Might be in a tuple on the left hand side.
      if(ast_childcount(parent) > 1)
        return false;

      return is_assigned_to(parent);
    }

    case TK_TUPLE:
      return is_assigned_to(parent);

    default: {}
  }

  return false;
}

static bool valid_reference(ast_t* ast, sym_status_t status)
{
  switch(status)
  {
    case SYM_DEFINED:
      return true;

    case SYM_CONSUMED:
      ast_error(ast, "can't use a consumed local in an expression");
      return false;

    case SYM_UNDEFINED:
      if(is_assigned_to(ast))
        return true;

      ast_error(ast, "can't use an undefined local in an expression");
      return false;

    default: {}
  }

  assert(0);
  return false;
}

bool expr_field(ast_t* ast)
{
  ast_t* type = ast_childidx(ast, 1);
  ast_t* init = ast_sibling(type);

  if(ast_id(init) != TK_NONE)
  {
    // initialiser type must match declared type
    if(!coerce_literals(init, type, NULL))
      return false;

    ast_t* init_type = alias(ast_type(init));
    bool ok = is_subtype(init_type, type);
    ast_free_unattached(init_type);

    if(!ok)
    {
      ast_error(init,
        "field/param initialiser is not a subtype of the field/param type");
      return false;
    }
  }

  ast_settype(ast, type);
  return true;
}

bool expr_fieldref(ast_t* ast, ast_t* left, ast_t* find, token_id t)
{
  // Attach the type.
  ast_t* type = ast_childidx(find, 1);
  ast_settype(find, type);

  // Viewpoint adapted type of the field.
  ast_t* ftype = viewpoint(left, find);

  if(ftype == NULL)
  {
    ast_error(ast, "can't read a field from a tag");
    return false;
  }

  if(ast_id(left) == TK_THIS)
  {
    // Handle symbol status if the left side is 'this'.
    ast_t* id = ast_child(find);
    const char* name = ast_name(id);

    sym_status_t status;
    ast_get(ast, name, &status);

    if(!valid_reference(ast, status))
      return false;
  }

  ast_setid(ast, t);
  ast_settype(ast, ftype);
  return true;
}

bool expr_typeref(ast_t* ast)
{
  assert(ast_id(ast) == TK_TYPEREF);
  ast_t* type = ast_type(ast);

  switch(ast_id(ast_parent(ast)))
  {
    case TK_QUALIFY:
      // doesn't have to be valid yet
      break;

    case TK_DOT:
      // has to be valid
      return expr_nominal(&type);

    case TK_CALL:
    {
      // has to be valid
      if(!expr_nominal(&type))
        return false;

      // transform to a default constructor
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(ast, dot);
      ast_add(dot, ast);

      return expr_dot(dot);
    }

    default:
    {
      // has to be valid
      if(!expr_nominal(&type))
        return false;

      // transform to a default constructor
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(ast, dot);
      ast_add(dot, ast);

      if(!expr_dot(dot))
        return false;

      // call the default constructor with no arguments
      ast_t* call = ast_from(ast, TK_CALL);
      ast_swap(dot, call);
      ast_add(call, dot); // receiver comes last
      ast_add(call, ast_from(ast, TK_NONE)); // named args
      ast_add(call, ast_from(ast, TK_NONE)); // positional args

      return expr_call(call);
    }
  }

  return true;
}

bool expr_reference(ast_t* ast)
{
  // Everything we reference must be in scope.
  const char* name = ast_name(ast_child(ast));

  sym_status_t status;
  ast_t* def = ast_get(ast, name, &status);

  if(def == NULL)
  {
    ast_error(ast, "can't find declaration of '%s'", name);
    return false;
  }

  switch(ast_id(def))
  {
    case TK_PACKAGE:
    {
      // Only allowed if in a TK_DOT with a type.
      if(ast_id(ast_parent(ast)) != TK_DOT)
      {
        ast_error(ast, "a package can only appear as a prefix to a type");
        return false;
      }

      ast_setid(ast, TK_PACKAGEREF);
      return true;
    }

    case TK_INTERFACE:
    {
      ast_error(ast, "can't use an interface in an expression");
      return false;
    }

    case TK_TRAIT:
    {
      ast_error(ast, "can't use a trait in an expression");
      return false;
    }

    case TK_TYPE:
    {
      ast_error(ast, "can't use a type alias in an expression");
      return false;
    }

    case TK_TYPEPARAM:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    {
      // It's a type name. This may not be a valid type, since it may need
      // type arguments.
      ast_t* id = ast_child(def);
      const char* name = ast_name(id);
      ast_t* type = type_sugar(ast, NULL, name);
      ast_settype(ast, type);
      ast_setid(ast, TK_TYPEREF);

      return expr_typeref(ast);
    }

    case TK_FVAR:
    case TK_FLET:
    {
      // Transform to "this.f".
      if(!def_before_use(def, ast, name))
        return false;

      ast_t* dot = ast_from(ast, TK_DOT);
      ast_swap(ast, dot);
      ast_add(dot, ast_child(ast));

      ast_t* self = ast_from(ast, TK_THIS);
      ast_add(dot, self);
      ast_free(ast);

      if(!expr_this(self))
        return false;

      return expr_dot(dot);
    }

    case TK_PARAM:
    {
      if(!def_before_use(def, ast, name))
        return false;

      if(!valid_reference(ast, status))
        return false;

      ast_t* type = ast_type(def);

      if(!sendable(type) && (ast_nearest(ast, TK_RECOVER) != NULL))
      {
        ast_error(ast,
          "can't access a non-sendable parameter from inside a recover "
          "expression");
        return false;
      }

      // Get the type of the parameter and attach it to our reference.
      ast_settype(ast, type);
      ast_setid(ast, TK_PARAMREF);
      return true;
    }

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      // Transform to "this.f".
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_swap(ast, dot);
      ast_add(dot, ast_child(ast));
      ast_free(ast);

      ast_t* self = ast_from(ast, TK_THIS);
      ast_add(dot, self);

      if(!expr_this(self))
        return false;

      return expr_dot(dot);
    }

    case TK_ID:
    {
      if(!def_before_use(def, ast, name))
        return false;

      if(!valid_reference(ast, status))
        return false;

      ast_t* idseq = ast_parent(def);
      ast_t* var = ast_parent(idseq);
      assert(ast_id(idseq) == TK_IDSEQ);

      switch(ast_id(var))
      {
        case TK_VAR:
          ast_setid(ast, TK_VARREF);
          break;

        case TK_LET:
          ast_setid(ast, TK_LETREF);
          break;

        default:
          assert(0);
          return false;
      }

      ast_t* type = ast_type(def);

      if(!sendable(type))
      {
        ast_t* recover = ast_nearest(ast, TK_RECOVER);

        if(recover != NULL)
        {
          ast_t* def_recover = ast_nearest(def, TK_RECOVER);

          if(recover != def_recover)
          {
            ast_error(ast, "can't access a non-sendable local defined outside "
              "of a recover expression from within that recover expression");
            return false;
          }
        }
      }

      // Get the type of the local and attach it to our reference.
      ast_settype(ast, type);
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

bool expr_local(ast_t* ast)
{
  ast_t* idseq = ast_child(ast);
  ast_settype(ast, ast_type(idseq));

  if((ast_id(ast) == TK_LET) &&
    (ast_id(ast_parent(ast)) != TK_ASSIGN) &&
    (ast_enclosing_pattern(ast) == NULL)
    )
  {
    ast_error(ast, "can't declare a let local without assigning to it");
    return false;
  }

  return true;
}

bool expr_idseq(ast_t* ast)
{
  assert(ast_id(ast) == TK_IDSEQ);
  ast_t* type = ast_sibling(ast);

  if(ast_id(type) == TK_NONE)
  {
    ast_t* parent = ast_parent(ast);

    switch(ast_id(parent))
    {
      case TK_VAR:
      case TK_LET:
        if(ast_id(ast_parent(parent)) == TK_ASSIGN)
          return true;
        break;

      default: {}
    }

    ast_error(ast, "locals must specify a type or be assigned something");
    return false;
  }

  ast_settype(ast, type);
  return type_for_idseq(ast, type);
}
