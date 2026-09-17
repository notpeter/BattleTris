#include "Game.H"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

BrowserGame::BrowserGame(bool computer)
    : board(&weapons, BT_BOARD_WTH, BT_BOARD_HGT, computer),
      pieces(nullptr, &board), computer_(computer) {
  // Ring packets carry borrowed payloads and must be delivered synchronously.
  weapons.next(&board);
  board.next(&pieces);
  pieces.next(this);
  next(&weapons);
}

BrowserGame::~BrowserGame() {
  if (active) active->reset();
}

void BrowserGame::receive(BTRingPacket *packet) {
  if (packet->token == BT_WPN_ON || packet->token == BT_WPN_OFF) {
    const int token = static_cast<BTWeapon *>(packet->data)->token();
    if (token == BT_HATTER) hatterElapsed_ = 0;
    if (token == BT_SLICK) slickElapsed_ = 0;
  }
  if (packet->token == BT_WPN_ON &&
      static_cast<BTWeapon *>(packet->data)->token() == BT_REAGAN)
    funds = -funds;
  if (packet->token == BT_LINE) {
    const int cleared = static_cast<BTLine *>(packet->data)->inc();
    lines += cleared;
    if (events) events->clearedLines(*this, cleared);
  }
  if (packet->token == BT_FUNDS) {
    const int gross = *static_cast<short *>(packet->data);
    funds += events ? events->taxFunds(*this, gross) : gross;
  }
  pass(packet);
}

void BrowserGame::reset(unsigned seed) {
  if (active) active->reset();
  active = nullptr;
  board.clear();
  pendingWeapons_.clear();
  board.random.seed(seed);
  lines = funds = score = 0;
  generation = motionRevision_ = 0;
  slickDirection_ = -1;
  elapsed = 0;
  paused = over = false;
  send(BT_START);
  spawn();
}

void BrowserGame::spawn() {
  hatterElapsed_ = slickElapsed_ = 0;
  slickSuppressed_ = false;
  sliding_ = fastDrop_ = false;
  slideElapsed_ = elapsed = 0;
  ++generation;
  dropAwarded_ = false;
  const int spawnY = gravityDirection() < 0 ? BT_BOARD_HGT - 4 : BT_DEFAULT_Y;
  active = pieces.create(BT_DEFAULT_X, spawnY);
  // Match BTGame's rotation-dependent spawn origin, including dice.
  if (!active->moveTo(BT_DEFAULT_X - active->rotationSize() / 2, spawnY)) {
    active->reset();
    active = nullptr;
    over = true;
  }
}

void BrowserGame::down() {
  if (!active || sliding_) return;
  if (moveActive(0, gravityDirection())) return;
  if (!weapons.BTActive[BT_NO_SLIDE]) {
    sliding_ = true;
    slideElapsed_ = 0;
    return;
  }
  lock();
}

void BrowserGame::lock() {
  if (!active) return;
  sliding_ = false;
  slideElapsed_ = 0;
  // BTComputer::run awards this flat amount for each successful AI placement.
  if (computer_) score += BT_BOARD_HGT / 2;
  pieces.dispose(active);
  active = nullptr;
  const int previousLines = lines;
  board.checkLines();
  // Native BTGame and BTComputer report the settled board before flushing
  // incoming weapons. A newly activated spy first reports on the next lock.
  if (events) events->placed(*this, lines - previousLines);
  applyPendingWeapons();
  spawn();
}

void BrowserGame::setPaused(bool value) {
  if (paused && !value) {
    // Xt removes paused timeouts and schedules their full interval on resume.
    elapsed = slideElapsed_ = hatterElapsed_ = slickElapsed_ = 0;
  }
  paused = value;
}

void BrowserGame::stop() {
  // Native cleanup removes the falling piece, retaining the settled board.
  if (active) active->reset();
  active = nullptr;
  pendingWeapons_.clear();
  sliding_ = false;
  elapsed = slideElapsed_ = hatterElapsed_ = slickElapsed_ = 0;
  paused = true;
}

void BrowserGame::input(int command) {
  if (command == 5) {
    if (!over) setPaused(!paused);
    return;
  }
  if (over || paused || !active) return;
  // BTGame::beginDrop awards once, based on the origin at drop initiation.
  // Manual step-down retains the same once-per-piece score award.
  if (!computer_ && (command == 3 || command == 4) && !dropAwarded_) {
    score += BT_BOARD_HGT - active->y();
    dropAwarded_ = true;
  }
  if (!computer_ && (command == 3 || command == 4)) slickSuppressed_ = true;
  switch (command) {
  case 0: moveActive(-gravityDirection(), 0); break;
  case 1: moveActive(gravityDirection(), 0); break;
  case 2: rotateActive(); break;
  case 3: down(); elapsed = 0; break;
  case 4:
    if (!fastDrop_) { fastDrop_ = true; elapsed = 0; }
    break;
  }
}

bool BrowserGame::moveActive(int dx, int dy) {
  if (!active || !active->moveTo(active->x() + dx, active->y() + dy)) return false;
  ++motionRevision_;
  return true;
}

bool BrowserGame::rotateActive() {
  if (!active || !active->rotate()) return false;
  ++motionRevision_;
  return true;
}

void BrowserGame::tick(double milliseconds, bool gravity) {
  if (paused || over || !std::isfinite(milliseconds) || milliseconds < 0) return;
  // Advance to the next actual event, independent of caller frame partitions.
  // Ties run Hatter, Slick, then landing/gravity. The physical event is last:
  // if it spawns a piece, no old-piece event can run against that fresh spawn.
  double remaining = std::min(milliseconds, 100.0);
  const double infinity = std::numeric_limits<double>::infinity();
  while (!over && active && remaining > 0) {
    const bool hatter = weapons.BTActive[BT_HATTER];
    const bool slick = weapons.BTActive[BT_SLICK] && !sliding_ && !slickSuppressed_;
    const double fastInterval = std::ldexp(double(BT_FAST_DROP_TIME),
      std::min(5, weapons.applications(BT_MEADOW)));
    const double physicalInterval = sliding_ ? BT_SLIDE_TIME : fastDrop_ ? fastInterval : gravityInterval();
    const double physicalElapsed = sliding_ ? slideElapsed_ : elapsed;
    const double untilPhysical = sliding_ || gravity || fastDrop_
      ? std::max(0.0, physicalInterval - physicalElapsed) : infinity;
    const double untilHatter = hatter ? std::max(0.0, 20.0 - hatterElapsed_) : infinity;
    const double untilSlick = slick ? std::max(0.0, 20.0 - slickElapsed_) : infinity;
    const double next = std::min(untilPhysical, std::min(untilHatter, untilSlick));
    if (next == infinity) return;
    const double advance = std::min(remaining, next);
    if (hatter) hatterElapsed_ += advance;
    if (slick) slickElapsed_ += advance;
    if (sliding_) slideElapsed_ += advance;
    else if (gravity || fastDrop_) elapsed += advance;
    remaining -= advance;
    if (advance < next) return;

    if (untilHatter == next) {
      hatterElapsed_ = 0;
      rotateActive();
    }
    if (untilSlick == next) {
      slickElapsed_ = 0;
      if (!moveActive(slickDirection_ * gravityDirection(), 0)) slickDirection_ = -slickDirection_;
    }
    if (untilPhysical == next) {
      if (sliding_) {
        sliding_ = false;
        slideElapsed_ = elapsed = 0;
        // Native place() retries descent when its non-resetting timer expires.
        if (!moveActive(0, gravityDirection())) lock();
      } else {
        elapsed = 0;
        down();
      }
    }
  }
}

int BrowserGame::cell(int x, int y) {
  if (x < 0 || x >= BT_BOARD_WTH || y < 0 || y >= BT_BOARD_HGT) return 0;
  if (active) {
    const int px = x - active->x(), py = y - active->y();
    if (px >= 0 && px < BT_PIECE_WIDTH && py >= 0 && py < BT_PIECE_HEIGHT) {
      const int id = active->cell(px, py);
      if (id) return id;
    }
  }
  return board.cell(x, y);
}

void BrowserGame::queueWeapon(const BTWeapon &weapon) {
  if (over) return;
  // Copy scalar metadata: the caller may discard its catalogue view immediately.
  pendingWeapons_.push_back({weapon.token_, weapon.duration_});
  if (events) events->queued(*this, weapon);
}

void BrowserGame::applyPendingWeapons() {
  // Called only with no active piece. Board mutations cannot overwrite live
  // cells or invalidate an AI plan; the next generation is planned from scratch.
  std::vector<PendingWeapon> pending;
  pending.swap(pendingWeapons_);
  for (const auto &effect : pending) {
    BTWeapon weapon(effect.token);
    weapon.duration_ = effect.duration;
    if (!events || !events->applyWeapon(*this, weapon))
      sendPlusMe(BT_WPN_ON, &weapon);
  }
}

double BrowserGame::gravityInterval() const {
  const int speedy = weapons.applications(BT_SPEEDY);
  const int meadow = weapons.applications(BT_MEADOW);
  const int exponent = std::max(-5, std::min(5, meadow - speedy));
  return std::ldexp(static_cast<double>(BT_DROP_TIME), exponent);
}

void BrowserGame::swapSettledBoard(BrowserGame &other) {
  if (&other == this || over || other.over) return;
  // The receiver is normally between lock and spawn. An active peer discards
  // its falling piece and gets a fresh spawn, never an overlapping live piece.
  const bool respawn = active != nullptr;
  const bool respawnOther = other.active != nullptr;
  if (active) active->reset();
  if (other.active) other.active->reset();
  active = other.active = nullptr;
  sliding_ = other.sliding_ = false;
  slideElapsed_ = other.slideElapsed_ = 0;
  // A neck belongs to its timed effect, not the transferred board. Native
  // Swap also cancels Bottleneck before installing the peer's grid.
  BTWeapon bottle(BT_BOTTLE);
  if (weapons.BTActive[BT_BOTTLE]) sendPlusMe(BT_WPN_OFF, &bottle);
  if (other.weapons.BTActive[BT_BOTTLE]) other.sendPlusMe(BT_WPN_OFF, &bottle);
  BTWeapon upside(BT_UPBYSIDE);
  if (weapons.BTActive[BT_UPBYSIDE]) sendPlusMe(BT_WPN_OFF, &upside);
  if (other.weapons.BTActive[BT_UPBYSIDE]) other.sendPlusMe(BT_WPN_OFF, &upside);
  board.swapContents(other.board);
  elapsed = other.elapsed = 0;
  if (respawn) spawn();
  if (respawnOther) other.spawn();
  // A receiver with no active piece remains at its lock boundary. Its caller
  // finishes the pending effect batch and then spawns/increments generation.
}
