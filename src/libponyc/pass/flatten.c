#include "flatten.h"
#include "../type/assemble.h"
#include "../type/cap.h"
#include <assert.h>

ast_result_t flatten_typeparamref(ast_t* ast)
{
  // Get the lowest capability that could fulfill the constraint.
  ast_t* def = (ast_t*)ast_data(ast);

  AST_GET_CHILDREN(def, id, constraint, default_type);
  token_id cap = cap_from_constraint(constraint);

  // Set the typeparamref cap.
  AST_GET_CHILDREN(ast, t_id, t_cap, t_ephemeral);
  ast_setid(t_cap, cap);

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
