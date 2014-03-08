#include <windows.h>

#include <algorithm>
#include <atomic>
#include <exception>
#include <map>
#include <mutex>
#include <unordered_map>
#include <set>
#include <string>
#include <sstream>
#include <thread>

static inline void print(std::wstring msg) {
  OutputDebugStringW(msg.c_str());
  OutputDebugStringW(L"\n");
}

static inline bool EndsWith(const std::wstring& fullString, const std::wstring& ending) {
  if (fullString.length() >= ending.length()) {
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
  } else {
    return false;
  }
}

static inline bool BeginsWith(const std::wstring& haystack, const std::wstring& needle) {
  if (needle.size() > haystack.size()) {
    return false;
  } else {
    return (0 == haystack.compare(0, needle.size(), needle));
  }
}
