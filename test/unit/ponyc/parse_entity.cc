#include <gtest/gtest.h>
#include "parse_util.h"


// Parsing tests regarding entities, methods, fields and use commamnds


class ParseEntityTest : public testing::Test
{};


// Basic tests

TEST(ParseEntityTest, Rubbish)
{
  const char* src = "rubbish";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, Empty)
{
  const char* src = "";

  const char* expect =
    "(program{scope} (package{scope} (module{scope})))";

  DO(parse_test_good(src, expect));
}


// Actors

TEST(ParseEntityTest, ActorMinimal)
{
  const char* src = "actor Foo";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (actor{scope} (id Foo) x x x members)"
    ")))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, ActorMaximal)
{
  const char* src =
    "actor Foo[A] is T"
    "  let f1:T1"
    "  let f2:T2 = 5"
    "  var f3:P3.T3"
    "  var f4:T4 = 9"
    "  new m1() => 1"
    "  be m2() => 2"
    "  fun ref m3() => 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (actor{scope} (id Foo) (typeparams (typeparam (id A) x x)) x"
    "    (types (nominal x (id T) x x x))"
    "    (members"
    "      (flet (id f1) (nominal x (id T1) x x x) x)"
    "      (flet (id f2) (nominal x (id T2) x x x) 5)"
    "      (fvar (id f3) (nominal (id P3) (id T3) x x x) x)"
    "      (fvar (id f4) (nominal x (id T4) x x x) 9)"
    "      (new{scope} x (id m1) x x x x (seq 1))"
    "      (be{scope} x (id m2) x x x x (seq 2))"
    "      (fun{scope} ref (id m3) x x x x (seq 3))"
    ")))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, ActorCanBeCalledMain)
{
  const char* src = "actor Main";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (actor{scope} (id Main) x x x members)"
    ")))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, ActorCannotSpecifyCapability)
{
  const char* src = "actor box Foo";

  DO(parse_test_bad(src));
}


// Classes

TEST(ParseEntityTest, ClassMinimal)
{
  const char* src = "class Foo";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Foo) x x x members)"
    ")))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, ClassMaximal)
{
  const char* src =
    "class Foo[A] box is T"
    "  let f1:T1"
    "  let f2:T2 = 5"
    "  var f3:P3.T3"
    "  var f4:T4 = 9"
    "  new m1() => 1"
    "  fun ref m2() => 2";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Foo) (typeparams (typeparam (id A) x x)) box"
    "    (types (nominal x (id T) x x x))"
    "    (members"
    "      (flet (id f1) (nominal x (id T1) x x x) x)"
    "      (flet (id f2) (nominal x (id T2) x x x) 5)"
    "      (fvar (id f3) (nominal (id P3) (id T3) x x x) x)"
    "      (fvar (id f4) (nominal x (id T4) x x x) 9)"
    "      (new{scope} x (id m1) x x x x (seq 1))"
    "      (fun{scope} ref (id m2) x x x x (seq 2))"
    ")))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, ClassCannotBeCalledMain)
{
  const char* src = "class Main";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, ClassCannotHaveBehaviour)
{
  const char* src = "class Foo be m() => 3";

  DO(parse_test_bad(src));
}


// Primitives

TEST(ParseEntityTest, PrimitiveMinimal)
{
  const char* src = "primitive Foo";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (primitive{scope} (id Foo) x x x members)"
    ")))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, PrimitiveMaximal)
{
  const char* src =
    "primitive Foo[A] is T"
    "  new m1() => 1"
    "  fun ref m3() => 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (primitive{scope} (id Foo) (typeparams (typeparam (id A) x x)) x"
    "    (types (nominal x (id T) x x x))"
    "    (members"
    "      (new{scope} x (id m1) x x x x (seq 1))"
    "      (fun{scope} ref (id m3) x x x x (seq 3))"
    ")))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, PrimitiveCannotBeCalledMain)
{
  const char* src = "primitive Main";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, PrimitiveCannotHaveFields)
{
  const char* src = "primitive Foo var x:U32";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, PrimitiveCannotSpecifyCapability)
{
  const char* src = "primitive box Foo";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, PrimitiveCannotHaveBehaviour)
{
  const char* src = "primitive Foo be m() => 3";

  DO(parse_test_bad(src));
}


// Traits

TEST(ParseEntityTest, TraitMinimal)
{
  const char* src = "trait Foo";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (trait{scope} (id Foo) x x x members)"
    ")))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, TraitMaximal)
{
  const char* src =
    "trait Foo[A] box is T"
    "  be m1() => 1"
    "  fun ref m2() => 2";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (trait{scope} (id Foo) (typeparams (typeparam (id A) x x)) box"
    "    (types (nominal x (id T) x x x))"
    "    (members"
    "      (be{scope} x (id m1) x x x x (seq 1))"
    "      (fun{scope} ref (id m2) x x x x (seq 2))"
    ")))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, TraitCannotBeCalledMain)
{
  const char* src = "trait Main";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, TraitCannotHaveFields)
{
  const char* src = "trait Foo var x:U32";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, TraitCannotHaveConstructor)
{
  const char* src = "trait Foo new m() => 3";

  DO(parse_test_bad(src));
}


// Aliases

TEST(ParseEntityTest, Alias)
{
  const char* src = "type Foo is Bar";
  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (type (id Foo) (nominal x (id Bar) x x x)))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, AliasMustHaveType)
{
  const char* src = "type Foo";

  DO(parse_test_bad(src));
}


// Functions

TEST(ParseEntityTest, FunctionMinimal)
{
  const char* src = "class Foo fun ref () => 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Foo) x x x"
    "    (members"
    "      (fun{scope} ref x x x x x (seq 3))"
    ")))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, FunctionMaximal)
{
  const char* src = "class Foo fun ref m[A](y:U32):I16 ? => 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Foo) x x x"
    "    (members"
    "      (fun{scope} ref (id m)"
    "        (typeparams (typeparam (id A) x x))"
    "        (params (param (id y) (nominal x (id U32) x x x) x))"
    "        (nominal x (id I16) x x x) ? (seq 3))"
    ")))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, FunctionMustHaveCapability)
{
  const char* src = "class Foo fun m() => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, FunctionCannotHaveIsoCapability)
{
  const char* src = "class Foo fun iso m() => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, FunctionCannotHaveTrnCapability)
{
  const char* src = "class Foo fun trn m() => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, FunctionCannotHaveEllipsis)
{
  const char* src = "class Foo ref fun m(x:U32, ...) => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, ClassFunctionMustHaveBody)
{
  const char* src = "class Foo fun ref m()";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, TraitFunctionBodiesAreOptional)
{
  const char* src = "trait Foo fun ref m()";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (trait{scope} (id Foo) x x x"
    "    (members"
    "      (fun{scope} ref (id m) x x x x x)"
    ")))))";

  DO(parse_test_good(src, expect));
}


// Behaviours

TEST(ParseEntityTest, Behaviour)
{
  const char* src = "actor Foo be m() => 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (actor{scope} (id Foo) x x x"
    "    (members"
    "      (be{scope} x (id m) x x x x (seq 3))"
    ")))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, BehaviourCannotHaveCapability)
{
  const char* src = "actor Foo be ref m() => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, BehaviourCannotHaveEllipsis)
{
  const char* src = "actor Foo be m(x:U32, ...) => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, BehaviourCannotHaveReturnType)
{
  const char* src = "actor Foo be m():U32 => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, BehaviourCannotBePartial)
{
  const char* src = "actor Foo be m() ? => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, ActorBehaviourMustHaveBody)
{
  const char* src = "actor Foo be m()";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, TraitBehaviourBodiesAreOptional)
{
  const char* src = "trait Foo be m()";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (trait{scope} (id Foo) x x x"
    "    (members"
    "      (be{scope} x (id m) x x x x x)"
    ")))))";

  DO(parse_test_good(src, expect));
}


// Constructors

TEST(ParseEntityTest, ConstructorMinimal)
{
  const char* src = "class Foo new () => 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Foo) x x x"
    "    (members"
    "      (new{scope} x x x x x x (seq 3))"
    ")))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, ConstructorMaximal)
{
  const char* src = "class Foo new m[A](y:U32) ? => 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Foo) x x x"
    "    (members"
    "      (new{scope} x (id m)"
    "        (typeparams (typeparam (id A) x x))"
    "        (params (param (id y) (nominal x (id U32) x x x) x))"
    "        x ? (seq 3))"
    ")))))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, ConstructorCannotHaveCapability)
{
  const char* src = "class Foo new ref m() => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, ConstructorCannotHaveEllipsis)
{
  const char* src = "class Foo new m(x:U32, ...) => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, ConstructorCannotHaveReturnType)
{
  const char* src = "class Foo new m():U32 => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, ActorConstructorCannotBePartial)
{
  const char* src = "actor Foo new m() ? => 3";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, ConstructorMustHaveBody)
{
  const char* src = "class Foo new m()";

  DO(parse_test_bad(src));
}


// Double arrow

TEST(ParseEntityTest, DoubleArrowMissing)
{
  const char* src = "class Foo fun ref f() 1";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, DoubleArrowWithoutBody)
{
  const char* src = "class Foo fun ref f() =>";

  DO(parse_test_bad(src));
}


// Field tests

TEST(ParseEntityTest, FieldMustHaveType)
{
  const char* src = "class Foo var bar";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, LetFieldMustHaveType)
{
  const char* src = "class Foo let bar";

  DO(parse_test_bad(src));
}


// Use command

TEST(ParseEntityTest, UseUri)
{
  const char* src =
    "use \"foo1\" "
    "use bar = \"foo2\" "
    "use \"foo3\" where wombat "
    "use bar = \"foo4\" where wombat";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (use x \"foo1\" x)"
    "  (use (id bar) \"foo2\" x)"
    "  (use x \"foo3\" (reference (id wombat)))"
    "  (use (id bar) \"foo4\" (reference (id wombat)))"
    ")))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, UseFfi)
{
  const char* src =
    "use @foo1[U32](a:I32, b:String, ...)"
    "use @foo2[U32]() where wombat";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (use x (ffidecl{scope} (id \"@foo1\")"
    "    (typeargs (nominal x (id U32) x x x))"
    "    (params"
    "      (param (id a) (nominal x (id I32) x x x) x)"
    "      (param (id b) (nominal x (id String) x x x) x)"
    "      ...)"
    "    x) x)"
    "  (use x (ffidecl{scope} (id \"@foo2\")"
    "    (typeargs (nominal x (id U32) x x x))"
    "    x x) (reference (id wombat)))"
    ")))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, UseFfiMustHaveReturnType)
{
  const char* src = "use @foo()";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, UseFfiMustHaveOnlyOneReturnType)
{
  const char* src = "use @foo[U8, U16]()";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, UseFfiCannotHaveDefaultArguments)
{
  const char* src = "use @foo[U32](x:U32 = 3)";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, UseFfiEllipsisMustBeLast)
{
  const char* src = "use @foo[U32](a:U32, ..., b:U32)";

  DO(parse_test_bad(src));
}


TEST(ParseEntityTest, UseGuardsAreExpressions)
{
  const char* src = "use \"foo\" where a and b";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (use x \"foo\" (and (reference (id a)) (reference (id b))))"
    ")))";

  DO(parse_test_good(src, expect));
}


TEST(ParseEntityTest, UseGuardsDoNotSupportOperatorPrecedence)
{
  const char* src1 = "use \"foo\" where (a and b) or c";

  const char* expect1 =
    "(program{scope} (package{scope} (module{scope}"
    "  (use x \"foo\""
    "    (or"
    "     (tuple (seq (and (reference (id a)) (reference (id b)))))"
    "     (reference (id c))))"
    ")))";

  DO(parse_test_good(src1, expect1));

  const char* src2 = "use \"foo\" where a and b or c";

  DO(parse_test_bad(src2));
}


TEST(ParseEntityTest, UseMustBeBeforeClass)
{
  const char* src = "class Foo use \"foo\"";

  DO(parse_test_bad(src));
}
