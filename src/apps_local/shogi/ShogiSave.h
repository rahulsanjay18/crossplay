#pragma once

// Shogi's save file, as a freestanding pack/unpack pair so the round trip is
// host-tested. One text line:
//
//   SHOGI1 <wins> <losses> <draws> <inProgress> <opponent> <seat> <hex of Game>
//
// The game travels as the raw bytes of `Game` in hex: it is trivially copyable
// and already the wire format, so there is exactly one serialisation of a
// position in this app. A different struct size is a different file version
// and is refused; unpack also refuses a board the rules could not have
// produced, so a corrupt card falls back to the menu rather than to a
// half-drawn game.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ShogiCore.h"
#include "ShogiFlow.h"

namespace shogisave {

constexpr char kMagic[] = "SHOGI1";
constexpr int kMaxLine = 64 + static_cast<int>(sizeof(shogi::Game)) * 2;

struct Save {
  int wins = 0;
  int losses = 0;
  int draws = 0;
  bool inProgress = false;
  shogi::Opponent opponent = shogi::Opponent::Computer;
  uint8_t seat = shogi::kSente;
  shogi::Game game{};
};

// Whether a board is one a game could actually reach: one king each, no more
// pieces of a kind than the set holds, a sane side to move.
inline bool plausible(const shogi::Game& game) {
  // Qualified rather than `using namespace shogi`: the device build has a global
  // `None` of its own, and the using-directive made every bare None ambiguous
  // there while the host build, which never sees that header, compiled clean.
  namespace g = shogi;
  if (game.board.turn > g::kGote) return false;
  if (game.keyCount > g::kKeyRing || game.keyHead >= g::kKeyRing) return false;
  static constexpr int kSet[8] = {0, 18, 4, 4, 4, 4, 2, 2};
  int count[8] = {};
  int kings[2] = {};
  for (int i = 0; i < g::kCells; ++i) {
    const uint8_t cell = game.board.cell[i];
    if (cell == 0) continue;
    if ((cell & ~(g::kTypeMask | g::kGoteFlag)) != 0) return false;
    const uint8_t type = g::typeOf(cell);
    if (type == g::None || type == 13) return false;
    if (type == g::King) {
      ++kings[g::ownerOf(cell)];
      continue;
    }
    ++count[g::baseOf(type)];
  }
  for (int seat = 0; seat < 2; ++seat) {
    if (game.board.hand[seat][0] != 0) return false;
    for (int t = 1; t < g::kHandSlots; ++t) count[t] += game.board.hand[seat][t];
  }
  if (kings[0] != 1 || kings[1] != 1) return false;
  for (int t = 1; t < g::kHandSlots; ++t) {
    if (count[t] > kSet[t]) return false;
  }
  return true;
}

// Writes the line (with a trailing newline) into `out`. Returns the byte count,
// or 0 when it would not fit.
inline int pack(const Save& save, char* out, const int capacity) {
  int used =
      std::snprintf(out, static_cast<size_t>(capacity), "%s %d %d %d %d %d %d ", kMagic, save.wins, save.losses,
                    save.draws, save.inProgress ? 1 : 0, static_cast<int>(save.opponent), static_cast<int>(save.seat));
  if (used <= 0 || used >= capacity) return 0;
  const auto* bytes = reinterpret_cast<const uint8_t*>(&save.game);
  static constexpr char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < sizeof(shogi::Game); ++i) {
    if (used + 3 >= capacity) return 0;
    out[used++] = kHex[bytes[i] >> 4];
    out[used++] = kHex[bytes[i] & 15];
  }
  out[used++] = '\n';
  out[used] = '\0';
  return used;
}

inline int hexValue(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Parses a line written by pack(). Commits to `out` only when the whole line is
// good; otherwise returns false and leaves `out` untouched.
inline bool unpack(const char* line, Save& out) {
  if (line == nullptr || std::strncmp(line, kMagic, sizeof(kMagic) - 1) != 0) return false;
  int wins = 0, losses = 0, draws = 0, inProgress = 0, opponent = 0, seat = 0, consumed = 0;
  if (std::sscanf(line + sizeof(kMagic) - 1, " %d %d %d %d %d %d %n", &wins, &losses, &draws, &inProgress, &opponent,
                  &seat, &consumed) != 6) {
    return false;
  }
  if (wins < 0 || losses < 0 || draws < 0 || (inProgress != 0 && inProgress != 1)) return false;
  if (opponent != 0 && opponent != 1) return false;
  if (seat != shogi::kSente && seat != shogi::kGote) return false;
  const char* hex = line + sizeof(kMagic) - 1 + consumed;
  Save parsed;
  auto* bytes = reinterpret_cast<uint8_t*>(&parsed.game);
  for (size_t i = 0; i < sizeof(shogi::Game); ++i) {
    const int hi = hexValue(hex[i * 2]);
    const int lo = hi < 0 ? -1 : hexValue(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    bytes[i] = static_cast<uint8_t>(hi * 16 + lo);
  }
  const char tail = hex[sizeof(shogi::Game) * 2];
  if (tail != '\0' && tail != '\n' && tail != '\r') return false;
  if (!plausible(parsed.game)) return false;
  parsed.wins = wins;
  parsed.losses = losses;
  parsed.draws = draws;
  parsed.inProgress = inProgress == 1;
  parsed.opponent = static_cast<shogi::Opponent>(opponent);
  parsed.seat = static_cast<uint8_t>(seat);
  out = parsed;
  return true;
}

}  // namespace shogisave
