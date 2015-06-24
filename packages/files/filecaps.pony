use "collections"

primitive FileCreate
  fun value(): U64 => 1 << 0

primitive FileChmod
  fun value(): U64 => 1 << 1

primitive FileChown
  fun value(): U64 => 1 << 2

primitive FileLink
  fun value(): U64 => 1 << 3

primitive FileLookup
  fun value(): U64 => 1 << 4

primitive FileRead
  fun value(): U64 => 1 << 5

primitive FileRemove
  fun value(): U64 => 1 << 6

primitive FileRename
  fun value(): U64 => 1 << 7

primitive FileSeek
  fun value(): U64 => 1 << 8

primitive FileStat
  fun value(): U64 => 1 << 9

primitive FileSync
  fun value(): U64 => 1 << 10

primitive FileTime
  fun value(): U64 => 1 << 11

primitive FileTruncate
  fun value(): U64 => 1 << 12

primitive FileWrite
  fun value(): U64 => 1 << 13

type FileCaps is Flags[
  ( FileCreate
  | FileChmod
  | FileChown
  | FileLink
  | FileLookup
  | FileRead
  | FileRemove
  | FileRename
  | FileSeek
  | FileStat
  | FileSync
  | FileTime
  | FileTruncate
  | FileWrite
  ), U64]
