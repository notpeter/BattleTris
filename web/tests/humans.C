#include "Match.H"
#include "BTBox.H"
#include <cassert>

namespace {
void clearRow(BrowserGame &game) {
  const int y = BT_BOARD_HGT - 1;
  for (int x = 0; x < BT_BOARD_WTH; ++x)
    if (!game.board.occupied(x, y))
      game.board.fill(x, y, game.board.box_manager_->create(x, y, BT_RED));
  game.board.landed(0, y);
  game.board.checkLines();
}
void bazaar(BrowserMatch &match) {
  for (int i = 0; i < 10; ++i) { clearRow(match.player); clearRow(match.opponent); }
  match.tick(10);
  assert(match.status() == 5);
  match.player.funds = match.opponent.funds = 1000;
}
void gravityAndInputs() {
  BrowserMatch match;
  match.start(42, 2, 0);
  assert(match.mode() == 2 && !match.reconEnabled() && !match.toggleRecon());
  assert(!match.recon(0).known() && !match.recon(1).known());
  assert(!match.sideInput(-1, 4) && !match.sideInput(2, 4));
  assert(!match.sideInput(0, -1) && !match.sideInput(1, 5));
  int y[] = {match.player.active->y(), match.opponent.active->y()};
  for (int i = 0; i < 7; ++i) match.tick(100);
  assert(match.player.active->y() > y[0] && match.opponent.active->y() > y[1]);
  assert(match.player.score == 0 && match.opponent.score == 0);
  const int award = BT_BOARD_HGT - match.opponent.active->y();
  assert(match.sideInput(1, 4) && match.opponent.score == award);
  assert(match.player.score == 0 && !match.recon(0).known());
  match.tick(90);
  assert(match.setPaused(true));
  const double elapsed = match.opponent.elapsed;
  match.tick(100);
  assert(!match.sideInput(0, 4) && match.opponent.elapsed == elapsed);
  assert(match.setPaused(false) && match.opponent.elapsed == 0 && match.player.elapsed == 0);
  match.reset(42);
  assert(match.mode() == 2 && !match.reconEnabled());
  match.start(42, 1, 0);
  assert(!match.sideInput(1, 4) && !match.sideSurrender(0));
  assert(match.reconEnabled());
}
void shoppingAndRecon() {
  BrowserMatch match;
  match.start(20, 2, 0);
  assert(!match.sideReady(0) && !match.sideBuy(1, BT_CONDOR));
  bazaar(match);
  assert(!match.leaveBazaar() && !match.sideReady(-1));
  for (int side = 0; side < 2; ++side) {
    assert(match.arsenalQuantity(side, 0) == 0); // No automatic shopping.
    assert(match.sideBuy(side, BT_CONDOR));
    assert(match.refundable(0, side) == 1);
    assert(match.sideRefund(side, 0));
    assert((side ? match.opponent : match.player).funds == 1000);
    assert(match.sideBuy(side, BT_CONDOR));
    assert(!match.sideLaunch(side, 0) && !match.sideInput(side, 4));
  }
  assert(match.sideReady(0) && match.ready(0) && !match.ready(1));
  assert(!match.sideReady(0) && !match.sideBuy(0, BT_FLIP_OUT) && !match.sideRefund(0, 0));
  match.tick(100);
  assert(match.status() == 5 && match.player.paused && match.opponent.paused);
  assert(match.sideReady(1) && match.status() == 0);
  assert(!match.ready(0) && !match.ready(1));
  assert(match.sideLaunch(0, 0) && match.opponent.pendingWeapons() == 1);
  assert(!match.recon(0).known() && !match.recon(1).known());
  assert(match.sideInput(1, 4)); // Flush spy, no report until next placement.
  assert(!match.recon(0).known());
  assert(match.sideInput(1, 4) && match.recon(0).known());
  assert(match.recon(0).funds() == match.opponent.funds);
  assert(!match.recon(1).known());
  assert(match.sideLaunch(1, 0) && match.player.pendingWeapons() == 1);
  assert(match.sideInput(0, 4) && !match.recon(1).known());
  assert(match.sideInput(0, 4) && match.recon(1).known());
  assert(match.sideSurrender(1) && match.status() == 3);
  assert(!match.recon(0).known() && !match.recon(1).known());
  assert(!match.player.active && !match.opponent.active);
  assert(!match.sideInput(0, 4) && !match.sideLaunch(0, 0) && !match.sideReady(1));
  assert(!match.sideSurrender(0) && !match.setPaused(false));
}
void attacksAndPause() {
  BrowserMatch match;
  match.start(77, 2, 0);
  bazaar(match);
  assert(match.setPaused(true));
  assert(!match.sideBuy(1, BT_FLIP_OUT) && !match.sideReady(0));
  assert(match.setPaused(false));
  for (int side = 0; side < 2; ++side) assert(match.sideBuy(side, BT_CARTER));
  assert(match.sideReady(1) && match.sideReady(0));
  for (int side = 0; side < 2; ++side) assert(match.sideLaunch(side, 0));
  assert(match.player.pendingWeapons() == 1 && match.opponent.pendingWeapons() == 1);
  assert(match.sideInput(0, 4) && match.sideInput(1, 4));
  assert(match.player.weapons.BTActive[BT_CARTER] && match.opponent.weapons.BTActive[BT_CARTER]);
  for (int side = 0; side < 2; ++side)
    assert(match.price(BT_FLIP_OUT, side) == 2 * catalogWeapon(BT_FLIP_OUT)->price());
  // A fresh shopping visit after Carter expires exercises mirrored delivery.
  bazaar(match);
  assert(match.sideBuy(0, BT_MIRROR) && match.sideBuy(1, BT_CARTER));
  assert(match.sideReady(0) && match.sideReady(1));
  assert(match.sideLaunch(0, 0));
  assert(match.sideInput(1, 4));
  assert(match.opponent.weapons.BTActive[BT_MIRROR]);
  assert(match.sideLaunch(1, 0));
  assert(match.player.pendingWeapons() == 0 && match.opponent.pendingWeapons() == 1);
  assert(match.sideInput(1, 4));
  assert(match.opponent.weapons.BTActive[BT_CARTER]);
}
void endings() {
  BrowserMatch match;
  match.start(42, 2, 0);
  assert(match.sideSurrender(0) && match.status() == 2);
  match.reset(42);
  bazaar(match);
  assert(match.sideSurrender(1) && match.status() == 3);
  match.reset(42);
  // Both boards top out on the same gravity quantum. Block the first descent
  // and each next spawn using existing active cells, without filling a row.
  for (BrowserGame *game : {&match.player, &match.opponent}) {
    for (int y = 0; y < 6; ++y)
      for (int x = 3; x < 7; ++x)
        if (!game->cell(x, y))
          game->board.fill(x, y, game->board.box_manager_->create(x, y, BT_RED));
    BTWeapon noSlide(BT_NO_SLIDE);
    noSlide.duration_ = 10;
    game->sendPlusMe(BT_WPN_ON, &noSlide);
    game->elapsed = game->gravityInterval() - 10;
  }
  match.tick(10);
  assert(match.status() == 4 && !match.player.active && !match.opponent.active);
}
}
void humanRules() { gravityAndInputs(); shoppingAndRecon(); attacksAndPause(); endings(); }
