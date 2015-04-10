#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

#include "lang.h"
#include "../asio/asio.h"
#include "../asio/event.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#ifdef PLATFORM_IS_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <mswsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
typedef int SOCKET;
#endif

typedef uintptr_t PONYFD;

PONY_EXTERN_C_BEGIN

void os_closesocket(PONYFD fd);

static bool would_block()
{
#ifdef PLATFORM_IS_WINDOWS
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EWOULDBLOCK;
#endif
}

#ifdef PLATFORM_IS_MACOSX
static int set_nonblocking(SOCKET s)
{
  int flags = fcntl(s, F_GETFL, 0);
  return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}
#endif

#ifdef PLATFORM_IS_WINDOWS

#define IOCP_ACCEPT_ADDR_LEN (sizeof(struct sockaddr_storage) + 16)

static LPFN_CONNECTEX ConnectEx;
static LPFN_ACCEPTEX AcceptEx;

typedef enum
{
  IOCP_CONNECT,
  IOCP_ACCEPT
} iocp_op_t;

typedef struct iocp_t
{
  OVERLAPPED ov;
  iocp_op_t op;
  asio_event_t* ev;
} iocp_t;

typedef struct iocp_accept_t
{
  iocp_t iocp;
  SOCKET ns;
  char buf[IOCP_ACCEPT_ADDR_LEN * 2];
} iocp_accept_t;

static iocp_t* iocp_create(iocp_op_t op, asio_event_t* ev)
{
  iocp_t* iocp = POOL_ALLOC(sizeof(iocp_t));
  memset(&iocp->ov, 0, sizeof(OVERLAPPED));
  iocp->op = op;
  iocp->ev = ev;

  return iocp;
}

static void iocp_destroy(iocp_t* iocp)
{
  POOL_FREE(iocp_t, iocp);
}

static iocp_accept_t* iocp_accept_create(SOCKET s, asio_event_t* ev)
{
  iocp_accept_t* iocp = POOL_ALLOC(sizeof(iocp_accept_t));
  memset(&iocp->iocp.ov, 0, sizeof(OVERLAPPED));
  iocp->iocp.op = IOCP_ACCEPT;
  iocp->iocp.ev = ev;
  iocp->ns = s;

  return iocp;
}

static void iocp_accept_destroy(iocp_accept_t* iocp)
{
  POOL_FREE(iocp_accept_t, iocp);
}

static void CALLBACK iocp_callback(DWORD err, DWORD bytes, OVERLAPPED* ov)
{
  switch(iocp->op)
  {
    case IOCP_CONNECT:
    {
      // Dispatch a write event.
      iocp_t* iocp = (iocp_t*)ov;
      asio_event_send(iocp->ev, ASIO_WRITE, 0);
      iocp_destroy(iocp);
      break;
    }

    case IOCP_ACCEPT:
    {
      iocp_accept_t* iocp = (iocp_accept_t*)ov;

      if(err == 0)
      {
        // Dispatch a read event with the new socket as the argument.
        asio_event_send(iocp->ev, ASIO_READ, iocp->s);
      } else {
        // Close the socket.
        closesocket(iocp->s);
      }

      iocp_accept_destroy(iocp);
      break;
    }
  }
}

static bool iocp_accept(asio_event_t* ev)
{
  SOCKET s = (SOCKET)ev->data;
  WSAPROTOCOL_INFO proto;

  if(WSADuplicateSocket(s, GetCurrentProcessId(), &proto) != 0)
    return false;

  SOCKET ns = WSASocket(proto.iAddressFamily, proto.iSocketType, proto.
    iProtocol, NULL, 0, WSA_FLAG_OVERLAPPED);

  if((ns == INVALID_SOCKET) ||
    !BindIoCompletionCallback((HANDLE)ns, iocp_callback, 0))
  {
    return false;
  }

  iocp_accept_t* iocp = iocp_accept_create(ns, ev);
  DWORD bytes;

  if(!AcceptEx(s, ns, iocp->buf, 0, IOCP_ACCEPT_ADDR_LEN,
    IOCP_ACCEPT_ADDR_LEN, &bytes, &iocp->iocp.ov))
  {
    if(GetLastError() != ERROR_IO_PENDING)
      return false;
  }

  return true;
}

#endif

static PONYFD socket_from_addrinfo(struct addrinfo* p, bool server)
{
#if defined(PLATFORM_IS_LINUX)
  SOCKET fd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK,
    p->ai_protocol);
#elif defined(PLATFORM_IS_WINDOWS)
  SOCKET fd = WSASocket(p->ai_family, p->ai_socktype, p->ai_protocol, NULL, 0,
    WSA_FLAG_OVERLAPPED);
#else
  SOCKET fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
#endif

  if(fd < 0)
    return -1;

  int r = 0;

  if(server)
  {
    int reuseaddr = 1;
    r |= setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseaddr,
      sizeof(int));
  }

#ifdef PLATFORM_IS_MACOSX
  int nosigpipe = 1;
  r |= setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(int));
  r |= set_nonblocking(fd);
#endif

#ifdef PLATFORM_IS_WINDOWS
  if(!BindIoCompletionCallback((HANDLE)s, iocp_callback, 0))
    r = 1;
#endif

  if(r == 0)
    return (PONYFD)fd;

  os_closesocket((PONYFD)fd);
  return -1;
}

static bool os_listen(pony_actor_t* owner, PONYFD fd, struct addrinfo *p)
{
  if(bind((SOCKET)fd, p->ai_addr, (int)p->ai_addrlen) != 0)
  {
    os_closesocket(fd);
    return false;
  }

  if(p->ai_socktype == SOCK_STREAM)
  {
    if(listen((SOCKET)fd, SOMAXCONN) != 0)
    {
      os_closesocket(fd);
      return false;
    }
  }

  // Create an event and subscribe it.
  asio_event_t* ev = asio_event_create(owner, fd, ASIO_READ | ASIO_WRITE,
    true);

#ifdef PLATFORM_IS_WINDOWS
  if(!iocp_accept(ev))
  {
    asio_event_unsubscribe(ev);
    os_closesocket(fd);
    return false;
  }
#endif

  // Send a read event, so that it can be unsubscribed before any connections
  // are accepted.
  asio_event_send(ev, ASIO_READ, 0);
  return true;
}

static bool os_connect(pony_actor_t* owner, PONYFD fd, struct addrinfo *p)
{
#ifdef PLATFORM_IS_WINDOWS
  // Create an event and subscribe it.
  asio_event_t* ev = asio_event_create(owner, fd, ASIO_READ | ASIO_WRITE,
    true);

  iocp_t* iocp = iocp_create(IOCP_CONNECT, ev);

  if(!ConnectEx((SOCKET)fd, p->ai_addr, (int)p->ai_addrlen, NULL, 0, NULL,
    &iocp->ov))
  {
    if(GetLastError() != ERROR_IO_PENDING)
    {
      asio_event_unsubscribe(ev);
      iocp_destroy(iocp);
      os_closesocket(fd);
      return false;
    }
  }
#else
  int r = connect((SOCKET)fd, p->ai_addr, (int)p->ai_addrlen);

  if((r != 0) && (errno != EINPROGRESS))
  {
    os_closesocket(fd);
    return false;
  }

  // Create an event and subscribe it.
  asio_event_create(owner, fd, ASIO_READ | ASIO_WRITE, true);
#endif

  return true;
}

/**
 * For a server, this finds an address to listen on and returns either a valid
 * file descriptor or -1. For a client, this starts Happy Eyeballs and returns
 * the number of connection attempts in-flight, which may be 0.
 */
static PONYFD os_socket(pony_actor_t* owner, const char* host,
  const char* service, int family, int socktype, int proto, bool server)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = family;
  hints.ai_socktype = socktype;
  hints.ai_protocol = proto;

  if(server)
    hints.ai_flags |= AI_PASSIVE;

  if((host != NULL) && (host[0] == '\0'))
    host = NULL;

  struct addrinfo *result;

  if(getaddrinfo(host, service, &hints, &result) != 0)
    return server ? -1 : 0;

  struct addrinfo* p = result;
  int count = 0;

  while(p != NULL)
  {
    PONYFD fd = socket_from_addrinfo(p, server);

    if(fd != (PONYFD)-1)
    {
      if(server)
      {
        if(!os_listen(owner, fd, p))
          fd = -1;

        freeaddrinfo(result);
        return fd;
      } else {
        if(os_connect(owner, fd, p))
          count++;
      }
    }

    p = p->ai_next;
  }

  freeaddrinfo(result);
  return count;
}

PONYFD os_listen_tcp(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket(owner, host, service, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP,
    true);
}

PONYFD os_listen_tcp4(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket(owner, host, service, AF_INET, SOCK_STREAM, IPPROTO_TCP,
    true);
}

PONYFD os_listen_tcp6(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket(owner, host, service, AF_INET6, SOCK_STREAM, IPPROTO_TCP,
    true);
}

PONYFD os_listen_udp(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket(owner, host, service, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP,
    true);
}

PONYFD os_listen_udp4(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket(owner, host, service, AF_INET, SOCK_DGRAM, IPPROTO_UDP,
    true);
}

PONYFD os_listen_udp6(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket(owner, host, service, AF_INET6, SOCK_DGRAM, IPPROTO_UDP,
    true);
}

PONYFD os_connect_tcp(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket(owner, host, service, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP,
    false);
}

PONYFD os_connect_tcp4(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket(owner, host, service, AF_INET, SOCK_STREAM, IPPROTO_TCP,
    false);
}

PONYFD os_connect_tcp6(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket(owner, host, service, AF_INET6, SOCK_STREAM, IPPROTO_TCP,
    false);
}

PONYFD os_accept(asio_event_t* ev, uint64_t arg)
{
#if defined(PLATFORM_IS_WINDOWS)
  // The arg is actually the new socket. We'll return that, and also kick off
  // a new asynchronous accept for this event.
  SOCKET ns = arg;
  iocp_accept(ev);
#elif defined(PLATFORM_IS_LINUX)
  (void)arg;
  SOCKET ns = accept4((SOCKET)ev->data, NULL, NULL, SOCK_NONBLOCK);
#else
  (void)arg;
  SOCKET ns = accept((SOCKET)ev->data, NULL, NULL);

  if(ns != -1)
    set_nonblocking(ns);
#endif

  return (PONYFD)ns;
}

// Check this when a connection gets its first writeable event.
bool os_connected(PONYFD fd)
{
  int val = 0;
  socklen_t len = sizeof(int);

  if(getsockopt((SOCKET)fd, SOL_SOCKET, SO_ERROR, (char*)&val, &len) == -1)
    return false;

  return val == 0;
}

typedef struct
{
  char* host;
  char* serv;
} hostserv_t;

typedef struct
{
  pony_type_t* type;
  struct sockaddr_storage addr;
} ipaddress_t;

static socklen_t address_length(ipaddress_t* ipaddr)
{
  switch(ipaddr->addr.ss_family)
  {
    case AF_INET:
      return sizeof(struct sockaddr_in);

    case AF_INET6:
      return sizeof(struct sockaddr_in6);

    default:
      pony_throw();
  }

  return 0;
}

hostserv_t os_nameinfo(ipaddress_t* ipaddr)
{
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];

  socklen_t len = address_length(ipaddr);

  int r = getnameinfo((struct sockaddr*)&ipaddr->addr, len, host, NI_MAXHOST,
    serv, NI_MAXSERV, 0);

  if(r != 0)
    pony_throw();

  hostserv_t h;

  size_t hostlen = strlen(host);
  h.host = (char*)pony_alloc(hostlen + 1);
  memcpy(h.host, host, hostlen + 1);

  size_t servlen = strlen(serv);
  h.serv = (char*)pony_alloc(servlen + 1);
  memcpy(h.serv, serv, servlen + 1);

  return h;
}

struct addrinfo* os_addrinfo(int family, const char* host, const char* service)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;
  hints.ai_family = family;

  if((host != NULL) && (host[0] == '\0'))
    host = NULL;

  struct addrinfo *result;

  if(getaddrinfo(host, service, &hints, &result) != 0)
    return NULL;

  return result;
}

void os_getaddr(struct addrinfo* addr, ipaddress_t* ipaddr)
{
  memcpy(&ipaddr->addr, addr->ai_addr, addr->ai_addrlen);
}

struct addrinfo* os_nextaddr(struct addrinfo* addr)
{
  return addr->ai_next;
}

void os_sockname(PONYFD fd, ipaddress_t* ipaddr)
{
  socklen_t len = sizeof(struct sockaddr_storage);
  getsockname((SOCKET)fd, (struct sockaddr*)&ipaddr->addr, &len);
}

void os_peername(PONYFD fd, ipaddress_t* ipaddr)
{
  socklen_t len = sizeof(struct sockaddr_storage);
  getpeername((SOCKET)fd, (struct sockaddr*)&ipaddr->addr, &len);
}

size_t os_send(PONYFD fd, const void* buf, size_t len)
{
  // TODO: iocp

#if defined(PLATFORM_IS_LINUX)
  ssize_t sent = send((SOCKET)fd, buf, len, MSG_NOSIGNAL);
#else
  ssize_t sent = send((SOCKET)fd, (const char*)buf, (int)len, 0);
#endif

  if(sent < 0)
  {
    if(would_block())
      return 0;

    pony_throw();
  }

  return (size_t)sent;
}

size_t os_recv(PONYFD fd, void* buf, size_t len)
{
  // TODO: iocp

  ssize_t recvd = recv((SOCKET)fd, (char*)buf, (int)len, 0);

  if(recvd < 0)
  {
    if(would_block())
      return 0;

    pony_throw();
  } else if(recvd == 0) {
    pony_throw();
  }

  return (size_t)recvd;
}

size_t os_sendto(PONYFD fd, const void* buf, size_t len, ipaddress_t* ipaddr)
{
  // TODO: iocp

  socklen_t addrlen = address_length(ipaddr);

#if defined(PLATFORM_IS_LINUX)
  ssize_t sent = sendto((SOCKET)fd, buf, len, MSG_NOSIGNAL,
    (struct sockaddr*)&ipaddr->addr, addrlen);
#else
  ssize_t sent = sendto((SOCKET)fd, (const char*)buf, (int)len, 0,
    (struct sockaddr*)&ipaddr->addr, addrlen);
#endif

  if(sent < 0)
  {
    if(would_block())
      return 0;

    pony_throw();
  }

  return (size_t)sent;
}

size_t os_recvfrom(PONYFD fd, void* buf, size_t len, ipaddress_t* ipaddr)
{
  // TODO: iocp

  socklen_t addrlen = sizeof(struct sockaddr_storage);

  ssize_t recvd = recvfrom((SOCKET)fd, (char*)buf, (int)len, 0,
    (struct sockaddr*)&ipaddr->addr, &addrlen);

  if(recvd < 0)
  {
    if(would_block())
      return 0;

    pony_throw();
  } else if(recvd == 0) {
    pony_throw();
  }

  return (size_t)recvd;
}

void os_keepalive(PONYFD fd, int secs)
{
  SOCKET s = (SOCKET)fd;

  int on = (secs > 0) ? 1 : 0;
  setsockopt(s, SOL_SOCKET,  SO_KEEPALIVE, (const char*)&on, sizeof(int));

  if(on == 0)
    return;

#if defined(PLATFORM_IS_LINUX)
  int probes = secs / 2;
  setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(int));

  int idle = secs / 2;
  setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int));

  int intvl = 1;
  setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(int));
#elif defined(PLATFORM_IS_MACOSX)
  setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE, &secs, sizeof(int));
#elif defined(PLATFORM_IS_WINDOWS)
  DWORD ret = 0;

  struct tcp_keepalive k;
  k.onoff = 1;
  k.keepalivetime = secs / 2;
  k.keepaliveinterval = 1;

  WSAIoctl(s, SIO_KEEPALIVE_VALS, NULL, sizeof(struct tcp_keepalive), NULL, 0,
    &ret, NULL, NULL);
#endif
}

void os_nodelay(PONYFD fd, bool state)
{
  int val = state;
  setsockopt((SOCKET)fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&val,
    sizeof(int));
}

void os_closesocket(PONYFD fd)
{
#ifdef PLATFORM_IS_WINDOWS
  closesocket((SOCKET)fd);
#else
  close((SOCKET)fd);
#endif
}

bool os_socket_init()
{
#ifdef PLATFORM_IS_WINDOWS
  WORD ver = MAKEWORD(2, 2);
  WSADATA data;

  // Load the winsock library.
  int r = WSAStartup(ver, &data);

  if(r != 0)
    return false;

  // We need a fake socket in order to get the extension functions for IOCP.
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if(s == INVALID_SOCKET)
  {
    WSACleanup();
    return false;
  }

  GUID guid;
  DWORD dw;

  // Find ConnectEx.
  guid = WSAID_CONNECTEX;

  r = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
    &ConnectEx, sizeof(ConnectEx), &dw, NULL, NULL);

  if(r == SOCKET_ERROR)
  {
    closesocket(s);
    WSACleanup();
    return false;
  }

  // Find AcceptEx.
  guid = WSAID_ACCEPTEX;

  r = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
    &AcceptEx, sizeof(AcceptEx), &dw, NULL, NULL);

  if(r == SOCKET_ERROR)
  {
    closesocket(s);
    WSACleanup();
    return false;
  }

  closesocket(s);
#endif

  return true;
}

void os_socket_shutdown()
{
#ifdef PLATFORM_IS_WINDOWS
  WSACleanup();
#endif
}

PONY_EXTERN_C_END
