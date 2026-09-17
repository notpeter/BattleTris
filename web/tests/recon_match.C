#include "Drop.H"
#include "Match.H"
#include "Recon.H"
#include "BTBox.H"
#include <array>
#include <cassert>

namespace {
using Report = std::array<int, BT_BOARD_WTH * BT_BOARD_HGT>;
Report report(const BrowserRecon &recon) {
  Report cells{};
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      cells[y * BT_BOARD_WTH + x] = recon.cell(x, y);
  return cells;
}
void spy(BrowserGame &target, BTWeaponToken token, unsigned short duration) {
  BTWeapon weapon(token);
  weapon.duration_ = duration;
  target.queueWeapon(weapon);
}
void row(BrowserGame &game) {
  for (int x = 0; x < BT_BOARD_WTH; ++x)
    game.board.fill(x, 27, game.board.box_manager_->create(x, 27, BT_RED));
  game.board.landed(0, 27);
}
int slotFor(BrowserMatch &match, int token) {
  for (int slot = 0; slot < BT_ARSENAL_SIZE; ++slot)
    if (match.arsenalToken(0, slot) == token && match.arsenalQuantity(0, slot)) return slot;
  return -1;
}
void shop(BrowserMatch &match) {
  match.start(404, 1, 2);
  assert(match.reconEnabled() && match.toggleRecon());
  match.player.lines = 20;
  match.player.funds = 10000;
  match.tick(10);
  assert(match.status() == 5);
}
void queuedReportAndCache() {
  BrowserMatch match;
  shop(match);
  assert(match.buy(BT_CONDOR));
  assert(match.leaveBazaar());
  const int slot = slotFor(match, BT_CONDOR);
  assert(slot >= 0 && match.launch(slot));
  assert(match.arsenalQuantity(0, slot) == 0);
  assert(!match.recon().known() && match.recon().remaining() == 40);
  assert(match.opponent.pendingWeapons() == 1);
  match.opponent.funds = 123;
  finishDrop(match.opponent);
  assert(!match.recon().known() && match.recon().remaining() == 40);
  assert(match.opponent.pendingWeapons() == 0);
  finishDrop(match.opponent);
  assert(match.recon().known() && match.recon().token() == BT_CONDOR);
  assert(match.recon().remaining() == 40 && match.recon().funds() == 123);
  assert(!match.opponent.weapons.BTActive[BT_CONDOR]);
  bool excludesFallingCell = false;
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) {
      assert(match.recon().cell(x, y) == match.opponent.board.cell(x, y));
      if (match.opponent.cell(x, y) != match.opponent.board.cell(x, y)) {
        excludesFallingCell = true;
        assert(match.recon().cell(x, y) == 0);
      }
    }
  assert(excludesFallingCell);
  const auto before = report(match.recon());
  const auto random = match.recon().randomState();
  match.opponent.input(0); match.opponent.input(2); match.opponent.input(3);
  match.opponent.funds = 456;
  for (int i = 0; i < 10; ++i) {
    assert(report(match.recon()) == before && match.recon().funds() == 123);
    assert(match.recon().randomState() == random);
  }
  finishDrop(match.opponent);
  assert(match.recon().funds() == 456);
  assert(!match.recon(1).known());
}
void expiryAndQueuedOverlap() {
  BrowserMatch match;
  match.start(501, 1, 2);
  assert(match.toggleRecon());
  spy(match.opponent, BT_CONDOR, 1);
  finishDrop(match.opponent);
  assert(!match.recon().known() && match.recon().remaining() == 1);
  finishDrop(match.opponent);
  assert(match.recon().known() && match.recon().remaining() == 1);
  // Clearing the viewer's lines must not consume the target's spying duration.
  row(match.player);
  match.player.board.checkLines();
  assert(match.recon().remaining() == 1);
  match.opponent.board.clear();
  row(match.opponent);
  finishDrop(match.opponent);
  assert(match.recon().remaining() == 0 && !match.recon().known());
  for (int cell : report(match.recon())) assert(cell == 0);

  spy(match.opponent, BT_AMES, 1);
  finishDrop(match.opponent);
  assert(!match.recon().known() && match.recon().token() == BT_AMES);
  finishDrop(match.opponent);
  assert(match.recon().known() && match.recon().token() == BT_AMES);
  match.opponent.board.clear();
  row(match.opponent);
  spy(match.opponent, BT_ACE, 3);
  spy(match.opponent, BT_CONDOR, 4);
  // Launched allowances extend the existing viewer before the next clear.
  // The active recipient can report immediately using the new viewer quality.
  finishDrop(match.opponent);
  assert(match.recon().known() && match.recon().token() == BT_CONDOR);
  assert(match.recon().remaining() == 7);
  finishDrop(match.opponent);
  assert(match.recon().known() && match.recon().remaining() == 7);
  spy(match.opponent, BT_AMES, 2);
  finishDrop(match.opponent);
  assert(match.recon().token() == BT_AMES && match.recon().remaining() == 9);
  assert(match.recon().known());
  match.opponent.board.clear();
  finishDrop(match.opponent);
  assert(match.recon().known());
}
void mirrorNullifiesAndBothViewers() {
  BrowserMatch match;
  shop(match);
  for (auto token : {BT_AMES, BT_ACE, BT_CONDOR}) assert(match.buy(token));
  assert(match.leaveBazaar());
  BTWeapon mirror(BT_MIRROR);
  mirror.duration_ = 10;
  match.player.sendPlusMe(BT_WPN_ON, &mirror);
  for (auto token : {BT_AMES, BT_ACE, BT_CONDOR}) {
    const int slot = slotFor(match, token);
    assert(slot >= 0 && match.launch(slot));
    assert(match.arsenalQuantity(0, slot) == 0);
    assert(match.opponent.pendingWeapons() == 0 && match.player.pendingWeapons() == 0);
    assert(!match.recon().known() && !match.recon(1).known());
  }
  match.reset(909);
  assert(match.toggleRecon());
  spy(match.player, BT_CONDOR, 3);
  match.player.funds = -50;
  finishDrop(match.player);
  assert(!match.recon(1).known() && match.recon(1).remaining() == 3);
  finishDrop(match.player);
  assert(match.recon(1).known() && match.recon(1).funds() == -50);
  assert(!match.recon(0).known());
  spy(match.opponent, BT_CONDOR, 4);
  match.opponent.funds = 60;
  finishDrop(match.opponent);
  assert(!match.recon().known() && match.recon().remaining() == 4);
  finishDrop(match.opponent);
  assert(match.recon().known() && match.recon().funds() == 60);
  assert(match.recon(1).remaining() == 3 && match.recon().remaining() == 4);
  const auto firstSeed = match.recon().randomState();
  const auto secondSeed = match.recon(1).randomState();
  assert(firstSeed != secondSeed);
  match.reset(909);
  assert(!match.recon().known() && !match.recon(1).known());
  assert(match.reconEnabled());
  assert(match.recon().token() == BT_CONDOR && match.recon().remaining() == 65535);
  assert(match.recon(1).remaining() == 0);
  for (int side = 0; side < 2; ++side)
    for (int cell : report(match.recon(side))) assert(cell == 0);
  const auto seed = match.recon().randomState();
  match.reset(909);
  assert(match.recon().randomState() == seed);
}
void freeCondorAndToggle() {
  BrowserMatch match;
  match.start(707, 1, 2);
  assert(match.reconEnabled());
  assert(match.recon().token() == BT_CONDOR && match.recon().remaining() == 65535);
  assert(!match.recon().known());
  match.opponent.funds = 72;
  finishDrop(match.opponent);
  assert(!match.recon().known());
  finishDrop(match.opponent);
  assert(match.recon().known() && match.recon().funds() == 72);
  spy(match.opponent, BT_AMES, 20);
  finishDrop(match.opponent);
  assert(match.reconEnabled() && match.recon().token() == BT_AMES);
  assert(match.recon().remaining() == 65555 && match.recon().known());
  match.opponent.board.clear();
  finishDrop(match.opponent);
  assert(match.recon().known());
  assert(match.toggleRecon() && !match.reconEnabled());
  assert(!match.recon().known() && match.recon().remaining() == 0);
  assert(match.toggleRecon() && match.reconEnabled());
  assert(match.recon().token() == BT_CONDOR && match.recon().remaining() == 65535);
  assert(!match.recon().known());
  finishDrop(match.opponent);
  assert(!match.recon().known());
  match.opponent.board.clear();
  finishDrop(match.opponent);
  assert(match.recon().known());
  match.start(708, 0, 0);
  assert(!match.reconEnabled() && !match.toggleRecon());
  assert(match.recon().remaining() == 0 && !match.recon().known());
}
void reportsPrecedeQueuedMutations() {
  BrowserMatch match;
  match.start(811, 1, 2);
  assert(match.toggleRecon());
  spy(match.opponent, BT_CONDOR, 10);
  finishDrop(match.opponent);
  assert(!match.recon().known());
  match.opponent.board.clear();
  match.opponent.board.fill(0, 20,
    match.opponent.board.box_manager_->create(0, 20, BT_RED));
  match.opponent.funds = 123;
  const int viewerFunds = match.player.funds;
  match.opponent.queueWeapon(BTWeapon(BT_FLIP_OUT));
  match.opponent.queueWeapon(BTWeapon(BT_KEATING));
  finishDrop(match.opponent);
  // Ernie reports the placement before flushing received weapons. The cached
  // board and balance must therefore describe the state before both attacks.
  assert(match.recon().known() && match.recon().funds() == 123);
  assert(match.recon().cell(0, 20) == BT_RED);
  assert(match.recon().cell(9, 20) == 0);
  assert(match.opponent.board.cell(0, 20) == 0);
  assert(match.opponent.board.cell(9, 20) == BT_RED);
  assert(match.opponent.funds == 0 && match.player.funds == viewerFunds + 123);
  assert(match.opponent.pendingWeapons() == 0);
  finishDrop(match.opponent);
  assert(match.recon().funds() == 0);
  assert(match.recon().cell(0, 20) == 0);
  assert(match.recon().cell(9, 20) == BT_RED);
}
void cancelPendingFreeCondor() {
  BrowserMatch match;
  match.start(812, 1, 2);
  assert(match.toggleRecon() && !match.reconEnabled());
  finishDrop(match.opponent);
  finishDrop(match.opponent);
  assert(!match.recon().known() && match.recon().remaining() == 0);
  assert(match.toggleRecon() && match.reconEnabled());
  // Cancel and replace an unacknowledged request. Only the new request may
  // activate, and it still requires a placement before reports can arrive.
  assert(match.toggleRecon() && !match.reconEnabled());
  assert(match.toggleRecon() && match.reconEnabled());
  match.opponent.board.clear();
  finishDrop(match.opponent);
  assert(!match.recon().known() && match.recon().remaining() == 65535);
  finishDrop(match.opponent);
  assert(match.recon().known() && match.recon().remaining() == 65535);
}
void gameplayRandomIsolation() {
  BrowserMatch observed, control;
  observed.start(1001, 1, 2); control.start(1001, 1, 2);
  assert(observed.toggleRecon() && control.toggleRecon());
  spy(observed.opponent, BT_AMES, 100);
  spy(observed.player, BT_ACE, 100);
  for (int turn = 0; turn < 12; ++turn) {
    // Keep empty settled grids so both streams progress without premature loss.
    observed.player.board.clear(); control.player.board.clear();
    observed.opponent.board.clear(); control.opponent.board.clear();
    for (int command : {0, 1, 2, 4}) {
      observed.player.input(command); control.player.input(command);
      observed.opponent.input(command); control.opponent.input(command);
    }
    finishDrop(observed.player); finishDrop(control.player);
    finishDrop(observed.opponent); finishDrop(control.opponent);
    assert(observed.player.board.random.state() == control.player.board.random.state());
    assert(observed.opponent.board.random.state() == control.opponent.board.random.state());
    for (int y = 0; y < BT_BOARD_HGT; ++y)
      for (int x = 0; x < BT_BOARD_WTH; ++x) {
        assert(observed.player.cell(x, y) == control.player.cell(x, y));
        assert(observed.opponent.cell(x, y) == control.opponent.cell(x, y));
      }
  }
  assert(observed.recon().known() && observed.recon(1).known());
}

void launchAllowanceAndTerminalCleanup() {
  BrowserMatch match;
  shop(match);
  assert(match.buy(BT_CONDOR));
  assert(match.leaveBazaar());
  const int slot = slotFor(match, BT_CONDOR);
  assert(match.launch(slot));
  // Lines cleared before activation already consume the viewer's allowance.
  row(match.opponent);
  finishDrop(match.opponent);
  assert(match.recon().remaining() == 39 && !match.recon().known());
  finishDrop(match.opponent);
  assert(match.recon().known());
  const auto cached = report(match.recon());
  const int cachedFunds = match.recon().funds();
  spy(match.opponent, BT_AMES, 3);
  assert(match.recon().remaining() == 42 && match.recon().token() == BT_AMES);
  assert(report(match.recon()) == cached && match.recon().funds() == cachedFunds);

  // End a match while both a falling piece and an incoming effect exist.
  match.player.queueWeapon(BTWeapon(BT_REAGAN));
  const int funds = match.player.funds;
  match.opponent.over = true;
  match.tick(10);
  assert(match.status() == 3 && !match.player.over);
  assert(!match.player.active && !match.opponent.active);
  assert(!match.player.pendingWeapons() && !match.opponent.pendingWeapons());
  assert(!match.recon().known() && !match.recon(1).known());
  assert(!match.reconEnabled() && !match.toggleRecon());
  assert(match.player.funds == funds);
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) {
      assert(match.player.cell(x, y) == match.player.board.cell(x, y));
      assert(match.opponent.cell(x, y) == match.opponent.board.cell(x, y));
    }
  match.reset(404);
  assert(match.player.active && match.opponent.active && match.reconEnabled());
  assert(!match.player.paused && !match.opponent.paused && match.status() == 0);

  // A one-line allowance can expire at the activating placement. A later
  // request must activate afresh, without reviving the expired reporter.
  assert(match.toggleRecon());
  spy(match.opponent, BT_CONDOR, 1);
  row(match.opponent);
  finishDrop(match.opponent);
  assert(match.recon().remaining() == 0 && !match.recon().known());
  spy(match.opponent, BT_CONDOR, 2);
  finishDrop(match.opponent);
  assert(match.recon().remaining() == 2 && !match.recon().known());
  finishDrop(match.opponent);
  assert(match.recon().known());
}
} // namespace

void reconMatchRules() {
  queuedReportAndCache();
  expiryAndQueuedOverlap();
  mirrorNullifiesAndBothViewers();
  gameplayRandomIsolation();
  freeCondorAndToggle();
  reportsPrecedeQueuedMutations();
  cancelPendingFreeCondor();
  launchAllowanceAndTerminalCleanup();
}
