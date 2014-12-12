primitive IntUTF32
primitive IntBinary
primitive IntOctal
primitive IntHex
primitive IntHexSmall

primitive IntTable
  fun tag large(): String => "FEDCBA9876543210123456789ABCDEF"
  fun tag small(): String => "fedcba9876543210123456789abcdef"

type IntFormat is
  ( FormatDefault
  | IntUTF32
  | IntBinary
  | IntOctal
  | IntHex
  | IntHexSmall)

primitive I8 is Integer[I8]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I8): I8 =>
    if this > that then this else that end

  fun box min(that: I8): I8 =>
    if this < that then this else that end

  fun box abs(): I8 => if this < 0 then -this else this end
  fun box bswap(): I8 => this
  fun box popcount(): I8 => @"llvm.ctpop.i8"[I8](this)
  fun box clz(): I8 => @"llvm.ctlz.i8"[I8](this, false)
  fun box ctz(): I8 => @"llvm.cttz.i8"[I8](this, false)
  fun box bitwidth(): I8 => 8

  fun box addc(y: I8): (I8, Bool) =>
    @"llvm.sadd.with.overflow.i8"[(I8, Bool)](this, y)
  fun box subc(y: I8): (I8, Bool) =>
    @"llvm.ssub.with.overflow.i8"[(I8, Bool)](this, y)
  fun box mulc(y: I8): (I8, Bool) =>
    @"llvm.smul.with.overflow.i8"[(I8, Bool)](this, y)

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    i64().string(fmt, prec, width, align)

primitive I16 is Integer[I16]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I16): I16 =>
    if this > that then this else that end

  fun box min(that: I16): I16 =>
    if this < that then this else that end

  fun box abs(): I16 => if this < 0 then -this else this end
  fun box bswap(): I16 => @"llvm.bswap.i16"[I16](this)
  fun box popcount(): I16 => @"llvm.ctpop.i16"[I16](this)
  fun box clz(): I16 => @"llvm.ctlz.i16"[I16](this, false)
  fun box ctz(): I16 => @"llvm.cttz.i16"[I16](this, false)
  fun box bitwidth(): I16 => 16

  fun box addc(y: I16): (I16, Bool) =>
    @"llvm.sadd.with.overflow.i16"[(I16, Bool)](this, y)
  fun box subc(y: I16): (I16, Bool) =>
    @"llvm.ssub.with.overflow.i16"[(I16, Bool)](this, y)
  fun box mulc(y: I16): (I16, Bool) =>
    @"llvm.smul.with.overflow.i16"[(I16, Bool)](this, y)

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    i64().string(fmt, prec, width, align)

primitive I32 is Integer[I32]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I32): I32 =>
    if this > that then this else that end

  fun box min(that: I32): I32 =>
    if this < that then this else that end

  fun box abs(): I32 => if this < 0 then -this else this end
  fun box bswap(): I32 => @"llvm.bswap.i32"[I32](this)
  fun box popcount(): I32 => @"llvm.ctpop.i32"[I32](this)
  fun box clz(): I32 => @"llvm.ctlz.i32"[I32](this, false)
  fun box ctz(): I32 => @"llvm.cttz.i32"[I32](this, false)
  fun box bitwidth(): I32 => 32

  fun box addc(y: I32): (I32, Bool) =>
    @"llvm.sadd.with.overflow.i32"[(I32, Bool)](this, y)
  fun box subc(y: I32): (I32, Bool) =>
    @"llvm.ssub.with.overflow.i32"[(I32, Bool)](this, y)
  fun box mulc(y: I32): (I32, Bool) =>
    @"llvm.smul.with.overflow.i32"[(I32, Bool)](this, y)

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    i64().string(fmt, prec, width, align)

primitive I64 is Integer[I64]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I64): I64 =>
    if this > that then this else that end

  fun box min(that: I64): I64 =>
    if this < that then this else that end

  fun box abs(): I64 => if this < 0 then -this else this end
  fun box bswap(): I64 => @"llvm.bswap.i64"[I64](this)
  fun box popcount(): I64 => @"llvm.ctpop.i64"[I64](this)
  fun box clz(): I64 => @"llvm.ctlz.i64"[I64](this, false)
  fun box ctz(): I64 => @"llvm.cttz.i64"[I64](this, false)
  fun box bitwidth(): I64 => 64

  fun box addc(y: I64): (I64, Bool) =>
    @"llvm.sadd.with.overflow.i64"[(I64, Bool)](this, y)
  fun box subc(y: I64): (I64, Bool) =>
    @"llvm.ssub.with.overflow.i64"[(I64, Bool)](this, y)
  fun box mulc(y: I64): (I64, Bool) =>
    @"llvm.smul.with.overflow.i64"[(I64, Bool)](this, y)

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    var table = IntTable.large()

    var base = match fmt
    | IntUTF32 => return recover String.append_utf32(u32()) end
    | IntBinary => I64(2)
    | IntOctal => I64(8)
    | IntHex => I64(16)
    | IntHexSmall => table = IntTable.small(); I64(16)
    else
      I64(10)
    end

    recover
      var s = String.reserve((prec + 1).max(width.max(31)))
      var value = this
      let div = base
      var i: I64 = 0

      try
        while value != 0 do
          let tmp = value
          value = value / div
          let index = tmp - (value * div)
          s.append_byte(table(index + 15))
          i = i + 1
        end

        while i < prec.i64() do
          s.append_byte('0')
          i = i + 1
        end

        if this < 0 then
          s.append_byte('-')
          i = i + 1
        end

        var pre: U64 = 0
        var post: U64 = 0

        if i.u64() < width then
          let rem = width - i.u64()

          match align
          | AlignLeft => post = rem
          | AlignRight => pre = rem
          | AlignCenter => pre = rem / 2; post = rem - pre
          end
        end

        while pre > 0 do
          s.append_byte(' ')
          pre = pre - 1
        end

        s.reverse_in_place()

        while post > 0 do
          s.append_byte(' ')
          post = post - 1
        end
      end

      consume s
    end

primitive I128 is Integer[I128]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I128): I128 =>
    if this > that then this else that end

  fun box min(that: I128): I128 =>
    if this < that then this else that end

  fun box abs(): I128 => if this < 0 then -this else this end
  fun box bswap(): I128 => @"llvm.bswap.i128"[I128](this)
  fun box popcount(): I128 => @"llvm.ctpop.i128"[I128](this)
  fun box clz(): I128 => @"llvm.ctlz.i128"[I128](this, false)
  fun box ctz(): I128 => @"llvm.cttz.i128"[I128](this, false)
  fun box bitwidth(): I128 => 128

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    var table = IntTable.large()

    var base = match fmt
    | IntUTF32 => return recover String.append_utf32(u32()) end
    | IntBinary => I64(2)
    | IntOctal => I64(8)
    | IntHex => I64(16)
    | IntHexSmall => table = IntTable.small(); I64(16)
    else
      I64(10)
    end

    recover
      var s = String.prealloc((prec + 1).max(width.max(31)))
      var value = this
      let div = base.i128()
      var i: I64 = 0

      try
        while value != 0 do
          let tmp = value
          value = value / div
          let index = tmp - (value * div)
          s.append_byte(table((index + 15).i64()))
          i = i + 1
        end

        while i < prec.i64() do
          s.append_byte('0')
          i = i + 1
        end

        if this < 0 then
          s.append_byte('-')
          i = i + 1
        end

        var pre: U64 = 0
        var post: U64 = 0

        if i.u64() < width then
          let rem = width - i.u64()

          match align
          | AlignLeft => post = rem
          | AlignRight => pre = rem
          | AlignCenter => pre = rem / 2; post = rem - pre
          end
        end

        while pre > 0 do
          s.append_byte(' ')
          pre = pre - 1
        end

        s.reverse_in_place()

        while post > 0 do
          s.append_byte(' ')
          post = post - 1
        end
      end

      consume s
    end

  fun box divmod(y: I128): (I128, I128) =>
    if Platform.has_i128() then
      (this / y, this % y)
    else
      if y == 0 then
        return (0, 0)
      end

      var num: I128 = this
      var den: I128 = y

      var minus = if num < 0 then
        num = -num
        true
      else
        false
      end

      if den < 0 then
        den = -den
        minus = not minus
      end

      let (q, r) = num.u128().divmod(den.u128())
      var (q', r') = (q.i128(), r.i128())

      if minus then
        q' = -q'
      end

      (q', r')
    end

  fun box div(y: I128): I128 =>
    if Platform.has_i128() then
      this / y
    else
      let (q, r) = divmod(y)
      q
    end

  fun box mod(y: I128): I128 =>
    if Platform.has_i128() then
      this % y
    else
      let (q, r) = divmod(y)
      r
    end

  fun box f32(): F32 => this.f64().f32()

  fun box f64(): F64 =>
    if this < 0 then
      -(-this).f64()
    else
      this.u128().f64()
    end
