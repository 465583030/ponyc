#include "../ast/ast.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include "../ds/stringtab.h"
#include <string.h>
#include <assert.h>


// use "[scheme:]locator" [as name] [where condition]


typedef bool (*use_handler_t)(ast_t* use, const char* locator,
  const char* name);


typedef struct use_t
{
  const char* scheme; // Interned textual identifier including :
  int scheme_len;     // Number of characters in scheme, including :
  bool allow_as;      // Is the "as" clause allowed
  bool allow_cond;    // Is the "where" clause allowed
  use_handler_t handler;
  struct use_t* next;
} use_t;


static bool eval_condition(ast_t* ast, bool* error);


use_t* handlers = NULL;
use_t* default_handler = NULL;


// Find the handler for the given URI
static use_t* find_handler(ast_t* uri, const char** out_locator)
{
  assert(uri != NULL);
  assert(out_locator != NULL);
  assert(ast_id(uri) == TK_STRING);

  const char* text = ast_name(uri);
  const char* colon = strchr(text, ':');

  if(colon == NULL)
  {
    // No scheme specified, use default
    *out_locator = text;
    return default_handler;
  }

  int scheme_len = (int)(colon - text + 1);  // +1 for colon

  // Search for matching handler
  for(use_t* p = handlers; p != NULL; p = p->next)
  {
    if(p->scheme_len == scheme_len &&
      strncmp(p->scheme, text, scheme_len) == 0)
    {
      // Matching scheme found
      *out_locator = colon + 1;
      return p;
    }
  }

  // No match found
#ifdef PLATFORM_IS_WINDOWS
  if(colon == text + 1)
  {
    // Special case error message
    ast_error(uri, "Use scheme %c: not found. "
      "If this is an absolute path use prefix \"file:\"", text[0]);
    return NULL;
  }
#endif

  ast_error(uri, "Use scheme %.*s not found", scheme_len, text);
  return NULL;
}


// Evalute the children of the given binary condition node
static void eval_binary(ast_t* ast, bool* out_left, bool* out_right,
  bool* error)
{
  assert(ast != NULL);
  assert(out_left != NULL);
  assert(out_right != NULL);

  AST_GET_CHILDREN(ast, left, right);
  *out_left = eval_condition(left, error);
  *out_right = eval_condition(right, error);
}


// Evaluate the given condition AST
static bool eval_condition(ast_t* ast, bool* error)
{
  assert(ast != NULL);

  bool left, right;

  switch(ast_id(ast))
  {
  case TK_AND:
    eval_binary(ast, &left, &right, error);
    return left & right;

  case TK_OR:
    eval_binary(ast, &left, &right, error);
    return left | right;

  case TK_XOR:
    eval_binary(ast, &left, &right, error);
    return left ^ right;

  case TK_NOT:
    return !eval_condition(ast_child(ast), error);

  case TK_TUPLE:
    return eval_condition(ast_child(ast), error);

  case TK_REFERENCE:
    {
      //const char* name = ast_name(ast_child(ast));

    }
    return true;

  default:
    assert(0);
    return false;
  }
}


/// Register the standard use handlers
void use_register_std()
{
  // TODO
}


/** Register a use handler. The first handler registered is the default.
 * @param scheme Scheme identifier string, excluding trailing :. String only
 * needs to be valid for duration of call.
 */
void use_register_handler(const char* scheme, bool allow_as,
  bool allow_cond, use_handler_t handler)
{
  use_t* s = (use_t*)calloc(1, sizeof(use_t));
  s->scheme = stringtab(scheme);
  s->allow_as = allow_as;
  s->allow_cond = allow_cond;
  s->handler = handler;
  s->next = handlers;

  handlers = s;

  if(default_handler == NULL)
    default_handler = s;
}


/// Unregister and free all registered use handlers
void use_clear_handlers()
{
  use_t* p = handlers;

  while(p != NULL)
  {
    use_t* next = p->next;
    free(p);
    p = next;
  }

  handlers = NULL;
  default_handler = NULL;
}


/// Process a use command
bool use_command(ast_t* ast)
{
  if(handlers == NULL)
  {
    ast_error(ast, "Internal error: no use scheme handlers registered");
    return false;
  }

  assert(default_handler != NULL);

  AST_GET_CHILDREN(ast, uri, alias, condition);

  const char* locator;
  const char* name = NULL;
  use_t* handler = find_handler(uri, &locator);

  if(handler == NULL) // Scheme not found
    return false;

  if(ast_id(alias) != TK_NONE)
  {
    if(!handler->allow_as)
    {
      ast_error(alias, "Use scheme %s may not have an alias", handler->scheme);
      return false;
    }

    name = ast_name(alias);
  }

  if(ast_id(condition) != TK_NONE)
  {
    if(!handler->allow_cond)
    {
      ast_error(alias, "Use scheme %s may not have a condition",
        handler->scheme);
      return false;
    }

    if(!eval_condition(condition, NULL)) // Condition false, ignore use command
      return true;
  }

  return handler->handler(ast, locator, name);
}




/**
* Import a package, either with a qualifying name or by merging it into the
* current scope.
*/
ast_t* use_package(ast_t* ast, ast_t* name, const char* path)
{
  ast_t* package = package_load(ast, path, NULL);

  if(package == ast)
    return package;

  if(package == NULL)
  {
    ast_error(ast, "can't load package '%s'", path);
    return NULL;
  }

  if(name && ast_id(name) == TK_ID)
  {
    //if(!set_scope(ast, name, package, NULL))
    //  return NULL;

    return package;
  }

  assert((name == NULL) || (ast_id(name) == TK_NONE));

  if(!ast_merge(ast, package))
  {
    ast_error(ast, "can't merge symbols from '%s'", path);
    return NULL;
  }

  return package;
}
