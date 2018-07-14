#ifndef SOCKET_H
#define SOCKET_H

#include <optional>
#include <string>

#include <cstdint>

// Portable address structure, we don't use the socket one because it's
// size isn't known and this should be a public interface.
struct Address
{
  enum Family { INET4, INET6 };
  Family family;
  uint16_t port;
  union
  {
    struct
    {
      uint32_t host;
    } v4;
    struct
    {
      uint32_t flow_info;
      uint8_t host[16];
      uint32_t scope_id;
    } v6;
  } ip;
};

struct Socket
{
  Socket();
  ~Socket();

  Socket(Socket&& other);
  Socket& operator=(Socket&& other);

  bool create(Address::Family family);
  bool bind(Address address);
  std::optional<Address> get_address() const;
  bool listen(int back_log);
  bool shutdown();
  std::optional<Socket> accept();
  int send(const uint8_t *data, size_t size);
  int recieve(uint8_t *data, size_t size);

  operator bool() const;

private:
  union
  {
    int i;
    unsigned u;
  } m_fd;
};

#endif
