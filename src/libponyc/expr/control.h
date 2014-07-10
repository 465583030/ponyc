#ifndef EXPR_CONTROL_H
#define EXPR_CONTROL_H

#include "../ast/ast.h"

bool expr_seq(ast_t* ast);
bool expr_if(ast_t* ast);
bool expr_while(ast_t* ast);
bool expr_repeat(ast_t* ast);
bool expr_try(ast_t* ast);
bool expr_break(ast_t* ast);
bool expr_continue(ast_t* ast);
bool expr_return(ast_t* ast);

#endif
