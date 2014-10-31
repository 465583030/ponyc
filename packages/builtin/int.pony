primitive I8 is Integer[I8]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I8): I8 =>
    if this > that then this else that end

  fun box min(that: I8): I8 =>
    if this < that then this else that end

  fun box abs(): I8 => if this < 0 then -this else this end
  fun box bswap(): I8 => this
  fun box popcount(): I8 => @llvm.ctpop.i8[I8](this)
  fun box clz(): I8 => @llvm.ctlz.i8[I8](this, false)
  fun box ctz(): I8 => @llvm.cttz.i8[I8](this, false)

  fun box string(): String iso^ => recover String.from_i8(this, 10) end

primitive I16 is Integer[I16]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I16): I16 =>
    if this > that then this else that end

  fun box min(that: I16): I16 =>
    if this < that then this else that end

  fun box abs(): I16 => if this < 0 then -this else this end
  fun box bswap(): I16 => @llvm.bswap.i16[I16](this)
  fun box popcount(): I16 => @llvm.ctpop.i16[I16](this)
  fun box clz(): I16 => @llvm.ctlz.i16[I16](this, false)
  fun box ctz(): I16 => @llvm.cttz.i16[I16](this, false)

  fun box string(): String iso^ => recover String.from_i16(this, 10) end

primitive I32 is Integer[I32]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I32): I32 =>
    if this > that then this else that end

  fun box min(that: I32): I32 =>
    if this < that then this else that end

  fun box abs(): I32 => if this < 0 then -this else this end
  fun box bswap(): I32 => @llvm.bswap.i32[I32](this)
  fun box popcount(): I32 => @llvm.ctpop.i32[I32](this)
  fun box clz(): I32 => @llvm.ctlz.i32[I32](this, false)
  fun box ctz(): I32 => @llvm.cttz.i32[I32](this, false)

  fun box string(): String iso^ => recover String.from_i32(this, 10) end

primitive I64 is Integer[I64]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I64): I64 =>
    if this > that then this else that end

  fun box min(that: I64): I64 =>
    if this < that then this else that end

  fun box abs(): I64 => if this < 0 then -this else this end
  fun box bswap(): I64 => @llvm.bswap.i64[I64](this)
  fun box popcount(): I64 => @llvm.ctpop.i64[I64](this)
  fun box clz(): I64 => @llvm.ctlz.i64[I64](this, false)
  fun box ctz(): I64 => @llvm.cttz.i64[I64](this, false)

  fun box string(): String iso^ => recover String.from_i64(this, 10) end

primitive I128 is Integer[I128]
  new create(from: I128) => compiler_intrinsic

  fun box max(that: I128): I128 =>
    if this > that then this else that end

  fun box min(that: I128): I128 =>
    if this < that then this else that end

  fun box abs(): I128 => if this < 0 then -this else this end
  fun box bswap(): I128 => @llvm.bswap.i128[I128](this)
  fun box popcount(): I128 => @llvm.ctpop.i128[I128](this)
  fun box clz(): I128 => @llvm.ctlz.i128[I128](this, false)
  fun box ctz(): I128 => @llvm.cttz.i128[I128](this, false)

  fun box string(): String iso^ => recover String.from_i128(this, 10) end

  fun box divmod(y: I128): (I128, I128) =>
    if Platform.has_i128() then
      (this / y, this % y)
    else
      if y == 0 then
        // TODO: returning (0, 0) causes a codegen error
        var qr: (I128, I128) = (I128(0), I128(0))
        return qr
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

      var (q, r) = num.u128().divmod(den.u128())
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
      var (q, r) = divmod(y)
      q
    end

  fun box mod(y: I128): I128 =>
    if Platform.has_i128() then
      this % y
    else
      var (q, r) = divmod(y)
      r
    end

  fun box f32(): F32 => this.f64().f32()

  fun box f64(): F64 =>
    if this < 0 then
      -(-this).f64()
    else
      this.u128().f64()
    end
