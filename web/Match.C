#include "Match.H"
#include <algorithm>
#include <cmath>

BrowserMatch::BrowserMatch() {
  player.events = opponent.events = this;
}

bool BrowserMatch::applyWeapon(BrowserGame &game, BTWeapon &weapon) {
  if (!versus_) return false;
  BrowserGame &peer = &game == &player ? opponent : player;
  switch (weapon.token()) {
  case BT_AMES: case BT_ACE: case BT_CONDOR:
    reporting_[&game == &player ? 1 : 0] = recon_[&game == &player ? 1 : 0].remaining() > 0;
    return true;
  case BT_SWAP:
    game.swapSettledBoard(peer);
    message_ = "Swap Meet exchanged the settled boards.";
    return true;
  case BT_SUSAN:
    std::swap(arsenals_[0], arsenals_[1]);
    message_ = "Susan exchanged the remaining arsenals.";
    return true;
  case BT_KEATING:
    // Local delivery uses the current signed balance, including debt.
    peer.funds += game.funds;
    game.funds = 0;
    message_ = "Keating transferred the recipient's balance.";
    return true;
  default: return false;
  }
}

int BrowserMatch::taxFunds(BrowserGame &game, int gross) {
  if (!versus_ || gross <= 0 || !game.weapons.BTActive[BT_MONDALE]) return gross;
  const int net = gross * 7 / 10;
  BrowserGame &peer = &game == &player ? opponent : player;
  // Credit directly: transfers must not recursively trigger the peer's tax.
  peer.funds += gross - net;
  return net;
}

void BrowserMatch::clearedLines(BrowserGame &game, int count) {
  if (!versus_) return;
  const int viewer = &game == &player ? 1 : 0;
  recon_[viewer].clearedLines(count);
  if (!recon_[viewer].remaining()) reporting_[viewer] = false;
  BrowserGame &peer = &game == &player ? opponent : player;
  if (!peer.weapons.BTActive[BT_LAWYERS]) return;
  // Earn the rises now, even if Lawyers expires before their delivery.
  for (int line = 0; line < count; ++line)
    peer.queueWeapon(catalogWeapon(BT_RISE_UP));
}

void BrowserMatch::queued(BrowserGame &game, const BTWeapon &weapon) {
  if (!versus_) return;
  switch (weapon.token_) {
  case BT_AMES: case BT_ACE: case BT_CONDOR:
    // BTRecon counts the viewer's allowance at launch, before the recipient's
    // weapon flush. Existing knowledge remains visible until the next report.
    recon_[&game == &player ? 1 : 0].activate(weapon.token_, weapon.duration_, true);
    break;
  default: break;
  }
}

void BrowserMatch::placed(BrowserGame &game, int count) {
  if (!versus_) return;
  if (&game == &opponent && freeReconPending_) {
    // Native Ernie activates the initial/C-toggle request at the weapon flush
    // after this placement's report. The viewer's allowance starts at launch.
    freeReconPending_ = false;
    reporting_[0] = true;
    return;
  }
  const int viewer = &game == &player ? 1 : 0;
  if (reporting_[viewer]) recon_[viewer].report(game.board, game.funds, count);
}

void BrowserMatch::start(unsigned seed, int mode, int level) {
  humans_ = mode == 2;
  versus_ = mode == 1 || humans_;
  ready_[0] = ready_[1] = false;
  opponent.setComputer(!humans_);
  level_ = std::max(0, std::min(level, 14));
  result_ = 0;
  paused_ = bazaar_ = false;
  nextBazaar_ = 20;
  aiAttackElapsed_ = 0;
  for (auto &arsenal : arsenals_) for (auto &slot : arsenal) slot = Slot{};
  message_.clear();
  pending_ = aiElapsed_ = aiInterval_ = 0;
  plannedGeneration_ = plannedMotion_ = 0;
  path_.clear();
  step_ = 0;
  player.reset(seed);
  // Independent sequences, with no dependency on event interleaving.
  opponent.reset(seed ^ 0x9e3779b9u);
  recon_[0].reset(seed ^ 0xa341316cu);
  recon_[1].reset(seed ^ 0xc8013ea4u);
  reporting_[0] = reporting_[1] = false;
  freeRecon_ = versus_ && !humans_;
  freeReconPending_ = freeRecon_;
  if (freeRecon_) recon_[0].activate(BT_CONDOR, 65535);
}

bool BrowserMatch::toggleRecon() {
  if (!versus_ || humans_ || result_) return false;
  freeRecon_ = !freeRecon_;
  freeReconPending_ = freeRecon_;
  recon_[0].deactivate();
  reporting_[0] = false;
  if (freeRecon_) recon_[0].activate(BT_CONDOR, 65535);
  return true;
}

void BrowserMatch::input(int command) {
  if (result_ || bazaar_) return;
  if (command == 5) {
    if (!humans_) setPaused(!paused_);
    return;
  }
  if (paused_) return;
  player.input(command);
  finish();
}

void BrowserMatch::finish() {
  if (result_) return;
  if (player.over && versus_ && opponent.over) result_ = 4;
  else if (player.over) result_ = 2;
  else if (versus_ && opponent.over) result_ = 3;
  if (result_) {
    ready_[0] = ready_[1] = false;
    player.stop();
    opponent.stop();
    recon_[0].deactivate();
    recon_[1].deactivate();
    reporting_[0] = reporting_[1] = freeRecon_ = freeReconPending_ = false;
    path_.clear();
    pending_ = aiElapsed_ = aiAttackElapsed_ = 0;
    return;
  }
  if (!result_ && versus_ && !bazaar_ && player.lines + opponent.lines >= nextBazaar_)
    enterBazaar();
}

void BrowserMatch::stepComputer(double milliseconds) {
  if (opponent.over || !opponent.active || opponent.sliding()) return;
  auto replan = [&](bool newPiece) {
    path_ = planner_.plan(*opponent.active, opponent.board, opponent.weapons);
    // A legal active piece always has a downward landing path. This fallback
    // keeps the session progressing if a future planner rejects a piece type.
    if (path_.empty()) {
      int y = opponent.active->y();
      const int direction = opponent.gravityDirection();
      while (opponent.active->canMoveTo(opponent.active->x(), y + direction)) {
        path_.push_back(3);
        y += direction;
      }
      path_.push_back(3);
    }
    step_ = 0;
    plannedGeneration_ = opponent.generation;
    plannedMotion_ = opponent.motionRevision();
    // These are native Ernie piece intervals. The browser spreads reachable
    // moves through that interval instead of teleporting to the landing cell.
    if (newPiece) {
      aiElapsed_ = 0;
      static const double delays[] = {4000, 3000, 2000, 1500, 1250, 1000, 750, 550, 400, 350, 300, 225, 100, 10, 0};
      aiInterval_ = delays[level_] * opponent.gravityInterval() / BT_DROP_TIME / path_.size();
    }
  };
  if (plannedGeneration_ != opponent.generation) replan(true);
  const bool disrupted = opponent.weapons.BTActive[BT_HATTER] || opponent.weapons.BTActive[BT_SLICK];
  // Replan only at a command deadline, preserving elapsed time so repeated
  // forced movement cannot continually postpone Ernie's next command.
  const double interval = disrupted ? std::max(40.0, aiInterval_) : aiInterval_;
  aiElapsed_ += milliseconds;
  while (aiElapsed_ >= interval && !opponent.over) {
    aiElapsed_ -= interval;
    if (plannedMotion_ != opponent.motionRevision() || step_ == path_.size()) replan(false);
    opponent.input(path_[step_++]);
    plannedMotion_ = opponent.motionRevision();
    if (opponent.generation != plannedGeneration_ || opponent.sliding()) break;
  }
}

void BrowserMatch::tick(double milliseconds) {
  if (paused_ || bazaar_ || result_ || !std::isfinite(milliseconds) || milliseconds < 0) return;
  // Fixed simulation increments make frame partitioning immaterial. Limit
  // catch-up after a slow frame; the frontend pauses on tab/focus loss.
  pending_ += std::min(milliseconds, 100.0);
  while (pending_ >= 10 && !result_ && !bazaar_) {
    pending_ -= 10;
    player.tick(10);
    if (versus_) {
      // Disruption uses the same gravity as the human board, guaranteeing
      // descent while forced movement keeps changing the computer's route.
      const bool disrupted = opponent.weapons.BTActive[BT_HATTER] || opponent.weapons.BTActive[BT_SLICK];
      opponent.tick(10, humans_ || disrupted);
      if (!humans_) stepComputer(10);
    }
    finish();
    if (versus_ && !humans_ && !result_ && !bazaar_) {
      aiAttackElapsed_ += 10;
      if (aiAttackElapsed_ >= 2000) {
        aiAttackElapsed_ = 0;
        for (int slot = 0; slot < BT_ARSENAL_SIZE; ++slot)
          if (launchFor(1, slot)) break;
      }
    }
  }
}

int BrowserMatch::linesUntilBazaar() const {
  if (!versus_) return -1;
  return bazaar_ ? 0 : std::max(0, nextBazaar_ - player.lines - opponent.lines);
}

int BrowserMatch::price(int token, int side) const {
  BTWeapon *weapon = catalogWeapon(token);
  if (!weapon || !supportedWeapon(token) || side < 0 || side > 1) return -1;
  const BrowserGame &game = side ? opponent : player;
  return weapon->price() * (1 + !!game.weapons.BTActive[BT_CARTER]);
}

int BrowserMatch::arsenalToken(int side, int slot) const {
  if (side < 0 || side > 1 || slot < 0 || slot >= BT_ARSENAL_SIZE) return -1;
  return arsenals_[side][slot].token;
}

int BrowserMatch::arsenalQuantity(int side, int slot) const {
  if (side < 0 || side > 1 || slot < 0 || slot >= BT_ARSENAL_SIZE) return 0;
  return arsenals_[side][slot].quantity;
}

int BrowserMatch::refundable(int slot, int side) const {
  if (result_ || (humans_ && paused_) || !bazaar_ || side < 0 || side > 1 || ready_[side] || slot < 0 || slot >= BT_ARSENAL_SIZE) return 0;
  return arsenals_[side][slot].purchased;
}

bool BrowserMatch::buyFor(int side, int token) {
  if ((humans_ && paused_) || !bazaar_ || result_ || side < 0 || side > 1 || !supportedWeapon(token) || ready_[side]) return false;
  BrowserGame &game = side ? opponent : player;
  const int cost = price(token, side);
  if (cost < 0 || cost > game.funds) return false;
  auto &arsenal = arsenals_[side];
  int index = -1;
  // Prefer an existing stack even if an earlier slot became empty.
  for (int i = 0; i < BT_ARSENAL_SIZE; ++i)
    if (arsenal[i].token == token) { index = i; break; }
  if (index < 0)
    for (int i = 0; i < BT_ARSENAL_SIZE; ++i)
      if (!arsenal[i].quantity) { index = i; break; }
  // Original counts were signed shorts; reject overflow instead of wrapping.
  if (index < 0 || arsenal[index].quantity >= 32767) return false;
  Slot &slot = arsenal[index];
  slot.token = token;
  ++slot.quantity;
  ++slot.purchased;
  slot.paid = cost;
  game.funds -= cost;
  if (!side) message_ = std::string("Purchased ") + catalogWeapon(token)->name_ + ".";
  return true;
}

bool BrowserMatch::refund(int index) { return refundFor(0, index); }

bool BrowserMatch::sideRefund(int side, int index) { return humans_ && refundFor(side, index); }

bool BrowserMatch::refundFor(int side, int index) {
  if (result_ || !refundable(index, side)) return false;
  Slot &slot = arsenals_[side][index];
  (side ? opponent : player).funds += slot.paid;
  --slot.purchased;
  if (!--slot.quantity) slot = Slot{};
  message_ = "Purchase refunded.";
  return true;
}

void BrowserMatch::enterBazaar() {
  bazaar_ = true;
  nextBazaar_ += 20;
  player.setPaused(true);
  opponent.setPaused(true);
  pending_ = 0;
  for (auto &arsenal : arsenals_) for (auto &slot : arsenal) slot.purchased = 0;
  ready_[0] = ready_[1] = false;
  if (!humans_) shopComputer();
  message_ = humans_ ? "Bazaar open. Both players must finish shopping to resume."
    : "Bazaar open. Ernie is ready; finish shopping to resume both boards.";
}

void BrowserMatch::shopComputer() {
  // A deterministic first combat policy, separate from native Ernie's adaptive
  // purchase/combination strategy. Buy at most three affordable attacks.
  static const int choices[] = {BT_NO_DICE, BT_MIRROR, BT_FOUR_BY_FOUR,
    BT_FEARED_WEIRD, BT_REAGAN, BT_FORCE, BT_BROKEN, BT_BUG,
    BT_SPEEDY, BT_CARTER, BT_SO_LONG, BT_PIECE_IT, BT_RISE_UP, BT_FLIP_OUT};
  for (int purchase = 0; purchase < 3; ++purchase) {
    bool bought = false;
    for (int token : choices) {
      if (buyFor(1, token)) { bought = true; break; }
    }
    if (!bought) break;
  }
}

bool BrowserMatch::leaveBazaar() {
  if (humans_) return false;
  return resumeBazaar();
}

bool BrowserMatch::resumeBazaar() {
  if (!bazaar_ || result_) return false;
  bazaar_ = false;
  ready_[0] = ready_[1] = false;
  paused_ = false;
  player.setPaused(false);
  opponent.setPaused(false);
  for (auto &arsenal : arsenals_) for (auto &slot : arsenal) slot.purchased = 0;
  aiElapsed_ = aiAttackElapsed_ = 0;
  message_ = "Battle resumed. Launch arsenal slots with keys 1-9 and 0.";
  return true;
}

bool BrowserMatch::launchFor(int side, int index) {
  if (!versus_ || status() != 0 || side < 0 || side > 1 ||
      index < 0 || index >= BT_ARSENAL_SIZE) return false;
  Slot &slot = arsenals_[side][index];
  if (!slot.quantity || !supportedWeapon(slot.token)) return false;
  BrowserGame &sender = side ? opponent : player;
  const bool reflected = sender.weapons.BTActive[BT_MIRROR];
  // Mirror is applied to the enemy: their next launches reflect onto them.
  const bool nullified = reflected && (slot.token == BT_NICE_DAY || slot.token == BT_MIRROR ||
    slot.token == BT_SWAP || slot.token == BT_SUSAN || slot.token == BT_MONDALE ||
    slot.token == BT_KEATING || slot.token == BT_AMES || slot.token == BT_ACE ||
    slot.token == BT_CONDOR);
  BrowserGame &target = reflected ? sender : (side ? player : opponent);
  if (!nullified && target.pendingWeapons() >= 64) return false;
  BTWeapon *weapon = catalogWeapon(slot.token);
  if (!--slot.quantity) slot = Slot{};
  if (!nullified) target.queueWeapon(*weapon);
  message_ = std::string(humans_ ? (side ? "Player 2 launched " : "Player 1 launched ")
    : side ? "Ernie launched " : "You launched ") + weapon->name_ +
    (nullified ? ": nullified by Mirror." : reflected ? ": reflected back to the launcher." : ": queued for the next piece.");
  return true;
}

// Online inputs are applied in server sequence order. Gravity evaluates both
// boards before finish(), permitting simultaneous top-outs on a shared step.
bool BrowserMatch::sideInput(int side, int command) {
  if (!humans_ || side < 0 || side > 1 || command < 0 || command > 4 || status() != 0) return false;
  (side ? opponent : player).input(command);
  finish();
  return true;
}

bool BrowserMatch::setPaused(bool paused) {
  if (result_) return false;
  paused_ = paused;
  player.setPaused(paused || bazaar_);
  opponent.setPaused(paused || bazaar_);
  if (!paused_) aiElapsed_ = aiAttackElapsed_ = 0;
  pending_ = 0;
  return true;
}

bool BrowserMatch::sideReady(int side) {
  if (!humans_ || side < 0 || side > 1 || !bazaar_ || result_ || paused_ || ready_[side]) return false;
  ready_[side] = true;
  if (ready_[0] && ready_[1]) return resumeBazaar();
  return true;
}

bool BrowserMatch::sideSurrender(int side) {
  if (!humans_ || side < 0 || side > 1 || result_) return false;
  (side ? opponent : player).over = true;
  finish();
  return true;
}
