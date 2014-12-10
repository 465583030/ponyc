#include "matchtype.h"
#include "subtype.h"
#include "viewpoint.h"
#include "cap.h"
#include "alias.h"
#include "reify.h"
#include "assemble.h"
#include <assert.h>

static matchtype_t could_subtype_with_union(ast_t* sub, ast_t* super)
{
  ast_t* child = ast_child(super);

  while(child != NULL)
  {
    child = ast_sibling(child);

    matchtype_t ok = could_subtype(sub, child);

    if(ok != MATCHTYPE_ACCEPT)
      return ok;

    child = ast_sibling(child);
  }

  return MATCHTYPE_ACCEPT;
}

static matchtype_t could_subtype_with_isect(ast_t* sub, ast_t* super)
{
  ast_t* child = ast_child(super);

  while(child != NULL)
  {
    child = ast_sibling(child);

    matchtype_t ok = could_subtype(sub, child);

    if(ok != MATCHTYPE_ACCEPT)
      return ok;

    child = ast_sibling(child);
  }

  return MATCHTYPE_ACCEPT;
}

static matchtype_t could_subtype_with_arrow(ast_t* sub, ast_t* super)
{
  // Check the lower bounds of super.
  ast_t* lower = viewpoint_lower(super);
  matchtype_t ok = could_subtype(sub, lower);

  if(lower != sub)
    ast_free_unattached(lower);

  return ok;
}

static matchtype_t could_subtype_with_typeparam(ast_t* sub, ast_t* super)
{
  // Could only be a subtype if it is a subtype, since we don't know the lower
  // bounds of the constraint any more accurately than is_subtype() does.
  return is_subtype(sub, super) ? MATCHTYPE_ACCEPT : MATCHTYPE_REJECT;
}

static matchtype_t could_subtype_trait_trait(ast_t* sub, ast_t* super)
{
  // The subtype cap/eph must be a subtype of the supertype cap/eph.
  AST_GET_CHILDREN(super, sup_pkg, sup_id, sup_typeargs, sup_cap, sup_eph);
  ast_t* r_type = set_cap_and_ephemeral(sub, ast_id(sup_cap), ast_id(sup_eph));
  bool ok = is_subtype(sub, r_type);
  ast_free_unattached(r_type);

  return ok ? MATCHTYPE_ACCEPT : MATCHTYPE_REJECT;
}

static matchtype_t could_subtype_trait_nominal(ast_t* sub, ast_t* super)
{
  ast_t* def = (ast_t*)ast_data(super);

  switch(ast_id(def))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      return could_subtype_trait_trait(sub, super);

    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    {
      // If we set sub cap/eph to the same as super, both sub and super must
      // be a subtype of the new type. That is, the concrete type must provide
      // the trait and the trait must be a subtype of the concrete type's
      // capability and ephemerality.
      AST_GET_CHILDREN(super, sup_pkg, sup_id, sup_typeargs, sup_cap, sup_eph);
      ast_t* r_type = set_cap_and_ephemeral(sub,
        ast_id(sup_cap), ast_id(sup_eph));

      bool ok = is_subtype(sub, r_type) && is_subtype(super, r_type);
      ast_free_unattached(r_type);
      return ok ? MATCHTYPE_ACCEPT : MATCHTYPE_REJECT;
    }

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t could_subtype_trait(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
      return could_subtype_trait_nominal(sub, super);

    case TK_UNIONTYPE:
      return could_subtype_with_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_with_isect(sub, super);

    case TK_TUPLETYPE:
      return MATCHTYPE_REJECT;

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    case TK_TYPEPARAMREF:
      return could_subtype_with_typeparam(sub, super);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t could_subtype_nominal(ast_t* sub, ast_t* super)
{
  ast_t* def = (ast_t*)ast_data(sub);

  switch(ast_id(def))
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      // With a concrete type, the subtype must be a subtype of the supertype.
      return is_subtype(sub, super) ? MATCHTYPE_ACCEPT : MATCHTYPE_REJECT;

    case TK_INTERFACE:
    case TK_TRAIT:
      return could_subtype_trait(sub, super);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t could_subtype_union(ast_t* sub, ast_t* super)
{
  // Some component type must be a possible match with the supertype.
  ast_t* child = ast_child(sub);
  matchtype_t ok = MATCHTYPE_REJECT;

  while(child != NULL)
  {
    matchtype_t sub_ok = could_subtype(child, super);

    if(sub_ok != MATCHTYPE_REJECT)
      ok = sub_ok;

    if(ok == MATCHTYPE_DENY)
      return ok;

    child = ast_sibling(child);
  }

  return ok;
}

static matchtype_t could_subtype_isect(ast_t* sub, ast_t* super)
{
  // All components type must be a possible match with the supertype.
  ast_t* child = ast_child(sub);

  while(child != NULL)
  {
    matchtype_t ok = could_subtype(child, super);

    if(ok != MATCHTYPE_ACCEPT)
      return ok;

    child = ast_sibling(child);
  }

  return MATCHTYPE_ACCEPT;
}

static matchtype_t could_subtype_tuple_tuple(ast_t* sub, ast_t* super)
{
  // Must pairwise match with the supertype.
  ast_t* sub_child = ast_child(sub);
  ast_t* super_child = ast_child(super);

  while((sub_child != NULL) && (super_child != NULL))
  {
    matchtype_t ok = could_subtype(sub_child, super_child);

    if(ok != MATCHTYPE_ACCEPT)
      return ok;

    sub_child = ast_sibling(sub_child);
    super_child = ast_sibling(super_child);
  }

  if((sub_child == NULL) && (super_child == NULL))
    return MATCHTYPE_ACCEPT;

  return MATCHTYPE_REJECT;
}

static matchtype_t could_subtype_tuple(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
      return MATCHTYPE_REJECT;

    case TK_UNIONTYPE:
      return could_subtype_with_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_with_isect(sub, super);

    case TK_TUPLETYPE:
      return could_subtype_tuple_tuple(sub, super);

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    case TK_TYPEPARAMREF:
      return could_subtype_with_typeparam(sub, super);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t could_subtype_arrow(ast_t* sub, ast_t* super)
{
  if(ast_id(super) == TK_ARROW)
  {
    // If we have the same viewpoint, check the right side.
    AST_GET_CHILDREN(sub, sub_left, sub_right);
    AST_GET_CHILDREN(super, super_left, super_right);

    if(is_eqtype(sub_left, super_left) && could_subtype(sub_right, super_right))
      return MATCHTYPE_ACCEPT;
  }

  // Check the upper bounds.
  ast_t* upper = viewpoint_upper(sub);
  matchtype_t ok = could_subtype(upper, super);

  if(upper != sub)
    ast_free_unattached(upper);

  return ok;
}

static matchtype_t could_subtype_typeparam(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
    {
      // If our constraint could be a subtype of super, some reifications could
      // be a subtype of super.
      ast_t* sub_def = (ast_t*)ast_data(sub);
      ast_t* constraint = ast_childidx(sub_def, 1);

      if(ast_id(constraint) == TK_TYPEPARAMREF)
      {
        ast_t* constraint_def = (ast_t*)ast_data(constraint);

        if(constraint_def == sub_def)
          return MATCHTYPE_REJECT;
      }

      return could_subtype(constraint, super);
    }

    case TK_UNIONTYPE:
      return could_subtype_with_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_with_isect(sub, super);

    case TK_TUPLETYPE:
      // A type parameter can't be constrained to a tuple.
      return MATCHTYPE_REJECT;

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    case TK_TYPEPARAMREF:
      // If the supertype is a typeparam, we have to be a subtype.
      return is_subtype(sub, super) ? MATCHTYPE_ACCEPT : MATCHTYPE_REJECT;

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

matchtype_t could_subtype(ast_t* sub, ast_t* super)
{
  if(ast_id(super) == TK_DONTCARE)
    return MATCHTYPE_ACCEPT;

  // Does a subtype of sub exist that is a subtype of super?
  switch(ast_id(sub))
  {
    case TK_NOMINAL:
      return could_subtype_nominal(sub, super);

    case TK_UNIONTYPE:
      return could_subtype_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_isect(sub, super);

    case TK_TUPLETYPE:
      return could_subtype_tuple(sub, super);

    case TK_ARROW:
      return could_subtype_arrow(sub, super);

    case TK_TYPEPARAMREF:
      return could_subtype_typeparam(sub, super);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

bool contains_interface(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(type);
      return ast_id(def) == TK_INTERFACE;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(contains_interface(child))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_ARROW:
      return contains_interface(ast_childidx(type, 1));

    case TK_TYPEPARAMREF:
      return false;

    default: {}
  }

  assert(0);
  return false;
}
