#ifndef TYPE_LOOKUP_H
#define TYPE_LOOKUP_H

#include "../ast/ast.h"

ast_t* lookup(ast_t* type, const char* name);

/**
 * Get the nth typedef out of a tuple definition.
 */
ast_t* lookup_tuple(ast_t* ast, int index);

#endif
