#include "type_valid.h"
#include "subtype.h"
#include "reify.h"
#include "typechecker.h"
#include <assert.h>

typedef enum
{
  TYPE_INITIAL = 0,
  TYPE_TRAITS_IN_PROGRESS,
  TYPE_TRAITS_DONE
} type_state_t;

static bool attach_method(ast_t* type, ast_t* method)
{
  ast_t* impl = ast_childidx(method, 6);

  // if we aren't a trait and it has no implementation, we're done
  if((ast_id(type) != TK_TRAIT) && (ast_id(impl) == TK_NONE))
    return true;

  // copy the method ast
  ast_t* members = ast_childidx(type, 4);
  ast_t* method_dup = ast_dup(method);

  // TODO: substitute for any type parameters

  // see if we have an existing method with this name
  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* existing = ast_get(type, name);

  if(existing != NULL)
  {
    // TODO: if we already have this method (by name):
    // check our version is a subtype of the supplied version
    ast_free(method_dup);
    return true;
  }

  // insert into our members
  ast_append(members, method_dup);

  // add to our scope
  ast_set(type, name, method_dup);

  return true;
}

static bool attach_traits(ast_t* type)
{
  type_state_t state = (type_state_t)ast_data(type);

  switch(state)
  {
    case TYPE_INITIAL:
      ast_attach(type, (void*)TYPE_TRAITS_IN_PROGRESS);
      break;

    case TYPE_TRAITS_IN_PROGRESS:
      ast_error(type, "traits cannot be recursive");
      return false;

    case TYPE_TRAITS_DONE:
      return true;
  }

  ast_t* traits = ast_childidx(type, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    ast_t* nominal = ast_child(trait);

    if(ast_id(nominal) != TK_NOMINAL)
    {
      ast_error(nominal, "traits must be nominal types");
      return false;
    }

    ast_t* def = nominal_def(nominal);

    if(ast_id(def) != TK_TRAIT)
    {
      ast_error(nominal, "must be a trait");
      return false;
    }

    if(!attach_traits(def))
      return false;

    ast_t* members = ast_childidx(def, 4);
    ast_t* member = ast_child(members);

    while(member != NULL)
    {
      switch(ast_id(member))
      {
        case TK_NEW:
        case TK_FUN:
        case TK_BE:
        {
          if(!attach_method(type, member))
            return false;

          break;
        }

        default: assert(0);
      }

      member = ast_sibling(member);
    }

    trait = ast_sibling(trait);
  }

  ast_attach(type, (void*)TYPE_TRAITS_DONE);
  return true;
}

static bool check_constraints(ast_t* type, ast_t* typeargs)
{
  if(ast_id(type) == TK_TYPEPARAM)
  {
    if(ast_id(typeargs) != TK_NONE)
    {
      ast_error(typeargs, "type parameters cannot have type arguments");
      return false;
    }

    return true;
  }

  // reify the type parameters with the typeargs
  ast_t* typeparams = ast_childidx(type, 1);

  if(ast_id(typeparams) == TK_NONE)
  {
    if(ast_id(typeargs) != TK_NONE)
    {
      ast_error(typeargs, "supplied type arguments where none were needed");
      return false;
    }

    return true;
  }

  ast_t* r_typeparams = reify_typeparams(typeparams, typeargs);

  if(r_typeparams == NULL)
  {
    ast_error(ast_parent(typeargs), "couldn't reify type parameters");
    return false;
  }

  ast_t* typeparam = ast_child(r_typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    ast_t* constraint = ast_childidx(typeparam, 1);

    if((ast_id(constraint) != TK_NONE) && !is_subtype(typearg, constraint))
    {
      ast_error(typearg, "type argument is outside its constraint");
      ast_error(typeparam, "constraint is here");
      ast_free(r_typeparams);
      return false;
    }

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  ast_free(r_typeparams);

  if(typeparam != NULL)
  {
    ast_error(typeargs, "not enough type arguments");
    return false;
  }

  if(typearg != NULL)
  {
    ast_error(typearg, "too many type arguments");
    return false;
  }

  return true;
}

static bool replace_alias(ast_t* def, ast_t* nominal, ast_t* typeargs)
{
  // see if this is a type alias
  if(ast_id(def) != TK_TYPE)
    return true;

  ast_t* alias_list = ast_childidx(def, 3);

  // if this type alias has no alias list, we're done
  // so type None stays as None
  // but type Bool is (True | False) must be replaced with (True | False)
  if(ast_id(alias_list) == TK_NONE)
    return true;

  // we should have a single TYPEDEF as our alias
  assert(ast_id(alias_list) == TK_TYPES);
  ast_t* alias = ast_child(alias_list);
  assert(ast_sibling(alias) == NULL);

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* r_alias;

  if(ast_id(typeparams) == TK_NONE)
  {
    // if we weren't expecting type arguments, we shouldn't have supplied any
    if(ast_id(typeargs) != TK_NONE)
    {
      ast_error(typeargs,
        "supplied type arguments where none were needed");
      return false;
    }

    // duplicate the alias and use that directly
    r_alias = ast_dup(alias);
  } else {
    // reify the alias with the type parameters
    r_alias = reify_type(alias, typeparams, typeargs);

    if(r_alias == NULL)
    {
      ast_error(ast_parent(typeargs), "couldn't reify type parameters");
      return false;
    }
  }

  // the thing being swapped in must be a TYPEDEF
  assert(ast_id(r_alias) == TK_TYPEDEF);

  // the thing being swapped out must be a TYPEDEF
  ast_t* type_def = ast_parent(nominal);
  assert(ast_id(type_def) == TK_TYPEDEF);

  // use the new ast and free the old one
  ast_swap(type_def, r_alias);
  ast_free(type_def);

  return true;
}

/**
 * This checks that all explicit types are valid.
 */
bool type_valid(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_TYPE:
      // TODO: constraints must be valid
      break;

    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      return attach_traits(ast);

    case TK_NOMINAL:
    {
      // TODO: capability
      ast_t* def = nominal_def(ast);

      if(def == NULL)
        return false;

      // make sure our typeargs are subtypes of our constraints
      ast_t* typeargs = ast_childidx(ast, 1);

      if(!check_constraints(def, typeargs))
        return false;

      if(!replace_alias(def, ast, typeargs))
        return false;

      return true;
    }

    case TK_ASSIGN:
      // TODO: check for syntactic sugar for update
      break;

    case TK_RECOVER:
      // TODO: remove things from following scope
      break;

    case TK_FOR:
      // TODO: syntactic sugar for a while loop
      break;

    default: {}
  }

  return true;
}
