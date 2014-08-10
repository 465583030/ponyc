#ifndef CODEGEN_GENLITERAL_H
#define CODEGEN_GENLITERAL_H

#include "codegen.h"

LLVMValueRef gen_this(compile_t* c, ast_t* ast);

LLVMValueRef gen_param(compile_t* c, ast_t* ast);

LLVMValueRef gen_fieldptr(compile_t* c, ast_t* ast);

LLVMValueRef gen_fieldload(compile_t* c, ast_t* ast);

LLVMValueRef gen_tuple(compile_t* c, ast_t* ast);

LLVMValueRef gen_localdecl(compile_t* c, ast_t* ast);

LLVMValueRef gen_localptr(compile_t* c, ast_t* ast);

LLVMValueRef gen_localload(compile_t* c, ast_t* ast);

LLVMValueRef gen_string(compile_t* c, ast_t* ast);

#endif
