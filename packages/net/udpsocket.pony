actor UDPSocket
  var _notify: UDPNotify
  var _fd: U32 = -1
  var _event: Pointer[_Event] tag = Pointer[_Event]
  var _readable: Bool = false
  var _closed: Bool = false
  var _packet_size: U64

  new create(notify: UDPNotify iso, host: String = "", service: String = "0",
    size: U64 = 1024)
  =>
    """
    Listens for both IPv4 and IPv6 datagrams.
    """
    _notify = consume notify
    _fd = @os_listen_udp[U32](this, host.cstring(), service.cstring())
    _packet_size = size
    _notify_listening()

  new ip4(notify: UDPNotify iso, host: String = "", service: String = "0",
    size: U64 = 1024)
  =>
    """
    Listens for IPv4 datagrams.
    """
    _notify = consume notify
    _fd = @os_listen_udp4[U32](this, host.cstring(), service.cstring())
    _packet_size = size
    _notify_listening()

  new ip6(notify: UDPNotify iso, host: String = "", service: String = "0",
    size: U64 = 1024)
  =>
    """
    Listens for IPv6 datagrams.
    """
    _notify = consume notify
    _fd = @os_listen_udp6[U32](this, host.cstring(), service.cstring())
    _packet_size = size
    _notify_listening()

  be write(data: Bytes val, to: IPAddress) =>
    """
    Write the datagram to the socket.
    """
    if not _closed then
      try
        @os_sendto[U64](_fd, data.cstring(), data.size(), to) ?
      else
        _close()
      end
    end

  be dispose() =>
    """
    Stop listening.
    """
    _close()

  fun box local_address(): IPAddress =>
    """
    Return the bound IP address.
    """
    let ip = recover IPAddress end
    @os_sockname[None](_fd, ip)
    ip

  fun ref set_notify(notify: UDPNotify) =>
    """
    Change the notifier.
    """
    _notify = notify

  fun box _get_fd(): U32 =>
    """
    Private call to get the file descriptor, used by the Socket trait.
    """
    _fd

  be _event_notify(event: Pointer[_Event] tag, flags: U32) =>
    """
    When we are readable, we accept new connections until none remain.
    """
    if not _closed then
      _event = event

      if _Event.readable(flags) then
        _readable = true
        _pending_reads()
      end
    end

    if _Event.disposable(flags) then
      _event = _Event.dispose(event)
    end

  be _read_again() =>
    """
    Resume reading.
    """
    if not _closed then
      _pending_reads()
    end

  fun ref _pending_reads() =>
    """
    Read while data is available, guessing the next packet length as we go. If
    we read 4 kb of data, send ourself a resume message and stop reading, to
    avoid starving other actors.
    """
    try
      var sum: U64 = 0

      while _readable do
        var len = _packet_size
        var data = recover Array[U8].undefined(len) end
        var from = recover IPAddress end
        len = @os_recvfrom[U64](_fd, data.cstring(), data.space(), from) ?

        if len == 0 then
          _readable = false
          return
        end

        data.truncate(len)
        _notify.received(this, consume data, consume from)

        sum = sum + len

        if sum > (1 << 12) then
          _read_again()
          return
        end
      end
    else
      _close()
    end

  fun ref _notify_listening() =>
    """
    Inform the notifier that we're listening.
    """
    if _fd != -1 then
      _notify.listening(this)
    else
      _notify.not_listening(this)
    end

  fun ref _close() =>
    """
    Inform the notifier that we've closed.
    """
    _Event.unsubscribe(_event)
    _readable = false
    _closed = true

    if _fd != -1 then
      _notify.closed(this)
      @os_closesocket[None](_fd)
      _fd = -1
    end
