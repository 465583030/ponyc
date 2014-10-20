actor Spreader
  var _env: Env
  var _count: U64

  var _parent: (Spreader | None) = None
  var _result: U64 = U64(0)
  var _received: U64 = U64(0)

  new create(env: Env) =>
    _env = env

    _count = try _env.args(1).u64() else U64(10) end

    if _count > 0 then
      spawn_child()
      spawn_child()
    else
      _env.stdout.print("1 actor")
    end

  new spread(parent: Spreader, count: U64) =>
    if count == 0 then
      parent.result(1)
    else
      _parent = parent
      _count = count

      spawn_child()
      spawn_child()
    end

  be result(i: U64) =>
    _received = _received + 1
    _result = _result + i

    if _received == 2 then
      match _parent
      | var p: Spreader =>
        p.result(_result + 1)
      | var p: None =>
        _env.stdout.print((_result + 1).string() + " actors")
      end
    end

  fun ref spawn_child() =>
    Spreader.spread(this, _count - 1)

actor Main
  new create(env: Env) =>
    Spreader(env)
