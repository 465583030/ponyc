#ifndef EXPR_REFERENCE_H
#define EXPR_REFERENCE_H

#include "../ast/ast.h"

bool expr_field(ast_t* ast);
bool expr_fieldref(ast_t* ast, ast_t* left, ast_t* find, token_id t);
bool expr_typeref(ast_t* ast);
bool expr_reference(ast_t* ast);
bool expr_local(ast_t* ast);
bool expr_idseq(ast_t* ast);

#endif
