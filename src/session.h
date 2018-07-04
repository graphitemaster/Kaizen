#ifndef SESSION_H
#define SESSION_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>

struct Session
{
  const std::string& token() const;

  std::string time_to_string() const;
  std::string expire_to_string() const;

  void update();

private:
  friend class SessionManager;

  std::string m_token;
  std::chrono::system_clock::time_point m_time;
  std::chrono::system_clock::time_point m_expire;
};

inline const std::string& Session::token() const {
  return m_token;
}

struct SessionManager
{
public:
  Session generate_session();

  bool login(std::string session);
  bool logout(std::string session);
  bool check(std::string session);

private:
  std::mutex mutex;
  std::unordered_map<std::string, Session> sessions;
};

#endif
