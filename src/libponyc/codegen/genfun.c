#include "genfun.h"
#include "gentype.h"
#include "genname.h"
#include "gencall.h"
#include "gencontrol.h"
#include "genexpr.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../type/lookup.h"
#include "../ds/hash.h"
#include <string.h>
#include <assert.h>

static void name_params(ast_t* params, LLVMValueRef func, bool ctor)
{
  int count = 0;

  // name the receiver 'this'
  if(!ctor)
  {
    LLVMValueRef fparam = LLVMGetParam(func, count++);
    LLVMSetValueName(fparam, "this");
  }

  // name each parameter
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    LLVMValueRef fparam = LLVMGetParam(func, count++);
    LLVMSetValueName(fparam, ast_name(ast_child(param)));
    param = ast_sibling(param);
  }
}

static bool gen_field_init(compile_t* c, gentype_t* g)
{
  LLVMValueRef this_ptr = LLVMGetParam(codegen_fun(c), 0);

  ast_t* def = (ast_t*)ast_data(g->ast);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  // Struct index of the current field.
  int index = 1;

  if(ast_id(def) == TK_ACTOR)
    index++;

  // Iterate through all fields.
  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        // Skip this field if it has no initialiser.
        AST_GET_CHILDREN(member, id, type, body);

        if(ast_id(body) != TK_NONE)
        {
          // Reify the initialiser.
          ast_t* var = lookup(NULL, g->ast, ast_name(id));
          assert(var != NULL);
          body = ast_childidx(var, 2);

          // Get the field pointer.
          LLVMValueRef l_value = LLVMBuildStructGEP(c->builder, this_ptr, index,
            "");

          // Cast the initialiser to the field type.
          LLVMValueRef r_value = gen_expr(c, body);

          if(r_value == NULL)
            return false;

          LLVMTypeRef l_type = LLVMGetElementType(LLVMTypeOf(l_value));
          LLVMValueRef cast_value = gen_assign_cast(c, l_type, r_value,
            ast_type(body));

          if(cast_value == NULL)
            return false;

          // Store the result.
          LLVMBuildStore(c->builder, cast_value, l_value);
        }

        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return true;
}

static ast_t* get_fun(gentype_t* g, const char* name, ast_t* typeargs)
{
  // reify with both the type and the function-level typeargs
  ast_t* fun = lookup(NULL, g->ast, name);
  assert(fun != NULL);

  if(typeargs != NULL)
  {
    ast_t* typeparams = ast_childidx(fun, 2);
    ast_t* r_fun = reify(fun, typeparams, typeargs);
    ast_free_unattached(fun);
    fun = r_fun;
    assert(fun != NULL);
  }

  return fun;
}

static LLVMValueRef get_prototype(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, ast_t* fun)
{
  // Get a fully qualified name: starts with the type name, followed by the
  // type arguments, followed by the function name, followed by the function
  // level type arguments.
  const char* funname = genname_fun(g->type_name, name, typeargs);

  // If the function already exists, just return it.
  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);

  if(func != NULL)
    return func;

  // count the parameters, including the receiver
  ast_t* params = ast_childidx(fun, 3);
  size_t count = ast_childcount(params) + 1;

  VLA(LLVMTypeRef, tparams, count);
  count = 0;

  // get a type for the receiver
  tparams[count++] = g->use_type;

  // get a type for each parameter
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* ptype = ast_childidx(param, 1);
    gentype_t ptype_g;

    if(!gentype(c, ptype, &ptype_g))
    {
      ast_error(ptype, "couldn't generate parameter type");
      return NULL;
    }

    tparams[count++] = ptype_g.use_type;
    param = ast_sibling(param);
  }

  // get a type for the result
  ast_t* rtype = ast_childidx(fun, 4);
  gentype_t rtype_g;

  if(!gentype(c, rtype, &rtype_g))
  {
    ast_error(rtype, "couldn't generate result type");
    return NULL;
  }

  LLVMTypeRef result = rtype_g.use_type;

  // generate the function type and the function prototype
  LLVMTypeRef ftype;

  // don't include the receiver for constructors
  if(ast_id(fun) == TK_NEW)
    ftype = LLVMFunctionType(result, &tparams[1], (unsigned int)(count - 1),
      false);
  else
    ftype = LLVMFunctionType(result, tparams, (unsigned int)count, false);

  func = codegen_addfun(c, funname, ftype);
  name_params(params, func, ast_id(fun) == TK_NEW);

  if(ast_id(fun) != TK_FUN)
  {
    // handlers always have a receiver and have no return value
    ftype = LLVMFunctionType(c->void_type, tparams, (int)count, false);
    const char* handler_name = genname_handler(g->type_name, name, typeargs);

    LLVMValueRef handler = codegen_addfun(c, handler_name, ftype);
    name_params(params, handler, false);
  }

  return func;
}

static LLVMValueRef get_handler(compile_t* c, gentype_t* g, const char* name,
  ast_t* typeargs)
{
  const char* handler_name = genname_handler(g->type_name, name, typeargs);
  return LLVMGetNamedFunction(c->module, handler_name);
}

static LLVMValueRef gen_newhandler(compile_t* c, gentype_t* g, const char* name,
  ast_t* typeargs, ast_t* body)
{
  LLVMValueRef handler = get_handler(c, g, name, typeargs);

  if(handler == NULL)
    return NULL;

  codegen_startfun(c, handler);

  if(!gen_field_init(c, g))
    return NULL;

  LLVMValueRef value = gen_seq(c, body);

  if(value == NULL)
    return NULL;

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
  return handler;
}

static void set_descriptor(compile_t* c, gentype_t* g, LLVMValueRef this_ptr)
{
  LLVMSetValueName(this_ptr, "this");
  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 0, "");
  LLVMBuildStore(c->builder, g->desc, desc_ptr);
}

static LLVMTypeRef send_message(compile_t* c, ast_t* fun, LLVMValueRef to,
  LLVMValueRef func, int index, bool ctor)
{
  // We need three extra slots if we're a constructor, but only two otherwise,
  // since we can reuse the receiver slot.
  int extra = ctor ? 3 : 2;

  // Get the parameter types.
  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(func));
  int count = LLVMCountParamTypes(f_type) + extra;
  VLA(LLVMTypeRef, f_params, count);
  LLVMGetParamTypes(f_type, &f_params[extra]);

  // The first one becomes the message size, the second the message ID.
  f_params[0] = c->i32;
  f_params[1] = c->i32;
  f_params[2] = c->void_ptr;
  LLVMTypeRef msg_type = LLVMStructTypeInContext(c->context, f_params, count,
    false);
  LLVMTypeRef msg_type_ptr = LLVMPointerType(msg_type, 0);

  // Calculate the index (power of 2) for the message size.
  size_t size = LLVMABISizeOfType(c->target_data, msg_type);
  size = next_pow2(size);

  // Subtract 7 because we are looking to make 64 come out to zero.
  if(size <= 64)
    size = 0;
  else
    size = __pony_ffsl(size) - 7;

  // Allocate the message, setting its size and ID.
  LLVMValueRef args[2];
  args[1] = LLVMConstInt(c->i32, size, false);
  args[0] = LLVMConstInt(c->i32, index, false);
  LLVMValueRef msg = gencall_runtime(c, "pony_alloc_msg", args, 2, "");
  LLVMValueRef msg_ptr = LLVMBuildBitCast(c->builder, msg, msg_type_ptr, "");

  // Trace while populating the message contents.
  LLVMValueRef start_trace = gencall_runtime(c, "pony_gc_send", NULL, 0, "");
  ast_t* params = ast_childidx(fun, 3);
  ast_t* param = ast_child(params);
  bool need_trace = false;

  for(int i = 3; i < count; i++)
  {
    LLVMValueRef arg = LLVMGetParam(func, i - extra);
    LLVMValueRef arg_ptr = LLVMBuildStructGEP(c->builder, msg_ptr, i, "");
    LLVMBuildStore(c->builder, arg, arg_ptr);

    need_trace |= gencall_trace(c, arg, ast_type(param));
    param = ast_sibling(param);
  }

  if(need_trace)
    gencall_runtime(c, "pony_send_done", NULL, 0, "");
  else
    LLVMInstructionEraseFromParent(start_trace);

  // Send the message.
  args[0] = LLVMBuildBitCast(c->builder, to, c->object_ptr, "");
  args[1] = msg;
  gencall_runtime(c, "pony_sendv", args, 2, "");

  // Return the type of the message.
  return msg_type_ptr;
}

static void add_dispatch_case(compile_t* c, gentype_t* g, ast_t* fun, int index,
  LLVMValueRef handler, LLVMTypeRef type)
{
  // Add a case to the dispatch function to handle this message.
  codegen_startfun(c, g->dispatch_fn);
  LLVMBasicBlockRef block = codegen_block(c, "handler");
  LLVMValueRef id = LLVMConstInt(c->i32, index, false);
  LLVMAddCase(g->dispatch_switch, id, block);

  // Destructure the message.
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMValueRef msg = LLVMBuildBitCast(c->builder, g->dispatch_msg, type, "");

  int count = LLVMCountParams(handler);
  VLA(LLVMValueRef, args, count);
  LLVMValueRef this_ptr = LLVMGetParam(g->dispatch_fn, 0);
  args[0] = LLVMBuildBitCast(c->builder, this_ptr, g->use_type, "");

  // Trace the message.
  LLVMValueRef start_trace = gencall_runtime(c, "pony_gc_recv", NULL, 0, "");
  ast_t* params = ast_childidx(fun, 3);
  ast_t* param = ast_child(params);
  bool need_trace = false;

  for(int i = 1; i < count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, msg, i + 2, "");
    args[i] = LLVMBuildLoad(c->builder, field, "");

    need_trace |= gencall_trace(c, args[i], ast_type(param));
    param = ast_sibling(param);
  }

  if(need_trace)
    gencall_runtime(c, "pony_recv_done", NULL, 0, "");
  else
    LLVMInstructionEraseFromParent(start_trace);

  // Call the handler.
  LLVMValueRef call = LLVMBuildCall(c->builder, handler, args, count, "");
  LLVMSetInstructionCallConv(call, LLVMFastCallConv);
  LLVMBuildRetVoid(c->builder);

  // Pause, otherwise the optimiser will run on what we have so far.
  codegen_pausefun(c);
}

LLVMValueRef genfun_proto(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  return get_prototype(c, g, name, typeargs, fun);
}

LLVMValueRef genfun_fun(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  if(LLVMCountBasicBlocks(func) != 0)
    return func;

  codegen_startfun(c, func);

  ast_t* body = ast_childidx(fun, 6);
  LLVMValueRef value = gen_seq(c, body);

  if(value == NULL)
  {
    return NULL;
  } else if(value != GEN_NOVALUE) {
    LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(func));
    LLVMTypeRef r_type = LLVMGetReturnType(f_type);

    LLVMValueRef ret = gen_assign_cast(c, r_type, value, ast_type(body));
    LLVMBuildRet(c->builder, ret);
  }

  codegen_finishfun(c);
  return func;
}

LLVMValueRef genfun_be(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, int index)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  codegen_startfun(c, func);
  LLVMValueRef this_ptr = LLVMGetParam(func, 0);

  // Send the arguments in a message to 'this'.
  LLVMTypeRef msg_type_ptr = send_message(c, fun, this_ptr, func, index, false);

  // Return 'this'.
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c);

  // Build the handler function.
  LLVMValueRef handler = get_handler(c, g, name, typeargs);

  if(handler == NULL)
    return NULL;

  codegen_startfun(c, handler);

  ast_t* body = ast_childidx(fun, 6);
  LLVMValueRef value = gen_seq(c, body);

  if(value == NULL)
  {
    return NULL;
  } else if(value != GEN_NOVALUE) {
    LLVMBuildRetVoid(c->builder);
  }

  codegen_finishfun(c);

  add_dispatch_case(c, g, fun, index, handler, msg_type_ptr);
  return func;
}

LLVMValueRef genfun_new(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  // Get the handler.
  LLVMValueRef handler = get_handler(c, g, name, typeargs);

  // If we have no handler, it's a builtin function.
  if(handler == NULL)
    return func;

  codegen_startfun(c, func);
  LLVMValueRef this_ptr;

  if(g->instance != NULL)
  {
    // Use the global instance.
    this_ptr = g->instance;
  } else {
    // Allocate the object as 'this'.
    this_ptr = gencall_alloc(c, g->use_type);
    set_descriptor(c, g, this_ptr);
  }

  // Call the handler.
  int count = LLVMCountParamTypes(LLVMGetElementType(LLVMTypeOf(handler)));
  VLA(LLVMValueRef, args, count);
  args[0] = this_ptr;

  for(int i = 1; i < count; i++)
    args[i] = LLVMGetParam(func, i - 1);

  LLVMValueRef call = LLVMBuildCall(c->builder, handler, args, count, "");
  LLVMSetInstructionCallConv(call, LLVMFastCallConv);

  // Return 'this'.
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c);

  // Generate the handler.
  handler = gen_newhandler(c, g, name, typeargs, ast_childidx(fun, 6));

  if(handler == NULL)
    return NULL;

  return func;
}

LLVMValueRef genfun_newbe(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, int index)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  codegen_startfun(c, func);

  // Allocate the actor as 'this'.
  LLVMValueRef this_ptr = gencall_create(c, g);

  // Send the arguments in a message to 'this'.
  LLVMTypeRef msg_type_ptr = send_message(c, fun, this_ptr, func, index, true);

  // Return 'this'.
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c);

  LLVMValueRef handler = gen_newhandler(c, g, name, typeargs,
    ast_childidx(fun, 6));

  if(handler == NULL)
    return NULL;

  add_dispatch_case(c, g, fun, index, handler, msg_type_ptr);
  return func;
}

LLVMValueRef genfun_box(compile_t* c, gentype_t* g)
{
  if(g->primitive == NULL)
    return NULL;

  // Create a boxing function.
  const char* box_name = genname_box(g->type_name);
  LLVMValueRef box_fn = LLVMGetNamedFunction(c->module, box_name);

  if(box_fn != NULL)
    return box_fn;

  LLVMTypeRef box_type = LLVMFunctionType(g->structure_ptr, &g->primitive, 1,
    false);
  box_fn = codegen_addfun(c, box_name, box_type);
  codegen_startfun(c, box_fn);

  // allocate the object as 'this'
  LLVMValueRef this_ptr = gencall_alloc(c, g->structure_ptr);
  set_descriptor(c, g, this_ptr);

  // Store the primitive in element 1.
  LLVMValueRef primitive = LLVMGetParam(box_fn, 0);
  LLVMValueRef primitive_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMBuildStore(c->builder, primitive, primitive_ptr);

  // return 'this'
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c);

  return box_fn;
}

bool genfun_methods(compile_t* c, gentype_t* g)
{
  ast_t* def = (ast_t*)ast_data(g->ast);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);
  int be_index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_NEW:
      {
        AST_GET_CHILDREN(member, ignore, id, typeparams);

        if(ast_id(typeparams) != TK_NONE)
        {
          // TODO: polymorphic constructors
          ast_error(typeparams,
            "not implemented (codegen for polymorphic constructors)");
          return false;
        }

        LLVMValueRef fun;

        switch(g->underlying)
        {
          case TK_ACTOR:
            fun = genfun_newbe(c, g, ast_name(id), NULL, be_index++);
            break;

          default:
            fun = genfun_new(c, g, ast_name(id), NULL);
        }

        if(fun == NULL)
          return false;

        break;
      }

      case TK_BE:
      {
        AST_GET_CHILDREN(member, ignore, id, typeparams);

        if(ast_id(typeparams) != TK_NONE)
        {
          // TODO: polymorphic behaviours
          ast_error(typeparams,
            "not implemented (codegen for polymorphic behaviours)");
          return false;
        }

        LLVMValueRef fun = genfun_be(c, g, ast_name(id), NULL, be_index++);

        if(fun == NULL)
          return false;

        break;
      }

      case TK_FUN:
      {
        AST_GET_CHILDREN(member, ignore, id, typeparams);

        if(ast_id(typeparams) != TK_NONE)
        {
          // TODO: polymorphic functions
          ast_error(typeparams,
            "not implemented (codegen for polymorphic functions)");
          return false;
        }

        LLVMValueRef fun = genfun_fun(c, g, ast_name(id), NULL);

        if(fun == NULL)
          return false;

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return true;
}
