#ifndef SOCKET_H
#define SOCKET_H

enum class SocketType {
  Stream,
  Datagram
};

struct Address
{
  uint32_t host;
  uint16_t port;
};

struct Socket
{
  Socket(SocketType type);
  bool bind(const Address& address);
  std::optional<Address> get_address() const;
  bool listen(int backlog);
  std::optional<Socket> accept(Address *address);

private:
  void* m_fd;
};

#endif
