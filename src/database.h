#ifndef DATABASE_H
#define DATABASE_H

#include <condition_variable>
#include <unordered_map>
#include <string_view>
#include <functional>
#include <thread>
#include <atomic>
#include <string>
#include <mutex>
#include <queue>
#include <variant>
#include <cstdint>

#include <sqlite3.h>

struct Database {
  Database();
  ~Database();

  typedef std::variant<bool, int64_t, std::string> Variant;

  bool open(std::string_view name);
  bool create(std::string_view name);

  // Thread safe
  bool log_http(const std::string& contents);
  bool log_system(const std::string& contents);

  // Thread safe
  std::optional<std::vector<Database::Variant>> query(
    const std::string& expression,
    const char *rd_spec = nullptr,
    const char *wr_spec = nullptr,
    ...
  );

private:
  bool log(const std::string& table, const std::string& contents);

  // Threaded function for the database
  void database_thread();
  bool create_tables();

  sqlite3 *m_db;

  // Statement cache for common prepared statements, we just reset and
  // clear bindings for reuse.
  sqlite3_stmt *create_statement(std::string_view contents);
  bool complete_statement(sqlite3_stmt *statement, int type);
  std::unordered_map<std::string, sqlite3_stmt*> m_statement_cache;

  // Enqueued tasks to run on the SQLite3 thread
  std::mutex m_mutex;
  std::condition_variable m_condition;
  std::atomic_bool m_running;
  std::queue<std::function<void()>> m_queue;

  // Needs to be the last thing initialized because the thread depends
  // on the objects above to be initialized.
  std::thread m_thread;

  void enqueue(std::function<void()> &&function);
};

#endif
