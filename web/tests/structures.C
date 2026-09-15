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
void put(BrowserGame &game, int x, int y, int color = BT_RED) {
  assert(!game.board.occupied(x, y));
  game.board.fill(x, y, game.board.box_manager_->create(x, y, color));
  game.board.landed(x, y);
}
bool wallCell(int x, int y) {
  return y >= BT_BOARD_HGT / 2 - BT_BOTTLE_Y && y < BT_BOARD_HGT / 2 + BT_BOTTLE_Y &&
    (x < BT_BOTTLE_X || x >= BT_BOARD_WTH - BT_BOTTLE_X);
}
void assertWalls(BrowserGame &game, bool hidden = false) {
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      if (wallCell(x, y)) {
        assert(game.board.occupied(x, y));
        assert(game.board.cell(x, y) == (hidden ? -1 : BT_STRUCT));
      }
}
int occupied(BrowserGame &game) {
  int count = 0;
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) count += game.board.occupied(x, y);
  return count;
}
void bottleLifecycle() {
  BrowserGame game;
  put(game, 0, 10); // Activation replaces existing cells in wall locations.
  activate(game, BT_BOTTLE, 3);
  assertWalls(game);
  assert(occupied(game) == 48);
  activate(game, BT_GIMP);
  activate(game, BT_BLIND);
  activate(game, BT_MISSING);
  assertWalls(game);
  assert(occupied(game) == 48);
  activate(game, BT_TWILIGHT);
  assertWalls(game, true);
  activate(game, BT_BOTTLE, 2);
  assert(game.weapons.remaining(BT_BOTTLE) == 5);
  assertWalls(game); // Repeated native activation reconstructs visible walls.
  expire(game, 4);
  assertWalls(game);
  assert(game.weapons.remaining(BT_BOTTLE) == 1);
  expire(game, 1);
  assert(!game.weapons.BTActive[BT_BOTTLE] && occupied(game) == 0);

  activate(game, BT_BOTTLE, 1);
  game.board.clear(); // Missing structure cells must not crash the OFF handler.
  put(game, 0, 10, BT_BLUE);
  expire(game, 1);
  assert(game.board.cell(0, 10) == BT_BLUE && occupied(game) == 1);
  game.reset(12);
  assert(!game.weapons.BTActive[BT_BOTTLE]);
}

void bottleClearAndRise() {
  for (int force = 0; force <= 1; ++force) {
    BrowserGame game;
    activate(game, BT_BOTTLE, 10);
    if (force) activate(game, BT_FORCE, 10);
    // Filling the four-cell neck clears a full line including its fixed sides.
    for (int x = BT_BOTTLE_X; x < BT_BOARD_WTH - BT_BOTTLE_X; ++x)
      put(game, x, 15);
    game.board.checkLines();
    assert(game.lines == 1 && game.weapons.remaining(BT_BOTTLE) == 9);
    assertWalls(game);
    for (int x = BT_BOTTLE_X; x < BT_BOARD_WTH - BT_BOTTLE_X; ++x)
      assert(!game.board.occupied(x, 15));
    for (int launch = 0; launch < 5; ++launch) {
      activate(game, BT_RISE_UP);
      assertWalls(game);
    }
    // A bottom clear exercises the transition from full width to the neck.
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      if (!game.board.occupied(x, BT_BOARD_HGT - 1)) put(game, x, BT_BOARD_HGT - 1);
    const int before = game.lines;
    game.board.checkLines();
    assert(game.lines > before);
    assertWalls(game);
    expire(game, 100);
    for (int y = 10; y < 18; ++y)
      for (int x = 0; x < BT_BOARD_WTH; ++x)
        if (wallCell(x, y)) assert(!game.board.occupied(x, y));
  }
}

void bottleActivationExpiry() {
  BrowserGame game;
  // Installing walls can itself complete rows and exhaust the new duration.
  for (int y = 10; y < 18; ++y)
    for (int x = BT_BOTTLE_X; x < BT_BOARD_WTH - BT_BOTTLE_X; ++x) put(game, x, y);
  activate(game, BT_BOTTLE, 3);
  assert(game.lines == 8 && !game.weapons.BTActive[BT_BOTTLE]);
  assert(occupied(game) == 0);
}

void falloutClears() {
  for (int force = 0; force <= 1; ++force) {
    for (int bottle = 0; bottle <= 1; ++bottle) {
      BrowserGame game;
      if (bottle) activate(game, BT_BOTTLE, 10);
      if (force) activate(game, BT_FORCE, 10);
      // Irregular material at different heights must all leave the central gap.
      for (int y = 0; y < BT_BOARD_HGT; ++y)
        for (int x = 0; x < BT_BOARD_WTH; ++x)
          if (!game.board.occupied(x, y) && (x + y) % 3 == 0) put(game, x, y);
      int before[BT_BOARD_WTH][BT_BOARD_HGT];
      for (int y = 0; y < BT_BOARD_HGT; ++y)
        for (int x = 0; x < BT_BOARD_WTH; ++x) before[x][y] = game.board.cell(x, y);
      activate(game, BT_FALL_OUT, 2);
      for (int y = 0; y < BT_BOARD_HGT; ++y)
        for (int x = 0; x < BT_BOARD_WTH; ++x) {
          if (x < BT_FALL_OUT_LEDGE || x >= BT_BOARD_WTH - BT_FALL_OUT_LEDGE)
            assert(game.board.cell(x, y) == before[x][y]);
          else if (bottle && wallCell(x, y)) assert(game.board.cell(x, y) == BT_STRUCT);
          else assert(!game.board.occupied(x, y));
        }
      if (bottle) assertWalls(game);
      activate(game, BT_FALL_OUT, 2);
      assert(game.weapons.remaining(BT_FALL_OUT) == 4);
      assert(!game.board.occupied(4, BT_BOARD_HGT));
      assert(game.board.occupied(0, BT_BOARD_HGT));
      assert(!game.board.occupied(4, -1));
      assert(game.board.occupied(0, -1));
      assert(game.board.occupied(4, BT_BOARD_HGT + BT_PIECE_HEIGHT + 1));
      expire(game, 4);
      assert(game.board.occupied(4, BT_BOARD_HGT));
      assert(game.board.occupied(4, -1));
      if (bottle) assertWalls(game);
    }
  }
}

void fallingPieceDisposal() {
  BrowserGame game;
  activate(game, BT_FALL_OUT, 3);
  BTDiePiece die(&game.board);
  die.construct(4, 0);
  int steps = 0;
  while (die.moveTo(die.x(), die.y() + 1)) {
    assert(++steps <= BT_BOARD_HGT + BT_PIECE_HEIGHT);
  }
  assert(die.y() > BT_BOARD_HGT);
  game.pieces.dispose(&die);
  assert(occupied(game) == 0); // Out-of-board boxes were disposed, not retained.
  assert(game.board.checkLines() == 0 && game.lines == 0 && game.funds == 0);
}
} // namespace

void structureRules() {
  bottleLifecycle();
  bottleClearAndRise();
  bottleActivationExpiry();
  falloutClears();
  fallingPieceDisposal();
}
