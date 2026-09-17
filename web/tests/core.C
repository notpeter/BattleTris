#include "Drop.H"
#include "Game.H"
#include "BTBox.H"
#include "Match.H"
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
#include <cstdint>
#include <limits>
#include <string>

void plannerRules();
void effectRules();
void combatRules();
void swapRules();
void peerRules();
void structureRules();
void landingRules();
void motionRules();
void upsideRules();
void upsidePlannerRules();
void reconRules();
void reconMatchRules();

static std::vector<int> state(BrowserGame &game) {
  std::vector<int> result{game.lines, game.funds, game.score,
    static_cast<int>(game.generation), game.over, game.paused,
    static_cast<int>(game.pendingWeapons()),
    game.sliding(), static_cast<int>(game.slideElapsed()), static_cast<int>(game.elapsed),
    static_cast<int>(game.motionRevision()), static_cast<int>(game.hatterElapsed()),
    static_cast<int>(game.slickElapsed()), game.slickDirection(), game.slickSuppressed(),
    static_cast<int>(game.board.random.state() & 65535),
    static_cast<int>(game.board.random.state() >> 16)};
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) {
      result.push_back(game.cell(x, y));
      result.push_back(game.board.occupied(x, y));
    }
  for (int token = 0; token < BT_MAX_WEAPONS; ++token) {
    result.push_back(game.weapons.remaining(token));
    result.push_back(game.weapons.applications(token));
  }
  return result;
}

static void fillRow(BrowserGame &game, int y, int dieValue) {
  // Replay fixtures may complete a partially filled row. fill() transfers
  // ownership into a vacant square; replacing existing boxes leaks them.
  for (int x = 0; x < BT_BOARD_WTH; ++x)
    if (!game.board.occupied(x, y))
      game.board.fill(x, y, x == 0 ? game.board.box_manager_->dieCreate(x, y, dieValue)
                                  : game.board.box_manager_->create(x, y, BT_RED));
  game.board.landed(0, y);
}

static void lineRules() {
  BrowserGame game;
  fillRow(game, 27, 3);
  fillRow(game, 26, 5);
  assert(game.board.checkLines() == 16);
  assert(game.lines == 2 && game.funds == 16);
  for (int y = 0; y < 28; ++y)
    for (int x = 0; x < 10; ++x) assert(game.board.cell(x, y) == 0);

  game.board.fill(0, 27, game.board.box_manager_->happyCreate(0, 27));
  game.board.landed(0, 27);
  assert(game.board.checkLines() == 0);
  assert(game.board.cell(0, 27) == BT_UNHAPPY);
  for (int x = 1; x < 10; ++x)
    game.board.fill(x, 27, game.board.box_manager_->create(x, 27, BT_RED));
  game.board.landed(1, 27);
  assert(game.board.checkLines() == 0); // Line without funds still emits BT_LINE.
  assert(game.lines == 3 && game.funds == 16);

  game.board.fill(0, 27, game.board.box_manager_->happyCreate(0, 27));
  for (int x = 1; x < 10; ++x)
    game.board.fill(x, 27, game.board.box_manager_->create(x, 27, BT_RED));
  game.board.landed(0, 27);
  assert(game.board.checkLines() == 150);
  assert(game.lines == 4 && game.funds == 166);
  for (int y = 24; y < 28; ++y) fillRow(game, y, 2);
  assert(game.board.checkLines() == 32);
  assert(game.lines == 8 && game.funds == 198 && game.score == 0);
}

static void pieceRules() {
  BrowserGame game;
  std::vector<std::unique_ptr<BTPiece>> pieces;
#define PIECE(type) pieces.emplace_back(new type(&game.board))
  PIECE(BTElPiece); PIECE(BTRevElPiece); PIECE(BTSldRtPiece); PIECE(BTSldLftPiece);
  PIECE(BTLongPiece); PIECE(BTPlugPiece); PIECE(BTBoxPiece); PIECE(BTDiePiece);
  PIECE(BTHappyPiece); PIECE(BTDogPiece); PIECE(BTRevDogPiece); PIECE(BTCapPiece);
  PIECE(BTWallPiece); PIECE(BTTowerPiece); PIECE(BTStarPiece); PIECE(BTWeirdLongPiece);
  PIECE(BTFourByFourPiece); PIECE(BTLongDongPiece);
#undef PIECE
  for (auto &piece : pieces) {
    piece->construct(1, 4);
    assert(piece->moveTo(1, 4));
    int before[64];
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x) before[y * 8 + x] = piece->cell(x, y);
    for (int turn = 0; turn < 12; ++turn) piece->rotate();
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x) assert(before[y * 8 + x] == piece->cell(x, y));
    assert(!piece->canMoveTo(-8, 4));
    assert(!piece->canMoveTo(10, 4));
    assert(!piece->canMoveTo(1, 28));
    while (piece->moveTo(piece->x(), piece->y() + 1)) {}
    assert(!piece->canMoveTo(piece->x(), piece->y() + 1));
    piece->landed();
    piece->reset();
    game.board.clear();
  }
}

static void lifecycle() {
  BrowserGame game;
  game.reset(42);
  const int startY = game.active->y();
  game.input(5);
  for (int i = 0; i < 20; ++i) { game.tick(100); finishDrop(game); }
  assert(game.active->y() == startY);
  game.input(5);
  for (int i = 0; i < 6; ++i) game.tick(100);
  assert(game.active->y() == startY + 1);
  for (int i = 0; i < 1000 && !game.over; ++i) finishDrop(game);
  assert(game.over && !game.active);
  game.reset(42);
  assert(!game.over && !game.paused && game.lines == 0 && game.funds == 0);
  // Restart while a piece is active, then destroy it under ASan.
  for (int i = 0; i < 100; ++i) {
    game.reset(i);
    game.input(0); game.input(2); finishDrop(game);
  }
}

static void scoreRules() {
  BrowserGame game;
  game.reset(42);
  for (int i = 0; i < 6; ++i) game.tick(100);
  assert(game.score == 0);
  const int award = BT_BOARD_HGT - game.active->y();
  game.input(3);
  game.input(3);
  assert(game.score == award);
  finishDrop(game);
  assert(game.score == award);
  finishDrop(game);
  assert(game.score == award + BT_BOARD_HGT);
  BrowserGame ai(true);
  ai.reset(42);
  ai.input(3);
  assert(ai.score == 0);
  finishDrop(ai);
  assert(ai.score == BT_BOARD_HGT / 2);
}

static void randomRules() {
  BTRandom rng;
  rng.seed(42);
  const int expected[] = {5677716, 1418009174, 238278529, 1824023008, 1879991778};
  for (int value : expected) assert(rng.integer() == value);
  rng.seed(0);
  assert(rng.integer() != 0);
  BrowserGame a, b, noise;
  a.reset(123); b.reset(123); noise.reset(987);
  for (int i = 0; i < 200; ++i) {
    assert(state(a) == state(b));
    assert(a.board.random.state() == b.board.random.state());
    const int action = i % 5;
    a.input(action);
    noise.reset(i);
    finishDrop(noise);
    b.input(action);
    if (a.over) { a.reset(123 + i); b.reset(123 + i); }
  }
}

static void matchRules() {
  int previousPlacement = 6000;
  for (int level = 0; level < 15; ++level) {
    BrowserMatch match;
    match.start(42, 1, level);
    const auto generation = match.opponent.generation;
    int elapsed = 0;
    while (match.opponent.generation == generation && elapsed < 6000) {
      match.tick(10);
      elapsed += 10;
    }
    assert(match.opponent.generation > generation);
    assert(elapsed <= previousPlacement);
    previousPlacement = elapsed;
  }
  assert(previousPlacement < 1000); // Bionic must not stall on its zero-delay path.

  BrowserMatch a, b;
  a.start(42, 1, 10); b.start(42, 1, 10);
  for (int i = 0; i < 200; ++i) {
    a.tick(20);
    b.tick(10); b.tick(10);
  }
  assert(state(a.player) == state(b.player));
  assert(state(a.opponent) == state(b.opponent));
  assert(a.opponent.generation > 5 && a.opponent.score > 0);
  a.input(5);
  const auto player = state(a.player), opponent = state(a.opponent);
  for (int i = 0; i < 50; ++i) { a.tick(100); finishDrop(a); }
  assert(a.status() == 1 && state(a.player) == player && state(a.opponent) == opponent);
  a.input(5);
  for (int i = 0; i < 1000 && a.status() == 0; ++i) finishDrop(a);
  assert(a.status() == 2);
  assert(!a.player.active && !a.opponent.active);
  assert(!a.recon().known() && !a.reconEnabled());
  const auto endedPlayer = state(a.player), endedOpponent = state(a.opponent);
  a.tick(100); a.input(0); a.input(5);
  assert(a.status() == 2 && state(a.player) == endedPlayer && state(a.opponent) == endedOpponent);
  a.reset(42);
  assert(a.status() == 0 && a.mode() == 1 && a.player.score == 0 && a.opponent.score == 0);
  // Verify both terminal signals and their simultaneous-tick tie policy.
  a.opponent.over = true;
  a.tick(10);
  assert(a.status() == 3);
  a.reset(42);
  a.player.over = a.opponent.over = true;
  a.tick(10);
  assert(a.status() == 4);
  a.start(42, 0, 0);
  const auto soloOpponent = state(a.opponent);
  a.tick(std::numeric_limits<double>::quiet_NaN());
  a.tick(-100);
  for (int i = 0; i < 20; ++i) a.tick(100);
  assert(a.status() == 0 && state(a.opponent) == soloOpponent);
}

// Canonical trace used by compare.cjs. Hash both complete board snapshots and
// counters after each action, so cross-build differences identify a trace step.
static void replay() {
  for (int mode : {1, 2}) for (int combat = 0; combat < 8; ++combat) {
  for (unsigned seed : {1u, 42u, 0xdeadbeefu}) {
    BrowserMatch match;
    match.start(seed, mode, 10);
    if (combat) {
      // Explicit combat fixture: seed a shopping boundary and earned funds to
      // exercise purchases and attacks without a long pre-bazaar warm-up.
      match.player.lines = 20;
      match.player.funds = match.opponent.funds = 10000;
      match.tick(10);
      const std::vector<int> attacks = combat == 1
        ? std::vector<int>{BT_FEARED_WEIRD, BT_SPEEDY, BT_MIRROR, BT_RISE_UP, BT_BROKEN}
        : combat == 2
          ? std::vector<int>{BT_MONDALE, BT_LAWYERS, BT_SWAP, BT_KEATING, BT_SUSAN}
          : combat == 3
            ? std::vector<int>{BT_BOTTLE, BT_FORCE, BT_FALL_OUT, BT_GIMP, BT_RISE_UP}
            : combat == 4
              ? std::vector<int>{BT_NO_SLIDE, BT_SPEEDY, BT_LAWYERS, BT_BOTTLE, BT_FALL_OUT}
              : combat == 5
                ? std::vector<int>{BT_HATTER, BT_SLICK, BT_MEADOW, BT_NO_SLIDE, BT_RISE_UP}
                : combat == 6
                  ? std::vector<int>{BT_UPBYSIDE, BT_BOTTLE, BT_FALL_OUT, BT_HATTER, BT_SLICK}
                  : std::vector<int>{BT_CONDOR, BT_ACE, BT_AMES, BT_RISE_UP, BT_NO_DICE};
      for (int token : attacks) {
        assert(match.buy(token));
        if (mode == 2) assert(match.sideBuy(1, token));
      }
      if (mode == 2) { assert(match.sideReady(0)); assert(match.sideReady(1)); }
      else assert(match.leaveBazaar());
      for (int slot = 0; slot < 5; ++slot) {
        assert(match.launch(slot));
        if (mode == 2) assert(match.sideLaunch(1, slot));
      }
      if (combat == 5) {
        match.player.queueWeapon(catalogWeapon(BT_HATTER));
        match.player.queueWeapon(catalogWeapon(BT_SLICK));
      }
      if (combat == 6) match.player.queueWeapon(catalogWeapon(BT_UPBYSIDE));
      if (combat == 7) match.player.queueWeapon(catalogWeapon(BT_ACE));
    }
    uint32_t digest = 2166136261u;
    for (int tick = 0; tick < 1200; ++tick) {
      if (combat == 2 && tick == 100) {
        // Exercise peer line callbacks and tax while their timed effects run.
        fillRow(match.player, BT_BOARD_HGT - 1, 3);
        match.player.board.checkLines();
        fillRow(match.opponent, BT_BOARD_HGT - 1, 3);
        match.opponent.board.checkLines();
      }
      if (tick % 17 == 0) match.input((tick / 17) % 3);
      if (tick % 80 == 0) match.input(4);
      if (mode == 2) {
        if (tick % 19 == 0) match.sideInput(1, (tick / 19) % 3);
        if (tick % 73 == 0) match.sideInput(1, 4);
      }
      if (tick == 350 || tick == 375) match.setPaused(tick == 350);
      match.tick(10);
      for (int value : state(match.player)) digest = (digest ^ uint32_t(value)) * 16777619u;
      for (int value : state(match.opponent)) digest = (digest ^ uint32_t(value)) * 16777619u;
      for (int side = 0; side < 2; ++side) {
        const BrowserRecon &report = match.recon(side);
        for (int value : {report.token(), report.remaining(), report.funds(), int(report.known())})
          digest = (digest ^ uint32_t(value)) * 16777619u;
        digest = (digest ^ report.randomState()) * 16777619u;
        for (int y = 0; y < BT_BOARD_HGT; ++y)
          for (int x = 0; x < BT_BOARD_WTH; ++x)
            digest = (digest ^ uint32_t(report.cell(x, y))) * 16777619u;
      }
      for (int side = 0; side < 2; ++side)
        for (int slot = 0; slot < BT_ARSENAL_SIZE; ++slot) {
          digest = (digest ^ uint32_t(match.arsenalToken(side, slot))) * 16777619u;
          digest = (digest ^ uint32_t(match.arsenalQuantity(side, slot))) * 16777619u;
        }
      digest = (digest ^ uint32_t(match.status())) * 16777619u;
      if (tick % 40 == 0) std::cout << mode << ':' << combat << ':' << seed << ':' << tick << ':' << digest << '\n';
      if (match.status() == 5) {
        if (mode == 2) { match.sideReady(0); match.sideReady(1); }
        else match.leaveBazaar();
      }
      else if (match.status() >= 2) match.reset(seed + tick);
    }
  }
  }
}

void humanRules();

int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--replay") { replay(); return 0; }
  lineRules();
  pieceRules();
  lifecycle();
  scoreRules();
  randomRules();
  plannerRules();
  matchRules();
  effectRules();
  combatRules();
  swapRules();
  peerRules();
  structureRules();
  landingRules();
  motionRules();
  upsideRules();
  upsidePlannerRules();
  reconRules();
  reconMatchRules();
  humanRules();
  std::cout << "Core tests passed: rules, scoring, RNG, planner, lifecycle, weapons, bazaar.\n";
}
