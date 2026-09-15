#include "Recon.H"
#include "Game.H"
#include "BTBox.H"
#include <cassert>
#include <cstdint>
#include <limits>

namespace {
void filledBoard(BrowserGame &game) {
  for (int y = 0; y < BT_BOARD_HGT; ++y) {
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      game.board.fill(x, y, game.board.box_manager_->create(x, y, BT_RED));
    game.board.landed(0, y);
  }
}
int visible(BrowserRecon &recon) {
  int count = 0;
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) count += recon.cell(x, y) > 0;
  return count;
}
void cachedProbabilities() {
  BrowserGame game;
  filledBoard(game);
  const auto boardRandom = game.board.random.state();
  BrowserRecon ames, twin, ace;
  ames.reset(1234); twin.reset(1234); ace.reset(1234);
  ames.activate(BT_AMES, 20); twin.activate(BT_AMES, 20); ace.activate(BT_ACE, 20);
  ames.report(game.board, 200, 1);
  twin.report(game.board, 200, 1);
  ace.report(game.board, 200, 1);
  assert(ames.known() && twin.known() && ace.known());
  assert(visible(ames) > 0 && visible(ames) < visible(ace));
  assert(visible(ace) < BT_BOARD_WTH * BT_BOARD_HGT);
  assert(ames.funds() >= 0 && ames.funds() <= 400 && ace.funds() == 200);
  assert(ames.randomState() == twin.randomState() && ames.funds() == twin.funds());
  const auto cachedRandom = ames.randomState();
  for (int repeat = 0; repeat < 20; ++repeat)
    for (int y = 0; y < BT_BOARD_HGT; ++y)
      for (int x = 0; x < BT_BOARD_WTH; ++x) {
        assert(ames.cell(x, y) == twin.cell(x, y));
        // With the same seed, the 50% sample is a subset of the 85% sample.
        if (ames.cell(x, y)) assert(ace.cell(x, y) == ames.cell(x, y));
      }
  assert(ames.randomState() == cachedRandom && game.board.random.state() == boardRandom);
  game.board.clear();
  assert(visible(ames) > 0); // Reports stay cached until explicitly refreshed.
  ames.report(game.board, 0, 0);
  assert(ames.known() && visible(ames) == 0 && ames.funds() == 0);
}

void precisionAndVisibility() {
  BrowserGame game;
  game.reset(45);
  game.board.fill(0, 20, game.board.box_manager_->dieCreate(0, 20, 6));
  BTBox *hidden = game.board.box_manager_->happyCreate(1, 20);
  hidden->hide();
  game.board.fill(1, 20, hidden);
  game.board.fill(2, 20, game.board.box_manager_->create(2, 20, BT_INVISIBLE));
  game.board.landed(0, 20);
  BrowserRecon recon;
  recon.reset(46);
  recon.activate(BT_CONDOR, 40);
  const auto random = recon.randomState(), boardRandom = game.board.random.state();
  recon.report(game.board, -123, 4);
  assert(recon.known() && recon.funds() == -123 && visible(recon) == 1);
  assert(recon.cell(0, 20) == BT_DIE_6 && recon.cell(1, 20) == 0 && recon.cell(2, 20) == 0);
  assert(recon.cell(-1, 0) == 0 && recon.cell(10, 0) == 0 && recon.cell(0, 28) == 0);
  // The game's falling piece is deliberately absent from a settled-only report.
  for (int y = 0; y < BT_PIECE_HEIGHT; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH; ++x)
      if (game.active->isMapped(x, y)) assert(recon.cell(game.active->x() + x, game.active->y() + y) == 0);
  assert(recon.randomState() == random && game.board.random.state() == boardRandom);
}

void durationsAndBounds() {
  BrowserGame game;
  BrowserRecon recon;
  recon.reset(99);
  recon.report(game.board, 12, 1);
  assert(!recon.known() && recon.token() == -1 && recon.remaining() == 0);
  recon.activate(BT_AMES, 3);
  recon.report(game.board, -1, 0); // Historical adjustFunds had a -1 FPE workaround.
  assert(recon.funds() >= -2 && recon.funds() <= 0);
  recon.activate(BT_ACE, 5);
  assert(recon.token() == BT_ACE && recon.remaining() == 8 && !recon.known());
  recon.report(game.board, -200, 3);
  assert(recon.funds() == -200);
  recon.report(game.board, -200, 4);
  assert(recon.funds() >= -299 && recon.funds() <= -101);
  recon.report(game.board, -200, 1);
  assert(recon.funds() == -200); // Four-line noise belongs only to that report.
  recon.activate(BT_CONDOR, 2);
  assert(recon.token() == BT_CONDOR && recon.remaining() == 10 && !recon.known());
  recon.clearedLines(-1);
  recon.clearedLines(9);
  assert(recon.remaining() == 1);
  recon.report(game.board, 42, 1);
  recon.clearedLines(1);
  assert(recon.token() == -1 && recon.remaining() == 0 && !recon.known() && visible(recon) == 0);
  assert(recon.funds() == 0);
  recon.activate(BT_AMES, std::numeric_limits<int>::max());
  recon.activate(BT_ACE, std::numeric_limits<int>::max());
  assert(recon.remaining() == 1000000000 && recon.token() == BT_ACE);
  recon.activate(BT_REAGAN, 20);
  recon.activate(BT_CONDOR, 0);
  assert(recon.remaining() == 1000000000 && recon.token() == BT_ACE);
  recon.activate(BT_AMES, 1);
  for (int iteration = 0; iteration < 100; ++iteration) {
    recon.report(game.board, std::numeric_limits<int>::min(), 0);
    assert(recon.funds() <= 0);
    recon.report(game.board, std::numeric_limits<int>::max(), 0);
    assert(recon.funds() >= 0);
  }
  const auto beforeDeactivation = recon.randomState();
  recon.deactivate();
  assert(!recon.known() && recon.token() == -1 && recon.remaining() == 0);
  assert(recon.randomState() == beforeDeactivation);
  recon.reset(99);
  assert(!recon.known() && recon.token() == -1 && recon.remaining() == 0 && recon.randomState() == 99);
}
} // namespace

void reconRules() {
  cachedProbabilities();
  precisionAndVisibility();
  durationsAndBounds();
}
