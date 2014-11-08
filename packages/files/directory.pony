class Directory val
  """
  TODO: on Windows this can find all the hard links to a file.
  """
  let path: String
  let files: Array[String] val

  new create(from: String) ? =>
    path = from
    files = recover
      let list = Array[String]

      @os_opendir[None](from.cstring()) ?

      while true do
        let entry = @os_readdir[Pointer[U8] iso^]()

        if entry.is_null() then
          break None
        end

        list.append(recover String.from_cstring(consume entry) end)
      end

      @os_closedir[None]()
      consume list
    end
