#include "Game.H"
#include "Match.H"
#include "BTBox.H"
#include <cassert>
#include <cstdint>

namespace {
void activate(BrowserGame &game, BTWeaponToken token, unsigned short duration = 3) {
  BTWeapon weapon(token);
  weapon.duration_ = duration;
  game.sendPlusMe(BT_WPN_ON, &weapon);
}
void clearLines(BrowserGame &game, int count) {
  BTLine lines;
  for (int i = 0; i < count; ++i) ++lines;
  game.send(BT_LINE, &lines);
}
void oneCell(BrowserGame &game) {
  game.reset(13);
  BTWeapon happy(BT_NICE_DAY);
  game.queueWeapon(happy);
  game.input(4);
  assert(game.active && game.active->isHappy());
  game.board.clear();
}
void elbow(BrowserGame &game) {
  game.reset(31);
  for (int i = 0; i < 500; ++i) {
    if (dynamic_cast<BTElPiece *>(game.active)) {
      game.board.clear();
      assert(game.active->moveTo(3, 8));
      return;
    }
    game.input(4);
    game.board.clear();
  }
  assert(false && "Seeded stream must produce an elbow");
}
std::uint64_t shape(const BTPiece &piece) {
  std::uint64_t result = 0;
  for (int y = 0; y < BT_PIECE_HEIGHT; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH; ++x)
      if (piece.cell(x, y)) result |= std::uint64_t(1) << (y * BT_PIECE_WIDTH + x);
  return result;
}
void deadlinesAndRevision() {
  BrowserGame game;
  elbow(game);
  activate(game, BT_HATTER);
  const auto initialShape = shape(*game.active);
  const unsigned revision = game.motionRevision();
  game.tick(19, false);
  assert(shape(*game.active) == initialShape && game.hatterElapsed() == 19);
  assert(game.motionRevision() == revision);
  game.tick(1, false);
  assert(shape(*game.active) != initialShape && game.hatterElapsed() == 0);
  assert(game.motionRevision() > revision);

  oneCell(game);
  activate(game, BT_SLICK);
  const int x = game.active->x();
  const unsigned beforeMove = game.motionRevision();
  game.tick(19, false);
  assert(game.active->x() == x && game.slickElapsed() == 19);
  game.tick(1, false);
  assert(game.active->x() == x - 1 && game.slickElapsed() == 0);
  assert(game.motionRevision() > beforeMove);
}
void wallAndBlockedRotation() {
  BrowserGame game;
  oneCell(game);
  assert(game.active->moveTo(-1, 5)); // Happy's occupied cell is local (1,1).
  activate(game, BT_SLICK);
  assert(game.slickDirection() == -1);
  game.tick(20, false);
  assert(game.active->x() == -1 && game.slickDirection() == 1);
  game.tick(20, false);
  assert(game.active->x() == 0 && game.slickDirection() == 1);

  elbow(game);
  const auto original = shape(*game.active);
  assert(game.active->rotate());
  int blockedX = -1, blockedY = -1;
  for (int y = 0; y < BT_PIECE_HEIGHT && blockedX < 0; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH; ++x)
      if (game.active->cell(x, y) && !(original & (std::uint64_t(1) << (y * BT_PIECE_WIDTH + x)))) {
        blockedX = game.active->x() + x;
        blockedY = game.active->y() + y;
        break;
      }
  for (int i = 0; i < 3; ++i) assert(game.active->rotate());
  assert(shape(*game.active) == original && blockedX >= 0);
  game.board.fill(blockedX, blockedY, game.board.box_manager_->create(blockedX, blockedY, BT_RED));
  game.board.landed(blockedX, blockedY);
  activate(game, BT_HATTER);
  game.tick(20, false);
  assert(shape(*game.active) == original); // Blocked automatic rotation is harmless.
}
void stackingAndExpiry() {
  BrowserGame game;
  elbow(game);
  activate(game, BT_HATTER, 1);
  activate(game, BT_SLICK, 1);
  game.tick(13, false);
  activate(game, BT_HATTER, 2);
  activate(game, BT_SLICK, 2);
  assert(game.weapons.remaining(BT_HATTER) == 3 && game.weapons.remaining(BT_SLICK) == 3);
  assert(game.hatterElapsed() == 0 && game.slickElapsed() == 0);
  const auto original = shape(*game.active);
  const int x = game.active->x();
  game.tick(19, false);
  assert(shape(*game.active) == original && game.active->x() == x);
  game.tick(1, false);
  assert(shape(*game.active) != original && game.active->x() == x - 1);
  game.tick(7, false);
  clearLines(game, 3);
  assert(!game.weapons.BTActive[BT_HATTER] && !game.weapons.BTActive[BT_SLICK]);
  assert(game.hatterElapsed() == 0 && game.slickElapsed() == 0);
  const auto expiredShape = shape(*game.active);
  const int expiredX = game.active->x();
  game.tick(100, false);
  assert(shape(*game.active) == expiredShape && game.active->x() == expiredX);
}
void landingAndSuppression() {
  BrowserGame game;
  elbow(game);
  const auto original = shape(*game.active);
  assert(game.active->rotate());
  const auto rotated = shape(*game.active);
  for (int i = 0; i < 3; ++i) assert(game.active->rotate());
  int supportX = -1, supportY = -1;
  for (int y = 1; y < BT_PIECE_HEIGHT && supportX < 0; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH; ++x) {
      const auto here = std::uint64_t(1) << (y * BT_PIECE_WIDTH + x);
      const auto above = here >> BT_PIECE_WIDTH;
      if ((original & above) && !(original & here) && !(rotated & here)) {
        supportX = game.active->x() + x;
        supportY = game.active->y() + y;
        break;
      }
    }
  assert(supportX >= 0);
  game.board.fill(supportX, supportY, game.board.box_manager_->create(supportX, supportY, BT_RED));
  game.board.landed(supportX, supportY);
  activate(game, BT_HATTER);
  activate(game, BT_SLICK);
  game.elapsed = game.gravityInterval() - 1;
  game.tick(1); // Gravity initiates landing without manual-down suppression.
  assert(game.sliding());
  const int x = game.active->x();
  game.tick(19, false);
  assert(game.active->x() == x && game.hatterElapsed() == 0);
  assert(shape(*game.active) == rotated); // Hatter still rotates during grace.
  assert(game.sliding() && game.slideElapsed() == 19);

  oneCell(game);
  activate(game, BT_SLICK);
  activate(game, BT_HATTER);
  game.tick(7, false);
  game.input(3);
  assert(game.slickSuppressed());
  const int manualX = game.active->x();
  game.tick(100, false);
  assert(game.active->x() == manualX);
  const unsigned generation = game.generation;
  game.input(4);
  assert(game.generation == generation + 1 && !game.slickSuppressed());
  assert(game.slickElapsed() == 0 && game.hatterElapsed() == 0);

  BrowserGame computer(true);
  oneCell(computer);
  activate(computer, BT_SLICK);
  computer.input(3);
  assert(!computer.slickSuppressed());
  const int computerX = computer.active->x();
  computer.tick(20, false);
  assert(computer.active->x() == computerX - 1);
}
void pauseResetAndDirection() {
  BrowserGame game;
  oneCell(game);
  activate(game, BT_SLICK);
  activate(game, BT_HATTER);
  assert(game.active->moveTo(-1, 5));
  game.tick(20, false);
  assert(game.slickDirection() == 1);
  game.tick(7, false);
  const unsigned revision = game.motionRevision();
  game.input(5);
  game.tick(100, false);
  assert(game.hatterElapsed() == 7 && game.slickElapsed() == 7);
  assert(game.motionRevision() == revision);
  game.input(5);
  assert(game.slickDirection() == 1 && game.slickElapsed() == 0 && game.hatterElapsed() == 0);
  game.tick(19, false);
  assert(game.active->x() == -1);
  game.setPaused(false);
  game.tick(1, false);
  assert(game.active->x() == 0 && game.slickDirection() == 1);
  game.input(4);
  assert(game.slickDirection() == 1 && game.slickElapsed() == 0 && game.hatterElapsed() == 0);
  game.reset(77);
  assert(game.slickDirection() == -1 && !game.slickSuppressed());
  assert(game.slickElapsed() == 0 && game.hatterElapsed() == 0);
}
void resumeMotion() {
  BrowserGame game;
  elbow(game);
  activate(game, BT_HATTER);
  activate(game, BT_SLICK);
  const auto original = shape(*game.active);
  BTPiece *piece = game.active;
  const int x = piece->x();
  game.tick(7, false);
  game.setPaused(true);
  game.tick(100, false);
  assert(game.hatterElapsed() == 7 && game.slickElapsed() == 7);
  game.setPaused(false);
  assert(game.active == piece && game.hatterElapsed() == 0 && game.slickElapsed() == 0);
  game.tick(19, false);
  assert(shape(*piece) == original && piece->x() == x);
  game.setPaused(false);
  game.tick(1, false);
  assert(shape(*piece) != original && piece->x() == x - 1);

  game.input(3);
  assert(game.slickSuppressed());
  game.setPaused(true);
  game.setPaused(false);
  assert(game.slickSuppressed());
  const int suppressedX = piece->x();
  game.tick(20, false);
  assert(piece->x() == suppressedX);
}
void partitionAndComputerProgress() {
  BrowserGame a, b;
  elbow(a); elbow(b);
  for (BrowserGame *game : {&a, &b}) {
    activate(*game, BT_HATTER);
    activate(*game, BT_SLICK);
  }
  a.tick(100, false);
  for (int dt : {7, 13, 19, 1, 29, 31}) b.tick(dt, false);
  assert(a.active->x() == b.active->x() && a.active->y() == b.active->y());
  assert(shape(*a.active) == shape(*b.active));
  assert(a.hatterElapsed() == b.hatterElapsed() && a.slickElapsed() == b.slickElapsed());
  assert(a.slickDirection() == b.slickDirection() && a.motionRevision() == b.motionRevision());

  for (BTWeaponToken token : {BT_HATTER, BT_SLICK}) {
    BrowserMatch match;
    match.start(52, 1, 2);
    match.player.paused = true;
    activate(match.opponent, token, 100);
    const unsigned initial = match.opponent.generation;
    for (int i = 0; i < 2500 && match.opponent.generation == initial && match.status() == 0; ++i)
      match.tick(10);
    assert(match.opponent.generation > initial && !match.opponent.over);
  }
}
} // namespace

void motionRules() {
  deadlinesAndRevision();
  wallAndBlockedRotation();
  stackingAndExpiry();
  landingAndSuppression();
  pauseResetAndDirection();
  resumeMotion();
  partitionAndComputerProgress();
}
