#include "Match.H"
#include "BTBox.H"
#include <cassert>
#include <vector>

namespace {
void clearRow(BrowserGame &game) {
  const int y = BT_BOARD_HGT - 1;
  for (int x = 0; x < BT_BOARD_WTH; ++x)
    game.board.fill(x, y, x == 0 ? game.board.box_manager_->dieCreate(x, y, 3)
                                : game.board.box_manager_->create(x, y, BT_RED));
  game.board.landed(0, y);
  game.board.checkLines();
}
void openBazaar(BrowserMatch &match) {
  // Reach the combined threshold through actual board clears and ring events.
  for (int i = 0; i < 10; ++i) { clearRow(match.player); clearRow(match.opponent); }
  match.tick(10);
  assert(match.status() == 5 && match.player.paused && match.opponent.paused);
}
int slotFor(BrowserMatch &match, int token, int side = 0) {
  for (int slot = 0; slot < BT_ARSENAL_SIZE; ++slot)
    if (match.arsenalToken(side, slot) == token && match.arsenalQuantity(side, slot)) return slot;
  return -1;
}
int inventory(BrowserMatch &match, int side) {
  int count = 0;
  for (int slot = 0; slot < BT_ARSENAL_SIZE; ++slot) count += match.arsenalQuantity(side, slot);
  return count;
}
void activate(BrowserGame &game, BTWeaponToken token, unsigned short duration) {
  BTWeapon weapon(token);
  weapon.duration_ = duration;
  game.sendPlusMe(BT_WPN_ON, &weapon);
}

void bazaarLifecycle() {
  BrowserMatch match;
  match.start(100, 1, 0);
  assert(match.linesUntilBazaar() == 20 && !match.buy(BT_RISE_UP));
  assert(!match.leaveBazaar() && !match.refund(0) && !match.launch(0));
  openBazaar(match);
  assert(match.player.lines == 10 && match.opponent.lines == 10);
  const int py = match.player.active->y(), oy = match.opponent.active->y();
  const unsigned pg = match.player.generation, og = match.opponent.generation;
  for (int i = 0; i < 100; ++i) { match.tick(100); match.input(4); }
  assert(match.player.active->y() == py && match.opponent.active->y() == oy);
  assert(match.player.generation == pg && match.opponent.generation == og);
  assert(match.linesUntilBazaar() == 0);

  match.player.funds = 1000;
  const int price = match.price(BT_RISE_UP);
  assert(price == catalogWeapon(BT_RISE_UP)->price());
  assert(match.buy(BT_RISE_UP) && match.buy(BT_RISE_UP));
  const int slot = slotFor(match, BT_RISE_UP);
  assert(slot >= 0 && match.arsenalQuantity(0, slot) == 2 && match.refundable(slot) == 2);
  assert(match.player.funds == 1000 - 2 * price);
  assert(match.refund(slot) && match.player.funds == 1000 - price);
  assert(match.arsenalQuantity(0, slot) == 1 && !match.launch(slot));
  assert(match.leaveBazaar() && match.status() == 0);
  assert(!match.player.paused && !match.opponent.paused);
  assert(match.linesUntilBazaar() == 20);
  assert(match.arsenalQuantity(0, slot) == 1 && !match.refund(slot));

  // Retained inventory cannot be refunded at the next visit; new purchases can.
  openBazaar(match);
  assert(match.player.lines + match.opponent.lines == 40);
  assert(match.arsenalQuantity(0, slot) == 1 && !match.refund(slot));
  const int before = match.player.funds;
  assert(match.buy(BT_RISE_UP) && match.refundable(slot) == 1);
  assert(match.refund(slot) && match.player.funds == before);
  assert(!match.refund(slot) && match.arsenalQuantity(0, slot) == 1);
  match.reset(100);
  assert(match.status() == 0 && match.linesUntilBazaar() == 20);
  assert(inventory(match, 0) == 0 && inventory(match, 1) == 0);
  assert(match.player.pendingWeapons() == 0 && match.opponent.pendingWeapons() == 0);
}

void inventoryBounds() {
  BrowserMatch match;
  match.start(7, 1, 0);
  openBazaar(match);
  match.player.funds = 1000000;
  assert(!match.buy(-1) && !match.buy(BT_MAX_WEAPONS));
  assert(match.price(-1) == -1 && match.price(BT_RISE_UP, -1) == -1);
  assert(match.arsenalToken(-1, 0) == -1 && match.arsenalToken(0, -1) == -1);
  assert(match.arsenalQuantity(2, 0) == 0 && !match.refund(BT_ARSENAL_SIZE));
  std::vector<int> supported;
  for (int token = 0; token < BT_MAX_WEAPONS; ++token)
    if (supportedWeapon(token)) supported.push_back(token);
  assert(supported.size() == 34);
  for (int i = 0; i < BT_ARSENAL_SIZE; ++i) assert(match.buy(supported[i]));
  assert(inventory(match, 0) == BT_ARSENAL_SIZE);
  const int funds = match.player.funds;
  assert(!match.buy(supported[BT_ARSENAL_SIZE]) && match.player.funds == funds);
  // Existing stacks can grow even when all ten slots are occupied.
  assert(match.buy(BT_FLIP_OUT));
  const int flip = slotFor(match, BT_FLIP_OUT);
  for (int quantity = match.arsenalQuantity(0, flip); quantity < 32767; ++quantity)
    assert(match.buy(BT_FLIP_OUT));
  const int atLimit = match.player.funds;
  assert(!match.buy(BT_FLIP_OUT) && match.player.funds == atLimit);
  assert(match.arsenalQuantity(0, flip) == 32767);
  assert(match.refund(flip) && match.buy(BT_FLIP_OUT));

  // Carter affects new purchases and refunds use the amount actually paid.
  BrowserMatch carter;
  carter.start(8, 1, 0);
  openBazaar(carter);
  carter.player.funds = 1000;
  activate(carter.player, BT_CARTER, 20);
  const int doubled = 2 * catalogWeapon(BT_RISE_UP)->price();
  assert(carter.price(BT_RISE_UP) == doubled);
  assert(carter.buy(BT_RISE_UP) && carter.player.funds == 1000 - doubled);
  assert(carter.refund(slotFor(carter, BT_RISE_UP)) && carter.player.funds == 1000);
  carter.player.funds = doubled - 1;
  assert(!carter.buy(BT_RISE_UP));
}

void launchAndMirror() {
  BrowserMatch match;
  match.start(44, 1, 0);
  openBazaar(match);
  match.player.funds = 10000;
  assert(match.buy(BT_NO_DICE) && match.buy(BT_RISE_UP));
  assert(match.buy(BT_NICE_DAY) && match.buy(BT_MIRROR));
  assert(match.leaveBazaar());
  const unsigned generation = match.opponent.generation;
  assert(match.launch(slotFor(match, BT_NO_DICE)));
  assert(match.opponent.pendingWeapons() == 1);
  assert(match.opponent.generation == generation && !match.opponent.weapons.BTActive[BT_NO_DICE]);
  match.opponent.input(4);
  assert(match.opponent.generation == generation + 1 && match.opponent.pendingWeapons() == 0);
  assert(match.opponent.weapons.remaining(BT_NO_DICE) == catalogWeapon(BT_NO_DICE)->duration());

  activate(match.player, BT_MIRROR, 10);
  assert(match.launch(slotFor(match, BT_RISE_UP)));
  assert(match.player.pendingWeapons() == 1 && match.opponent.pendingWeapons() == 0);
  assert(match.launch(slotFor(match, BT_NICE_DAY)));
  assert(match.launch(slotFor(match, BT_MIRROR)));
  assert(match.player.pendingWeapons() == 1 && match.opponent.pendingWeapons() == 0);
  assert(inventory(match, 0) == 0); // Nullified weapons are still consumed.
  match.input(4);
  assert(match.player.pendingWeapons() == 0);

  // A full target queue rejects the launch without spending its inventory.
  BrowserMatch capped;
  capped.start(45, 1, 0);
  openBazaar(capped);
  capped.player.funds = 1000;
  assert(capped.buy(BT_FLIP_OUT));
  const int slot = slotFor(capped, BT_FLIP_OUT);
  capped.leaveBazaar();
  for (int i = 0; i < 64; ++i) capped.opponent.queueWeapon(catalogWeapon(BT_FLIP_OUT));
  assert(!capped.launch(slot) && capped.arsenalQuantity(0, slot) == 1);
  assert(capped.opponent.pendingWeapons() == 64);
}

void aiCombatAndSolo() {
  BrowserMatch match;
  match.start(55, 1, 0);
  match.opponent.funds = 1500;
  openBazaar(match);
  const int bought = inventory(match, 1);
  assert(bought > 0 && bought <= 3 && match.opponent.funds < 1500);
  assert(match.leaveBazaar());
  for (int i = 0; i < 20; ++i) match.tick(100);
  assert(inventory(match, 1) == bought - 1);
  assert(match.player.pendingWeapons() == 1);
  assert(match.player.generation == 1); // Queuing does not force a premature lock.
  match.input(4);
  assert(match.player.pendingWeapons() == 0 && match.player.generation == 2);

  BrowserMatch solo;
  solo.start(66, 0, 0);
  for (int i = 0; i < 20; ++i) clearRow(solo.player);
  solo.tick(10);
  assert(solo.status() == 0 && solo.linesUntilBazaar() == -1);
  assert(!solo.buy(BT_FLIP_OUT) && !solo.launch(0) && !solo.leaveBazaar());
}

void bazaarFreezesLanding() {
  BrowserMatch match;
  match.start(67, 1, 0);
  for (BrowserGame *game : {&match.player, &match.opponent}) {
    while (game->active->moveTo(game->active->x(), game->active->y() + 1)) {}
    game->input(3);
    game->tick(40, false);
  }
  match.player.queueWeapon(catalogWeapon(BT_NO_SLIDE));
  match.player.lines = 20;
  match.tick(10);
  assert(match.status() == 5);
  for (int i = 0; i < 20; ++i) match.tick(100);
  for (BrowserGame *game : {&match.player, &match.opponent})
    assert(game->sliding() && game->slideElapsed() == 50 && game->generation == 1);
  assert(match.player.pendingWeapons() == 1);
  assert(!match.player.weapons.BTActive[BT_NO_SLIDE]);
  match.leaveBazaar();
  assert(match.player.slideElapsed() == 0 && match.opponent.slideElapsed() == 0);
  match.tick(100);
  match.tick(40);
  assert(match.player.sliding() && match.player.generation == 1);
  match.tick(10);
  assert(match.player.generation == 2 && match.opponent.generation == 2);
  assert(match.player.pendingWeapons() == 0);
  assert(match.player.weapons.remaining(BT_NO_SLIDE) == catalogWeapon(BT_NO_SLIDE)->duration());
}
void matchResumeTimers() {
  for (bool bazaar : {false, true}) {
    BrowserMatch match;
    match.start(68, 1, 0);
    for (int i = 0; i < 4; ++i) match.tick(100);
    const int y = match.player.active->y();
    if (bazaar) {
      match.player.lines = 20;
      match.tick(10);
      assert(match.status() == 5);
    } else match.input(5);
    const double elapsed = match.player.elapsed;
    for (int i = 0; i < 10; ++i) match.tick(100);
    assert(match.player.elapsed == elapsed && match.player.active->y() == y);
    if (bazaar) assert(match.leaveBazaar());
    else match.input(5);
    assert(match.player.elapsed == 0);
    for (int i = 0; i < 5; ++i) match.tick(100);
    match.tick(10);
    assert(match.player.active->y() == y);
    match.tick(10); // First 10 ms match step past the full 512 ms interval.
    assert(match.player.active->y() == y + 1);
  }
  // Measure the adapted AI's first command interval, then pause just before
  // that command. Resume must wait a full command interval again.
  BrowserMatch control, paused;
  control.start(69, 1, 0);
  paused.start(69, 1, 0);
  const unsigned revision = control.opponent.motionRevision();
  int firstCommand = 0;
  do {
    control.tick(10);
    firstCommand += 10;
    assert(firstCommand < 2000);
  } while (control.opponent.motionRevision() == revision);
  assert(firstCommand > 10);
  for (int time = 10; time < firstCommand; time += 10) paused.tick(10);
  paused.input(5);
  paused.tick(100);
  paused.input(5);
  for (int time = 10; time < firstCommand; time += 10) paused.tick(10);
  assert(paused.opponent.motionRevision() == revision);
  paused.tick(10);
  assert(paused.opponent.motionRevision() > revision);
}
} // namespace

void combatRules() {
  bazaarLifecycle();
  inventoryBounds();
  launchAndMirror();
  aiCombatAndSolo();
  bazaarFreezesLanding();
  matchResumeTimers();
}
