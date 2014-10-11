#ifndef CODEGEN_GENDESC_H
#define CODEGEN_GENDESC_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

LLVMTypeRef gendesc_type(compile_t* c, gentype_t* g);

void gendesc_init(compile_t* c, gentype_t* g);

LLVMValueRef gendesc_fetch(compile_t* c, LLVMValueRef object);

LLVMValueRef gendesc_trace(compile_t* c, LLVMValueRef object);

LLVMValueRef gendesc_dispatch(compile_t* c, LLVMValueRef object);

LLVMValueRef gendesc_vtable(compile_t* c, LLVMValueRef object, size_t colour);

LLVMValueRef gendesc_fieldcount(compile_t* c, LLVMValueRef object);

LLVMValueRef gendesc_fielddesc(compile_t* c, LLVMValueRef object, size_t index);

LLVMValueRef gendesc_typeid(compile_t* c, ast_t* type);

LLVMValueRef gendesc_isdesc(compile_t* c, LLVMValueRef object,
  LLVMValueRef desc);

LLVMValueRef gendesc_istrait(compile_t* c, LLVMValueRef object, ast_t* trait);

PONY_EXTERN_C_END

#endif
