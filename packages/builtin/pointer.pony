class Pointer[A]
  """
  A Pointer[A] is a raw memory pointer. It has no descriptor and thus can't be
  included in a union or intersection, or be a subtype of any interface. Most
  functions on a Pointer[A] are private to maintain memory safety.
  """
  new _create(len: U64) =>
    """
    Space for len instances of A.
    """
    compiler_intrinsic

  new null() =>
    """
    A null pointer.
    """
    compiler_intrinsic


  fun ref _realloc(len: U64): Pointer[A] =>
    """
    Keep the array, but reserve space for len instances of A.
    """
    compiler_intrinsic

  fun box _apply(i: U64): this->A =>
    """
    Retrieve index i.
    """
    compiler_intrinsic

  fun ref _update(i: U64, value: A): A^ =>
    """
    Set index i and return the previous value.
    """
    compiler_intrinsic

  fun ref _delete(offset: U64, n: U64, len: U64): A^ =>
    """
    Delete count elements from pointer + offset, compact remaining elements of
    the underlying array, and return the new length (offset + len). The
    array length before this should be offset + n + len. Returns the first
    deleted element.
    """
    compiler_intrinsic


  fun ref _copy(offset: U64, src: Pointer[A] box, len: U64): U64 =>
    """
    Copy len instances of A from src to &this[offset]
    """
    compiler_intrinsic

  fun box _concat(len: U64, with: Pointer[A] box, withlen: U64): Pointer[A] iso^
    =>
    """
    Create a new array that is this + with, length len + withlen.
    """
    compiler_intrinsic

  fun tag is_null(): Bool =>
    """
    Return true for a null pointer, false for anything else.
    """
    compiler_intrinsic

  fun tag u64(): U64 =>
    """
    Convert the pointer into a 64 bit integer.
    """
    compiler_intrinsic
