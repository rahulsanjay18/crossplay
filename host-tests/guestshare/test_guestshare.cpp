#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "GuestShareCore.h"

namespace {
int checks = 0;
int failures = 0;

void check(const bool condition, const std::string& what) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("FAIL %s\n", what.c_str());
  }
}

void testPaths() {
  for (const char* good : {"/share", "/share/", "/share/a.txt", "/share/sub/b.epub", "/share/a.b.c", "/share/sub/"}) {
    check(guestshare::isSharedPath(good), std::string("allowed: ") + good);
  }
  for (const char* bad : {"", "/", "/books/a.epub", "/sharefoo/x", "/SHARE/x", "/share/../x", "/share/sub/../../x",
                          "/share/.. /x", "/share/ ../x", "/share/.../x", "/share/.hidden", "/share/sub/.hidden",
                          "/share//x", "/share\\..\\x", "share/a", "/.crosspoint/settings.json"}) {
    check(!guestshare::isSharedPath(bad), std::string("refused: ") + bad);
  }
  check(!guestshare::isSharedPath(nullptr), "refused: null");
}

void testPassword() {
  uint32_t state = 0x12345678;
  const auto randomWord = [&state] {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  };
  std::set<std::string> seen;
  for (int i = 0; i < 2000; ++i) {
    char password[guestshare::kPasswordLength + 1];
    guestshare::generatePassword(password, randomWord);
    check(std::strlen(password) == guestshare::kPasswordLength, "password length");
    check(std::strlen(password) >= 8, "long enough for WPA2");
    for (const char c : std::string(password)) {
      check(std::strchr(guestshare::kPasswordAlphabet, c) != nullptr, "password alphabet");
      check(c != '0' && c != 'O' && c != '1' && c != 'l' && c != 'I', "no look-alikes");
    }
    seen.insert(password);
  }
  check(seen.size() == 2000, "passwords do not repeat");
  check(std::strlen(guestshare::kPasswordAlphabet) == 32, "masking is unbiased");
}

void testQr() {
  check(guestshare::wifiQrPayload("CrossPlay", "abc") == "WIFI:T:WPA;S:CrossPlay;P:abc;;", "plain payload");
  check(guestshare::wifiQrPayload("a;b", "c:d") == R"(WIFI:T:WPA;S:a\;b;P:c\:d;;)", "separators escaped");
  check(guestshare::escapeWifiField(R"(\,")") == R"(\\\,\")", "every special character escaped");
}
}  // namespace

int main() {
  testPaths();
  testPassword();
  testQr();
  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
