// Shogi and Xiangqi, played between two devices over a hostile link.
//
// Both games send their whole `Game` struct -- board, hands, the repetition
// ring -- rather than a move, the same discipline as chess's FEN. What this
// pins is that the ring travels too: two devices that agreed on every piece but
// disagreed on how often a position had occurred would call sennichite (or a
// xiangqi threefold) on one side only, which no board comparison could see.

#include <cstdio>
#include <cstring>
#include <type_traits>
#include <vector>

#include "../../src/apps_local/link/LinkPlay.h"
#include "../../src/apps_local/shogi/ShogiCore.h"
#include "../../src/apps_local/xiangqi/XiangqiCore.h"
#include "FakeLink.h"

namespace {

int checksRun = 0;
int checksFailed = 0;

void check(const bool condition, const char* what, const int line) {
  checksRun++;
  if (condition) return;
  checksFailed++;
  std::printf("FAIL test_boardlink.cpp:%d  %s\n", line, what);
}

#define CHECK(expr) check((expr), #expr, __LINE__)

using namespace linkplay;
using namespace linktest;
using Phase = PlayBase::Phase;

// What the two games have in common, so one Device template drives both.
struct ShogiRules {
  using Game = shogi::Game;
  using MoveList = shogi::MoveList;
  static constexpr GameId kId = GameId::Shogi;
  static void start(Game& game) { shogi::startGame(game); }
  static void legal(const Game& game, MoveList& list) { shogi::generateLegal(game.board, list); }
  static bool play(Game& game, const shogi::Move& move) { return shogi::play(game, move); }
  static bool running(const Game& game) { return shogi::outcome(game) == shogi::Outcome::Running; }
  static bool same(const Game& a, const Game& b) {
    return std::memcmp(&a.board, &b.board, sizeof(a.board)) == 0 && a.plies == b.plies && a.keyCount == b.keyCount &&
           a.keyHead == b.keyHead && std::memcmp(a.keys, b.keys, sizeof(a.keys)) == 0;
  }
};

struct XiangqiRules {
  using Game = xiangqi::Game;
  using MoveList = xiangqi::MoveList;
  static constexpr GameId kId = GameId::Xiangqi;
  static void start(Game& game) { xiangqi::startGame(game); }
  static void legal(const Game& game, MoveList& list) { xiangqi::generateLegal(game.board, list); }
  static bool play(Game& game, const xiangqi::Move& move) { return xiangqi::play(game, move); }
  static bool running(const Game& game) { return xiangqi::outcome(game) == xiangqi::Outcome::Running; }
  static bool same(const Game& a, const Game& b) {
    return std::memcmp(&a.board, &b.board, sizeof(a.board)) == 0 && a.plies == b.plies && a.idlePlies == b.idlePlies &&
           a.keyCount == b.keyCount && a.keyHead == b.keyHead && std::memcmp(a.keys, b.keys, sizeof(a.keys)) == 0;
  }
};

template <typename Rules>
struct Device {
  Device(Medium& medium, const uint8_t last) : transport(medium, addressOf(last)), play(&transport) {}

  FakeTransport transport;
  Play<typename Rules::Game> play;
  typename Rules::Game game{};
  typename Rules::MoveList legal;
  bool first = false;
  int plies = 0;
  bool refused = false;
  bool rejected = false;

  bool start() { return play.start(Rules::kId, nullptr); }

  void pump(const uint32_t nowMs, const int moveLimit) {
    const Phase phase = play.update(nowMs);
    if (play.takeMatchStart()) {
      // Both sides set up the fixed opening; whoever goes first moves first.
      first = play.goesFirst();
      Rules::start(game);
      Rules::legal(game, legal);
      plies = 0;
    }
    if (play.takeOpponent(game)) Rules::legal(game, legal);

    if (phase != Phase::YourTurn || plies >= moveLimit || legal.count == 0 || !Rules::running(game)) return;
    const auto move = legal.moves[static_cast<int>(nowMs / 10) % legal.count];
    // The rules refuse anything they did not offer; a refusal here is a bug.
    if (!Rules::play(game, move)) {
      rejected = true;
      return;
    }
    Rules::legal(game, legal);
    if (!play.play(game)) refused = true;
    plies++;
  }
};

template <typename Rules>
void run(Medium& medium, std::vector<Device<Rules>*>& devices, const uint32_t durationMs, const int moveLimit) {
  const uint32_t until = medium.nowMs + durationMs;
  while (medium.nowMs < until) {
    for (Device<Rules>* device : devices) device->pump(medium.nowMs, moveLimit);
    medium.collect();
    medium.nowMs += 10;
  }
}

template <typename Rules>
void manyGames(const char* name) {
  constexpr int kGames = 20;
  int agreed = 0;
  int fewest = 1000;
  for (int index = 0; index < kGames; ++index) {
    Medium medium;
    medium.random = static_cast<uint32_t>(index * 2654435761u) | 1u;
    medium.nowMs = static_cast<uint32_t>(index) * 811u;
    medium.lossPercent = index % 28;
    medium.duplicatePercent = index % 15;
    medium.maxJitterMs = index % 45;

    Device<Rules> a(medium, static_cast<uint8_t>(0x50 + (index % 6)));
    Device<Rules> b(medium, static_cast<uint8_t>(0xC0 + (index % 4)));
    std::vector<Device<Rules>*> devices = {&a, &b};
    a.start();
    b.start();
    run<Rules>(medium, devices, 40000, 12);

    CHECK(a.first != b.first);
    if (Rules::same(a.game, b.game) && !a.refused && !b.refused && !a.rejected && !b.rejected) agreed++;
    const int total = a.plies + b.plies;
    if (total < fewest) fewest = total;
  }
  if (agreed != kGames) std::printf("  %s: %d of %d games agreed\n", name, agreed, kGames);
  CHECK(agreed == kGames);
  // Every game got properly under way, rather than agreeing by never starting.
  CHECK(fewest >= 20);
}

void theStatesFitOnePacket() {
  static_assert(std::is_trivially_copyable<shogi::Game>::value, "copied as bytes");
  static_assert(std::is_trivially_copyable<xiangqi::Game>::value, "copied as bytes");
  CHECK(sizeof(shogi::Game) <= kMaxPayloadBytes);
  CHECK(sizeof(xiangqi::Game) <= kMaxPayloadBytes);
  CHECK(GameId::Shogi != GameId::Xiangqi);
}

}  // namespace

int main() {
  theStatesFitOnePacket();
  manyGames<ShogiRules>("shogi");
  manyGames<XiangqiRules>("xiangqi");
  std::printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}
