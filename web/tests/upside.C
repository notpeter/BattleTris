#include "Drop.H"
#include "Game.H"
#include "BTBox.H"
#include <cassert>

namespace {
void activate(BrowserGame &game, BTWeaponToken token, unsigned short duration = 0) {
  BTWeapon weapon(token);
  weapon.duration_ = duration;
  game.sendPlusMe(BT_WPN_ON, &weapon);
}
void expire(BrowserGame &game, int count) {
  BTLine lines;
  for (int i = 0; i < count; ++i) ++lines;
  game.send(BT_LINE, &lines);
}
void put(BrowserGame &game, int x, int y, BTBox *box = nullptr) {
  assert(!game.board.occupied(x, y));
  game.board.fill(x, y, box ? box : game.board.box_manager_->create(x, y, BT_RED));
  game.board.landed(x, y);
}
void walls(BrowserGame &game) {
  for (int y = 10; y < 18; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      if (x < 3 || x >= 7) {
        assert(game.board.occupied(x, y));
        assert(game.board.cell(x, y) == BT_STRUCT);
      }
}
void flipMetadataAndExpiry(bool computer) {
  BrowserGame game(computer);
  BTBox *die = game.board.box_manager_->dieCreate(2, 4, 6);
  die->hide();
  BTBox *gimp = game.board.box_manager_->createGimp(3, 4, 11);
  BTBox *invisible = game.board.box_manager_->create(4, 4, BT_INVISIBLE);
  put(game, 2, 4, die); put(game, 3, 4, gimp); put(game, 4, 4, invisible);
  activate(game, BT_UPBYSIDE, 2);
  assert(game.board.cell(2, 23) == -1 && game.board.occupied(2, 23));
  assert(game.board.cell(3, 23) == BT_GIMP_ID);
  assert(game.board.cell(4, 23) == 0 && game.board.occupied(4, 23));
  assert(die->value() == 6 && gimp->value() == 11);
  activate(game, BT_UPBYSIDE, 1);
  assert(game.weapons.remaining(BT_UPBYSIDE) == 3);
  assert(game.board.occupied(2, 23) && !game.board.occupied(2, 4));
  expire(game, 2);
  assert(game.board.occupied(2, 23));
  expire(game, 1);
  assert(!game.weapons.BTActive[BT_UPBYSIDE]);
  assert(game.board.cell(2, 4) == -1 && game.board.cell(3, 4) == BT_GIMP_ID);
  // Duplicate OFF must not turn an already restored board upside down again.
  BTWeapon off(BT_UPBYSIDE);
  game.sendPlusMe(BT_WPN_OFF, &off);
  assert(game.board.occupied(2, 4));
  activate(game, BT_UPBYSIDE, 1);
  game.reset(9);
  put(game, 0, 4);
  activate(game, BT_UPBYSIDE, 1);
  assert(game.board.cell(0, 23) == BT_RED && !game.board.occupied(0, 4));
}
void upwardClearsAndRise(bool computer) {
  for (int force = 0; force <= 1; ++force) {
    BrowserGame game(computer);
    activate(game, BT_UPBYSIDE, 100);
    if (force) activate(game, BT_FORCE, 100);
    // Upward gravity's source edge is row 27, which must be explicitly freed.
    for (int x = 0; x < BT_BOARD_WTH; ++x) put(game, x, 27);
    assert(game.board.checkLines() == 0 && game.lines == 1);
    for (int x = 0; x < BT_BOARD_WTH; ++x) assert(!game.board.occupied(x, 27));
    put(game, 2, 1, game.board.box_manager_->dieCreate(2, 1, 4));
    for (int x = 0; x < BT_BOARD_WTH; ++x) put(game, x, 0);
    game.board.checkLines();
    assert(game.lines == 2);
    assert(game.board.cell(2, force ? 1 : 0) == BT_DIE_4);
    assert(!game.board.occupied(2, force ? 0 : 1));
    // Garbage enters row zero and pushes settled cells downward, even for AI.
    game.board.clear();
    put(game, 1, 8);
    put(game, 0, 27); // Source-edge overflow must be disposed without leaking.
    activate(game, BT_RISE_UP);
    assert(game.board.cell(1, 9) == BT_RED && !game.board.occupied(1, 8));
    int garbage = 0;
    for (int x = 0; x < BT_BOARD_WTH; ++x) garbage += game.board.occupied(x, 0);
    assert(garbage == BT_BOARD_WTH - 1);
  }
}
void bottleAndFallout(bool computer) {
  for (int force = 0; force <= 1; ++force) {
    BrowserGame game(computer);
    activate(game, BT_BOTTLE, 100);
    activate(game, BT_UPBYSIDE, 100);
    walls(game); // The neck is symmetric around the horizontal flip axis.
    if (force) activate(game, BT_FORCE, 100);
    for (int x = 3; x < 7; ++x) put(game, x, 12);
    game.board.checkLines();
    assert(game.lines == 1);
    walls(game);
    activate(game, BT_RISE_UP);
    walls(game);
    put(game, 4, 23);
    activate(game, BT_FALL_OUT, 20);
    walls(game);
    assert(!game.board.occupied(4, 23));
    BTDiePiece die(&game.board);
    die.construct(4, 4);
    int moves = 0;
    while (die.moveTo(die.x(), die.y() - 1)) assert(++moves <= BT_BOARD_HGT + BT_PIECE_HEIGHT);
    assert(die.y() < -1);
    game.pieces.dispose(&die);
    const int lines = game.lines, funds = game.funds;
    game.board.checkLines();
    assert(game.lines == lines && game.funds == funds);
    walls(game);
    expire(game, 100);
    assert(!game.weapons.BTActive[BT_UPBYSIDE] && !game.weapons.BTActive[BT_BOTTLE]);
    for (int y = 10; y < 18; ++y)
      for (int x = 0; x < BT_BOARD_WTH; ++x)
        if (x < 3 || x >= 7) assert(!game.board.occupied(x, y));
  }
}
void queuedDirectionAndSwap() {
  BrowserGame game, peer(true);
  game.reset(3); peer.reset(4);
  BTWeapon up(BT_UPBYSIDE), happy(BT_NICE_DAY);
  up.duration_ = 10;
  game.queueWeapon(up); game.queueWeapon(happy);
  finishDrop(game);
  assert(game.active && game.active->isHappy() && !game.over);
  assert(game.active->y() == BT_BOARD_HGT - 4);
  const int x = game.active->x(), y = game.active->y();
  game.input(0);
  assert(game.active->x() == x + 1);
  game.input(1);
  assert(game.active->x() == x);
  game.input(3);
  assert(game.active->y() == y - 1);
  game.queueWeapon(up);
  finishDrop(game);
  assert(game.weapons.remaining(BT_UPBYSIDE) == 20);
  assert(game.active && game.active->y() == BT_BOARD_HGT - 4);
  peer.queueWeapon(up); finishDrop(peer);
  assert(peer.active && peer.active->y() == BT_BOARD_HGT - 4);
  game.swapSettledBoard(peer);
  assert(!game.weapons.BTActive[BT_UPBYSIDE] && !peer.weapons.BTActive[BT_UPBYSIDE]);
  assert(game.active && peer.active && game.active->y() == 0 && peer.active->y() == 0);
}
} // namespace

void upsideRules() {
  for (bool computer : {false, true}) {
    flipMetadataAndExpiry(computer);
    upwardClearsAndRise(computer);
    bottleAndFallout(computer);
  }
  queuedDirectionAndSwap();
}
