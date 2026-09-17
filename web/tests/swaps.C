#include "Drop.H"
#include "Game.H"
#include "BTBox.H"
#include <cassert>
#include <vector>

namespace {
void place(BrowserGame &game, int x, int y, BTBox *box) {
  game.board.fill(x, y, box);
  game.board.landed(x, y);
}
void red(BrowserGame &game, int x, int y) {
  place(game, x, y, game.board.box_manager_->create(x, y, BT_RED));
}
void metadataTransfer() {
  BrowserGame a, b;
  a.board.random.seed(123);
  b.board.random.seed(456);
  a.funds = 25; b.funds = 70;
  a.lines = 4; b.lines = 9;
  a.score = 30; b.score = 60;
  place(a, 0, 27, a.board.box_manager_->dieCreate(0, 27, 3));
  BTBox *happy = a.board.box_manager_->happyCreate(1, 27);
  happy->hide();
  place(a, 1, 27, happy);
  place(a, 2, 27, a.board.box_manager_->createGimp(2, 27, 7));
  BTBox *structure = a.board.box_manager_->structureCreate(3, 27);
  place(a, 3, 27, structure);
  place(a, 4, 27, a.board.box_manager_->create(4, 27, BT_INVISIBLE));
  red(b, 8, 20);
  a.swapSettledBoard(b);
  assert(a.board.random.state() == 123 && b.board.random.state() == 456);
  assert(a.funds == 25 && b.funds == 70 && a.lines == 4 && b.lines == 9);
  assert(a.score == 30 && b.score == 60);
  assert(a.board.cell(8, 20) == BT_RED && !a.board.occupied(0, 27));
  assert(b.board.cell(0, 27) == BT_DIE_3 && b.board.cell(1, 27) == -1);
  assert(b.board.cell(2, 27) == BT_GIMP_ID && b.board.cell(3, 27) == BT_STRUCT);
  assert(b.board.occupied(4, 27) && b.board.cell(4, 27) == 0);
  assert(!structure->isRemoveable() && happy->value() == BT_HAPPY_VAL);
  for (int x = 5; x < BT_BOARD_WTH; ++x) red(b, x, 27);
  // Serialization by ID would lose hidden happy value and gimp's carried value.
  assert(b.board.checkLines() == 160);
  assert(b.funds == 230 && b.lines == 10);
  for (int x = 0; x < BT_BOARD_WTH; ++x) assert(!b.board.occupied(x, 27));
}

struct SwapObserver : BrowserGame::Events {
  BrowserGame *peer;
  std::vector<int> seen;
  explicit SwapObserver(BrowserGame &other) : peer(&other) {}
  bool applyWeapon(BrowserGame &game, BTWeapon &weapon) override {
    assert(game.active == nullptr); // Includes effects after Swap in the batch.
    seen.push_back(weapon.token());
    if (weapon.token() != BT_SWAP) return false;
    game.swapSettledBoard(*peer);
    assert(game.active == nullptr);
    return true;
  }
};

void queuedSwapLifecycle() {
  BrowserGame target, peer(true);
  target.reset(11); peer.reset(22);
  SwapObserver observer(peer);
  target.events = &observer;
  red(peer, 0, 24);
  const auto targetGeneration = target.generation, peerGeneration = peer.generation;
  const int targetScore = target.score, peerScore = peer.score;
  BTWeapon swap(BT_SWAP), rise(BT_RISE_UP), noDice(BT_NO_DICE);
  noDice.duration_ = 5;
  peer.queueWeapon(noDice);
  target.queueWeapon(swap);
  target.queueWeapon(rise);
  finishDrop(target);
  assert(observer.seen.size() == 2 && observer.seen[0] == BT_SWAP && observer.seen[1] == BT_RISE_UP);
  assert(target.generation == targetGeneration + 1 && peer.generation == peerGeneration + 1);
  assert(target.active && peer.active && !target.over && !peer.over);
  assert(target.active->canMoveTo(target.active->x(), target.active->y()));
  assert(peer.active->canMoveTo(peer.active->x(), peer.active->y()));
  assert(target.board.cell(0, 23) == BT_RED); // RiseUp runs on the swapped board.
  assert(target.pendingWeapons() == 0 && peer.pendingWeapons() == 1);
  assert(target.score >= targetScore && peer.score == peerScore);
  target.reset(33);
  assert(target.events == &observer);
  const auto unchanged = peer.generation;
  peer.swapSettledBoard(peer);
  assert(peer.generation == unchanged);
}

struct AccountingObserver : BrowserGame::Events {
  int grossSeen = 0, clears = 0, handled = 0;
  int taxFunds(BrowserGame &, int gross) override { grossSeen += gross; return gross - 1; }
  void clearedLines(BrowserGame &game, int count) override {
    clears += count;
    assert(game.lines == clears); // Local counter updates before the observer.
    assert(game.weapons.remaining(BT_FORCE) == 1); // Expiration occurs afterward.
    assert(game.funds == 2); // BT_FUNDS already credited net funds.
  }
  bool applyWeapon(BrowserGame &game, BTWeapon &weapon) override {
    if (weapon.token() != BT_KEATING) return false;
    assert(game.active == nullptr);
    ++handled;
    game.funds = 0;
    return true;
  }
};

void observerOrdering() {
  BrowserGame game;
  AccountingObserver observer;
  game.events = &observer;
  BTWeapon force(BT_FORCE);
  force.duration_ = 1;
  game.sendPlusMe(BT_WPN_ON, &force);
  place(game, 0, 27, game.board.box_manager_->dieCreate(0, 27, 3));
  for (int x = 1; x < BT_BOARD_WTH; ++x) red(game, x, 27);
  game.board.checkLines();
  assert(observer.grossSeen == 3 && observer.clears == 1);
  assert(game.funds == 2 && !game.weapons.BTActive[BT_FORCE]);
  game.reset(44);
  assert(game.events == &observer);
  game.funds = 100;
  BTWeapon keating(BT_KEATING);
  game.queueWeapon(keating);
  finishDrop(game);
  assert(observer.handled == 1 && game.funds == 0);
  assert(!game.weapons.BTActive[BT_KEATING]);
}

void swapTopOut() {
  BrowserGame a, b;
  a.reset(55); b.reset(66);
  for (int y = 0; y < BT_PIECE_HEIGHT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) red(b, x, y);
  const auto ag = a.generation, bg = b.generation;
  a.swapSettledBoard(b);
  assert(a.over && !a.active && !b.over && b.active);
  assert(a.generation == ag + 1 && b.generation == bg + 1);
  const int before = a.board.cell(0, 0);
  a.swapSettledBoard(b);
  assert(a.over && !a.active && a.board.cell(0, 0) == before);
  assert(a.generation == ag + 1 && b.generation == bg + 1);
}
} // namespace

void swapRules() {
  metadataTransfer();
  queuedSwapLifecycle();
  observerOrdering();
  swapTopOut();
}
