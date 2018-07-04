#include <unistd.h> // close, write

#include "client.h"

Client::Client()
  : m_fd { -1 }
{
}

Client::Client(int fd)
  : m_fd { fd }
{
}

Client::Client(Client &&other)
  : m_fd { other.m_fd }
{
  other.m_fd = -1;
}

void Client::operator =(Client &&other) {
  m_fd = other.m_fd;
  other.m_fd = -1;
}

Client::~Client() {
  if (m_fd != -1) {
    ::close(m_fd);
  }
}

std::optional<std::string> Client::read() {
  std::string contents;
  char buffer[512];
  for (;;) {
    int n = ::read(m_fd, buffer, sizeof buffer - 1);
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
  ::write(m_fd, contents.data(), contents.size());
  ::write(m_fd, "\r\n", 2);
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
