#include "traits.h"
#include "../ast/token.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../type/assemble.h"
#include <assert.h>

/** We use a 3 stage process to flatten traits for each concrete type.
 *
 * 1. We process the traits used by the type. For each trait we add methods
 *    from any other trait the base trait provides to the base trait. These are
 *    not added to the symbol table as duplicates may be legal.
 * 2. We go through all methods of the traits the concrete type provides
 *    looking for any that have bodies that are not overridden by the concrete
 *    type. Such methods are added to the concrete type's symbol table,
 *    duplicates are an error.
 * 3. We go through all methods of the traits the concrete type provides again,
 *    checking that they are compatible with the corresponding body. Any method
 *    found without a corresponding body is an error.
 */


typedef bool(*trait_method_fn)(ast_t* target, ast_t* method);

static bool build_trait_def(ast_t* trait);


/// Add the given method definition to the given entity
static void add_method(ast_t* target, ast_t* method)
{
  ast_t* existing_members = ast_childidx(target, 4);

  // If the method being added has no implementation, accept it.
  ast_t* method_body = ast_childidx(method, 6);

  if(ast_id(method_body) == TK_NONE)
  {
    ast_append(existing_members, method);
    return;
  }

  // Find existing method with this name.
  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* existing = ast_get(target, name, NULL);

  if(existing == NULL)
  {
    // If there's no existing method, accept it.
    ast_append(existing_members, method);
    return;
  }

  // If the existing method has no implementation, accept it.
  ast_t* existing_body = ast_childidx(method, 6);

  if(ast_id(existing_body) == TK_NONE)
  {
    ast_append(existing_members, method);
    return;
  }

  // Strip the incoming implementation before accepting it.
  method = ast_dup(method);
  method_body = ast_childidx(method, 6);

  ast_t* none = ast_from(method_body, TK_NONE);
  ast_replace(&method_body, none);
  ast_append(existing_members, method);
}


/// Execute the given function for each reified method in each trait the target
/// node provides
static bool foreach_provided_method(ast_t* target, trait_method_fn fn)
{
  assert(ast_id(target) == TK_ACTOR || ast_id(target) == TK_CLASS ||
    ast_id(target) == TK_PRIMITIVE || ast_id(target) == TK_TRAIT ||
    ast_id(target) == TK_INTERFACE);
  assert(fn != NULL);

  ast_t* trait_refs = ast_childidx(target, 3);

  for(ast_t* t = ast_child(trait_refs); t != NULL; t = ast_sibling(t))
  {
    assert(ast_id(t) == TK_NOMINAL);
    ast_t* trait_def = (ast_t*)ast_data(t);
    assert(trait_def != NULL);

    if((ast_id(trait_def) != TK_TRAIT) && (ast_id(trait_def) != TK_INTERFACE))
      return false;

    if(!build_trait_def(trait_def))
      return false;

    ast_t* type_params = ast_childidx(trait_def, 1);
    ast_t* type_args = ast_childidx(t, 2);
    ast_t* methods = ast_childidx(trait_def, 4);

    for(ast_t* m = ast_child(methods); m != NULL; m = ast_sibling(m))
    {
      assert(ast_id(m) == TK_FUN || ast_id(m) == TK_BE);

      // Reify the method with the type parameters from trait definition and
      // the reified type arguments from trait reference
      ast_t* r_method = reify(m, type_params, type_args);

      if(!fn(target, r_method))
        return false;
    }
  }

  return true;
}


/// Add trait method body to target trait
static bool attach_method_to_trait(ast_t* target, ast_t* method)
{
  assert(target != NULL);
  assert(method != NULL);

  add_method(target, method);
  return true;
}


/// Build up a trait definition to include all the methods it gets from other
/// traits
static bool build_trait_def(ast_t* trait)
{
  assert((ast_id(trait) == TK_TRAIT) || (ast_id(trait) == TK_INTERFACE));
  ast_state_t state = (ast_state_t)(uint64_t)ast_data(trait);

  // Check for recursive definitions
  switch(state)
  {
    case AST_STATE_INITIAL:
      ast_setdata(trait, (void*)AST_STATE_INPROGRESS);
      break;

    case AST_STATE_INPROGRESS:
      ast_error(trait, "traits can't be recursive");
      return false;

    case AST_STATE_DONE:
      return true;

    default:
      assert(0);
      return false;
  }

  ast_t* methods = ast_childidx(trait, 4);

  for(ast_t* m = ast_child(methods); m != NULL; m = ast_sibling(m))
  {
    // Point method to original trait
    ast_setdata(m, trait);
  }

  if(!foreach_provided_method(trait, attach_method_to_trait))
    return false;

  ast_setdata(trait, (void*)AST_STATE_DONE);
  return true;
}


/// Add trait method with body to target concrete type
static bool attach_body_to_concrete(ast_t* target, ast_t* method)
{
  assert(target != NULL);
  assert(method != NULL);
  assert(ast_childidx(method, 6) != NULL);

  if(ast_id(ast_childidx(method, 6)) == TK_NONE) // Method has no body
    return true;

  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* existing_body = ast_get(target, name, NULL);

  if(existing_body == NULL)
  {
    // First body we've found for this name, use it
    ast_t* existing_members = ast_childidx(target, 4);
    ast_append(existing_members, method);

    if(!ast_set(target, name, method, SYM_NONE))
    {
      ast_t* previous = ast_get_case(target, name, NULL);
      assert(previous != NULL);

      ast_error(method, "method name differs only in case");
      ast_error(previous, "previous definition is here");
      return false;
    }

    return true;
  }

  if(ast_data(existing_body) == NULL)
  {
    // Existing body is from the target type, use that
    return true;
  }

  ast_error(method, "conflicting implementation for %s in %s", name,
    ast_name(ast_child(target)));

  ast_error(existing_body, "previous implementation is here");
  return false;
}


/// Check the given method signature is compatible with the target's definition
static bool check_sig_with_body(ast_t* target, ast_t* method)
{
  if(ast_id(method) == TK_BE)
  {
    switch(ast_id(target))
    {
      case TK_PRIMITIVE:
        ast_error(target, "a primitive can't have traits that have behaviours");
        return false;

      case TK_CLASS:
        ast_error(target, "a class can't have traits that have behaviours");
        return false;

      default:
        break;
    }
  }

  // Find existing method with this name
  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* existing = ast_get(target, name, NULL);

  if(existing == NULL)
  {
    ast_error(target, "method '%s' has no implementation", name);
    return false;
  }

  // Check our version is a subtype of the implementation used
  if(!is_subtype(existing, method))
  {
    ast_error(existing, "existing method is not a subtype of trait method");
    ast_error(method, "trait method is here");
    return false;
  }

  return true;
}


ast_result_t pass_traits(ast_t** astp, pass_opt_t* options)
{
  (void)options;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      if(!build_trait_def(ast))
        return AST_ERROR;

      break;

    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      if(!foreach_provided_method(ast, attach_body_to_concrete) ||
        !foreach_provided_method(ast, check_sig_with_body))
        return AST_ERROR;

      break;

    default:
      break;
  }

  return AST_OK;
}
