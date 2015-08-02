use "ponytest"
use "collections"


actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestBase64)
    test(_TestBase64Decode)


class _TestBase64 iso is UnitTest
  """
  Test base64 encoding.
  Using test examples from RFC 4648.
  """
  fun name(): String => "encode/Base64.encode"

  fun apply(h: TestHelper): TestResult =>
    h.expect_eq[String]("", Base64.encode(""))
    h.expect_eq[String]("Zg==", Base64.encode("f"))
    h.expect_eq[String]("Zm8=", Base64.encode("fo"))
    h.expect_eq[String]("Zm9v", Base64.encode("foo"))
    h.expect_eq[String]("Zm9vYg==", Base64.encode("foob"))
    h.expect_eq[String]("Zm9vYmE=", Base64.encode("fooba"))
    h.expect_eq[String]("Zm9vYmFy", Base64.encode("foobar"))
    true

class _TestBase64Decode iso is UnitTest
  """
  Test base64 decoding. Examples with and without padding are tested.
  Using test examples from RFC 4648.
  """
  fun name(): String => "encode/Base64.decode"

  fun apply(h: TestHelper): TestResult ? =>
    h.expect_eq[String]("", Base64.decode[String iso](""))
    h.expect_eq[String]("f", Base64.decode[String iso]("Zg=="))
    h.expect_eq[String]("fo", Base64.decode[String iso]("Zm8="))
    h.expect_eq[String]("foo", Base64.decode[String iso]("Zm9v"))
    h.expect_eq[String]("foob", Base64.decode[String iso]("Zm9vYg=="))
    h.expect_eq[String]("fooba", Base64.decode[String iso]("Zm9vYmE="))
    h.expect_eq[String]("foobar", Base64.decode[String iso]("Zm9vYmFy"))

    h.expect_eq[String]("", Base64.decode[String iso](""))
    h.expect_eq[String]("f", Base64.decode[String iso]("Zg"))
    h.expect_eq[String]("fo", Base64.decode[String iso]("Zm8"))
    h.expect_eq[String]("foo", Base64.decode[String iso]("Zm9v"))
    h.expect_eq[String]("foob", Base64.decode[String iso]("Zm9vYg"))
    h.expect_eq[String]("fooba", Base64.decode[String iso]("Zm9vYmE"))
    h.expect_eq[String]("foobar", Base64.decode[String iso]("Zm9vYmFy"))
    true
