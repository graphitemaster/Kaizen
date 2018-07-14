#ifndef SERVER_H
#define SERVER_H

#include <thread> // std::thread
#include <atomic> // std::atomic_bool
#include <mutex> // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable

#include <queue> // std::queue
#include <vector> // std::vector
#include <optional> // std::optional
#include <memory> // std::unique_ptr
#include <unordered_map> // std::unordered_map

#include "client.h"
#include "socket.h"

struct SessionManager;
struct Database;

struct Server
{
  Server(uint16_t port, size_t threads, Database& db);
  ~Server();

private:
  bool do_login(Client& client, std::unordered_map<std::string, std::string>&& params);
  bool do_logout(Client& client, std::unordered_map<std::string, std::string>&& params);

  bool server_thread();
  bool client_thread();

  bool handle(Client&& client);
  bool get(Client& client,
           std::string&& url,
           std::unordered_map<std::string, std::string>&& header_fields,
           std::unordered_map<std::string, std::string>&& params);

  bool listen();
  std::optional<Client> accept();

  std::atomic_bool m_running;
  std::thread m_thread;
  std::unique_ptr<SessionManager> m_sessions;
  Socket m_socket;
  uint16_t m_port;

  // thread pool for clients and queued clients
  std::mutex m_mutex;
  std::queue<Client> m_clients;
  std::condition_variable m_condition;
  std::vector<std::thread> m_threads;

  Database& m_db;
};

#endif
