#include "sugar.h"
#include "../type/assemble.h"
#include "../type/nominal.h"
#include "../ds/stringtab.h"
#include <assert.h>

static ast_t* make_idseq(ast_t* ast, ast_t* id)
{
  ast_t* idseq = ast_from(ast, TK_IDSEQ);
  ast_add(idseq, id);
  return idseq;
}

static ast_t* make_var(ast_t* ast, ast_t* idseq, ast_t* type)
{
  ast_t* var = ast_from(ast, TK_VAR);
  ast_add(var, type);
  ast_add(var, idseq);
  return var;
}

static ast_t* make_ref(ast_t* ast, ast_t* id)
{
  ast_t* ref = ast_from(ast, TK_REFERENCE);
  ast_add(ref, id);
  return ref;
}

static ast_t* make_dot(ast_t* ast, ast_t* left, const char* right)
{
  ast_t* dot = ast_from(ast, TK_DOT);
  ast_t* r_right = ast_from_string(ast, stringtab(right));
  ast_add(dot, r_right);
  ast_add(dot, left);
  return dot;
}

static ast_t* make_call(ast_t* ast, ast_t* left)
{
  ast_t* call = ast_from(ast, TK_CALL);
  ast_add(call, ast_from(ast, TK_NONE));
  ast_add(call, ast_from(ast, TK_NONE));
  ast_add(call, left);

  return call;
}

static ast_t* make_assign(ast_t* ast, ast_t* left, ast_t* right)
{
  ast_t* assign = ast_from(ast, TK_ASSIGN);
  ast_add(assign, right);
  ast_add(assign, left);
  return assign;
}

static ast_t* make_empty(ast_t* ast)
{
  ast_t* seq = ast_from(ast, TK_SEQ);
  ast_add(seq, make_ref(ast, ast_from_string(ast, "None")));
  return seq;
}

static ast_t* make_create(ast_t* ast, ast_t* type)
{
  ast_t* create = ast_from(ast, TK_NEW);
  ast_add(create, make_empty(ast)); // body
  ast_add(create, ast_from(ast, TK_NONE)); // error
  ast_add(create, type); // result
  ast_add(create, ast_from(ast, TK_NONE)); // params
  ast_add(create, ast_from(ast, TK_NONE)); // typeparams
  ast_add(create, ast_from_string(ast, stringtab("create"))); // name
  ast_add(create, ast_from(ast, TK_NONE)); // cap

  return create;
}

static bool sugar_constructor(ast_t* ast)
{
  ast_t* members = ast_childidx(ast, 4);
  ast_t* member = ast_child(members);

  // if we have no fields and have no "create" constructor, add one
  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
        return true;

      case TK_NEW:
      {
        ast_t* id = ast_childidx(member, 1);

        if(ast_name(id) == stringtab("create"))
          return true;

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  ast_add(members, make_create(ast, type_for_this(ast, TK_REF, true)));
  return true;
}

static bool sugar_class(ast_t* ast)
{
  if(!sugar_constructor(ast))
    return false;

  ast_t* defcap = ast_childidx(ast, 2);

  if(ast_id(defcap) == TK_NONE)
    ast_replace(&defcap, ast_from(defcap, TK_REF));

  return true;
}

static bool sugar_actor(ast_t* ast)
{
  if(!sugar_constructor(ast))
    return false;

  ast_t* defcap = ast_childidx(ast, 2);

  if(ast_id(defcap) == TK_NONE)
    ast_replace(&defcap, ast_from(defcap, TK_TAG));

  return true;
}

static bool sugar_trait(ast_t* ast)
{
  ast_t* defcap = ast_childidx(ast, 2);

  if(ast_id(defcap) == TK_NONE)
    ast_replace(&defcap, ast_from(defcap, TK_REF));

  return true;
}

static bool sugar_new(ast_t* ast)
{
  // the capability is tag
  ast_t* cap = ast_child(ast);
  assert(ast_id(cap) == TK_NONE);
  ast_replace(&cap, ast_from(cap, TK_TAG));

  // set the name to "create" if there isn't one
  ast_t* id = ast_sibling(cap);

  if(ast_id(id) == TK_NONE)
    ast_replace(&id, ast_from_string(id, "create"));

  // return type is This ref^ if not already set
  ast_t* result = ast_childidx(ast, 4);

  if(ast_id(result) == TK_NONE)
    ast_replace(&result, type_for_this(ast, TK_REF, true));

  return true;
}

static bool sugar_be(ast_t* ast)
{
  if(ast_nearest(ast, TK_CLASS) != NULL)
  {
    ast_error(ast, "can't have a behaviour in a class");
    return false;
  }

  // the capability is tag
  ast_t* cap = ast_child(ast);
  assert(ast_id(cap) == TK_NONE);
  ast_replace(&cap, ast_from(cap, TK_TAG));

  // return type is This tag
  ast_t* result = ast_childidx(ast, 4);
  assert(ast_id(result) == TK_NONE);
  ast_replace(&result, type_for_this(ast, TK_TAG, false));

  return true;
}

static bool sugar_fun(ast_t* ast)
{
  // set the name to "apply" if there isn't one
  ast_t* id = ast_childidx(ast, 1);

  if(ast_id(id) == TK_NONE)
    ast_replace(&id, ast_from_string(id, "apply"));

  ast_t* result = ast_childidx(ast, 4);

  if(ast_id(result) != TK_NONE)
    return true;

  // set the return type to None
  ast_t* type = nominal_sugar(ast, NULL, stringtab("None"));
  ast_replace(&result, type);

  // add None at the end of the body, if there is one
  ast_t* body = ast_childidx(ast, 6);

  if(ast_id(body) == TK_SEQ)
  {
    ast_t* last = ast_childlast(body);
    ast_t* ref = ast_from(last, TK_REFERENCE);
    ast_t* none = ast_from_string(last, stringtab("None"));
    ast_add(ref, none);
    ast_append(body, ref);
  }

  return true;
}

static bool sugar_nominal(ast_t* ast)
{
  // if we didn't have a package, the first two children will be ID NONE
  // change them to NONE ID so the package is always first
  ast_t* package = ast_child(ast);
  ast_t* type = ast_sibling(package);

  if(ast_id(type) == TK_NONE)
  {
    ast_pop(ast);
    ast_pop(ast);
    ast_add(ast, package);
    ast_add(ast, type);
  }

  return true;
}

static bool sugar_structural(ast_t* ast)
{
  ast_t* cap = ast_childidx(ast, 1);

  if(ast_id(cap) != TK_NONE)
    return true;

  token_id def_cap;

  // if it's a typeparam, default capability is tag, otherwise it is ref
  if(ast_nearest(ast, TK_TYPEPARAM) != NULL)
    def_cap = TK_TAG;
  else
    def_cap = TK_REF;

  ast_replace(&cap, ast_from(ast, def_cap));
  return true;
}

static bool sugar_else(ast_t* ast)
{
  ast_t* right = ast_childidx(ast, 2);

  if(ast_id(right) == TK_NONE)
    ast_replace(&right, make_empty(right));

  return true;
}

static bool sugar_try(ast_t* ast)
{
  ast_t* else_clause = ast_childidx(ast, 1);
  ast_t* then_clause = ast_sibling(else_clause);

  if(ast_id(else_clause) == TK_NONE)
    ast_replace(&else_clause, make_empty(else_clause));

  if(ast_id(then_clause) == TK_NONE)
    ast_replace(&then_clause, make_empty(then_clause));

  return true;
}

static bool sugar_for(ast_t* ast)
{
  assert(ast_id(ast) == TK_FOR);

  ast_t* for_idseq = ast_child(ast);
  ast_t* for_type = ast_sibling(for_idseq);
  ast_t* for_iter = ast_sibling(for_type);
  ast_t* for_body = ast_sibling(for_iter);
  ast_t* for_else = ast_sibling(for_body);

  ast_t* id = ast_hygienic_id(ast);
  const char* name = ast_name(id);

  ast_t* body = ast_from(ast, TK_SEQ);
  ast_scope(body);

  ast_add(body, for_body);
  ast_add(body,
    make_assign(ast,
      make_var(ast, for_idseq, for_type),
      make_call(ast, make_dot(ast, make_ref(ast, id), "next"))
      )
    );

  ast_t* whileloop = ast_from(ast, TK_WHILE);
  ast_scope(whileloop);

  ast_add(whileloop, for_else);
  ast_add(whileloop, body);
  ast_add(whileloop,
    make_call(ast, make_dot(ast, make_ref(ast, id), "has_next"))
    );
  sugar_else(whileloop);

  ast_t* iter = make_assign(ast,
    make_var(ast, make_idseq(ast, id), ast_from(ast, TK_NONE)),
    for_iter
    );

  ast_t* seq = ast_from(ast, TK_SEQ);
  ast_scope(seq);
  ast_add(seq, whileloop);
  ast_add(seq, iter);

  ast_replace(&ast, seq);

  if(!ast_set(seq, name, id))
  {
    assert(0);
    return false;
  }

  return true;
}

static bool sugar_case(ast_t* ast)
{
  assert(ast_id(ast) == TK_CASE);
  ast_t* body = ast_childidx(ast, 3);

  if(ast_id(body) != TK_NONE)
    return true;

  ast_t* next = ast;
  ast_t* next_body;

  do
  {
    next = ast_sibling(next);

    if(next == NULL)
    {
      ast_error(body, "case with no body has no following case body");
      return false;
    }

    assert(ast_id(next) == TK_CASE);
    next_body = ast_childidx(next, 3);
  } while(ast_id(next_body) == TK_NONE);

  ast_replace(&body, next_body);
  return true;
}

static bool sugar_update(ast_t* ast)
{
  assert(ast_id(ast) == TK_ASSIGN);
  ast_t* call = ast_child(ast);
  ast_t* value = ast_sibling(call);

  if(ast_id(call) != TK_CALL)
    return true;

  ast_t* expr = ast_child(call);

  ast_t* positional = ast_sibling(expr);
  ast_t* named = ast_sibling(positional);
  positional = ast_dup(positional);

  if(ast_id(positional) == TK_NONE)
    ast_setid(positional, TK_POSITIONALARGS);

  ast_t* seq = ast_from(positional, TK_SEQ);
  ast_add(seq, value);
  ast_append(positional, seq);

  ast_t* dot = make_dot(ast, expr, "update");

  ast_t* update = ast_from(ast, TK_CALL);
  ast_add(update, named);
  ast_add(update, positional);
  ast_add(update, dot);

  ast_replace(&ast, update);
  return true;
}

ast_result_t pass_sugar(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_CLASS:
      if(!sugar_class(ast))
        return AST_ERROR;
      break;

    case TK_ACTOR:
      if(!sugar_actor(ast))
        return AST_ERROR;
      break;

    case TK_TRAIT:
      if(!sugar_trait(ast))
        return AST_ERROR;
      break;

    case TK_NEW:
      if(!sugar_new(ast))
        return AST_ERROR;
      break;

    case TK_BE:
      if(!sugar_be(ast))
        return AST_ERROR;
      break;

    case TK_FUN:
      if(!sugar_fun(ast))
        return AST_ERROR;
      break;

    case TK_NOMINAL:
      if(!sugar_nominal(ast))
        return AST_ERROR;
      break;

    case TK_STRUCTURAL:
      if(!sugar_structural(ast))
        return AST_ERROR;
      break;

    case TK_IF:
    case TK_WHILE:
      if(!sugar_else(ast))
        return AST_FATAL;
      break;

    case TK_TRY:
      if(!sugar_try(ast))
        return AST_FATAL;
      break;

    case TK_FOR:
      if(!sugar_for(ast))
        return AST_FATAL;
      break;

    case TK_BANG:
      // TODO: syntactic sugar for partial application
      break;

    case TK_CASE:
      if(!sugar_case(ast))
        return AST_FATAL;
      break;

    case TK_ASSIGN:
      if(!sugar_update(ast))
        return AST_FATAL;
      break;

    default: {}
  }

  return AST_OK;
}
