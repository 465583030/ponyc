#include "genname.h"
#include "../ds/stringtab.h"
#include <string.h>
#include <assert.h>

static void name_append(char* name, const char* append)
{
  pony_strcat(name, "_");
  pony_strcat(name, append);
}

static size_t typeargs_len(ast_t* typeargs)
{
  if(typeargs == NULL)
    return 0;

  ast_t* typearg = ast_child(typeargs);
  size_t len = 0;

  while(typearg != NULL)
  {
    const char* argname = genname_type(typearg);
    len += strlen(argname) + 1;
    typearg = ast_sibling(typearg);
  }

  return len;
}

static void typeargs_append(char* name, ast_t* typeargs)
{
  if(typeargs == NULL)
    return;

  ast_t* typearg = ast_child(typeargs);

  while(typearg != NULL)
  {
    name_append(name, genname_type(typearg));
    typearg = ast_sibling(typearg);
  }
}

static const char* build_name(const char* a, const char* b, ast_t* typeargs)
{
  size_t len = typeargs_len(typeargs);

  if(a != NULL)
    len += strlen(a) + 1;

  if(b != NULL)
    len += strlen(b) + 1;

  PONY_VL_ARRAY(char, name, len);

  if(a != NULL)
    pony_strcpy(name, a);

  if(b != NULL)
  {
    if(a != NULL)
      name_append(name, b);
    else
      pony_strcpy(name, b);
  }

  typeargs_append(name, typeargs);
  return stringtab(name);
}

static const char* nominal_name(ast_t* ast)
{
  AST_GET_CHILDREN(ast, package, name, typeargs);

  return build_name(ast_name(package), ast_name(name), typeargs);
}

const char* genname_type(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_STRUCTURAL:
      return stringtab("$object");

    case TK_TUPLETYPE:
      return build_name("$1", "$tuple", ast);

    case TK_NOMINAL:
      return nominal_name(ast);

    default: {}
  }

  assert(0);
  return NULL;
}

const char* genname_trace(const char* type)
{
  return build_name(type, "$trace", NULL);
}

const char* genname_serialise(const char* type)
{
  return build_name(type, "$serialise", NULL);
}

const char* genname_deserialise(const char* type)
{
  return build_name(type, "$deserialise", NULL);
}

const char* genname_dispatch(const char* type)
{
  return build_name(type, "$dispatch", NULL);
}

const char* genname_finalise(const char* type)
{
  return build_name(type, "$finalise", NULL);
}

const char* genname_descriptor(const char* type)
{
  return build_name(type, "$desc", NULL);
}

const char* genname_instance(const char* type)
{
  return build_name(type, "$inst", NULL);
}

const char* genname_fun(const char* type, const char* name, ast_t* typeargs)
{
  return build_name(type, name, typeargs);
}

const char* genname_handler(const char* type, const char* name,
  ast_t* typeargs)
{
  const char* handler_name = build_name(name, "$handler", NULL);
  return genname_fun(type, handler_name, typeargs);
}

const char* genname_unbox(const char* name)
{
  return build_name(name, "$unbox", NULL);
}
