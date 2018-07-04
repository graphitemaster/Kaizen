#include <chrono>
#include <future>

#include "database.h"

static constexpr const char k_schema[] =
R"(
BEGIN TRANSACTION;

PRAGMA foreign_keys = ON;

CREATE TABLE configuration(
  http_port                     INTEGER NOT NULL,
  http_threads                  INTEGER NOT NULL
);

CREATE TABLE users(
  id                            INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  username                      VARCHAR(32) NOT NULL UNIQUE,
  email                         VARCHAR(320) NOT NULL UNIQUE,
  pw_version                    INTEGER NOT NULL,
  pw_salt                       VARCHAR(32) NOT NULL UNIQUE,
  pw_hash                       TEXT NOT NULL
);

CREATE TABLE projects(
  id                            INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name                          TEXT NOT NULL UNIQUE,
  enabled                       BOOLEAN NOT NULL
);

CREATE TABLE users_projects(
  user_id                       INTEGER NOT NULL,
  project_id                    INTEGER NOT NULL,

  PRIMARY KEY(user_id, project_id),

  FOREIGN KEY(user_id)          REFERENCES users(id),
  FOREIGN KEY(project_id)       REFERENCES projects(id)
);

CREATE TABLE configurations(
  id                            INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name                          VARCHAR(128) NOT NULL UNIQUE,
  project_id                    INTEGER NOT NULL,

  FOREIGN KEY(project_id)       REFERENCES projects(id)
);

CREATE TABLE projects_configuration(
  project_id                    INTEGER NOT NULL,
  configuration_id              INTEGER NOT NULL,

  PRIMARY KEY(project_id, configuration_id),

  FOREIGN KEY(project_id)       REFERENCES projects(id),
  FOREIGN KEY(configuration_id) REFERENCES configurations(id)
);

CREATE TABLE builds(
  id                            INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  project_id                    INTEGER NOT NULL,
  status                        INTEGER NOT NULL,
  start_timestamp               INTEGER NOT NULL,
  end_timestamp                 INTEGER,

  FOREIGN KEY(project_id)       REFERENCES projects(id)
);

CREATE TABLE build_logs(
  id                            INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  project_id                    INTEGER NOT NULL,
  build_id                      INTEGER NOT NULL,
  configuration_id              INTEGER NOT NULL,
  contents                      TEXT NOT NULL,

  FOREIGN KEY(project_id)       REFERENCES projects(id),
  FOREIGN KEY(build_id)         REFERENCES builds(id),
  FOREIGN KEY(configuration_id) REFERENCES configurations(id)
);

CREATE TABLE system_logs(
  id                            INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  timestamp                     INTEGER NOT NULL,
  contents                      TEXT NOT NULL
);

CREATE TABLE http_logs(
  id                            INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  timestamp                     INTEGER NOT NULL,
  contents                      TEXT NOT NULL
);

INSERT INTO configuration VALUES(80, 4);

CREATE TRIGGER configuration_prevent_insertion
  BEFORE INSERT ON configuration WHEN(SELECT COUNT(*) FROM configuration) >= 1
BEGIN
  SELECT RAISE(FAIL, 'Only one row allowed for configuration');
END;

COMMIT;
)";

Database::Database()
  : m_db      { nullptr }
  , m_running { true }
  , m_thread  { &Database::database_thread, this }
{
}

Database::~Database() {
  log_system("Closed database");

  // No longer running
  m_running.store(false);

  // Wake up the blocked thread
  m_condition.notify_one();

  // Join the thread
  if (m_thread.joinable()) {
    m_thread.join();
  }

  // Release memory held by the database
  if (m_db) {
    // Finalize any prepared statements in cache
    for (auto &[_, statement] : m_statement_cache) {
      sqlite3_finalize(statement);
    }

    // Close the database
    sqlite3_close(m_db);
  }
}

bool Database::open(std::string_view name) {
  if (sqlite3_open_v2(name.data(), &m_db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK) {
    log_system("Opened database (Existing)");
    return true;
  }
  return false;
}

bool Database::create(std::string_view name) {
  if (sqlite3_open(name.data(), &m_db) == SQLITE_OK) {
    if (create_tables()) {
      log_system("Opened database (Created)");
      return true;
    }
  }
  return false;
}

std::optional<std::vector<Database::Variant>> Database::query(
  const std::string& expression,
  const char *rd_spec,
  const char *wr_spec,
  ...
) {
  std::vector<Variant> wr_data;
  std::vector<Variant> rd_data;

  // Collect variants to write to the database
  va_list va;
  va_start(va, wr_spec);
  for (const char *ch = wr_spec; ch && *ch; ch++) {
    if (*ch == 's') {
      wr_data.emplace_back(std::string(va_arg(va, const char *)));
    } else if (*ch == 'i') {
      wr_data.emplace_back(static_cast<int64_t>(va_arg(va, int64_t)));
    } else if (*ch == 'b') {
      wr_data.emplace_back(static_cast<int64_t>(va_arg(va, int)));
    } else {
      return std::nullopt;
    }
  }
  va_end(va);

  std::promise<std::optional<std::vector<Variant>>> promise;
  std::future<std::optional<std::vector<Variant>>> future = promise.get_future();

  std::string error_message("Unknown");
  enqueue([&]{
    sqlite3_stmt *statement = create_statement(expression);

    if (!statement) {
      goto error;
    }

    // Bind statements
    for (const char *ch = wr_spec; ch && *ch; ch++) {
      const size_t index = ch - wr_spec;
      if (*ch == 's') {
        const std::string& text = std::get<std::string>(wr_data[index]);
        if (sqlite3_bind_text(statement, index + 1, text.data(), text.size(), nullptr) != SQLITE_OK) {
          goto error;
        }
      } else if (*ch == 'i') {
        const auto value = std::get<int64_t>(wr_data[index]);
        if (sqlite3_bind_int(statement, index + 1, value) != SQLITE_OK) {
          goto error;
        }
      } else if (*ch == 'b') {
        const auto value = std::get<bool>(wr_data[index]);
        if (sqlite3_bind_int(statement, index + 1, static_cast<int>(value)) != SQLITE_OK) {
          goto error;
        }
      }
    }

    if (!complete_statement(statement, rd_spec ? SQLITE_ROW : SQLITE_DONE)) {
      goto error;
    }

    if (rd_spec) {
      for (const char *ch = rd_spec; *ch; ch++) {
        const size_t index = ch - rd_spec;
        if (*ch == 's') {
          rd_data.emplace_back(sqlite3_column_text(statement, index));
        } else if (*ch == 'i') {
          rd_data.emplace_back(static_cast<int64_t>(sqlite3_column_int(statement, index)));
        } else if (*ch == 'b') {
          rd_data.emplace_back(static_cast<bool>(sqlite3_column_int(statement, index)));
        }
      }
    }

    promise.set_value(rd_data);
    return;

  error:
    error_message = sqlite3_errmsg(m_db);
    promise.set_value(std::nullopt);
  });

  // Wait for the database thread to execute the operation
  return future.get();
}

bool Database::create_tables() {
  return sqlite3_exec(m_db, k_schema, nullptr, nullptr, nullptr) == SQLITE_OK;
}

sqlite3_stmt *Database::create_statement(std::string_view contents) {
  auto find = m_statement_cache.find(contents.data());
  if (find != m_statement_cache.end()) {
    auto *statement = find->second;
    if (sqlite3_reset(statement) != SQLITE_OK || sqlite3_clear_bindings(statement) != SQLITE_OK) {
      sqlite3_finalize(statement);
      m_statement_cache.erase(find);
      goto create;
    }
    return statement;
  }

create:
  // Keep trying until we can create a statement
  sqlite3_stmt *statement = nullptr;
  int prepare = 0;
  while ((prepare = sqlite3_prepare_v2(m_db, contents.data(), -1, &statement, nullptr)) == SQLITE_BUSY) {
    ;
  }

  // Couldn't create a prepared statement
  if (prepare != SQLITE_OK) {
    return nullptr;
  }

  m_statement_cache.insert({ contents.data(), statement });
  return statement;
}

bool Database::complete_statement(sqlite3_stmt *statement, int type) {
  int attempt = 0;
  while ((attempt = sqlite3_step(statement)) == SQLITE_BUSY) {
    ;
  }
  return attempt == type;
}

void Database::enqueue(std::function<void()> &&function)
{
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queue.emplace(std::move(function));
  }
  m_condition.notify_one();
}

bool Database::log(const std::string& table, const std::string& contents) {
  const auto now = std::chrono::system_clock::now();
  const auto epoch = now.time_since_epoch();
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
  const auto timestamp = static_cast<int64_t>(seconds.count());
  const auto result = query("INSERT INTO " + table + "(timestamp, contents) VALUES(?, ?)", nullptr, "is", timestamp, contents.c_str());
  return !!result;
}

bool Database::log_http(const std::string& contents) {
  return log("http_logs", contents);
}

bool Database::log_system(const std::string& contents) {
  return log("system_logs", contents);
}

// Database thread
void Database::database_thread() {
  for (;;) {
    std::function<void()> function;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_condition.wait(lock, [this] {
          return !m_running.load() || !m_queue.empty();
      });
      if (!m_running.load() && m_queue.empty()) {
        return;
      }
      function = std::move(m_queue.front());
      m_queue.pop();
    }
    function();
  }
}
