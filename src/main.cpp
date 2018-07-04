#include <csignal>

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>

#include "server.h"
#include "database.h"

static std::atomic_bool running_flag(true);
static std::condition_variable running_condition;
static std::mutex running_mutex;

int main() {
  signal(SIGINT, +[](int){
    running_flag.store(false);
    running_condition.notify_one();
  });

  Database db;
  if (!db.open("db.db")) {
    if (!db.create("db.db")) {
      std::cerr << "Failed to create database" << std::endl;
      return 1;
    }
  }

  const auto& contents = db.query("SELECT * FROM configuration", "ii");
  if (!contents) {
    std::cerr << "Could not read configuration from database" << std::endl;
    return 1;
  }

  const auto &config = *contents;
  const auto port = std::get<int64_t>(config[0]);
  const auto threads = std::get<int64_t>(config[1]);

  Server server(port, threads, db);

  while (running_flag.load()) {
    std::unique_lock<std::mutex> lock(running_mutex);
    running_condition.wait(lock, []{ return !running_flag.load(); });
  }

  return 0;
}
