#include <random>
#include <sstream> // std::ostringstream
#include <iomanip> // std::put_time
#include "session.h"

// Session token alphabet
static const char k_alphabet[] = "0123456789"
                                 "abcdefghijklmnopqrstuvwxyz"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Session token length
static const size_t k_length = 128;

// Generates a random token
static std::string generate_token() {
  static thread_local std::mt19937 rg{std::random_device{}()};
  static thread_local std::uniform_int_distribution<size_t> next(0, sizeof k_alphabet - 2);

  std::string result;
  result.reserve(k_length);
  for (size_t length = k_length; length--; ) {
    result += k_alphabet[next(rg)];
  }

  return result;
}

static std::string time_point_to_string(std::chrono::system_clock::time_point now) {
  const auto convert = std::chrono::system_clock::to_time_t(now);
  std::ostringstream ss;
  ss << std::put_time(std::gmtime(&convert), "%FT%TZ");
  return ss.str();
}

std::string Session::time_to_string() const
{
  return time_point_to_string(m_time);
}

std::string Session::expire_to_string() const
{
  return time_point_to_string(m_expire);
}

void Session::update()
{
  m_time = std::chrono::system_clock::now();
  m_expire = m_time + std::chrono::hours(8);
}

bool SessionManager::login(std::string session) {
  std::lock_guard<std::mutex> lock(mutex);
  auto find = sessions.find(session);
  if (find != sessions.end()) {
    return false;
  }

  sessions[session].update();
  return true;
}

bool SessionManager::logout(std::string session) {
  std::lock_guard<std::mutex> lock(mutex);
  auto find = sessions.find(session);
  if (find != sessions.end()) {
    sessions.erase(find);
    return true;
  }
  return false;
}

bool SessionManager::check(std::string session) {
  std::lock_guard<std::mutex> lock(mutex);
  auto find = sessions.find(session);
  return find != sessions.end();
}

Session SessionManager::generate_session()
{
  Session result;
  result.m_token = generate_token();
  result.update();
  return result;
}
