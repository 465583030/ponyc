actor Main
  var _tuple: (U32, U32)

  new create(env: Env) =>
    var (a, b): (U32, U32) = (1, 2)

    _tuple = var tuple = (a, b) = (b, a)

    @printf[I32]("%d, %d\n"._cstring(), a, b)
    @printf[I32]("%d, %d\n"._cstring(), tuple.0, tuple.1)
    @printf[I32]("%d, %d\n"._cstring(), _tuple.0, _tuple.1)
