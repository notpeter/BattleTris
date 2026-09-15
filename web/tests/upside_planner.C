#include "Game.H"
#include "BTPlanner.H"
#include "BTBox.H"
#include <array>
#include <cassert>
#include <memory>
#include <vector>

namespace {
void activate(BrowserGame &game, BTWeaponToken token, unsigned short duration = 100) {
  BTWeapon weapon(token);
  weapon.duration_ = duration;
  game.sendPlusMe(BT_WPN_ON, &weapon);
}
void put(BrowserGame &game, int x, int y) {
  game.board.fill(x, y, game.board.box_manager_->create(x, y, BT_RED));
  game.board.landed(x, y);
}
std::vector<int> unchangedPlan(BTPlanner &planner, BTPiece &piece, BrowserGame &game) {
  const int px = piece.x(), py = piece.y();
  const auto rng = game.board.random.state();
  std::array<int, BT_PIECE_WIDTH * BT_PIECE_HEIGHT> before;
  std::array<int, BT_BOARD_WTH * BT_BOARD_HGT> board;
  for (int y = 0; y < BT_PIECE_HEIGHT; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH; ++x) before[y * BT_PIECE_WIDTH + x] = piece.cell(x, y);
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) board[y * BT_BOARD_WTH + x] = game.board.cell(x, y);
  auto path = planner.plan(piece, game.board, game.weapons);
  assert(!path.empty() && planner.visited() > 0 && planner.evaluated() > 0);
  assert(planner.visited() <= BTPlanner::MaxStates);
  assert(piece.x() == px && piece.y() == py && game.board.random.state() == rng);
  for (int y = 0; y < BT_PIECE_HEIGHT; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH; ++x) assert(before[y * BT_PIECE_WIDTH + x] == piece.cell(x, y));
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) assert(board[y * BT_BOARD_WTH + x] == game.board.cell(x, y));
  assert(planner.plan(piece, game.board, game.weapons) == path);
  return path;
}
void replay(BTPiece &piece, const std::vector<int> &path) {
  for (std::size_t index = 0; index < path.size(); ++index) {
    bool moved = false;
    switch (path[index]) {
    case 0: moved = piece.moveTo(piece.x() + 1, piece.y()); break;
    case 1: moved = piece.moveTo(piece.x() - 1, piece.y()); break;
    case 2: moved = piece.rotate(0); break;
    case 3: moved = piece.moveTo(piece.x(), piece.y() - 1); break;
    default: assert(false);
    }
    if (index + 1 == path.size()) assert(path[index] == 3 && !moved);
    else assert(moved);
  }
}

void allPieces() {
  BrowserGame game(true);
  activate(game, BT_UPBYSIDE);
  BTPlanner planner;
  std::vector<std::unique_ptr<BTPiece>> pieces;
#define PIECE(Type) pieces.emplace_back(new Type(&game.board))
  PIECE(BTElPiece); PIECE(BTRevElPiece); PIECE(BTSldLftPiece); PIECE(BTSldRtPiece);
  PIECE(BTLongPiece); PIECE(BTPlugPiece); PIECE(BTBoxPiece); PIECE(BTDiePiece);
  PIECE(BTHappyPiece); PIECE(BTDogPiece); PIECE(BTRevDogPiece); PIECE(BTCapPiece);
  PIECE(BTWallPiece); PIECE(BTTowerPiece); PIECE(BTStarPiece); PIECE(BTWeirdLongPiece);
  PIECE(BTFourByFourPiece); PIECE(BTLongDongPiece);
#undef PIECE
  for (auto &piece : pieces) {
    piece->construct(BT_DEFAULT_X, BT_BOARD_HGT - 4);
    assert(piece->moveTo(BT_DEFAULT_X - piece->rotationSize() / 2, BT_BOARD_HGT - 4));
    auto path = unchangedPlan(planner, *piece, game);
    replay(*piece, path);
    game.pieces.dispose(piece.get());
    game.board.checkLines();
    // Keep placements to exercise reachable rotations under an uneven ceiling.
  }
}

void upsideTetris() {
  BrowserGame game(true);
  activate(game, BT_UPBYSIDE);
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      if (x != 4) put(game, x, y);
  }
  BTLongPiece piece(&game.board);
  piece.construct(BT_DEFAULT_X, BT_BOARD_HGT - 4);
  assert(piece.moveTo(BT_DEFAULT_X - piece.rotationSize() / 2, BT_BOARD_HGT - 4));
  BTPlanner planner;
  replay(piece, unchangedPlan(planner, piece, game));
  game.pieces.dispose(&piece);
  game.board.checkLines();
  assert(game.lines == 4);
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) assert(!game.board.occupied(x, y));
}

void negativeOriginAndFallout() {
  BrowserGame game(true);
  activate(game, BT_UPBYSIDE);
  BTPlanner planner;
  BTDiePiece die(&game.board);
  die.construct(4, -1); // Local die offset is (1,1), so its cell is still visible.
  assert(die.canMoveTo(4, -1));
  replay(die, unchangedPlan(planner, die, game));
  die.reset();

  // Block both ledges completely. The only landing is through the open ceiling,
  // beyond the visible board, and must terminate at native collision bounds.
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      if (x < BT_FALL_OUT_LEDGE || x >= BT_BOARD_WTH - BT_FALL_OUT_LEDGE) put(game, x, y);
  activate(game, BT_FALL_OUT);
  die.construct(4, BT_BOARD_HGT - 4);
  assert(die.moveTo(4, BT_BOARD_HGT - 4));
  replay(die, unchangedPlan(planner, die, game));
  assert(die.y() < -BT_PIECE_HEIGHT);
  game.pieces.dispose(&die);
  assert(game.board.checkLines() == 0 && game.lines == 0);
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = BT_FALL_OUT_LEDGE; x < BT_BOARD_WTH - BT_FALL_OUT_LEDGE; ++x)
      assert(!game.board.occupied(x, y));
}
} // namespace

void upsidePlannerRules() {
  allPieces();
  upsideTetris();
  negativeOriginAndFallout();
}
