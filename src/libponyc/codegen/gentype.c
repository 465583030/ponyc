#include "gentype.h"
#include "genname.h"
#include "gendesc.h"
#include "genprim.h"
#include "gencall.h"
#include "genfun.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void make_box_type(compile_t* c, gentype_t* g)
{
  if(g->structure == NULL)
  {
    const char* box_name = genname_box(g->type_name);
    g->structure = LLVMGetTypeByName(c->module, box_name);

    if(g->structure == NULL)
      g->structure = LLVMStructCreateNamed(c->context, box_name);
  }

  if(LLVMIsOpaqueStruct(g->structure))
  {
    LLVMTypeRef elements[2];
    elements[0] = LLVMPointerType(g->desc_type, 0);
    elements[1] = g->primitive;
    LLVMStructSetBody(g->structure, elements, 2, false);
  }

  g->structure_ptr = LLVMPointerType(g->structure, 0);
}

static void make_global_descriptor(compile_t* c, gentype_t* g)
{
  // Fetch or create a descriptor type.
  if(g->underlying == TK_TUPLETYPE)
  {
    // Tuples have no vtable.
    g->vtable_size = 0;
  } else {
    // Get the vtable size from the painter.
    ast_t* def = (ast_t*)ast_data(g->ast);
    g->vtable_size = painter_get_vtable_size(c->painter, def);
  }

  const char* desc_name = genname_descriptor(g->type_name);
  g->desc_type = gendesc_type(c, desc_name, g->vtable_size);

  // Check for an existing descriptor.
  g->desc = LLVMGetNamedGlobal(c->module, desc_name);

  if(g->desc != NULL)
    return;

  g->desc = LLVMAddGlobal(c->module, g->desc_type, desc_name);
  LLVMSetGlobalConstant(g->desc, true);
}

static void make_global_instance(compile_t* c, gentype_t* g)
{
  // Not a primitive type.
  if(g->underlying != TK_PRIMITIVE)
    return;

  if(g->primitive != NULL)
  {
    // A primitive type, use an uninitialised value.
    if(g->instance == NULL)
      g->instance = LLVMGetUndef(g->primitive);

    return;
  }

  // Check for an existing instance.
  const char* inst_name = genname_instance(g->type_name);
  g->instance = LLVMGetNamedGlobal(c->module, inst_name);

  if(g->instance != NULL)
    return;

  // Create a unique global instance.
  LLVMValueRef args[1];
  args[0] = g->desc;
  LLVMValueRef value = LLVMConstNamedStruct(g->structure, args, 1);

  g->instance = LLVMAddGlobal(c->module, g->structure, inst_name);
  LLVMSetInitializer(g->instance, value);
  LLVMSetGlobalConstant(g->instance, true);
}

/**
 * Return true if the type already exists or if we are only being asked to
 * generate a preliminary type, false otherwise.
 */
static bool setup_name(compile_t* c, ast_t* ast, gentype_t* g, bool prelim)
{
  if(ast_id(ast) == TK_NOMINAL)
  {
    ast_t* def = (ast_t*)ast_data(ast);
    g->underlying = ast_id(def);

    // Find the primitive type, if there is one.
    AST_GET_CHILDREN(ast, pkg, id);
    const char* package = ast_name(pkg);
    const char* name = ast_name(id);

    if(!strcmp(package, "$1"))
    {
      if(!strcmp(name, "True"))
      {
        g->primitive = c->i1;
        g->instance = LLVMConstInt(c->i1, 1, false);
      } else if(!strcmp(name, "False")) {
        g->primitive = c->i1;
        g->instance = LLVMConstInt(c->i1, 0, false);
      } else if(!strcmp(name, "I8"))
        g->primitive = c->i8;
      else if(!strcmp(name, "U8"))
        g->primitive = c->i8;
      else if(!strcmp(name, "I16"))
        g->primitive = c->i16;
      else if(!strcmp(name, "U16"))
        g->primitive = c->i16;
      else if(!strcmp(name, "I32"))
        g->primitive = c->i32;
      else if(!strcmp(name, "U32"))
        g->primitive = c->i32;
      else if(!strcmp(name, "I64"))
        g->primitive = c->i64;
      else if(!strcmp(name, "U64"))
        g->primitive = c->i64;
      else if(!strcmp(name, "I128"))
        g->primitive = c->i128;
      else if(!strcmp(name, "U128"))
        g->primitive = c->i128;
      else if(!strcmp(name, "SIntLiteral"))
        g->primitive = c->i128;
      else if(!strcmp(name, "UIntLiteral"))
        g->primitive = c->i128;
      else if(!strcmp(name, "F32"))
        g->primitive = c->f32;
      else if(!strcmp(name, "F64"))
        g->primitive = c->f64;
      else if(!strcmp(name, "FloatLiteral"))
        g->primitive = c->f64;
      else if(!strcmp(name, "_Pointer"))
        return genprim_pointer(c, g, prelim);
    }
  } else {
    g->underlying = TK_TUPLETYPE;
  }

  // Find or create the structure type.
  g->structure = LLVMGetTypeByName(c->module, g->type_name);

  if(g->structure == NULL)
    g->structure = LLVMStructCreateNamed(c->context, g->type_name);

  bool opaque = LLVMIsOpaqueStruct(g->structure) != 0;

  if(g->underlying == TK_TUPLETYPE)
  {
    // This is actually our primitive type.
    g->primitive = g->structure;
    g->structure = NULL;
  } else {
    g->structure_ptr = LLVMPointerType(g->structure, 0);
  }

  // Fill in our global descriptor.
  make_global_descriptor(c, g);

  if(g->primitive != NULL)
  {
    // We're primitive, so use the primitive type.
    g->use_type = g->primitive;
  } else {
    // We're not primitive, so use a pointer to our structure.
    g->use_type = g->structure_ptr;
  }

  if(!opaque)
  {
    // Fill in our global instance if the type is not opaque.
    make_global_instance(c, g);
    return true;
  }

  return prelim;
}

static void setup_tuple_fields(gentype_t* g)
{
  g->field_count = (int)ast_childcount(g->ast);
  g->fields = (ast_t**)calloc(g->field_count, sizeof(ast_t*));

  ast_t* child = ast_child(g->ast);
  size_t index = 0;

  while(child != NULL)
  {
    g->fields[index++] = child;
    child = ast_sibling(child);
  }
}

static bool setup_type_fields(gentype_t* g)
{
  assert(ast_id(g->ast) == TK_NOMINAL);

  g->field_count = 0;
  g->fields = NULL;

  ast_t* def = (ast_t*)ast_data(g->ast);

  if(ast_id(def) == TK_PRIMITIVE)
    return true;

  ast_t* typeargs = ast_childidx(g->ast, 2);
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member;

  member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        g->field_count++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  if(g->field_count == 0)
    return true;

  g->fields = (ast_t**)calloc(g->field_count, sizeof(ast_t*));

  member = ast_child(members);
  size_t index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        g->fields[index] = reify(ast_type(member), typeparams, typeargs);
        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return true;
}

static void free_fields(gentype_t* g)
{
  for(size_t i = 0; i < g->field_count; i++)
    ast_free_unattached(g->fields[i]);

  free(g->fields);

  g->field_count = 0;
  g->fields = NULL;
}

static void make_dispatch(compile_t* c, gentype_t* g)
{
  // Do nothing if we're not an actor.
  if(g->underlying != TK_ACTOR)
    return;

  // Create a dispatch function.
  const char* dispatch_name = genname_dispatch(g->type_name);
  g->dispatch_fn = codegen_addfun(c, dispatch_name, c->dispatch_type);
  codegen_startfun(c, g->dispatch_fn);

  LLVMBasicBlockRef unreachable = LLVMAppendBasicBlockInContext(c->context, g->dispatch_fn,
    "unreachable");

  LLVMValueRef this_ptr = LLVMGetParam(g->dispatch_fn, 0);
  LLVMSetValueName(this_ptr, "this");

  g->dispatch_msg = LLVMGetParam(g->dispatch_fn, 1);
  LLVMSetValueName(g->dispatch_msg, "msg");

  // Read the message ID.
  LLVMValueRef id_ptr = LLVMBuildStructGEP(c->builder, g->dispatch_msg, 0, "");
  LLVMValueRef id = LLVMBuildLoad(c->builder, id_ptr, "id");

  // Store a reference to the dispatch switch. When we build behaviours, we
  // will add cases to this switch statement based on message ID.
  g->dispatch_switch = LLVMBuildSwitch(c->builder, id, unreachable, 0);

  // Mark the default case as unreachable.
  LLVMPositionBuilderAtEnd(c->builder, unreachable);
  LLVMBuildUnreachable(c->builder);

  // Pause, otherwise the optimiser will run on what we have so far.
  codegen_pausefun(c);
}

static bool trace_fields(compile_t* c, gentype_t* g, LLVMValueRef object,
  int extra)
{
  bool need_trace = false;

  for(int i = 0; i < g->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra,
      "");
    LLVMValueRef value = LLVMBuildLoad(c->builder, field, "");
    need_trace |= gencall_trace(c, value, g->fields[i]);
  }

  return need_trace;
}

static bool trace_elements(compile_t* c, gentype_t* g, LLVMValueRef tuple)
{
  bool need_trace = false;

  for(int i = 0; i < g->field_count; i++)
  {
    LLVMValueRef value = LLVMBuildExtractValue(c->builder, tuple, i, "");
    need_trace |= gencall_trace(c, value, g->fields[i]);
  }

  return need_trace;
}

static bool make_trace(compile_t* c, gentype_t* g)
{
  // Do nothing if we have no fields.
  if(g->field_count == 0)
    return true;

  if(g->underlying == TK_CLASS)
  {
    // Special case the array trace function.
    AST_GET_CHILDREN(g->ast, pkg, id);
    const char* package = ast_name(pkg);
    const char* name = ast_name(id);

    if(!strcmp(package, "$1") && !strcmp(name, "Array"))
    {
      genprim_array_trace(c, g);
      return true;
    }
  }

  // Create a trace function.
  const char* trace_name = genname_trace(g->type_name);
  LLVMValueRef trace_fn = codegen_addfun(c, trace_name, c->trace_type);
  codegen_startfun(c, trace_fn);

  LLVMValueRef arg = LLVMGetParam(trace_fn, 0);
  LLVMSetValueName(arg, "arg");

  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, g->structure_ptr,
    "object");

  // If we don't ever trace anything, delete this function.
  bool need_trace;

  if(g->underlying == TK_TUPLETYPE)
  {
    // Create another function that traces the tuple members.
    const char* trace_tuple_name = genname_tracetuple(g->type_name);
    LLVMTypeRef trace_tuple_type = LLVMFunctionType(c->void_type, &g->primitive,
      1, false);
    LLVMValueRef trace_tuple_fn = codegen_addfun(c, trace_tuple_name,
      trace_tuple_type);
    codegen_startfun(c, trace_tuple_fn);

    LLVMValueRef arg = LLVMGetParam(trace_tuple_fn, 0);
    LLVMSetValueName(arg, "arg");
    need_trace = trace_elements(c, g, arg);

    LLVMBuildRetVoid(c->builder);
    codegen_finishfun(c);

    if(need_trace)
    {
      // Get the tuple primitive.
      LLVMValueRef tuple_ptr = LLVMBuildStructGEP(c->builder, object, 1, "");
      LLVMValueRef tuple = LLVMBuildLoad(c->builder, tuple_ptr, "");

      // Call the tuple trace function with the unboxed primitive type.
      LLVMValueRef result = LLVMBuildCall(c->builder, trace_tuple_fn, &tuple,
        1, "");
      LLVMSetInstructionCallConv(result, LLVMFastCallConv);
    } else {
      LLVMDeleteFunction(trace_tuple_fn);
    }
  } else {
    int extra = 1;

    // Actors have a pad.
    if(g->underlying == TK_ACTOR)
      extra++;

    need_trace = trace_fields(c, g, object, extra);
  }

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);

  if(!need_trace)
    LLVMDeleteFunction(trace_fn);

  return true;
}

static bool make_struct(compile_t* c, gentype_t* g)
{
  LLVMTypeRef type;
  int extra = 0;

  if(g->underlying != TK_TUPLETYPE)
  {
    type = g->structure;
    extra++;
  } else {
    type = g->primitive;
  }

  if(g->underlying == TK_ACTOR)
    extra++;

  VLA(LLVMTypeRef, elements, g->field_count + extra);

  // Create the type descriptor as element 0.
  if(g->underlying != TK_TUPLETYPE)
    elements[0] = LLVMPointerType(g->desc_type, 0);

  // Create the actor pad as element 1.
  if(g->underlying == TK_ACTOR)
    elements[1] = c->actor_pad;

  // Get a preliminary type for each field and set the struct body. This is
  // needed in case a struct for the type being generated here is required when
  // generating a field.
  for(int i = 0; i < g->field_count; i++)
  {
    gentype_t field_g;

    if(!gentype_prelim(c, g->fields[i], &field_g))
      return false;

    elements[i + extra] = field_g.use_type;
  }

  LLVMStructSetBody(type, elements, g->field_count + extra, false);

  // Create a box type for tuples.
  if(g->underlying == TK_TUPLETYPE)
    make_box_type(c, g);

  return true;
}

static bool make_components(compile_t* c, gentype_t* g)
{
  for(size_t i = 0; i < g->field_count; i++)
  {
    gentype_t field_g;

    if(!gentype(c, g->fields[i], &field_g))
      return false;
  }

  return true;
}

static bool make_nominal(compile_t* c, ast_t* ast, gentype_t* g, bool prelim)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = (ast_t*)ast_data(ast);

  // For traits, just return a raw object pointer.
  if(ast_id(def) == TK_TRAIT)
  {
    g->use_type = c->object_ptr;
    return true;
  }

  // If we already exist or we're preliminary, we're done.
  if(setup_name(c, ast, g, prelim))
    return true;

  if(g->primitive == NULL)
  {
    // Not a primitive type. Generate all the fields and a trace function.
    if(!setup_type_fields(g))
      return false;

    bool ok = make_struct(c, g) && make_trace(c, g) && make_components(c, g);
    free_fields(g);

    if(!ok)
      return false;
  } else {
    // Create a box type.
    make_box_type(c, g);

    // Generate a boxing function.
    genfun_box(c, g);
  }

  // Generate a dispatch function if necessary.
  make_dispatch(c, g);

  // Create a unique global instance if we need one.
  make_global_instance(c, g);

  // Generate all the methods.
  if(!genfun_methods(c, g))
    return false;

  // TODO: for actors: create a finaliser
  gendesc_init(c, g);

  // Finish off the dispatch function.
  if(g->underlying == TK_ACTOR)
  {
    codegen_startfun(c, g->dispatch_fn);
    codegen_finishfun(c);
  }

  return true;
}

static bool make_tuple(compile_t* c, ast_t* ast, gentype_t* g)
{
  // An anonymous structure with no functions and no vtable.
  if(setup_name(c, ast, g, false))
    return true;

  setup_tuple_fields(g);
  bool ok = make_struct(c, g) && make_trace(c, g) && make_components(c, g);
  free_fields(g);

  // Generate a boxing function and a descriptor.
  genfun_box(c, g);
  gendesc_init(c, g);

  return ok;
}

bool gentype_prelim(compile_t* c, ast_t* ast, gentype_t* g)
{
  if(ast_id(ast) == TK_NOMINAL)
  {
    memset(g, 0, sizeof(gentype_t));

    g->ast = ast;
    g->type_name = genname_type(ast);

    return make_nominal(c, ast, g, true);
  }

  return gentype(c, ast, g);
}

bool gentype(compile_t* c, ast_t* ast, gentype_t* g)
{
  memset(g, 0, sizeof(gentype_t));

  g->ast = ast;
  g->type_name = genname_type(ast);

  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    {
      // Special case Bool. Otherwise it's just a raw object pointer.
      if(is_bool(ast))
      {
        g->primitive = c->i1;
        g->use_type = g->primitive;
      } else {
        g->use_type = c->object_ptr;
      }

      return true;
    }

    case TK_ISECTTYPE:
      // Just a raw object pointer.
      g->use_type = c->object_ptr;
      return true;

    case TK_TUPLETYPE:
      return make_tuple(c, ast, g);

    case TK_NOMINAL:
      return make_nominal(c, ast, g, false);

    case TK_STRUCTURAL:
      // Just a raw object pointer.
      g->use_type = c->object_ptr;
      return true;

    default: {}
  }

  assert(0);
  return false;
}
