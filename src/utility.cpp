#include <algorithm> // std::find_if, std::begin, std::end, std::rbegin, std::rend
#include <cctype> // std::isspace

#include "utility.h"

void strltrim(std::string &s) {
  s.erase(
    std::begin(s),
    std::find_if(
      std::begin(s),
      std::end(s),
      [](int c) { return !std::isspace(c); }
    )
  );
}

void strrtrim(std::string &s) {
  s.erase(
    std::find_if(
      std::rbegin(s),
      std::rend(s),
      [](int c) { return !std::isspace(c); }
    ).base(),
    std::end(s)
  );
}

void strtrim(std::string &s) {
  strltrim(s);
  strrtrim(s);
}
