#include "subtype.h"
#include "reify.h"
#include "cap.h"
#include "assemble.h"
#include "../pass/names.h"
#include "../ds/stringtab.h"
#include <assert.h>

static bool is_throws_sub_throws(ast_t* sub, ast_t* super)
{
  assert(
    (ast_id(sub) == TK_NONE) ||
    (ast_id(sub) == TK_QUESTION)
    );
  assert(
    (ast_id(super) == TK_NONE) ||
    (ast_id(super) == TK_QUESTION)
    );

  switch(ast_id(sub))
  {
    case TK_NONE:
      return true;

    case TK_QUESTION:
      return ast_id(super) == TK_QUESTION;

    default: {}
  }

  assert(0);
  return false;
}

static bool check_cap_and_ephemeral(ast_t* sub, ast_t* super)
{
  int sub_index = ast_id(sub) == TK_NOMINAL ? 4 : 2;
  int super_index = ast_id(super) == TK_NOMINAL ? 4 : 2;

  ast_t* sub_ephemeral = ast_childidx(sub, sub_index);
  ast_t* super_ephemeral = ast_childidx(super, super_index);

  token_id sub_tcap = cap_for_type(sub);
  token_id super_tcap = cap_for_type(super);

  // ignore ephemerality if it isn't iso/trn and can't be recovered to iso/trn
  if((ast_id(super_ephemeral) == TK_HAT) &&
    (super_tcap < TK_VAL) &&
    (ast_id(sub_ephemeral) != TK_HAT))
    return false;

  return is_cap_sub_cap(sub_tcap, super_tcap);
}

static bool is_eq_typeargs(ast_t* a, ast_t* b)
{
  assert(ast_id(a) == TK_NOMINAL);
  assert(ast_id(b) == TK_NOMINAL);

  // check typeargs are the same
  ast_t* a_arg = ast_child(ast_childidx(a, 2));
  ast_t* b_arg = ast_child(ast_childidx(b, 2));

  while((a_arg != NULL) && (b_arg != NULL))
  {
    if(!is_eqtype(a_arg, b_arg))
      return false;

    a_arg = ast_sibling(a_arg);
    b_arg = ast_sibling(b_arg);
  }

  // make sure we had the same number of typeargs
  return (a_arg == NULL) && (b_arg == NULL);
}

static bool is_fun_sub_fun(ast_t* sub, ast_t* super)
{
  // must be the same type of function
  // TODO: could relax this
  if(ast_id(sub) != ast_id(super))
    return false;

  ast_t* sub_params = ast_childidx(sub, 3);
  ast_t* sub_result = ast_sibling(sub_params);
  ast_t* sub_throws = ast_sibling(sub_result);

  ast_t* super_params = ast_childidx(super, 3);
  ast_t* super_result = ast_sibling(super_params);
  ast_t* super_throws = ast_sibling(super_result);

  // TODO: reify with our own constraints?

  // contravariant receiver
  if(!is_cap_sub_cap(cap_for_fun(super), cap_for_fun(sub)))
    return false;

  // contravariant parameters
  ast_t* sub_param = ast_child(sub_params);
  ast_t* super_param = ast_child(super_params);

  while((sub_param != NULL) && (super_param != NULL))
  {
    // extract the type if this is a parameter
    // otherwise, this is already a type
    ast_t* sub_type = (ast_id(sub_param) == TK_PARAM) ?
      ast_childidx(sub_param, 1) : sub_param;

    ast_t* super_type = (ast_id(super_param) == TK_PARAM) ?
      ast_childidx(super_param, 1) : super_param;

    if(!is_subtype(super_type, sub_type))
      return false;

    sub_param = ast_sibling(sub_param);
    super_param = ast_sibling(super_param);
  }

  if((sub_param != NULL) || (super_param != NULL))
    return false;

  // covariant results
  if(!is_subtype(sub_result, super_result))
    return false;

  // covariant throws
  if(!is_throws_sub_throws(sub_throws, super_throws))
    return false;

  return true;
}

static bool is_structural_sub_fun(ast_t* sub, ast_t* fun)
{
  // must have some function that is a subtype of fun
  ast_t* members = ast_child(sub);
  ast_t* sub_fun = ast_child(members);

  while(sub_fun != NULL)
  {
    if(is_fun_sub_fun(sub_fun, fun))
      return true;

    sub_fun = ast_sibling(sub_fun);
  }

  return false;
}

static bool is_structural_sub_structural(ast_t* sub, ast_t* super)
{
  // must be a subtype of every function in super
  ast_t* members = ast_child(super);
  ast_t* fun = ast_child(members);

  while(fun != NULL)
  {
    if(!is_structural_sub_fun(sub, fun))
      return false;

    fun = ast_sibling(fun);
  }

  return true;
}

static bool is_member_sub_fun(ast_t* member, ast_t* typeparams,
  ast_t* typeargs, ast_t* fun)
{
  switch(ast_id(member))
  {
    case TK_FVAR:
    case TK_FLET:
      return false;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      ast_t* r_fun = reify(member, typeparams, typeargs);
      bool is_sub = is_fun_sub_fun(r_fun, fun);
      ast_free_unattached(r_fun);
      return is_sub;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool is_type_sub_fun(ast_t* def, ast_t* typeargs,
  ast_t* fun)
{
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    if(is_member_sub_fun(member, typeparams, typeargs, fun))
      return true;

    member = ast_sibling(member);
  }

  return false;
}

static bool is_nominal_sub_structural(ast_t* sub, ast_t* super)
{
  assert(ast_id(sub) == TK_NOMINAL);
  assert(ast_id(super) == TK_STRUCTURAL);

  if(!check_cap_and_ephemeral(sub, super))
    return false;

  ast_t* def = ast_data(sub);
  assert(def != NULL);

  // must be a subtype of every function in super
  ast_t* typeargs = ast_childidx(sub, 2);
  ast_t* members = ast_child(super);
  ast_t* fun = ast_child(members);

  while(fun != NULL)
  {
    if(!is_type_sub_fun(def, typeargs, fun))
      return false;

    fun = ast_sibling(fun);
  }

  return true;
}

static bool is_nominal_sub_nominal(ast_t* sub, ast_t* super)
{
  assert(ast_id(sub) == TK_NOMINAL);
  assert(ast_id(super) == TK_NOMINAL);

  if(!check_cap_and_ephemeral(sub, super))
    return false;

  ast_t* sub_def = ast_data(sub);
  ast_t* super_def = ast_data(super);
  assert(sub_def != NULL);
  assert(super_def != NULL);

  // if we are the same nominal type, our typeargs must be the same
  if(sub_def == super_def)
    return is_eq_typeargs(sub, super);

  // get our typeparams and typeargs
  ast_t* typeparams = ast_childidx(sub_def, 1);
  ast_t* typeargs = ast_childidx(sub, 2);

  // check traits, depth first
  ast_t* traits = ast_childidx(sub_def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    // reify the trait
    ast_t* r_trait = reify(trait, typeparams, typeargs);

    // use the cap and ephemerality of the subtype
    reify_cap_and_ephemeral(sub, &r_trait);
    bool is_sub = is_subtype(r_trait, super);
    ast_free_unattached(r_trait);

    if(is_sub)
      return true;

    trait = ast_sibling(trait);
  }

  return false;
}

bool is_subtype(ast_t* sub, ast_t* super)
{
  // check if the subtype is a union, isect, type param, type alias or viewpoint
  switch(ast_id(sub))
  {
    case TK_UNIONTYPE:
    {
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(left, super) && is_subtype(right, super);
    }

    case TK_ISECTTYPE:
    {
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(left, super) || is_subtype(right, super);
    }

    case TK_TYPEPARAMREF:
    {
      // check if our constraint is a subtype of super
      ast_t* def = ast_data(sub);
      ast_t* constraint = ast_childidx(def, 1);

      // if it isn't, keep trying
      if(ast_id(constraint) != TK_NONE)
      {
        // use the cap and ephemerality of the typeparam
        reify_cap_and_ephemeral(sub, &constraint);
        bool ok = is_subtype(constraint, super);
        ast_free_unattached(constraint);

        if(ok)
          return true;
      }
      break;
    }

    case TK_ARROW:
    {
      // TODO: actually do viewpoint adaptation
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(right, super);
    }

    default: {}
  }

  // check if the supertype is a union, isect, type alias or viewpoint
  switch(ast_id(super))
  {
    case TK_UNIONTYPE:
    {
      ast_t* left = ast_child(super);
      ast_t* right = ast_sibling(left);
      return is_subtype(sub, left) || is_subtype(sub, right);
    }

    case TK_ISECTTYPE:
    {
      ast_t* left = ast_child(super);
      ast_t* right = ast_sibling(left);
      return is_subtype(sub, left) && is_subtype(sub, right);
    }

    case TK_ARROW:
    {
      // TODO: actually do viewpoint adaptation
      ast_t* left = ast_child(super);
      ast_t* right = ast_sibling(left);
      return is_subtype(sub, right);
    }

    case TK_TYPEPARAMREF:
    {
      // we also can be a subtype of a typeparam if its constraint is concrete
      ast_t* def = ast_data(super);
      ast_t* constraint = ast_childidx(def, 1);

      if(ast_id(constraint) == TK_NOMINAL)
      {
        ast_t* constraint_def = ast_data(constraint);

        switch(ast_id(constraint_def))
        {
          case TK_CLASS:
          case TK_ACTOR:
            if(is_eqtype(sub, constraint))
              return true;
            break;

          default: {}
        }
      }
      break;
    }

    default: {}
  }

  switch(ast_id(sub))
  {
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      return is_fun_sub_fun(sub, super);

    case TK_TUPLETYPE:
    {
      switch(ast_id(super))
      {
        case TK_STRUCTURAL:
        {
          // a tuple is a subtype of an empty structural type
          ast_t* members = ast_child(super);
          ast_t* member = ast_child(members);
          return member == NULL;
        }

        case TK_TUPLETYPE:
        {
          ast_t* left = ast_child(sub);
          ast_t* right = ast_sibling(left);
          ast_t* super_left = ast_child(super);
          ast_t* super_right = ast_sibling(super_left);

          return is_subtype(left, super_left) &&
            is_subtype(right, super_right);
        }

        default: {}
      }

      return false;
    }

    case TK_NOMINAL:
    {
      // check for numeric literals and special case them
      if(is_literal(sub, "IntLiteral"))
      {
        if(is_literal(super, "IntLiteral") ||
          is_literal(super, "FloatLiteral")
          )
          return true;

        // an integer literal is a subtype of any arithmetic type
        ast_t* math = type_builtin(sub, "Arithmetic");
        bool ok = is_subtype(super, math);
        ast_free(math);

        if(ok)
          return true;
      } else if(is_literal(sub, "FloatLiteral")) {
        if(is_literal(super, "FloatLiteral"))
          return true;

        // a float literal is a subtype of any float type
        ast_t* float_type = type_builtin(sub, "Float");
        bool ok = is_subtype(super, float_type);
        ast_free(float_type);

        if(ok)
          return true;
      }

      switch(ast_id(super))
      {
        case TK_NOMINAL:
          return is_nominal_sub_nominal(sub, super);

        case TK_STRUCTURAL:
          return is_nominal_sub_structural(sub, super);

        default: {}
      }

      return false;
    }

    case TK_STRUCTURAL:
    {
      if(ast_id(super) != TK_STRUCTURAL)
        return false;

      return is_structural_sub_structural(sub, super);
    }

    case TK_TYPEPARAMREF:
    {
      if(!check_cap_and_ephemeral(sub, super))
        return false;

      return ast_data(sub) == ast_data(super);
    }

    default: {}
  }

  assert(0);
  return false;
}

bool is_eqtype(ast_t* a, ast_t* b)
{
  return is_subtype(a, b) && is_subtype(b, a);
}

bool is_literal(ast_t* type, const char* name)
{
  if(ast_id(type) != TK_NOMINAL)
    return false;

  // don't have to check the package, since literals are all builtins
  return ast_name(ast_childidx(type, 1)) == stringtab(name);
}

bool is_builtin(ast_t* type, const char* name)
{
  ast_t* builtin = ast_from(type, TK_NOMINAL);
  ast_add(builtin, ast_from(type, TK_NONE));
  ast_add(builtin, ast_from(type, TK_NONE));
  ast_add(builtin, ast_from(type, TK_NONE));
  ast_add(builtin, ast_from_string(type, name));
  ast_add(builtin, ast_from(type, TK_NONE));

  bool ok = names_nominal(type, &builtin) && is_subtype(type, builtin);
  ast_free_unattached(builtin);

  return ok;
}

bool is_bool(ast_t* type)
{
  return is_builtin(type, "Bool");
}

bool is_arithmetic(ast_t* type)
{
  return is_builtin(type, "Arithmetic");
}

bool is_integer(ast_t* type)
{
  return is_builtin(type, "Integer");
}

bool is_float(ast_t* type)
{
  return is_builtin(type, "Float");
}

bool is_math_compatible(ast_t* a, ast_t* b)
{
  if(is_literal(a, "IntLiteral"))
    return is_arithmetic(b);

  if(is_literal(a, "FloatLiteral"))
    return is_float(b);

  if(is_literal(b, "IntLiteral"))
    return is_arithmetic(a);

  if(is_literal(b, "FloatLiteral"))
    return is_float(a);

  return is_eqtype(a, b);
}

bool is_id_compatible(ast_t* a, ast_t* b)
{
  // TODO: make both sides tag types
  // if either is a subtype of the other, they are compatible
  // also, the presence of a structural type on either side makes them
  // compatible
  return true;
}
