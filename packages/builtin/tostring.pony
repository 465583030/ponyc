primitive IntUTF32
primitive IntBinary
primitive IntBinaryBare
primitive IntOctal
primitive IntOctalBare
primitive IntHex
primitive IntHexBare
primitive IntHexSmall
primitive IntHexSmallBare

type IntFormat is
  ( FormatDefault
  | IntUTF32
  | IntBinary
  | IntBinaryBare
  | IntOctal
  | IntOctalBare
  | IntHex
  | IntHexBare
  | IntHexSmall
  | IntHexSmallBare)

primitive NumberSpacePrefix
primitive NumberSignPrefix

type NumberPrefix is
  ( PrefixDefault
  | NumberSpacePrefix
  | NumberSignPrefix)

primitive FloatExp
primitive FloatExpLarge
primitive FloatFix
primitive FloatFixLarge
primitive FloatGeneral
primitive FloatGeneralLarge

type FloatFormat is
  ( FormatDefault
  | FloatExp
  | FloatExpLarge
  | FloatFix
  | FloatFixLarge
  | FloatGeneral
  | FloatGeneralLarge)

primitive ToString
  fun tag _large(): String => "FEDCBA9876543210123456789ABCDEF"
  fun tag _small(): String => "fedcba9876543210123456789abcdef"

  fun tag _fmt_int(fmt: IntFormat): (U64, String, String) =>
    match fmt
    | IntBinary => (U64(2), "b0", _large())
    | IntBinaryBare => (U64(2), "", _large())
    | IntOctal => (U64(8), "o0", _large())
    | IntOctalBare => (U64(8), "", _large())
    | IntHex => (U64(16), "x0", _large())
    | IntHexBare => (U64(16), "", _large())
    | IntHexSmall => (U64(16), "x0", _small())
    | IntHexSmallBare => (U64(16), "", _small())
    else
      (U64(10), "", _large())
    end

  fun tag _prefix(neg: Bool, prefix: NumberPrefix): String =>
    if neg then
      "-"
    else
      match prefix
      | NumberSpacePrefix => " "
      | NumberSignPrefix => "+"
      else
        ""
      end
    end

  fun tag _extend_digits(s: String ref, digits: U64) =>
    while s.size() < digits do
      s.append_byte('0')
    end

  fun tag _pad(s: String ref, width: U64, align: Align, fill: U8) =>
    var pre: U64 = 0
    var post: U64 = 0

    if s.size() < width then
      let rem = width - s.size()

      match align
      | AlignLeft => post = rem
      | AlignRight => pre = rem
      | AlignCenter => pre = rem / 2; post = rem - pre
      end
    end

    while pre > 0 do
      s.append_byte(fill)
      pre = pre - 1
    end

    s.reverse_in_place()

    while post > 0 do
      s.append_byte(fill)
      post = post - 1
    end

  fun tag _u64(x: U64, neg: Bool, fmt: IntFormat, prefix: NumberPrefix,
    prec: U64, width: U64, align: Align, fill: U8): String iso^
  =>
    match fmt
    | IntUTF32 => return recover String.append_utf32(x.u32()) end
    end

    (var base: U64, var typestring: String, var table: String) = _fmt_int(fmt)
    var prestring = _prefix(neg, prefix)

    recover
      var s = String.reserve((prec + 1).max(width.max(31)))
      var value = x
      var i: I64 = 0

      try
        while value != 0 do
          let tmp = value
          value = value / base
          let index = tmp - (value * base)
          s.append_byte(table((index + 15).i64()))
          i = i + 1
        end
      end

      s.append(typestring)
      _extend_digits(s, prec)
      s.append(prestring)
      _pad(s, width, align, fill)
      consume s
    end

  fun tag _u128(x: U128, neg: Bool, fmt: IntFormat = FormatDefault,
    prefix: NumberPrefix = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U8 = ' '): String iso^
  =>
    match fmt
    | IntUTF32 => return recover String.append_utf32(x.u32()) end
    end

    (var base': U64, var typestring: String, var table: String) = _fmt_int(fmt)
    var prestring = _prefix(neg, prefix)
    var base = base'.u128()

    recover
      var s = String.reserve((prec + 1).max(width.max(31)))
      var value = x
      var i: I64 = 0

      try
        while value != 0 do
          let tmp = value
          value = value / base
          let index = tmp - (value * base)
          s.append_byte(table((index + 15).i64()))
          i = i + 1
        end
      end

      s.append(typestring)
      _extend_digits(s, prec)
      s.append(prestring)
      _pad(s, width, align, fill)
      consume s
    end

  fun tag _f64(x: F64, fmt: FloatFormat = FormatDefault,
    prefix: NumberPrefix = PrefixDefault, prec: U64 = 6, width: U64 = 0,
    align: Align = AlignRight, fill: U8 = ' '): String iso^
  =>
    // TODO: prefix, align, fill
    recover
      var s = String.prealloc((prec + 8).max(width.max(31)))
      var f = String.reserve(31).append("%")

      if width > 0 then f.append(width.string()) end
      f.append(".").append(prec.string())

      match fmt
      | FloatExp => f.append("e")
      | FloatExpLarge => f.append("E")
      | FloatFix => f.append("f")
      | FloatFixLarge => f.append("F")
      | FloatGeneral => f.append("g")
      | FloatGeneralLarge => f.append("G")
      else
        f.append("g")
      end

      if Platform.windows() then
        @_snprintf[I32](s.cstring(), s.space(), f.cstring(), x)
      else
        @snprintf[I32](s.cstring(), s.space(), f.cstring(), x)
      end

      s.recalc()
    end
