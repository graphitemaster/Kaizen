#include <unistd.h> // close, write

#include "client.h"

Client::Client()
  : m_socket { }
  , m_fields { }
{
}

Client::Client(Socket&& socket)
  : m_socket { std::move(socket) }
  , m_fields { }
{
}

Client::Client(Client&& other)
  : Client(std::move(other.m_socket))
{
}

Client::~Client() {
  // { empty }
}

void Client::operator=(Client &&other) {
  m_socket = std::move(other.m_socket);
  m_fields = std::move(other.m_fields);
}

std::optional<std::string> Client::read() {
  std::string contents;
  char buffer[512];
  for (;;) {
    int n = m_socket.recieve(reinterpret_cast<uint8_t *>(buffer), sizeof buffer - 1);
    if (n < 0) {
      return std::nullopt;
    }
    buffer[n] = '\0';
    contents += buffer;
    if (n < static_cast<int>(sizeof buffer - 1)) {
      break;
    }
  }
  return contents;
}

void Client::write_line(std::string_view contents) {
  m_socket.send(reinterpret_cast<const uint8_t*>(contents.data()), contents.size());
  m_socket.send(reinterpret_cast<const uint8_t*>("\r\n"), 2);
}

void Client::write_html(std::string_view contents) {
  // 200 OK
  write_line("HTTP/1.1 200 OK");

  write_line("Server: ElastCI");

  // Content information
  write_line("Content-Type: text/html; charset=utf-8");
  write_line("Content-Length: " + std::to_string(contents.size()));

  // Write fields
  for (const auto &field : m_fields) {
    write_line(field);
  }

  // Empty \r\n followed by body
  write_line("");
  write_line(contents);

  m_fields.clear();
}

bool Client::write_file(const std::string& name) {
  write_html("Requested: " + name);
  return true;
}
