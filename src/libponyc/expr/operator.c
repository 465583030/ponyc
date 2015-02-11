#include "operator.h"
#include "literal.h"
#include "postfix.h"
#include "control.h"
#include "reference.h"
#include "../pass/expr.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/matchtype.h"
#include "../type/alias.h"
#include "../type/viewpoint.h"
#include <assert.h>

static bool assign_id(typecheck_t* t, ast_t* ast, bool let, bool need_value)
{
  assert(ast_id(ast) == TK_ID);
  const char* name = ast_name(ast);

  sym_status_t status;
  ast_get(ast, name, &status);

  switch(status)
  {
    case SYM_UNDEFINED:
      if(need_value)
      {
        ast_error(ast, "the left side is undefined but its value is used");
        return false;
      }

      ast_setstatus(ast, name, SYM_DEFINED);
      return true;

    case SYM_DEFINED:
      if(let)
      {
        ast_error(ast, "can't assign to a let definition more than once");
        return false;
      }

      return true;

    case SYM_CONSUMED:
    {
      bool ok = true;

      if(need_value)
      {
        ast_error(ast, "the left side is consumed but its value is used");
        ok = false;
      }

      if(let)
      {
        ast_error(ast, "can't assign to a let definition more than once");
        ok = false;
      }

      if(t->frame->try_expr != NULL)
      {
        ast_error(ast,
          "can't reassign to a consumed identifier in a try expression");
        ok = false;
      }

      if(ok)
        ast_setstatus(ast, name, SYM_DEFINED);

      return ok;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool is_lvalue(typecheck_t* t, ast_t* ast, bool need_value)
{
  switch(ast_id(ast))
  {
    case TK_DONTCARE:
      // Can only assign to it if we don't need the value.
      return !need_value;

    case TK_VAR:
    case TK_LET:
      return assign_id(t, ast_child(ast), ast_id(ast) == TK_LET, need_value);

    case TK_VARREF:
    {
      ast_t* id = ast_child(ast);
      return assign_id(t, id, false, need_value);
    }

    case TK_LETREF:
    {
      ast_error(ast, "can't assign to a let local");
      return false;
    }

    case TK_FVARREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) == TK_THIS)
        return assign_id(t, right, false, need_value);

      return true;
    }

    case TK_FLETREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
      {
        ast_error(ast, "can't assign to a let field");
        return false;
      }

      if(t->frame->loop_body != NULL)
      {
        ast_error(ast, "can't assign to a let field in a loop");
        return false;
      }

      return assign_id(t, right, true, need_value);
    }

    case TK_TUPLE:
    {
      // A tuple is an lvalue if every component expression is an lvalue.
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!is_lvalue(t, child, need_value))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_SEQ:
    {
      // A sequence is an lvalue if it has a single child that is an lvalue.
      // This is used because the components of a tuple are sequences.
      ast_t* child = ast_child(ast);

      if(ast_sibling(child) != NULL)
        return false;

      return is_lvalue(t, child, need_value);
    }

    default: {}
  }

  return false;
}

bool expr_identity(pass_opt_t* opt, ast_t* ast)
{
  ast_settype(ast, type_builtin(opt, ast, "Bool"));
  ast_inheriterror(ast);
  return true;
}

bool expr_assign(pass_opt_t* opt, ast_t* ast)
{
  // Left and right are swapped in the AST to make sure we type check the
  // right side before the left. Fetch them in the opposite order.
  assert(ast != NULL);

  AST_GET_CHILDREN(ast, right, left);
  ast_t* l_type = ast_type(left);

  if(!is_lvalue(&opt->check, left, is_result_needed(ast)))
  {
    ast_error(ast, "left side must be something that can be assigned to");
    return false;
  }

  assert(l_type != NULL);

  if(!coerce_literals(&right, l_type, opt))
    return false;

  // Assignment is based on the alias of the right hand side.
  ast_t* r_type = ast_type(right);

  if(r_type == NULL)
    return false;

  ast_t* a_type = alias(r_type);

  if(!is_subtype(a_type, l_type))
  {
    ast_error(ast, "right side must be a subtype of left side");
    ast_error(a_type, "right side type: %s", ast_print_type(a_type));
    ast_error(l_type, "left side type: %s", ast_print_type(l_type));
    ast_free_unattached(a_type);
    return false;
  }

  bool ok_safe = safe_to_write(left, a_type);

  if(!ok_safe)
  {
    ast_error(ast, "not safe to write right side to left side");
    ast_error(a_type, "right side type: %s", ast_print_type(a_type));
    ast_free_unattached(a_type);
    return false;
  }

  ast_free_unattached(a_type);

  ast_settype(ast, consume_type(l_type, TK_NONE));
  ast_inheriterror(ast);
  return true;
}

bool expr_consume(typecheck_t* t, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, term);

  if(ast_type(term) == NULL)
    return false;

  switch(ast_id(term))
  {
    case TK_VARREF:
    case TK_LETREF:
    case TK_PARAMREF:
      break;

    default:
      ast_error(ast, "consume must take a local or parameter");
      return false;
  }

  ast_t* id = ast_child(term);
  const char* name = ast_name(id);

  // Can't consume from an outer scope while in a loop condition.
  if((t->frame->loop_cond != NULL) &&
    !ast_within_scope(t->frame->loop_cond, ast, name))
  {
    ast_error(ast, "can't consume from an outer scope in a loop condition");
    return false;
  }

  ast_setstatus(ast, name, SYM_CONSUMED);

  ast_t* type = ast_type(term);

  if(type == NULL)
    return false;

  token_id tcap = ast_id(cap);
  ast_t* c_type = consume_type(type, tcap);

  if(c_type == NULL)
  {
    ast_error(ast, "can't consume to this capability");
    ast_error(term, "expression type is %s", ast_print_type(type));
    return false;
  }

  ast_settype(ast, c_type);
  return true;
}
