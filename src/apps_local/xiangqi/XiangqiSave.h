#pragma once

// Xiangqi's save file: a freestanding pack/unpack pair, host-tested. One line:
//
//   XIANGQI1 <wins> <losses> <draws> <inProgress> <opponent> <seat> <hex of Game>
//
// Same shape and reasoning as ShogiSave.h: the Game struct is the wire format,
// so it is also the only serialisation of a position, and unpack refuses any
// board the rules could not have produced.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "XiangqiCore.h"
#include "XiangqiFlow.h"

namespace xiangqisave {

constexpr char kMagic[] = "XIANGQI1";
constexpr int kMaxLine = 64 + static_cast<int>(sizeof(xiangqi::Game)) * 2;

struct Save {
  int wins = 0;
  int losses = 0;
  int draws = 0;
  bool inProgress = false;
  xiangqi::Opponent opponent = xiangqi::Opponent::Computer;
  uint8_t seat = xiangqi::kRed;
  xiangqi::Game game{};
};

// One general each inside its own palace, nothing beyond the set's counts,
// elephants and advisors on the squares they can reach, soldiers never behind
// their starting rank.
inline bool plausible(const xiangqi::Game& game) {
  // Qualified rather than `using namespace xiangqi`: the device build has a global
  // `None` of its own, and the using-directive made every bare None ambiguous
  // there while the host build, which never sees that header, compiled clean.
  namespace g = xiangqi;
  if (game.board.turn > g::kBlack) return false;
  if (game.keyCount > g::kKeyRing || game.keyHead >= g::kKeyRing) return false;
  static constexpr int kSet[8] = {0, 1, 2, 2, 2, 2, 2, 5};
  int count[2][8] = {};
  for (int i = 0; i < g::kPoints; ++i) {
    const uint8_t cell = game.board.cell[i];
    if (cell == 0) continue;
    if ((cell & ~(g::kTypeMask | g::kBlackFlag)) != 0) return false;
    const uint8_t type = g::typeOf(cell);
    const uint8_t owner = g::ownerOf(cell);
    if (type == g::None) return false;
    const int f = g::fileOf(i);
    const int r = g::rankOf(i);
    if ((type == g::General || type == g::Advisor) && !g::inPalace(f, r, owner)) return false;
    if (type == g::Elephant && !g::ownSide(r, owner)) return false;
    if (type == g::Soldier && (owner == g::kRed ? r > 6 : r < 3)) return false;
    ++count[owner][type];
  }
  for (int seat = 0; seat < 2; ++seat) {
    if (count[seat][g::General] != 1) return false;
    for (int t = 1; t < 8; ++t) {
      if (count[seat][t] > kSet[t]) return false;
    }
  }
  return true;
}

inline int pack(const Save& save, char* out, const int capacity) {
  int used =
      std::snprintf(out, static_cast<size_t>(capacity), "%s %d %d %d %d %d %d ", kMagic, save.wins, save.losses,
                    save.draws, save.inProgress ? 1 : 0, static_cast<int>(save.opponent), static_cast<int>(save.seat));
  if (used <= 0 || used >= capacity) return 0;
  const auto* bytes = reinterpret_cast<const uint8_t*>(&save.game);
  static constexpr char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < sizeof(xiangqi::Game); ++i) {
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

inline bool unpack(const char* line, Save& out) {
  if (line == nullptr || std::strncmp(line, kMagic, sizeof(kMagic) - 1) != 0) return false;
  int wins = 0, losses = 0, draws = 0, inProgress = 0, opponent = 0, seat = 0, consumed = 0;
  if (std::sscanf(line + sizeof(kMagic) - 1, " %d %d %d %d %d %d %n", &wins, &losses, &draws, &inProgress, &opponent,
                  &seat, &consumed) != 6) {
    return false;
  }
  if (wins < 0 || losses < 0 || draws < 0 || (inProgress != 0 && inProgress != 1)) return false;
  if (opponent != 0 && opponent != 1) return false;
  if (seat != xiangqi::kRed && seat != xiangqi::kBlack) return false;
  const char* hex = line + sizeof(kMagic) - 1 + consumed;
  Save parsed;
  auto* bytes = reinterpret_cast<uint8_t*>(&parsed.game);
  for (size_t i = 0; i < sizeof(xiangqi::Game); ++i) {
    const int hi = hexValue(hex[i * 2]);
    const int lo = hi < 0 ? -1 : hexValue(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    bytes[i] = static_cast<uint8_t>(hi * 16 + lo);
  }
  const char tail = hex[sizeof(xiangqi::Game) * 2];
  if (tail != '\0' && tail != '\n' && tail != '\r') return false;
  if (!plausible(parsed.game)) return false;
  parsed.wins = wins;
  parsed.losses = losses;
  parsed.draws = draws;
  parsed.inProgress = inProgress == 1;
  parsed.opponent = static_cast<xiangqi::Opponent>(opponent);
  parsed.seat = static_cast<uint8_t>(seat);
  out = parsed;
  return true;
}

}  // namespace xiangqisave
