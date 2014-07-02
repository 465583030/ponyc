#include "postfix.h"
#include "reference.h"
#include "../type/nominal.h"
#include "../type/lookup.h"
#include "../type/cap.h"
#include "../ds/stringtab.h"
#include <assert.h>

bool expr_qualify(ast_t* ast)
{
  // left is a postfix expression, right is a typeargs
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  assert(ast_id(right) == TK_TYPEARGS);

  switch(ast_id(left))
  {
    case TK_TYPEREF:
    {
      // qualify the type
      ast_t* type = ast_type(left);
      assert(ast_id(type) == TK_NOMINAL);

      if(ast_id(ast_childidx(type, 2)) != TK_NONE)
      {
        ast_error(ast, "can't qualify an already qualified type");
        return false;
      }

      type = ast_dup(type);
      ast_t* typeargs = ast_childidx(type, 2);
      ast_replace(typeargs, right);
      ast_settype(ast, type);
      ast_setid(ast, TK_TYPEREF);

      return expr_typeref(ast);
    }

    case TK_FUNREF:
    {
      // TODO: qualify the function
      ast_error(ast, "not implemented (qualify a function)");
      return false;
    }

    default: {}
  }

  assert(0);
  return false;
}

bool expr_dot(ast_t* ast)
{
  // left is a postfix expression, right is an integer or an id
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  switch(ast_id(right))
  {
    case TK_ID:
    {
      switch(ast_id(left))
      {
        case TK_PACKAGEREF:
        {
          // must be a type in a package
          const char* package_name = ast_name(ast_child(left));
          ast_t* package = ast_get(left, package_name);

          if(package == NULL)
          {
            ast_error(right, "can't find package '%s'", package_name);
            return false;
          }

          assert(ast_id(package) == TK_PACKAGE);
          const char* typename = ast_name(right);
          type = ast_get(package, typename);

          if(type == NULL)
          {
            ast_error(right, "can't find type '%s' in package '%s'",
              typename, package_name);
            return false;
          }

          ast_settype(ast, nominal_type(ast, package_name, typename));
          ast_setid(ast, TK_TYPEREF);
          return expr_typeref(ast);
        }

        case TK_TYPEREF:
        {
          // TODO: constructor on a type
          ast_error(ast, "not implemented (constructor of type)");
          return false;
        }

        default: {}
      }

      // TODO: constructor, field or method access
      ast_t* find = lookup(ast, type, ast_name(right));

      if(find == NULL)
        return false;

      ast_error(ast, "found %s", ast_name(right));
      return false;
    }

    case TK_INT:
    {
      // element of a tuple
      if((type == NULL) || (ast_id(type) != TK_TUPLETYPE))
      {
        ast_error(right, "member by position can only be used on a tuple");
        return false;
      }

      type = tuple_index(type, ast_int(right));

      if(type == NULL)
      {
        ast_error(right, "tuple index is out of bounds");
        return false;
      }

      ast_settype(ast, type);
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

bool expr_call(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* type = ast_type(left);

  switch(ast_id(left))
  {
    case TK_INT:
    case TK_FLOAT:
    case TK_STRING:
    case TK_ARRAY:
    case TK_OBJECT:
    case TK_THIS:
    case TK_FIELDREF:
    case TK_PARAMREF:
    case TK_LOCALREF:
    {
      // apply sugar
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, stringtab("apply")));
      ast_swap(left, dot);
      ast_add(dot, left);

      if(!expr_dot(dot))
        return false;

      return expr_call(ast);
    }

    case TK_FUNREF:
    {
      assert((ast_id(type) == TK_NEW) ||
        (ast_id(type) == TK_BE) ||
        (ast_id(type) == TK_FUN)
        );

      // first check if the receiver capability is ok
      token_id rcap = cap_for_receiver(ast);
      token_id fcap = cap_for_fun(type);

      if(!is_cap_sub_cap(rcap, fcap))
      {
        ast_error(ast,
          "receiver capability is not a subtype of method capability");
        return false;
      }

      // TODO: use args to decide unbound type parameters
      // TODO: mark enclosing as "may error" if we might error
      // TODO: generate return type for constructors and behaviours
      ast_settype(ast, ast_childidx(type, 4));
      return true;
    }

    case TK_TYPEREF:
      // shouldn't happen, should be handled in expr_typeref
      break;

    case TK_TUPLE:
    {
      ast_error(ast, "can't call a tuple");
      return false;
    }

    case TK_DOT:
    case TK_QUALIFY:
    case TK_CALL:
    {
      // TODO: function call
      ast_error(ast, "not implemented (function call)");
      return false;
    }

    default: {}
  }

  assert(0);
  return false;
}
