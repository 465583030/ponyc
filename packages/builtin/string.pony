class String val is Ordered[String box], Stringable
  """
  Strings don't specify an encoding.
  """
  var _size: U64
  var _alloc: U64
  var _ptr: Pointer[U8]

  new create(size: U64 = 0) =>
    """
    An empty string. Enough space for size bytes is reserved.
    """
    _size = 0
    _alloc = size + 1
    _ptr = Pointer[U8]._create(_alloc)
    _ptr._update(0, 0)

  new from_cstring(str: Pointer[U8] ref, len: U64 = 0, copy: Bool = true) =>
    """
    If the cstring is not copied, this should be done with care.
    """
    _size = len

    if len == 0 then
      while str._apply(_size) != 0 do
        _size = _size + 1
      end
    end

    if copy then
      _alloc = _size + 1
      _ptr = Pointer[U8]._create(_alloc)
      @memcpy[Pointer[U8]](_ptr, str, _alloc)
    else
      _alloc = _size + 1
      _ptr = str
    end

  new from_utf32(value: U32) =>
    """
    Create a UTF-8 string from a single UTF-32 code point.
    """
    if value < 0x80 then
      _size = 1
      _alloc = _size + 1
      _ptr = Pointer[U8]._create(_alloc)
      _ptr._update(0, value.u8())
    elseif value < 0x800 then
      _size = 2
      _alloc = _size + 1
      _ptr = Pointer[U8]._create(_alloc)
      _ptr._update(0, ((value >> 6) or 0xC0).u8())
      _ptr._update(1, ((value and 0x3F) or 0x80).u8())
    elseif value < 0xD800 then
      _size = 3
      _alloc = _size + 1
      _ptr = Pointer[U8]._create(_alloc)
      _ptr._update(0, ((value >> 12) or 0xE0).u8())
      _ptr._update(1, (((value >> 6) and 0x3F) or 0x80).u8())
      _ptr._update(2, ((value and 0x3F) or 0x80).u8())
    elseif value < 0xE000 then
      // UTF-16 surrogate pairs are not allowed.
      _size = 3
      _alloc = _size + 1
      _ptr = Pointer[U8]._create(_alloc)
      _ptr._update(0, 0xEF)
      _ptr._update(1, 0xBF)
      _ptr._update(2, 0xBD)
      _size = _size + 3
    elseif value < 0x10000 then
      _size = 3
      _alloc = _size + 1
      _ptr = Pointer[U8]._create(_alloc)
      _ptr._update(0, ((value >> 12) or 0xE0).u8())
      _ptr._update(1, (((value >> 6) and 0x3F) or 0x80).u8())
      _ptr._update(2, ((value and 0x3F) or 0x80).u8())
    elseif value < 0x110000 then
      _size = 4
      _alloc = _size + 1
      _ptr = Pointer[U8]._create(_alloc)
      _ptr._update(0, ((value >> 18) or 0xF0).u8())
      _ptr._update(1, (((value >> 12) and 0x3F) or 0x80).u8())
      _ptr._update(2, (((value >> 6) and 0x3F) or 0x80).u8())
      _ptr._update(3, ((value and 0x3F) or 0x80).u8())
    else
      // Code points beyond 0x10FFFF are not allowed.
      _size = 3
      _alloc = _size + 1
      _ptr = Pointer[U8]._create(_alloc)
      _ptr._update(0, 0xEF)
      _ptr._update(1, 0xBF)
      _ptr._update(2, 0xBD)
    end
    _ptr._update(_size, 0)

  fun box cstring(): Pointer[U8] tag =>
    """
    Returns a C compatible pointer to a null terminated string.
    """
    _ptr

  fun box size(): U64 =>
    """
    Returns the length of the string.
    """
    _size

  fun box space(): U64 =>
    """
    Returns the amount of allocated space.
    """
    _alloc

  fun ref reserve(len: U64): String ref^ =>
    """
    Reserve space for len bytes. An additional byte will be reserved for the
    null terminator.
    """
    if _alloc <= len then
      _alloc = (len + 1).max(8).next_pow2()
      _ptr = _ptr._realloc(_alloc)
    end
    this

  fun ref recalc(): String ref^ =>
    """
    Recalculates the string length. This is only needed if the string is
    changed via an FFI call.
    """
    _size = 0

    while (_size < _alloc) and (_ptr._apply(_size) > 0) do
      _size = _size + 1
    end
    this

  fun box utf32(offset: I64): (U32, U8) ? =>
    """
    Return a UTF32 representation of the character at the given offset and the
    number of bytes needed to encode that character. If the offset does not
    point to the beginning of a valid UTF8 encoding, return 0xFFFD (the unicode
    replacement character) and a length of one. Raise an error if the offset is
    out of bounds.
    """
    let i = offset_to_index(offset)
    let err: (U32, U8) = (0xFFFD, 1)

    if i >= _size then error end
    var c = _ptr._apply(i)

    if c < 0x80 then
      // 1-byte
      (c.u32(), U8(1))
    elseif c < 0xC2 then
      // Stray continuation.
      err
    elseif c < 0xE0 then
      // 2-byte
      if (i + 1) >= _size then
        // Not enough bytes.
        err
      else
        var c2 = _ptr._apply(i + 1)
        if (c2 and 0xC0) != 0x80 then
          // Not a continuation byte.
          err
        else
          (((c.u32() << 6) + c2.u32()) - 0x3080, U8(2))
        end
      end
    elseif c < 0xF0 then
      // 3-byte.
      if (i + 2) >= _size then
        // Not enough bytes.
        err
      else
        var c2 = _ptr._apply(i + 1)
        var c3 = _ptr._apply(i + 2)
        if
          // Not continuation bytes.
          ((c2 and 0xC0) != 0x80) or
          ((c3 and 0xC0) != 0x80) or
          // Overlong encoding.
          ((c == 0xE0) and (c2 < 0xA0))
        then
          err
        else
          (((c.u32() << 12) + (c2.u32() << 6) + c3.u32()) - 0xE2080, U8(3))
        end
      end
    elseif c < 0xF5 then
      // 4-byte.
      if (i + 3) >= _size then
        // Not enough bytes.
        err
      else
        var c2 = _ptr._apply(i + 1)
        var c3 = _ptr._apply(i + 2)
        var c4 = _ptr._apply(i + 3)
        if
          // Not continuation bytes.
          ((c2 and 0xC0) != 0x80) or
          ((c3 and 0xC0) != 0x80) or
          ((c4 and 0xC0) != 0x80) or
          // Overlong encoding.
          ((c == 0xF0) and (c2 < 0x90)) or
          // UTF32 would be > 0x10FFFF.
          ((c == 0xF4) and (c2 >= 0x90))
        then
          err
        else
          (((c.u32() << 18) +
            (c2.u32() << 12) +
            (c3.u32() << 6) +
            c4.u32()) - 0x3C82080, U8(4))
        end
      end
    else
      // UTF32 would be > 0x10FFFF.
      err
    end

  fun box apply(offset: I64): U8 ? =>
    """
    Returns the byte at the given offset. Raise an error if the offset is out
    of bounds.
    """
    let i = offset_to_index(offset)
    if i < _size then _ptr._apply(i) else error end

  fun ref update(offset: I64, value: U8): U8 ? =>
    """
    Changes a byte in the string, returning the previous byte at that offset.
    Raise an error if the offset is out of bounds.
    """
    let i = offset_to_index(offset)

    if i < _size then
      if value == 0 then
        _size = i
      end

      _ptr._update(i, value)
    else
      error
    end

  fun box clone(): String iso^ =>
    """
    Returns a copy of the string.
    """
    let len = _size
    let str = recover String(len) end
    @memcpy[Pointer[U8]](str._ptr, _ptr, len + 1)
    str._size = len
    consume str

  fun box find(s: String box, offset: I64 = 0, nth: U64 = 0): I64 ? =>
    """
    Return the index of the n-th instance of s in the string starting from the
    beginning. Raise an error if there is no n-th occurence of s or s is empty.
    """
    var i = offset_to_index(offset)
    var steps = nth + 1

    while i < _size do
      var j: U64 = 0

      var same = while j < s._size do
        if _ptr._apply(i + j) != s._ptr._apply(j) then
          break false
        end
        j = j + 1
        true
      else
        false
      end

      if same and ((steps = steps - 1) == 1) then
        return i.i64()
      end

      i = i + 1
    end
    error

  fun box rfind(s: String, offset: I64 = -1, nth: U64 = 0): I64 ? =>
    """
    Return the index of n-th instance of s in the string starting from the end.
    Raise an error if there is no n-th occurence of s or s is empty.
    """
    var i = offset_to_index(offset) - s._size
    var steps = nth + 1

    while i < _size do
      var j: U64 = 0

      var same = while j < s._size do
        if _ptr._apply(i + j) != s._ptr._apply(j) then
          break false
        end
        j = j + 1
        true
      else
        false
      end

      if same and ((steps = steps - 1) == 1) then
        return i.i64()
      end

      i = i - 1
    end
    error

  fun box count(s: String, offset: I64 = 0): U64 =>
    """
    Counts the non-overlapping occurrences of s in the string.
    """
    let j: I64 = (_size - s.size()).i64()
    var i: U64 = 0
    var k = offset

    if j < 0 then
      return 0
    elseif (j == 0) and (this == s) then
      return 1
    end

    try
      while k < j do
        k = find(s, k) + s.size().i64()
        i = i + 1
      end
    end

    i

  fun box at(s: String, offset: I64): Bool =>
    """
    Returns true if the substring s is present at the given offset.
    """
    var i = offset_to_index(offset)
    var j: U64 = 0

    while j < s._size do
      if _ptr._apply(i + j) != s._ptr._apply(j) then
        return false
      end
      j = j + 1
    end
    false

  fun ref delete(offset: I64, len: U64): String ref^ =>
    """
    Delete len bytes at the supplied offset, compacting the string in place.
    """
    var i = offset_to_index(offset)

    if i < _size then
      var n = len.min(_size - i)
      _size = _size - n
      _ptr._delete(i, n, _size - i)
      _ptr._update(_size, 0)
    end
    this

  fun box substring(from: I64, to: I64): String iso^ =>
    """
    Returns a substring. From and to are inclusive. Returns an empty string if
    nothing is in the range.
    """
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)
    let ptr = _ptr.u64()

    if (start < _size) and (start <= finish) then
      recover
        let len = (finish - start) + 1
        var str = String(len)
        @memcpy[Pointer[U8]](str._ptr, ptr + start, len)
        str._size = len
        str._set(len, 0)
        consume str
      end
    else
      recover String end
    end

  fun box lower(): String iso^ =>
    """
    Returns a lower case version of the string.
    """
    var s = clone()
    s.lower_in_place()
    consume s

  fun ref lower_in_place(): String ref^ =>
    """
    Transforms the string to lower case. Currently only knows ASCII case.
    """
    var i: U64 = 0

    while i < _size do
      var c = _ptr._apply(i)

      if (c >= 0x41) and (c <= 0x5A) then
        _set(i, c + 0x20)
      end

      i = i + 1
    end
    this

  fun box upper(): String iso^ =>
    """
    Returns an upper case version of the string. Currently only knows ASCII
    case.
    """
    var s = clone()
    s.upper_in_place()
    consume s

  fun ref upper_in_place(): String ref^ =>
    """
    Transforms the string to upper case.
    """
    var i: U64 = 0

    while i < _size do
      var c = _ptr._apply(i)

      if (c >= 0x61) and (c <= 0x7A) then
        _set(i, c - 0x20)
      end

      i = i + 1
    end
    this

  fun box reverse(): String iso^ =>
    """
    Returns a reversed version of the string.
    """
    var s = clone()
    s.reverse_in_place()
    consume s

  fun ref reverse_in_place(): String ref^ =>
    """
    Reverses the byte order in the string. This needs to be changed to handle
    UTF-8 correctly.
    """
    if _size > 1 then
      var i: U64 = 0
      var j = _size - 1

      while i < j do
        let x = _ptr._apply(i)
        _ptr._update(i, _ptr._apply(j))
        _ptr._update(j, x)
        i = i + 1
        j = j - 1
      end
    end
    this

  fun ref append(that: String box): String ref^ =>
    """
    Append that to this.
    """
    reserve(_size + that._size)
    @memcpy[Pointer[U8]](_ptr.u64() + _size, that._ptr, that._size + 1)
    _size = _size + that._size
    this

  fun ref append_byte(value: U8): String ref^ =>
    """
    Append an arbitrary byte to the string.
    """
    reserve(_size + 1)
    _ptr._update(_size, value)
    _size = _size + 1
    _ptr._update(_size, 0)
    this

  fun box insert(offset: I64, that: String): String iso^ =>
    """
    Returns a version of the string with the given string inserted at the given
    offset.
    """
    var s = clone()
    s.insert_in_place(offset, that)
    consume s

  fun ref insert_in_place(offset: I64, that: String): String ref^ =>
    """
    Inserts the given string at the given offset. Appends the string if the
    offset is out of bounds.
    """
    reserve(_size + that._size)
    var index = offset_to_index(offset).min(_size)
    @memmove[Pointer[U8]](_ptr.u64() + index + that._size,
      _ptr.u64() + index, that._size)
    @memcpy[Pointer[U8]](_ptr.u64() + index, that._ptr, that._size)
    _size = _size + that._size
    _ptr._update(_size, 0)
    this

  fun box cut(from: I64, to: I64): String iso^ =>
    """
    Returns a version of the string with the given range deleted. The range is
    inclusive.
    """
    var s = clone()
    s.cut_in_place(from, to)
    consume s

  fun ref cut_in_place(from: I64, to: I64): String ref^ =>
    """
    Cuts the given range out of the string.
    """
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)

    if (start < _size) and (start <= finish) and (finish < _size) then
      let len = _size - ((finish - start) + 1)
      var j = finish + 1

      while j < _size do
        _ptr._update(start + (j - (finish + 1)), _ptr._apply(j))
        j = j + 1
      end

      _size = len
      _ptr._update(len, 0)
    end
    this

  fun ref strip(s: String): U64 =>
    """
    Remove all instances of s from the string. Returns the count of removed
    instances.
    """
    var i: I64 = 0
    var n: U64 = 0

    try
      while true do
        i = find(s, i)
        cut_in_place(i, (i + s.size().i64()) - 1)
        n = n + 1
      end
    end
    n

  fun box add(that: String box): String =>
    """
    Return a string that is a concatenation of this and that.
    """
    let len = _size + that._size
    var s = recover String(len) end
    @memcpy[Pointer[U8]](s._ptr, _ptr, _size)
    @memcpy[Pointer[U8]](s._ptr.u64() + _size, that._ptr, that._size + 1)
    s._size = len
    consume s

  fun box compare(that: String box, n: U64, offset: I64 = 0,
    that_offset: I64 = 0): I32
  =>
    """
    Starting at this + offset, compare n characters with that + offset. Return
    zero if the strings are the same. Return a negative number if this is
    less than that, a positive number if this is more than that.
    """
    var i = n
    var j: U64 = offset_to_index(offset)
    var k: U64 = offset_to_index(that_offset)

    if (j + n) > _size then
      return -1
    elseif (k + n) > that._size then
      return 1
    end

    while i > 0 do
      if _ptr._apply(j) != that._ptr._apply(k) then
        return _ptr._apply(j).i32() - that._ptr._apply(k).i32()
      end

      j = j + 1
      k = k + 1
      i = i - 1
    end
    0

  fun box eq(that: String box): Bool =>
    """
    Returns true if the two strings have the same contents.
    """
    if _size == that._size then
      @memcmp[I32](_ptr, that._ptr, _size) == 0
    else
      false
    end

  fun box lt(that: String box): Bool =>
    """
    Returns true if this is lexically less than that. Needs to be made UTF-8
    safe.
    """
    let len = _size.min(that._size)
    var i: U64 = 0

    while i < len do
      if _ptr._apply(i) < that._ptr._apply(i) then
        return true
      elseif _ptr._apply(i) > that._ptr._apply(i) then
        return false
      end
      i = i + 1
    end
    _size < that._size

  fun box le(that: String box): Bool =>
    """
    Returns true if this is lexically less than or equal to that. Needs to be
    made UTF-8 safe.
    """
    let len = _size.min(that._size)
    var i: U64 = 0

    while i < len do
      if _ptr._apply(i) < that._ptr._apply(i) then
        return true
      elseif _ptr._apply(i) > that._ptr._apply(i) then
        return false
      end
      i = i + 1
    end
    _size <= that._size

  fun box offset_to_index(i: I64): U64 =>
    if i < 0 then i.u64() + _size else i.u64() end

  fun box i8(): I8 => i64().i8()
  fun box i16(): I16 => i64().i16()
  fun box i32(): I32 => i64().i32()

  fun box i64(): I64 =>
    if Platform.windows() then
      @_strtoi64[I64](_ptr, U64(0), I32(10))
    else
      @strtol[I64](_ptr, U64(0), I32(10))
    end

  fun box i128(): I128 =>
    if Platform.windows() then
      i64().i128()
    else
      @strtoll[I128](_ptr, U64(0), I32(10))
    end

  fun box u8(): U8 => u64().u8()
  fun box u16(): U16 => u64().u16()
  fun box u32(): U32 => u64().u32()

  fun box u64(): U64 =>
    if Platform.windows() then
      @_strtoui64[U64](_ptr, U64(0), I32(10))
    else
      @strtoul[U64](_ptr, U64(0), I32(10))
    end

  fun box u128(): U128 =>
    if Platform.windows() then
      u64().u128()
    else
      @strtoull[U128](_ptr, U64(0), I32(10))
    end

  fun box f32(): F32 => @strtof[F32](_ptr, U64(0))
  fun box f64(): F64 => @strtod[F64](_ptr, U64(0))

  fun box hash(): U64 => @hash_block[U64](_ptr, _size)

  fun box string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
  =>
    // TODO: fill character
    let copy_len = _size.min(prec.u64())
    let len = copy_len.max(width.u64())
    let str = recover String(len) end

    match align
    | AlignLeft =>
      @memcpy[Pointer[U8]](str._ptr, _ptr, copy_len)
      @memset[Pointer[U8]](str._ptr.u64() + copy_len, U32(' '), len - copy_len)
    | AlignRight =>
      @memset[Pointer[U8]](str._ptr, U32(' '), len - copy_len)
      @memcpy[Pointer[U8]](str._ptr.u64() + (len - copy_len), _ptr, copy_len)
    | AlignCenter =>
      let half = (len - copy_len) / 2
      @memset[Pointer[U8]](str._ptr, U32(' '), half)
      @memcpy[Pointer[U8]](str._ptr.u64() + half, _ptr, copy_len)
      @memset[Pointer[U8]](str._ptr.u64() + copy_len + half, U32(' '),
        len - copy_len - half)
    end

    str._size = len
    str._set(len, 0)
    consume str

  // fun box format(args: Array[String] box): String ? =>
  //   recover
  //     var s = String.
  //   var i: U64 = 0
  //
  //   while i < _size do

  fun ref _set(i: U64, value: U8): U8 =>
    """
    Unsafe update, used internally.
    """
    _ptr._update(i, value)
