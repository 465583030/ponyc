#ifndef TYPE_EXPR_H
#define TYPE_EXPR_H

#include "../ast/ast.h"

ast_result_t type_expr(ast_t* ast, int verbose);

ast_t* typedef_for_name(ast_t* ast, const char* package, const char* name);

#endif
