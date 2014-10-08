#include "subtype.h"
#include "reify.h"
#include "cap.h"
#include "alias.h"
#include "assemble.h"
#include "viewpoint.h"
#include "../ds/stringtab.h"
#include "../expr/literal.h"
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

  ast_t* def = (ast_t*)ast_data(sub);
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
  assert(sub != NULL);
  assert(super != NULL);
  assert(ast_id(sub) == TK_NOMINAL);
  assert(ast_id(super) == TK_NOMINAL);

  if(!check_cap_and_ephemeral(sub, super))
    return false;

  ast_t* sub_def = (ast_t*)ast_data(sub);
  ast_t* super_def = (ast_t*)ast_data(super);
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
  assert(traits != NULL);
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

static bool is_literal(ast_t* type, const char* name)
{
  if(type == NULL)
    return false;

  if(ast_id(type) != TK_NOMINAL)
    return false;

  // don't have to check the package, since literals are all builtins
  return ast_name(ast_childidx(type, 1)) == stringtab(name);
}

static bool pointer_supertype(ast_t* type)
{
  // Note that a pointer can't be a subtype of any structural type, not even
  // the empty structural type.

  // Things that can be a supertype of a Pointer.
  switch(ast_id(type))
  {
    case TK_NOMINAL:
      // A nominal must also be a Pointer. The type parameter will be checked
      // later on.
      return is_literal(type, "Pointer");

    case TK_TYPEPARAMREF:
      // A typeparam checks correctly already.
      return true;

    case TK_ARROW:
      // Check the right side of an arrow type.
      return pointer_supertype(ast_childidx(type, 1));

    default: {}
  }

  return false;
}

bool is_subtype(ast_t* sub, ast_t* super)
{
  assert(sub != NULL);
  assert(super != NULL);

  // check if the subtype is a union, isect, type param, type alias or viewpoint
  switch(ast_id(sub))
  {
    case TK_UNIONTYPE:
    {
      // all elements of the union must be subtypes
      ast_t* child = ast_child(sub);

      while(child != NULL)
      {
        if(!is_subtype(child, super))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ISECTTYPE:
    {
      // one element of the intersection must be a subtype
      ast_t* child = ast_child(sub);

      while(child != NULL)
      {
        if(is_subtype(child, super))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_NOMINAL:
    {
      // Special case Pointer[A]. It can only be a subtype of itself, because
      // in the code generator, a pointer has no vtable.
      if(is_literal(sub, "Pointer") && !pointer_supertype(super))
        return false;

      break;
    }

    case TK_INTLITERAL:
    case TK_FLOATLITERAL:
      // We only coerce integer and float literals if they are a subtype
      return is_literal_subtype(ast_id(sub), super) != NULL;

    case TK_TYPEPARAMREF:
    {
      // check if our constraint is a subtype of super
      ast_t* def = (ast_t*)ast_data(sub);
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
      // an arrow type can be a subtype if its upper bounds is a subtype
      ast_t* upper = viewpoint_upper(sub);
      bool ok = is_subtype(upper, super);
      ast_free_unattached(upper);

      if(ok)
        return true;
      break;
    }

    case TK_FUNTYPE:
      return false;

    default: {}
  }

  // check if the supertype is a union, isect, type alias or viewpoint
  switch(ast_id(super))
  {
    case TK_UNIONTYPE:
    {
      // must be a subtype of one element of the union
      ast_t* child = ast_child(super);

      while(child != NULL)
      {
        if(is_subtype(sub, child))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_ISECTTYPE:
    {
      // must be a subtype of all elements of the intersection
      ast_t* child = ast_child(super);

      while(child != NULL)
      {
        if(!is_subtype(sub, child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ARROW:
    {
      // an arrow type can be a supertype if its lower bounds is a supertype
      ast_t* lower = viewpoint_lower(super);
      bool ok = is_subtype(sub, lower);
      ast_free_unattached(lower);

      if(ok)
        return true;
      break;
    }

    case TK_TYPEPARAMREF:
    {
      // we can be a subtype of a typeparam if its constraint is concrete
      ast_t* def = (ast_t*)ast_data(super);
      ast_t* constraint = ast_childidx(def, 1);

      if(ast_id(constraint) == TK_NOMINAL)
      {
        ast_t* constraint_def = (ast_t*)ast_data(constraint);

        switch(ast_id(constraint_def))
        {
          case TK_PRIMITIVE:
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

    case TK_FUNTYPE:
      return false;

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
          // A tuple is a subtype of {} tag.
          AST_GET_CHILDREN(super, members, cap);
          return (ast_child(members) == NULL) && (ast_id(cap) == TK_TAG);
        }

        case TK_TUPLETYPE:
        {
          // elements must be pairwise subtypes
          ast_t* sub_child = ast_child(sub);
          ast_t* super_child = ast_child(super);

          while((sub_child != NULL) && (super_child != NULL))
          {
            if(!is_subtype(sub_child, super_child))
              return false;

            sub_child = ast_sibling(sub_child);
            super_child = ast_sibling(super_child);
          }

          return (sub_child == NULL) && (super_child == NULL);
        }

        default: {}
      }

      return false;
    }

    case TK_NOMINAL:
    {
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
      if(ast_id(super) != TK_TYPEPARAMREF)
        return false;

      if(!check_cap_and_ephemeral(sub, super))
        return false;

      return ast_data(sub) == ast_data(super);
    }

    case TK_ARROW:
    {
      if(ast_id(super) != TK_ARROW)
        return false;

      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      ast_t* super_left = ast_child(super);
      ast_t* super_right = ast_sibling(super_left);

      return is_eqtype(left, super_left) && is_subtype(right, super_right);
    }

    case TK_THISTYPE:
      return ast_id(super) == TK_THISTYPE;

    default: {}
  }

  assert(0);
  return false;
}

bool is_eqtype(ast_t* a, ast_t* b)
{
  return is_subtype(a, b) && is_subtype(b, a);
}

bool is_none(ast_t* type)
{
  return is_literal(type, "None");
}

bool is_bool(ast_t* type)
{
  return is_literal(type, "Bool");
}

bool is_signed(ast_t* type)
{
  if(type == NULL)
    return false;

  ast_t* builtin = type_builtin(type, "Signed");

  if(builtin == NULL)
    return false;

  bool ok = is_subtype(type, builtin);
  ast_free_unattached(builtin);
  return ok;
}

static bool is_interface_id_compatible(ast_t* iface, ast_t* b)
{
  // If the right side is also an interface, check local compatibility of the
  // capabilities. Otherwise, check in the other direction.
  switch(ast_id(b))
  {
    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(b);

      if(ast_id(def) == TK_TRAIT)
        return cap_compatible(iface, b);

      break;
    }

    case TK_STRUCTURAL:
      return cap_compatible(iface, b);

    default: {}
  }

  return is_id_compatible(b, iface);
}

bool is_id_compatible(ast_t* a, ast_t* b)
{
  switch(ast_id(a))
  {
    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(a);

      switch(ast_id(def))
      {
        case TK_PRIMITIVE:
        case TK_CLASS:
        case TK_ACTOR:
          // With a concrete type, one side must be a subtype of the other and
          // the capabilities must be compatible.
          return (is_subtype(a, b) || is_subtype(b, a)) && cap_compatible(a, b);

        case TK_TRAIT:
          return is_interface_id_compatible(a, b);

        default: {}
      }
      break;
    }

    case TK_STRUCTURAL:
      return is_interface_id_compatible(a, b);

    case TK_TYPEPARAMREF:
    {
      // Check if our constraint is compatible.
      ast_t* def = (ast_t*)ast_data(a);
      ast_t* constraint = ast_childidx(def, 1);
      return is_id_compatible(constraint, b);
    }

    case TK_ARROW:
    {
      // Check the upper bounds of the right side.
      ast_t* right = ast_childidx(a, 1);
      ast_t* upper = viewpoint_upper(right);
      bool ok = is_id_compatible(upper, b);
      ast_free_unattached(upper);
      return ok;
    }

    case TK_UNIONTYPE:
    {
      ast_t* child = ast_child(a);

      while(child != NULL)
      {
        if(is_id_compatible(child, b))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(a);

      while(child != NULL)
      {
        if(!is_id_compatible(child, b))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_TUPLETYPE:
      // TODO: id compatibility for tuples
      break;

    default: {}
  }

  assert(0);
  return false;
}
