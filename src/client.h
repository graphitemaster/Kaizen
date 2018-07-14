#ifndef CLIENT_H
#define CLIENT_H

#include <string_view>
#include <optional>
#include <string>
#include <vector>

#include "socket.h"

struct Client
{
  Client();
  Client(Socket&& socket);
  Client(Client&& other);
  void operator=(Client&& other);
  ~Client();

  void write_line(std::string_view contents);
  void write_html(std::string_view contents);
  bool write_file(const std::string& name);

  // Header fields and cookie writing
  void write_field(std::string_view contents);
  void write_cookie(const std::string& cookie);

  std::optional<std::string> read();

  const Socket& socket() const { return m_socket; };

private:
  Socket m_socket;
  std::vector<std::string> m_fields;
};

inline void Client::write_field(std::string_view contents) {
  m_fields.emplace_back(contents);
}

inline void Client::write_cookie(const std::string& cookie) {
  write_field("Set-Cookie:" + cookie);
}

#endif
