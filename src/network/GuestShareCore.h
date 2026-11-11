#pragma once

// The rules of the guest file share, free of Arduino so host-tests/guestshare
// can pin them: the hotspot password, its QR payload, and which paths a guest
// may name.

#include <cstddef>
#include <cstdint>
#include <string>

namespace guestshare {

constexpr const char* kShareRoot = "/share";
constexpr size_t kPasswordLength = 10;
// 32 symbols with no look-alikes, so masking a random word is unbiased.
constexpr char kPasswordAlphabet[] = "abcdefghijkmnpqrstuvwxyz23456789";

template <typename RandomWord>
void generatePassword(char (&out)[kPasswordLength + 1], RandomWord randomWord) {
  for (size_t i = 0; i < kPasswordLength; ++i) out[i] = kPasswordAlphabet[randomWord() & 31u];
  out[kPasswordLength] = '\0';
}

inline std::string escapeWifiField(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 4);
  for (const char c : value) {
    if (c == '\\' || c == ';' || c == ',' || c == '"' || c == ':') escaped += '\\';
    escaped += c;
  }
  return escaped;
}

inline std::string wifiQrPayload(const std::string& ssid, const std::string& password) {
  return "WIFI:T:WPA;S:" + escapeWifiField(ssid) + ";P:" + escapeWifiField(password) + ";;";
}

// True when `path` names the share root or something inside it. Segments are
// judged as SdFat opens them: it skips leading spaces and drops trailing dots
// and spaces, so ".. " is a parent reference too.
inline bool isSharedPath(const char* path) {
  if (path == nullptr) return false;
  const std::string value(path);
  const std::string root(kShareRoot);
  if (value.find('\\') != std::string::npos || value.compare(0, root.size(), root) != 0) return false;
  if (value.size() == root.size()) return true;
  if (value[root.size()] != '/') return false;

  size_t start = root.size() + 1;
  while (start <= value.size()) {
    size_t end = value.find('/', start);
    if (end == std::string::npos) end = value.size();
    size_t first = start;
    size_t last = end;
    while (first < last && value[first] == ' ') ++first;
    while (last > first && (value[last - 1] == '.' || value[last - 1] == ' ')) --last;
    const bool finalEmptySegment = start == end && end == value.size();
    if (!finalEmptySegment && (first == last || value[first] == '.')) return false;
    start = end + 1;
  }
  return true;
}

}  // namespace guestshare
