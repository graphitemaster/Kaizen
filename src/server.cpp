#include <sstream> // std::istringstream, std::getline
#include <regex> // std::regex, std::regex_search, std::smatch

#include "server.h"
#include "session.h"
#include "database.h"
#include "utility.h"

#include <cstring> // std::memset

bool Server::client_thread() {
  while (m_running.load()) {
    Client client;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_condition.wait(lock, [this] {
          return !m_running.load() || !m_clients.empty();
      });
      if (!m_running.load() && m_clients.empty()) {
        return true;
      }
      client = std::move(m_clients.front());
      m_clients.pop();
    }
    handle(std::move(client));
  }
  return false;
}

bool Server::server_thread() {
  if (!m_socket.create(Address::INET4)) {
    return false;
  }

  if (!listen()) {
    return false;
  }

  while (m_running.load()) {
    auto client = accept();
    if (client) {
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_clients.emplace(std::move(*client));
      }
      m_condition.notify_one();
    }
  }

  return true;
}

Server::Server(uint16_t port, size_t threads, Database& db)
  : m_running  { true }
  , m_thread   { &Server::server_thread, this }
  , m_sessions { new SessionManager }
  , m_port     { port }
  , m_db       { db }
{
  db.log_system("Starting server");

  for (size_t i = 0; i < threads; i++) {
    db.log_system("Starting server accept thread: " + std::to_string(i));
    m_threads.emplace_back(&Server::client_thread, this);
  }
}

Server::~Server()
{
  m_db.log_system("Stopping server");

  // So threads don't continue
  m_running.store(false);

  // Drain the clients
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_clients.empty()) {
      m_db.log_system("Terminating active client connections");
      while (!m_clients.empty()) {
        m_db.log_system("Terminating client " + std::to_string(m_clients.size()));
        m_clients.pop();
      }
    }
  }

  // Stop client thread pool
  m_db.log_system("Stopping server accept threads");
  m_condition.notify_all();
  for (std::thread &thread : m_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  if (m_socket) {
    m_socket.shutdown();
  }

  // Stop the listening thread
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

bool Server::listen() {
  Address address;
  address.family = Address::INET4;
  address.port = m_port;
  address.ip.v4.host = 0;
  if (!m_socket.bind(address)) {
    return false;
  }
  return m_socket.listen(-1);
}

std::optional<Client> Server::accept() {
  std::optional<Socket> socket = m_socket.accept();
  if (!socket) {
    return std::nullopt;
  }
  return { std::move(socket) };
}

static std::unordered_map<std::string, std::string> parse_http_header(const std::string& contents) {
  std::unordered_map<std::string, std::string> fields;
  std::istringstream response(contents);
  std::string header;
  std::string::size_type index = 0;
  while (std::getline(response, header) && header != "\r") {
    index = header.find(':', 0);
    if (index != std::string::npos) {
      std::string k = header.substr(0, index);
      std::string v = header.substr(index + 1);
      // Trim trailing and leading whitespace and newlines
      strtrim(k);
      strtrim(v);
      fields.insert({k, v});
    }
  }
  return fields;
}

bool Server::handle(Client&& client) {
  const auto &contents = client.read();
  if (contents) {
    // Read the first line
    std::string line;
    std::istringstream stream(*contents);
    std::getline(stream, line);

    // Seperate the parts
    stream.clear();
    stream.str(line);
    std::string method;
    std::string query;
    std::string protocol;

    if (!(stream >> method >> query >> protocol)) {
      client.write_line("HTTP/1.1 300 Error");
      return false;
    }

    // Fetch the URL
    stream.clear();
    stream.str(query);
    std::string url;
    if (!std::getline(stream, url, '?')) {
      client.write_line("HTTP/1.1 300 Error");
      return false;
    }

    // Read the parameters
    std::unordered_map<std::string, std::string> parameters;
    std::string pair;
    std::string key;
    std::string value;

    while (std::getline(stream, pair, '&')) {
      std::istringstream split(pair);
      if (std::getline(std::getline(split, key, '='), value)) {
        parameters[key] = value;
      }
    }

    m_db.log_http(method + " " + url);

    if (method == "GET") {
      auto&& header_fields = parse_http_header(*contents);
      return get(client, std::move(url), std::move(header_fields), std::move(parameters));
    }
  }

  return false;
}

bool Server::get(Client& client,
                 std::string&& url,
                 std::unordered_map<std::string, std::string>&& header_fields,
                 std::unordered_map<std::string, std::string>&& params)
{
  if (url == "/login") {
    return do_login(client, std::move(params));
  } else if (url == "/logout") {
    return do_logout(client, std::move(header_fields));
  } else if (url.find("/api") == 0) {
    client.write_html("Content: " + url);
    return true;
  } else {
    if (url != "/") {
      return client.write_file(url);
    }
    return client.write_file("/resource/login/html");
  }
  return false;
}

bool Server::do_login(Client& client,
                      std::unordered_map<std::string, std::string>&& params)
{
  const auto username = params.find("username");
  const auto password = params.find("password");

  bool valid = true;

  if (username == params.end() || password == params.end()) {
    valid = false;
  }

  // Refresh to /
  client.write_field("Refresh: 0; url=/");
  client.write_html("");

  return valid;
}

bool Server::do_logout(Client& client,
                       std::unordered_map<std::string, std::string>&& header_fields)
{
  // Generate HTML to refresh to /
  client.write_field("Refresh: 0; url=/");
  client.write_html("");
  return true;
}
