#include "gencall.h"
#include "gentype.h"
#include "genexpr.h"
#include "genfun.h"
#include "genname.h"
#include <assert.h>

static LLVMValueRef call_fun(compile_t* c, LLVMValueRef fun, LLVMValueRef* args,
  int count, const char* ret)
{
  if(fun == NULL)
    return NULL;

  return LLVMBuildCall(c->builder, fun, args, count, ret);
}

static LLVMValueRef make_arg(compile_t* c, ast_t* arg, LLVMTypeRef type)
{
  LLVMValueRef value = gen_expr(c, arg);

  if(value == NULL)
    return NULL;

  // TODO: how to determine if the parameter is signed or not
  return gen_assign_cast(c, ast_type(arg), type, value, false);
}

LLVMValueRef gen_call(compile_t* c, ast_t* ast)
{
  ast_t* postfix;
  ast_t* positional;
  ast_t* named;
  AST_GET_CHILDREN(ast, &postfix, &positional, &named);

  int need_receiver;

  switch(ast_id(postfix))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
      need_receiver = 0;
      break;

    case TK_BEREF:
    case TK_FUNREF:
      need_receiver = 1;
      break;

    default:
      assert(0);
      return NULL;
  }

  ast_t* receiver;
  ast_t* method;
  ast_t* typeargs = NULL;
  AST_GET_CHILDREN(postfix, &receiver, &method);

  // dig through function qualification
  if(ast_id(receiver) == TK_FUNREF)
  {
    AST_GET_CHILDREN(receiver, &receiver, &typeargs);
  }

  if(typeargs != NULL)
  {
    ast_error(typeargs,
      "not implemented (codegen for polymorphic methods)");
    return NULL;
  }

  ast_t* type = ast_type(receiver);
  LLVMTypeRef l_type = gentype(c, type);

  if(l_type == NULL)
    return NULL;

  LLVMValueRef l_value;

  if(need_receiver == 1)
    l_value = gen_expr(c, receiver);

  // static or virtual call?
  const char* method_name = ast_name(method);
  LLVMTypeRef f_type;
  LLVMValueRef func;

  if(l_type == c->object_ptr)
  {
    // virtual, get the function by selector colour
    int colour = painter_get_colour(c->painter, method_name);

    // cast the field to a generic object pointer
    l_value = LLVMBuildBitCast(c->builder, l_value, c->object_ptr, "object");

    // get the type descriptor from the object pointer
    LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, l_value, 0, "");
    LLVMValueRef desc = LLVMBuildLoad(c->builder, desc_ptr, "desc");

    // get the function from the vtable
    LLVMValueRef vtable = LLVMBuildStructGEP(c->builder, desc, 7, "");

    LLVMValueRef index[2];
    index[0] = LLVMConstInt(LLVMInt32Type(), 0, false);
    index[1] = LLVMConstInt(LLVMInt32Type(), colour, false);

    LLVMValueRef func_ptr = LLVMBuildGEP(c->builder, vtable, index, 2, "");
    func = LLVMBuildLoad(c->builder, func_ptr, "");

    // cast to the right function type
    f_type = genfun_proto(c, type, method_name, typeargs);
    func = LLVMBuildBitCast(c->builder, func, f_type, "method");
  } else {
    // static, get the actual function
    const char* type_name = genname_type(type);
    const char* name = genname_fun(type_name, method_name, NULL);
    func = LLVMGetNamedFunction(c->module, name);

    if(func == NULL)
    {
      ast_error(ast, "couldn't locate '%s'", name);
      return NULL;
    }

    f_type = LLVMTypeOf(func);
  }

  int count = ast_childcount(positional) + need_receiver;

  PONY_VL_ARRAY(LLVMValueRef, args, count);
  PONY_VL_ARRAY(LLVMTypeRef, params, count);
  LLVMGetParamTypes(LLVMGetElementType(f_type), params);

  if(need_receiver == 1)
  {
    LLVMValueRef value = make_arg(c, receiver, params[0]);

    if(value == NULL)
      return NULL;

    args[0] = value;
  }

  ast_t* arg = ast_child(positional);

  for(int i = need_receiver; i < count; i++)
  {
    LLVMValueRef value = make_arg(c, arg, params[i]);

    if(value == NULL)
      return NULL;

    args[i] = value;
    arg = ast_sibling(arg);
  }

  return call_fun(c, func, args, count, "");
}

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret)
{
  return call_fun(c, LLVMGetNamedFunction(c->module, name), args, count, ret);
}

LLVMValueRef gencall_alloc(compile_t* c, LLVMTypeRef type)
{
  LLVMTypeRef l_type = LLVMGetElementType(type);
  size_t size = LLVMABISizeOfType(c->target, l_type);

  LLVMValueRef args[1];
  args[0] = LLVMConstInt(LLVMInt64Type(), size, false);

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 1, "");
  return LLVMBuildBitCast(c->builder, result, type, "");
}

void gencall_tracetag(compile_t* c, LLVMValueRef field)
{
  // load the contents of the field
  LLVMValueRef field_val = LLVMBuildLoad(c->builder, field, "");

  // cast the field to a generic object pointer
  LLVMValueRef args[1];
  args[0] = LLVMBuildBitCast(c->builder, field_val, c->object_ptr, "");

  gencall_runtime(c, "pony_trace", args, 1, "");
}

void gencall_traceactor(compile_t* c, LLVMValueRef field)
{
  // load the contents of the field
  LLVMValueRef field_val = LLVMBuildLoad(c->builder, field, "");

  // cast the field to a pony_actor_t*
  LLVMValueRef args[1];
  args[0] = LLVMBuildBitCast(c->builder, field_val, c->actor_ptr, "");

  gencall_runtime(c, "pony_traceactor", args, 1, "");
}

void gencall_traceknown(compile_t* c, LLVMValueRef field, const char* name)
{
  // load the contents of the field
  LLVMValueRef field_val = LLVMBuildLoad(c->builder, field, "");

  // cast the field to a generic object pointer
  LLVMValueRef args[2];
  args[0] = LLVMBuildBitCast(c->builder, field_val, c->object_ptr, "");

  // get the trace function statically
  const char* fun = genname_trace(name);
  args[1] = LLVMGetNamedFunction(c->module, fun);

  gencall_runtime(c, "pony_traceobject", args, 2, "");
}

void gencall_traceunknown(compile_t* c, LLVMValueRef field)
{
  // load the contents of the field
  LLVMValueRef field_val = LLVMBuildLoad(c->builder, field, "");

  // cast the field to a generic object pointer
  LLVMValueRef args[2];
  args[0] = LLVMBuildBitCast(c->builder, field_val, c->object_ptr, "object");

  // get the type descriptor from the object pointer
  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, args[0], 0, "");
  LLVMValueRef desc = LLVMBuildLoad(c->builder, desc_ptr, "desc");

  // determine if this is an actor or not
  LLVMValueRef dispatch_ptr = LLVMBuildStructGEP(c->builder, desc, 3, "");
  LLVMValueRef dispatch = LLVMBuildLoad(c->builder, dispatch_ptr, "dispatch");
  LLVMValueRef is_object = LLVMBuildIsNull(c->builder, dispatch, "is_object");

  // build a conditional
  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(fun, "then");
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(fun, "else");
  LLVMBasicBlockRef merge_block = LLVMAppendBasicBlock(fun, "merge");

  LLVMBuildCondBr(c->builder, is_object, then_block, else_block);

  // if we're an object
  LLVMPositionBuilderAtEnd(c->builder, then_block);

  // get the trace function from the type descriptor
  LLVMValueRef trace_ptr = LLVMBuildStructGEP(c->builder, desc, 0, "");
  args[1] = LLVMBuildLoad(c->builder, trace_ptr, "trace");

  gencall_runtime(c, "pony_traceobject", args, 2, "");
  LLVMBuildBr(c->builder, merge_block);

  // if we're an actor
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  args[0] = LLVMBuildBitCast(c->builder, field_val, c->actor_ptr, "actor");
  gencall_runtime(c, "pony_traceactor", args, 1, "");
  LLVMBuildBr(c->builder, merge_block);

  // continue in the merge block
  LLVMPositionBuilderAtEnd(c->builder, merge_block);
}
