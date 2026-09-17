#include "Drop.H"
#include "Game.H"
#include "Match.H"
#include "BTBox.H"
#include <cassert>

namespace {
void activate(BrowserGame &game, BTWeaponToken token, unsigned short duration) {
  BTWeapon weapon(token);
  weapon.duration_ = duration;
  game.sendPlusMe(BT_WPN_ON, &weapon);
}
void ground(BrowserGame &game) {
  assert(game.active);
  while (game.active->moveTo(game.active->x(), game.active->y() + 1)) {}
  assert(!game.active->canMoveTo(game.active->x(), game.active->y() + 1));
}
void oneCell(BrowserGame &game) {
  game.reset(13);
  BTWeapon happy(BT_NICE_DAY);
  game.queueWeapon(happy);
  finishDrop(game);
  assert(game.active && game.active->isHappy());
  game.board.clear();
}

void animatedDrop() {
  BrowserGame game;
  oneCell(game);
  const unsigned generation = game.generation;
  const int y = game.active->y();
  game.input(4);
  assert(game.active->y() == y && game.generation == generation);
  game.tick(9); assert(game.active->y() == y);
  game.input(4); // Repeated Space cannot restart the descent timer.
  game.tick(1); assert(game.active->y() == y + 1);
  while (!game.sliding()) game.tick(10);
  const int x = game.active->x();
  game.tick(100); game.input(0);
  assert(game.active->x() == x - 1 && game.generation == generation);
  game.tick(49); assert(game.generation == generation);
  game.tick(1); assert(game.generation == generation + 1);
  activate(game, BT_MEADOW, 10);
  const int slowY = game.active->y();
  game.input(4); game.tick(19); assert(game.active->y() == slowY);
  game.tick(1); assert(game.active->y() == slowY + 1);
}

void exactGraceAndSpam() {
  BrowserGame game;
  oneCell(game);
  ground(game);
  const unsigned generation = game.generation;
  game.input(3);
  assert(game.sliding() && game.generation == generation && game.slideElapsed() == 0);
  game.tick(50, false);
  assert(game.slideElapsed() == 50);
  for (int i = 0; i < 20; ++i) {
    game.input(3); game.input(2); game.input(0); game.input(1);
  }
  assert(game.sliding() && game.slideElapsed() == 50 && game.generation == generation);
  game.tick(99, false);
  assert(game.sliding() && game.slideElapsed() == 149 && game.generation == generation);
  game.tick(1, false);
  assert(!game.sliding() && game.generation == generation + 1 && game.slideElapsed() == 0);

  // A collision caused by normal gravity enters the same grace state.
  ground(game);
  game.elapsed = game.gravityInterval() - 1;
  game.tick(1);
  assert(game.sliding());
}

void lateralEscape() {
  BrowserGame game;
  oneCell(game);
  assert(game.active->moveTo(4, 24));
  // Happy occupies local (1,1), so block the cell below that mapped square.
  game.board.fill(5, 26, game.board.box_manager_->create(5, 26, BT_RED));
  game.board.landed(5, 26);
  const unsigned generation = game.generation;
  game.input(3);
  assert(game.sliding());
  game.tick(100, false);
  game.input(1);
  assert(game.active->x() == 5 && game.active->y() == 24);
  assert(game.slideElapsed() == 100); // Lateral input does not restart grace.
  game.tick(50, false);
  assert(!game.sliding() && game.generation == generation && game.active->y() == 25);
  game.tick(100, false);
  assert(game.active->y() == 25); // AI's grace-only clock adds no gravity.
  for (int i = 0; i < 5; ++i) game.tick(100);
  game.tick(12);
  assert(game.active->y() == 26 && game.generation == generation);
}

void pauseAndHardDrop() {
  BrowserGame game;
  oneCell(game);
  ground(game);
  game.input(3);
  game.tick(40, false);
  const unsigned generation = game.generation;
  game.input(5);
  for (int i = 0; i < 20; ++i) { game.tick(100); game.input(3); }
  assert(game.sliding() && game.slideElapsed() == 40 && game.generation == generation);
  game.input(5);
  assert(game.sliding() && game.slideElapsed() == 0);
  game.tick(100, false);
  game.setPaused(false); // A redundant resume must not restart the running clock.
  game.tick(49, false);
  assert(game.sliding() && game.slideElapsed() == 149 && game.generation == generation);
  game.tick(1, false);
  assert(!game.sliding() && game.generation == generation + 1);
  ground(game);
  game.input(3);
  assert(game.sliding());
  game.input(4);
  assert(game.sliding() && game.generation == generation + 1);
  game.tick(100); game.tick(49);
  assert(game.generation == generation + 1);
  game.tick(1);
  assert(game.generation == generation + 2);
}

void resumeGravity() {
  BrowserGame game;
  game.reset(13);
  assert(game.gravityInterval() == 512);
  BTPiece *piece = game.active;
  const int y = piece->y();
  for (int i = 0; i < 4; ++i) game.tick(100);
  assert(game.elapsed == 400 && piece->y() == y);
  game.setPaused(true);
  game.tick(100);
  assert(game.elapsed == 400 && game.active == piece && piece->y() == y);
  game.setPaused(false);
  assert(game.elapsed == 0 && game.active == piece);
  for (int i = 0; i < 4; ++i) game.tick(100);
  game.setPaused(false);
  game.tick(100);
  game.tick(11);
  assert(game.elapsed == 511 && piece->y() == y);
  game.tick(1);
  assert(game.elapsed == 0 && piece->y() == y + 1);
}

void deniedAndExpiry() {
  BrowserGame game;
  game.reset(14);
  activate(game, BT_NO_SLIDE, 1);
  ground(game);
  const unsigned generation = game.generation;
  game.input(3);
  assert(!game.sliding() && game.generation == generation + 1);
  BTLine line;
  ++line;
  game.send(BT_LINE, &line);
  assert(!game.weapons.BTActive[BT_NO_SLIDE]);
  ground(game);
  game.input(3);
  assert(game.sliding() && game.generation == generation + 1);
  game.tick(100, false);
  game.tick(50, false);
  assert(game.generation == generation + 2 && !game.sliding());
}

void aiLandingClock() {
  for (int denied = 0; denied <= 1; ++denied) {
    BrowserMatch match;
    match.start(15, 1, 10);
    match.player.paused = true; // Isolate the computer clock in this fixture.
    if (denied) activate(match.opponent, BT_NO_SLIDE, 1000);
    const unsigned generation = match.opponent.generation;
    bool sawGrace = false;
    for (int step = 0; step < 500 && match.opponent.generation == generation; ++step) {
      match.tick(10);
      sawGrace = sawGrace || match.opponent.sliding();
    }
    assert(match.opponent.generation == generation + 1 && !match.opponent.over);
    assert(sawGrace == !denied);
  }
}

void resetAndSwap() {
  BrowserGame a, b;
  a.reset(16); b.reset(17);
  ground(a); ground(b);
  a.input(3); b.input(3);
  a.tick(70, false); b.tick(30, false);
  assert(a.sliding() && b.sliding());
  const unsigned ag = a.generation, bg = b.generation;
  a.swapSettledBoard(b);
  assert(!a.sliding() && !b.sliding());
  assert(a.slideElapsed() == 0 && b.slideElapsed() == 0);
  assert(a.generation == ag + 1 && b.generation == bg + 1);
  ground(a);
  a.input(3);
  a.tick(60, false);
  a.reset(18);
  assert(!a.sliding() && a.slideElapsed() == 0 && a.generation == 1);
  assert(a.active && !a.over && !a.paused);
}
} // namespace

void landingRules() {
  animatedDrop();
  exactGraceAndSpam();
  lateralEscape();
  pauseAndHardDrop();
  resumeGravity();
  deniedAndExpiry();
  aiLandingClock();
  resetAndSwap();
}
