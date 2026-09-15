#include "Match.H"
#include "BTBox.H"
#include <array>
#include <cassert>

namespace {
void row(BrowserGame &game, int y, int die) {
  for (int x = 0; x < BT_BOARD_WTH; ++x)
    game.board.fill(x, y, x == 0 ? game.board.box_manager_->dieCreate(x, y, die)
                                : game.board.box_manager_->create(x, y, BT_RED));
  game.board.landed(0, y);
}
void activate(BrowserGame &game, BTWeaponToken token, unsigned short duration) {
  BTWeapon weapon(token);
  weapon.duration_ = duration;
  game.sendPlusMe(BT_WPN_ON, &weapon);
}
void shop(BrowserMatch &match, unsigned seed) {
  match.start(seed, 1, 0);
  match.player.lines = 20; // Physical bazaar threshold coverage is in combat.C.
  match.player.funds = 10000;
  match.opponent.funds = 1000;
  match.tick(10);
  assert(match.status() == 5);
}
int find(BrowserMatch &match, int token, int side = 0) {
  for (int slot = 0; slot < BT_ARSENAL_SIZE; ++slot)
    if (match.arsenalToken(side, slot) == token && match.arsenalQuantity(side, slot)) return slot;
  return -1;
}
using Inventory = std::array<int, BT_MAX_WEAPONS>;
Inventory inventory(BrowserMatch &match, int side) {
  Inventory counts{};
  for (int slot = 0; slot < BT_ARSENAL_SIZE; ++slot) {
    int token = match.arsenalToken(side, slot);
    if (token >= 0) counts[token] += match.arsenalQuantity(side, slot);
  }
  return counts;
}

void signedKeating() {
  BrowserMatch match;
  shop(match, 101);
  assert(match.buy(BT_KEATING));
  assert(match.leaveBazaar());
  match.player.funds = 100;
  match.opponent.funds = -80;
  const auto generation = match.opponent.generation;
  assert(match.launch(find(match, BT_KEATING)));
  assert(match.player.funds == 100 && match.opponent.funds == -80);
  assert(match.opponent.pendingWeapons() == 1 && match.opponent.generation == generation);
  // The transfer uses the signed balance at delivery, not a stale launch value.
  match.opponent.funds = -120;
  match.opponent.input(4);
  assert(match.opponent.funds == 0 && match.player.funds == -20);
  assert(match.opponent.pendingWeapons() == 0 && !match.opponent.weapons.BTActive[BT_KEATING]);
}

void mondaleFunds() {
  BrowserMatch match;
  match.start(202, 1, 0);
  BTWeapon tax(BT_MONDALE);
  tax.duration_ = 2;
  match.opponent.queueWeapon(tax);
  match.opponent.input(4);
  assert(match.opponent.weapons.remaining(BT_MONDALE) == 2);
  match.opponent.board.clear();
  activate(match.player, BT_MONDALE, 5);
  match.player.funds = match.opponent.funds = 0;
  row(match.opponent, 27, 3);
  row(match.opponent, 26, 4);
  assert(match.opponent.board.checkLines() == 14);
  assert(match.opponent.funds == 9 && match.player.funds == 5);
  assert(match.player.weapons.remaining(BT_MONDALE) == 5); // No recursive tax/line event.
  assert(!match.opponent.weapons.BTActive[BT_MONDALE]);
  row(match.opponent, 27, 3);
  match.opponent.board.checkLines();
  assert(match.opponent.funds == 12 && match.player.funds == 5);
  short loss = -7;
  match.opponent.sendPlusMe(BT_FUNDS, &loss);
  assert(match.opponent.funds == 5 && match.player.funds == 5);
  activate(match.opponent, BT_MONDALE, 2);
  match.opponent.sendPlusMe(BT_FUNDS, &loss);
  assert(match.opponent.funds == -2 && match.player.funds == 5);
}

void lawyersDeferred() {
  BrowserMatch match;
  match.start(303, 1, 0);
  BTWeapon lawyers(BT_LAWYERS);
  lawyers.duration_ = 2;
  match.opponent.queueWeapon(lawyers);
  match.opponent.input(4);
  match.opponent.board.clear();
  const auto victimGeneration = match.opponent.generation;
  row(match.player, 27, 3);
  row(match.player, 26, 4);
  match.player.board.checkLines();
  assert(match.opponent.pendingWeapons() == 2);
  assert(match.opponent.generation == victimGeneration);
  assert(match.opponent.weapons.remaining(BT_LAWYERS) == 2);
  row(match.opponent, 27, 3);
  match.opponent.board.checkLines();
  assert(match.opponent.weapons.remaining(BT_LAWYERS) == 1);
  row(match.opponent, 27, 3);
  match.opponent.board.checkLines();
  assert(!match.opponent.weapons.BTActive[BT_LAWYERS]);
  assert(match.opponent.pendingWeapons() == 2); // Already-earned rises remain queued.
  row(match.player, 27, 3);
  match.player.board.checkLines();
  assert(match.opponent.pendingWeapons() == 2); // No new rise after expiry.
  match.opponent.input(4);
  assert(match.opponent.pendingWeapons() == 0);
  for (int y = 26; y <= 27; ++y) {
    int count = 0;
    for (int x = 0; x < BT_BOARD_WTH; ++x) count += match.opponent.board.occupied(x, y);
    assert(count == 9);
  }
}

void susanInventory() {
  BrowserMatch match;
  shop(match, 404);
  assert(match.buy(BT_SUSAN));
  assert(match.buy(BT_SUSAN));
  assert(match.buy(BT_GIMP));
  assert(match.buy(BT_RISE_UP));
  Inventory sender = inventory(match, 0), target = inventory(match, 1);
  assert(match.leaveBazaar());
  assert(match.launch(find(match, BT_SUSAN)));
  --sender[BT_SUSAN];
  assert(inventory(match, 0) == sender && inventory(match, 1) == target);
  match.opponent.input(4);
  assert(inventory(match, 0) == target && inventory(match, 1) == sender);
  for (int slot = 0; slot < BT_ARSENAL_SIZE; ++slot) assert(!match.refundable(slot));
  match.reset(405);
  assert(inventory(match, 0) == Inventory{} && inventory(match, 1) == Inventory{});
}

void swapThroughMatch() {
  BrowserMatch match;
  shop(match, 505);
  assert(match.buy(BT_SWAP));
  assert(match.leaveBazaar());
  BTBox *valuable = match.player.board.box_manager_->createGimp(0, 20, 17);
  valuable->hide();
  match.player.board.fill(0, 20, valuable);
  match.player.board.landed(0, 20);
  const auto pg = match.player.generation, og = match.opponent.generation;
  assert(match.launch(find(match, BT_SWAP)));
  assert(match.player.generation == pg && match.opponent.generation == og);
  match.opponent.input(4);
  assert(match.player.generation == pg + 1 && match.opponent.generation == og + 1);
  assert(match.opponent.board.occupied(0, 20) && match.opponent.board.cell(0, 20) == -1);
  assert(!match.player.board.occupied(0, 20) && valuable->value() == 17);
  assert(match.player.active && match.opponent.active);
  assert(match.player.active->canMoveTo(match.player.active->x(), match.player.active->y()));
  assert(match.opponent.active->canMoveTo(match.opponent.active->x(), match.opponent.active->y()));
}

void nullifiedPeersAndReset() {
  BrowserMatch match;
  shop(match, 606);
  const int tokens[] = {BT_SWAP, BT_SUSAN, BT_MONDALE, BT_KEATING};
  for (int token : tokens) assert(match.buy(token));
  assert(match.leaveBazaar());
  activate(match.player, BT_MIRROR, 10);
  const auto opponentInventory = inventory(match, 1);
  const int playerFunds = match.player.funds, opponentFunds = match.opponent.funds;
  for (int token : tokens) {
    assert(match.launch(find(match, token)));
    assert(find(match, token) < 0);
    assert(match.player.pendingWeapons() == 0 && match.opponent.pendingWeapons() == 0);
  }
  assert(inventory(match, 1) == opponentInventory);
  assert(match.player.funds == playerFunds && match.opponent.funds == opponentFunds);
  match.player.queueWeapon(catalogWeapon(BT_SUSAN));
  match.reset(607);
  assert(match.player.events && match.opponent.events);
  assert(match.player.pendingWeapons() == 0 && match.opponent.pendingWeapons() == 0);
  assert(!match.player.weapons.BTActive[BT_MIRROR]);
  assert(inventory(match, 0) == Inventory{} && inventory(match, 1) == Inventory{});
}

void bottleSwapOrdering() {
  BrowserMatch match;
  match.start(707, 1, 0);
  activate(match.player, BT_BOTTLE, 10);
  activate(match.opponent, BT_BOTTLE, 10);
  activate(match.player, BT_FALL_OUT, 5);
  const auto generation = match.player.generation;
  match.opponent.queueWeapon(catalogWeapon(BT_SWAP));
  match.opponent.input(4);
  assert(match.player.generation == generation + 1);
  for (BrowserGame *game : {&match.player, &match.opponent}) {
    assert(!game->weapons.BTActive[BT_BOTTLE]);
    assert(game->weapons.remaining(BT_BOTTLE) == 0);
    for (int y = 0; y < BT_BOARD_HGT; ++y)
      for (int x = 0; x < BT_BOARD_WTH; ++x)
        assert(game->board.cell(x, y) != BT_STRUCT);
  }
  assert(match.player.weapons.remaining(BT_FALL_OUT) == 5);
  assert(!match.opponent.weapons.BTActive[BT_FALL_OUT]);
  // A later attack in the same batch can establish a new neck after Swap.
  match.opponent.queueWeapon(catalogWeapon(BT_SWAP));
  match.opponent.queueWeapon(catalogWeapon(BT_BOTTLE));
  match.opponent.input(4);
  assert(match.opponent.weapons.remaining(BT_BOTTLE) == catalogWeapon(BT_BOTTLE)->duration());
  assert(match.opponent.board.cell(0, 10) == BT_STRUCT);
  assert(!match.player.weapons.BTActive[BT_BOTTLE]);
}

void upsideMatchLifecycle() {
  BrowserMatch match;
  shop(match, 808);
  assert(match.buy(BT_UPBYSIDE));
  match.leaveBazaar();
  const auto generation = match.opponent.generation;
  assert(match.launch(find(match, BT_UPBYSIDE)));
  assert(match.opponent.gravityDirection() == 1);
  match.opponent.input(4);
  assert(match.opponent.gravityDirection() == -1);
  assert(match.opponent.generation == generation + 1);
  assert(match.opponent.active->y() == BT_BOARD_HGT - 4);
  const auto invertedGeneration = match.opponent.generation;
  for (int i = 0; i < 500 && match.opponent.generation == invertedGeneration; ++i) match.tick(10);
  assert(match.opponent.generation > invertedGeneration && !match.opponent.over);

  match.player.queueWeapon(catalogWeapon(BT_UPBYSIDE));
  match.player.input(4);
  assert(match.player.gravityDirection() == -1);
  match.opponent.queueWeapon(catalogWeapon(BT_SWAP));
  match.opponent.input(4);
  assert(match.player.gravityDirection() == 1 && match.opponent.gravityDirection() == 1);
  assert(match.player.active->y() == 0 && match.opponent.active->y() == 0);
  assert(!match.player.weapons.remaining(BT_UPBYSIDE));
  assert(!match.opponent.weapons.remaining(BT_UPBYSIDE));
  // A later queued inversion affects the received grid after Swap normalizes it.
  match.opponent.queueWeapon(catalogWeapon(BT_SWAP));
  match.opponent.queueWeapon(catalogWeapon(BT_UPBYSIDE));
  match.opponent.input(4);
  assert(match.opponent.gravityDirection() == -1 && match.player.gravityDirection() == 1);
  match.reset(809);
  assert(match.opponent.gravityDirection() == 1 && match.player.gravityDirection() == 1);
}
} // namespace

void peerRules() {
  signedKeating();
  mondaleFunds();
  lawyersDeferred();
  susanInventory();
  swapThroughMatch();
  nullifiedPeersAndReset();
  bottleSwapOrdering();
  upsideMatchLifecycle();
}
