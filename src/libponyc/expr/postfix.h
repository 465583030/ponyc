#ifndef EXPR_POSTFIX_H
#define EXPR_POSTFIX_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

bool expr_qualify(pass_opt_t* opt, ast_t* ast);
bool expr_dot(pass_opt_t* opt, ast_t* ast);
bool expr_call(pass_opt_t* opt, ast_t* ast);
bool expr_ffi(ast_t* ast);

PONY_EXTERN_C_END

#endif
