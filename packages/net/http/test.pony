use "ponytest"

actor Main
  new create(env: Env) =>
    let test = PonyTest(env)
    test(_TestURL)
    test.complete()

class _TestURL iso is UnitTest
  new iso create() => None
  fun name(): String => "net/http/URL.build"

  fun apply(h: TestHelper): TestResult ? =>
    var url = URL.build(
      "https://user:password@host.name:port/path?query#fragment")
    h.expect_eq[String](url.scheme, "https")
    h.expect_eq[String](url.user, "user")
    h.expect_eq[String](url.password, "password")
    h.expect_eq[String](url.host, "host.name")
    h.expect_eq[String](url.service, "port")
    h.expect_eq[String](url.path, "/path")
    h.expect_eq[String](url.query, "query")
    h.expect_eq[String](url.fragment, "fragment")

    url = URL.build("http://rosettacode.org/wiki/Category:Pony")
    h.expect_eq[String](url.scheme, "http")
    h.expect_eq[String](url.user, "")
    h.expect_eq[String](url.password, "")
    h.expect_eq[String](url.host, "rosettacode.org")
    h.expect_eq[String](url.service, "80")
    h.expect_eq[String](url.path, "/wiki/Category%3APony")
    h.expect_eq[String](url.query, "")
    h.expect_eq[String](url.fragment, "")
