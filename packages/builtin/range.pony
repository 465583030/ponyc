// Produces [min, max)
class Range[A: (Real[A] & Number) = U64] is Iterator[A]
  let _min: A
  let _max: A
  let _inc: A
  var _idx: A

  new create(min: A, max: A) =>
    _min = min
    _max = max
    _inc = 1
    _idx = min

  new step(min: A, max: A, inc: A) =>
    _min = min
    _max = max
    _inc = inc
    _idx = min

  fun box has_next(): Bool => _idx < _max

  fun ref next(): A^ => if _idx < _max then _idx = _idx + _inc else _idx end

  fun ref rewind() => _idx = _min
