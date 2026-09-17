#include "Drop.H"
#include "Game.H"
#include "BTBox.H"
#include <cassert>
#include <typeinfo>

namespace {
void activate(BrowserGame &game, BTWeaponToken token, unsigned short duration = 0) {
  BTWeapon weapon(token);
  weapon.duration_ = duration;
  game.sendPlusMe(BT_WPN_ON, &weapon);
}
void clearLines(BrowserGame &game, int count) {
  BTLine lines;
  for (int i = 0; i < count; ++i) ++lines;
  game.send(BT_LINE, &lines);
}
int occupied(BrowserGame &game) {
  int count = 0;
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) count += game.board.occupied(x, y);
  return count;
}
void put(BrowserGame &game, int x, int y) {
  game.board.fill(x, y, game.board.box_manager_->create(x, y, BT_RED));
  game.board.landed(x, y);
}
void queuedAndTimed() {
  BrowserGame game;
  game.reset(77);
  BTWeapon speed(BT_SPEEDY);
  speed.duration_ = 3;
  game.queueWeapon(speed);
  speed.duration_ = 99; // Queue owns scalar metadata.
  assert(game.pendingWeapons() == 1 && !game.weapons.BTActive[BT_SPEEDY]);
  game.input(5);
  for (int i = 0; i < 20; ++i) { game.tick(100); finishDrop(game); }
  assert(game.pendingWeapons() == 1 && !game.weapons.BTActive[BT_SPEEDY]);
  game.input(5);
  const auto generation = game.generation;
  finishDrop(game);
  assert(game.generation == generation + 1 && game.pendingWeapons() == 0);
  assert(game.weapons.remaining(BT_SPEEDY) == 3);
  assert(game.gravityInterval() == BT_DROP_TIME / 2);
  activate(game, BT_SPEEDY, 3);
  assert(game.weapons.remaining(BT_SPEEDY) == 6);
  assert(game.gravityInterval() == BT_DROP_TIME / 4);
  activate(game, BT_MEADOW, 2);
  assert(game.gravityInterval() == BT_DROP_TIME / 2);
  clearLines(game, 2);
  assert(!game.weapons.BTActive[BT_MEADOW]);
  assert(game.gravityInterval() == BT_DROP_TIME / 4);
  clearLines(game, 4);
  assert(!game.weapons.BTActive[BT_SPEEDY] && game.gravityInterval() == BT_DROP_TIME);
  activate(game, BT_CARTER, 20);
  activate(game, BT_MIRROR, 10);
  assert(game.weapons.BTActive[BT_CARTER] && game.weapons.BTActive[BT_MIRROR]);
  clearLines(game, 10);
  assert(game.weapons.BTActive[BT_CARTER] && !game.weapons.BTActive[BT_MIRROR]);
  game.queueWeapon(speed);
  game.reset(77);
  assert(game.pendingWeapons() == 0);
  for (int i = 0; i < BT_MAX_WEAPONS; ++i) {
    assert(!game.weapons.BTActive[i]);
    assert(game.weapons.remaining(i) == 0 && game.weapons.applications(i) == 0);
  }
  game.funds = 37;
  BTWeapon reagan(BT_REAGAN);
  game.queueWeapon(reagan);
  assert(game.funds == 37);
  finishDrop(game);
  assert(game.funds == -37 && !game.weapons.BTActive[BT_REAGAN]);
}

void pieceEffects() {
  BrowserGame game;
  activate(game, BT_NICE_DAY);
  BTPiece *piece = game.pieces.create(BT_DEFAULT_X, 0);
  assert(dynamic_cast<BTHappyPiece *>(piece));
  piece->reset();
  activate(game, BT_NO_DICE, 5);
  activate(game, BT_SO_LONG, 5);
  for (int i = 0; i < 300; ++i) {
    piece = game.pieces.create(BT_DEFAULT_X, 0);
    assert(!dynamic_cast<BTDiePiece *>(piece));
    assert(!dynamic_cast<BTLongPiece *>(piece));
    piece->reset();
  }
  clearLines(game, 5);
  assert(!game.weapons.BTActive[BT_NO_DICE] && !game.weapons.BTActive[BT_SO_LONG]);
  activate(game, BT_FOUR_BY_FOUR, 3);
  bool giant = false;
  for (int i = 0; i < 300; ++i) {
    piece = game.pieces.create(BT_DEFAULT_X, 0);
    assert(!dynamic_cast<BTBoxPiece *>(piece));
    giant = giant || dynamic_cast<BTFourByFourPiece *>(piece);
    piece->reset();
  }
  assert(giant);
  clearLines(game, 3);
  activate(game, BT_FEARED_WEIRD, 2);
  activate(game, BT_FEARED_WEIRD, 2);
  assert(game.weapons.remaining(BT_FEARED_WEIRD) == 4);
  bool weird = false;
  for (int i = 0; i < 300; ++i) {
    piece = game.pieces.create(BT_DEFAULT_X, 0);
    assert(!dynamic_cast<BTElPiece *>(piece));
    assert(!dynamic_cast<BTLongPiece *>(piece));
    weird = weird || dynamic_cast<BTWallPiece *>(piece) || dynamic_cast<BTStarPiece *>(piece);
    piece->reset();
  }
  assert(weird);
  clearLines(game, 4);
  assert(!game.weapons.BTActive[BT_FEARED_WEIRD]);
  // Broken Record intentionally permits occasional new pieces in native rules.
  piece = game.pieces.create(BT_DEFAULT_X, 0);
  const std::type_info *last = &typeid(*piece);
  piece->reset();
  activate(game, BT_BROKEN, 2);
  int repeated = 0;
  for (int i = 0; i < 100; ++i) {
    piece = game.pieces.create(BT_DEFAULT_X, 0);
    if (*last == typeid(*piece)) ++repeated;
    last = &typeid(*piece);
    piece->reset();
  }
  assert(repeated > 70);
  clearLines(game, 2);
  assert(!game.weapons.BTActive[BT_BROKEN]);
}

void sampleRestrictions(BrowserGame &game, bool weird, bool noLong, bool giant) {
  bool sawEl = false, sawLong = false, sawBox = false, sawGiant = false;
  for (int i = 0; i < 600; ++i) {
    BTPiece *piece = game.pieces.create(BT_DEFAULT_X, 0);
    const bool el = dynamic_cast<BTElPiece *>(piece) != nullptr;
    const bool longPiece = dynamic_cast<BTLongPiece *>(piece) != nullptr;
    const bool box = dynamic_cast<BTBoxPiece *>(piece) != nullptr;
    const bool giantPiece = dynamic_cast<BTFourByFourPiece *>(piece) != nullptr;
    if (weird) {
      assert(!el && !longPiece && !box);
      assert(!dynamic_cast<BTRevElPiece *>(piece));
      assert(!dynamic_cast<BTSldLftPiece *>(piece));
      assert(!dynamic_cast<BTSldRtPiece *>(piece));
      assert(!dynamic_cast<BTPlugPiece *>(piece));
    }
    if (noLong) assert(!longPiece);
    if (giant) assert(!box);
    else assert(!giantPiece);
    sawEl = sawEl || el;
    sawLong = sawLong || longPiece;
    sawBox = sawBox || box;
    sawGiant = sawGiant || giantPiece;
    piece->reset();
  }
  if (!weird) {
    assert(sawEl);
    if (!noLong) assert(sawLong);
    if (!giant) assert(sawBox);
  }
  if (giant) assert(sawGiant);
}

void overlappingRestrictions() {
  BrowserGame game;
  // Feared Weird expires first: Long and normal Box remain prohibited.
  activate(game, BT_FEARED_WEIRD, 1);
  activate(game, BT_SO_LONG, 3);
  activate(game, BT_FOUR_BY_FOUR, 3);
  sampleRestrictions(game, true, true, true);
  clearLines(game, 1);
  sampleRestrictions(game, false, true, true);
  clearLines(game, 2);
  sampleRestrictions(game, false, false, false);
  // The other effects expire first: their OFF handlers must not restore normal
  // pieces until Feared Weird itself expires.
  activate(game, BT_FEARED_WEIRD, 3);
  activate(game, BT_SO_LONG, 1);
  activate(game, BT_FOUR_BY_FOUR, 1);
  sampleRestrictions(game, true, true, true);
  clearLines(game, 1);
  sampleRestrictions(game, true, false, false);
  clearLines(game, 2);
  sampleRestrictions(game, false, false, false);
}

void boardEffects() {
  BrowserGame game;
  put(game, 1, 20);
  activate(game, BT_FLIP_OUT);
  assert(!game.board.occupied(1, 20) && game.board.occupied(8, 20));
  assert(!game.weapons.BTActive[BT_FLIP_OUT]);
  activate(game, BT_RISE_UP);
  assert(game.board.occupied(8, 19) && occupied(game) == 10);
  activate(game, BT_MISSING);
  assert(occupied(game) == 9);
  activate(game, BT_PIECE_IT);
  assert(occupied(game) == 10);
  activate(game, BT_BUG);
  assert(occupied(game) == 11);
  int invisible = 0;
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      invisible += game.board.occupied(x, y) && game.board.cell(x, y) == 0;
  assert(invisible == 1);
  activate(game, BT_GIMP);
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      if (game.board.occupied(x, y)) assert(game.board.cell(x, y) == BT_GIMP_ID);
  activate(game, BT_TWILIGHT);
  assert(occupied(game) == 11);
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      if (game.board.occupied(x, y)) assert(game.board.cell(x, y) == -1);
  activate(game, BT_BLIND);
  assert(occupied(game) > 0 && occupied(game) < 11);

  // Full target region used to hang indefinitely in Piece It / Bug Report.
  game.board.clear();
  for (int y = BT_BOARD_HGT / 4; y < BT_BOARD_HGT / 4 + BT_BOARD_HGT / 2; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) put(game, x, y);
  const int before = occupied(game);
  activate(game, BT_PIECE_IT);
  activate(game, BT_BUG);
  assert(occupied(game) == before);
}

void boundaryLineRules() {
  for (int upside = 0; upside <= 1; ++upside) {
    for (int force = 0; force <= 1; ++force) {
      BrowserGame game;
      // Exercise boundary clearing directly, without rotating the board.
      game.weapons.BTActive[BT_UPBYSIDE] = upside;
      if (force) activate(game, BT_FORCE, 2);
      const int row = upside ? BT_BOARD_HGT - 1 : 0;
      const int sentinel = upside ? 3 : BT_BOARD_HGT - 4;
      put(game, 0, sentinel);
      for (int x = 0; x < BT_BOARD_WTH; ++x) put(game, x, row);
      game.board.checkLines();
      assert(game.lines == 1 && occupied(game) == 1);
      for (int x = 0; x < BT_BOARD_WTH; ++x) assert(!game.board.occupied(x, row));
      assert(game.board.occupied(0, sentinel));
      // A repeated scan must not count the same Force-cleared boundary again.
      game.board.checkLines();
      assert(game.lines == 1);
    }
  }
}

void forceRules() {
  BrowserGame game;
  put(game, 0, 20);
  for (int x = 0; x < BT_BOARD_WTH; ++x) put(game, x, BT_BOARD_HGT - 1);
  activate(game, BT_FORCE, 2);
  game.board.checkLines();
  assert(game.lines == 1 && game.board.occupied(0, 20));
  assert(game.weapons.remaining(BT_FORCE) == 1);
  for (int x = 0; x < BT_BOARD_WTH; ++x) put(game, x, BT_BOARD_HGT - 1);
  game.board.checkLines();
  assert(game.lines == 2 && !game.weapons.BTActive[BT_FORCE]);
  assert(game.board.occupied(0, 20));
  for (int x = 0; x < BT_BOARD_WTH; ++x) put(game, x, BT_BOARD_HGT - 1);
  game.board.checkLines();
  assert(game.lines == 3 && game.board.occupied(0, 21));
}
} // namespace

void effectRules() {
  queuedAndTimed();
  pieceEffects();
  overlappingRestrictions();
  boardEffects();
  forceRules();
  boundaryLineRules();
}
