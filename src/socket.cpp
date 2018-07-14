#include <cstring> // std::memset, std::memcpy

#include "socket.h"

#if defined(_WIN32)
#include <winsock2.h>
#define get_fd(ptr) ((ptr)->m_fd.u)
#define SocketType SOCKET
#else
#include <sys/types.h>
#include <sys/socket.h> // socket
#include <netdb.h>
#include <unistd.h> // read,write,close
#define SocketType int
#define get_fd(ptr) ((ptr)->m_fd.i)
#define INVALID_SOCKET -1
#endif

Socket::Socket() {
  get_fd(this) = INVALID_SOCKET;
}

Socket::Socket(Socket&& other) {
  get_fd(this) = get_fd(&other);
  get_fd(&other) = INVALID_SOCKET;
}

Socket& Socket::operator=(Socket&& other) {
  get_fd(this) = get_fd(&other);
  get_fd(&other) = INVALID_SOCKET;
  return *this;
}

Socket::~Socket() {
  if (get_fd(this) != INVALID_SOCKET) {
#if defined(_WIN32)
    closesocket(get_fd(this));
#else
    close(get_fd(this));
#endif
  }
}

bool Socket::create(Address::Family family) {
  SocketType result = INVALID_SOCKET;
  switch (family) {
  case Address::INET4:
    result = socket(AF_INET, SOCK_STREAM, 0);
    break;
  case Address::INET6:
    result = socket(AF_INET6, SOCK_STREAM, 0);
    break;
  }
  if (result == INVALID_SOCKET) {
    return false;
  }
  get_fd(this) = result;
  return true;
}

bool Socket::bind(Address address) {
  if (address.family == Address::INET4) {
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(address.port);
    sin.sin_addr.s_addr = htonl(address.ip.v4.host);
    return ::bind(get_fd(this), reinterpret_cast<sockaddr *>(&sin), sizeof sin) == 0;
  } else if (address.family == Address::INET6) {
    struct sockaddr_in6 sin;
    std::memset(&sin, 0, sizeof sin);
    sin.sin6_family = AF_INET6;
    sin.sin6_port = htons(address.port);
    sin.sin6_flowinfo = htonl(address.ip.v6.flow_info);
    std::memcpy(sin.sin6_addr.s6_addr, address.ip.v6.host, sizeof address.ip.v6.host);
    sin.sin6_scope_id = htonl(address.ip.v6.scope_id);
    return ::bind(get_fd(this), reinterpret_cast<sockaddr *>(&sin), sizeof sin) == 0;
  }
  return false;
}

std::optional<Address> Socket::get_address() const {
  Address address;

  struct sockaddr_storage storage;
  socklen_t length = sizeof storage;
  std::memset(&storage, 0, sizeof storage);

  if (getpeername(get_fd(this), reinterpret_cast<sockaddr *>(&storage), &length) != 0) {
    return std::nullopt;
  }

  if (storage.ss_family == AF_INET) {
    struct sockaddr_in *sin = reinterpret_cast<sockaddr_in *>(&storage);
    address.family = Address::INET4;
    address.port = ntohs(sin->sin_port);
    address.ip.v4.host = sin->sin_addr.s_addr;
    return address;
  } else if (storage.ss_family == AF_INET6) {
    struct sockaddr_in6 *sin = reinterpret_cast<sockaddr_in6 *>(&storage);
    address.family = Address::INET6;
    address.port = ntohs(sin->sin6_port);
    std::memcpy(address.ip.v6.host, sin->sin6_addr.s6_addr, sizeof sin->sin6_addr.s6_addr);
    return address;
  }

  return std::nullopt;
}

bool Socket::listen(int back_log) {
  return ::listen(get_fd(this), back_log < 0 ? SOMAXCONN : back_log) == 0;
}

bool Socket::shutdown() {
#if defined(_WIN32)
  return ::shutdown(get_fd(this), SD_BOTH) == 0;
#else
  return ::shutdown(get_fd(this), SHUT_RDWR) == 0;
#endif
}

std::optional<Socket> Socket::accept() {
  SocketType result = ::accept(get_fd(this), nullptr, nullptr);
  if (result == INVALID_SOCKET) {
    return std::nullopt;
  }

  Socket value;
  get_fd(&value) = result;
  return value;
}

int Socket::send(const uint8_t *data, size_t size) {
#if defined(_WIN32)
  return send(get_fd(this), reinterpret_cast<const void *>(data), size);
#else
  return write(get_fd(this), reinterpret_cast<const void *>(data), size);
#endif
}

int Socket::recieve(uint8_t *data, size_t size) {
#if defined(_WIN32)
  return recv(get_fd(this), reinterpret_cast<void *>(data), size);
#else
  return read(get_fd(this), reinterpret_cast<void *>(data), size);
#endif
}

Socket::operator bool() const {
  return get_fd(this) != INVALID_SOCKET;
}
