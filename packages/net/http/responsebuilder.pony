use "net"

class _ResponseBuilder is TCPConnectionNotify
  """
  This builds a response payload using received chunks of data.
  """
  let _client: Client
  let _buffer: Buffer = Buffer
  let _builder: _PayloadBuilder = _PayloadBuilder.response()

  new iso create(client: Client) =>
    """
    The response builder needs to know which client to forward the response to.
    """
    _client = client

  fun ref connected(conn: TCPConnection ref) =>
    """
    Tell the client we have connected.
    """
    _client._connected()

  fun ref connect_failed(conn: TCPConnection ref) =>
    """
    The connection could not be established. Tell the client not to proceed.
    """
    _client._connect_failed()

  fun ref auth_failed(conn: TCPConnection ref) =>
    """
    SSL authentication failed. Tell the client not to proceed.
    """
    _client._auth_failed()

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    """
    Assemble chunks of data into a response. When we have a whole response,
    give it to the client and start a new one.
    """
    _buffer.append(consume data)
    _builder.parse(_buffer)

    match _builder.state()
    | _PayloadReady
    | _PayloadError =>
      _client._response(_builder.done())
    end

  fun ref closed(conn: TCPConnection ref) =>
    """
    The connection has closed, possibly prematurely.
    """
    _builder.closed(_buffer)
    _client._response(_builder.done())
    _buffer.clear()
    _client._closed()
