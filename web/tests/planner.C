#include "Game.H"
#include "BTPlanner.H"
#include "BTBox.H"
#include "Match.H"
#include <array>
#include <cassert>
#include <memory>
#include <vector>

namespace {
std::vector<int> unchangedPlan(BTPlanner &planner, BTPiece &piece,
                               BrowserGame &game) {
  const int initialX = piece.x(), initialY = piece.y();
  const auto randomState = game.board.random.state();
  const int lines = game.lines, funds = game.funds, score = game.score;
  std::array<int, BT_PIECE_WIDTH * BT_PIECE_HEIGHT> active;
  std::array<int, BT_BOARD_WTH * BT_BOARD_HGT> cells, occupied;
  for (int y = 0; y < BT_PIECE_HEIGHT; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH; ++x)
      active[y * BT_PIECE_WIDTH + x] = piece.cell(x, y);
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) {
      cells[y * BT_BOARD_WTH + x] = game.board.cell(x, y);
      occupied[y * BT_BOARD_WTH + x] = game.board.occupied(x, y);
    }
  auto path = planner.plan(piece, game.board, game.weapons);
  assert(piece.x() == initialX && piece.y() == initialY);
  assert(game.board.random.state() == randomState);
  assert(game.lines == lines && game.funds == funds && game.score == score);
  for (int y = 0; y < BT_PIECE_HEIGHT; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH; ++x)
      assert(active[y * BT_PIECE_WIDTH + x] == piece.cell(x, y));
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) {
      assert(cells[y * BT_BOARD_WTH + x] == game.board.cell(x, y));
      assert(occupied[y * BT_BOARD_WTH + x] == game.board.occupied(x, y));
    }
  assert(planner.visited() <= BTPlanner::MaxStates);
  assert(planner.evaluated() <= planner.visited());
  return path;
}

void replay(BTPiece &piece, const std::vector<int> &path) {
  assert(!path.empty());
  for (std::size_t step = 0; step < path.size(); ++step) {
    // No independent gravity: these are the exact transitions the planner saw.
    // Only the final down command is allowed to fail and lock the piece.
    bool moved = false;
    switch (path[step]) {
    case 0: moved = piece.moveTo(piece.x() - 1, piece.y()); break;
    case 1: moved = piece.moveTo(piece.x() + 1, piece.y()); break;
    case 2: moved = piece.rotate(0) != 0; break;
    case 3: moved = piece.moveTo(piece.x(), piece.y() + 1); break;
    default: assert(false);
    }
    if (step + 1 == path.size()) assert(path[step] == 3 && !moved);
    else assert(moved);
  }
}

void allPieces(bool fallout = false) {
  BrowserGame game(true);
  if (fallout) {
    BTWeapon weapon(BT_FALL_OUT);
    weapon.duration_ = 10;
    game.sendPlusMe(BT_WPN_ON, &weapon);
  }
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
    piece->construct(BT_DEFAULT_X, BT_DEFAULT_Y);
    assert(piece->moveTo(BT_DEFAULT_X - piece->rotationSize() / 2, BT_DEFAULT_Y));
    auto path = unchangedPlan(planner, *piece, game);
    assert(planner.visited() > 0 && planner.evaluated() > 0);
    // Deterministic output also exercises private custom-rotation state: a
    // previous search must not change how wall/star/weird-long pieces rotate.
    assert(unchangedPlan(planner, *piece, game) == path);
    replay(*piece, path);
    game.pieces.dispose(piece.get());
    game.board.checkLines();
    // Keep earlier placements, so subsequent searches see an uneven stack.
  }
}

void tetrisFixture() {
  BrowserGame game(true);
  BTPlanner planner;
  // Four almost-full rows require a reachable rotation into the central well.
  for (int y = BT_BOARD_HGT - 4; y < BT_BOARD_HGT; ++y) {
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      if (x != 4) game.board.fill(x, y,
        x == 0 ? game.board.box_manager_->dieCreate(x, y, 3)
               : game.board.box_manager_->create(x, y, BT_RED));
    game.board.landed(0, y);
  }
  BTLongPiece piece(&game.board);
  piece.construct(BT_DEFAULT_X, BT_DEFAULT_Y);
  assert(piece.moveTo(BT_DEFAULT_X - piece.rotationSize() / 2, BT_DEFAULT_Y));
  const auto path = unchangedPlan(planner, piece, game);
  replay(piece, path);
  game.pieces.dispose(&piece);
  assert(game.board.checkLines() == 48);
  assert(game.lines == 4 && game.funds == 48);
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) assert(!game.board.occupied(x, y));
}

void obstructedSpawn() {
  BrowserGame game;
  BTPlanner planner;
  BTBoxPiece piece(&game.board);
  piece.construct(BT_DEFAULT_X, BT_DEFAULT_Y);
  assert(piece.moveTo(BT_DEFAULT_X, BT_DEFAULT_Y));
  bool blocked = false;
  for (int y = 0; y < BT_PIECE_HEIGHT && !blocked; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH && !blocked; ++x)
      if (piece.isMapped(x, y)) {
        const int bx = piece.x() + x, by = piece.y() + y;
        game.board.fill(bx, by, game.board.box_manager_->create(bx, by, BT_RED));
        game.board.landed(bx, by);
        blocked = true;
      }
  assert(blocked && !piece.canMoveTo(piece.x(), piece.y()));
  assert(unchangedPlan(planner, piece, game).empty());
  assert(planner.visited() == 0 && planner.evaluated() == 0);
  piece.reset();
}

void falloutEscape() {
  BrowserGame game(true);
  BTPlanner planner;
  BTWeapon fallout(BT_FALL_OUT);
  fallout.duration_ = 5;
  game.sendPlusMe(BT_WPN_ON, &fallout);
  // Tall ledges force the central escape route. The planner must reach the
  // original off-board collision boundary and finish with a failed down move.
  for (int y = 0; y < BT_BOARD_HGT; ++y) {
    for (int x : {1, 8})
      game.board.fill(x, y, game.board.box_manager_->create(x, y, BT_RED));
    game.board.landed(0, y);
  }
  BTDiePiece piece(&game.board);
  piece.construct(5, 0);
  assert(piece.moveTo(5, 0));
  const auto path = unchangedPlan(planner, piece, game);
  replay(piece, path);
  assert(piece.y() >= BT_BOARD_HGT);
  game.pieces.dispose(&piece);
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 2; x < 8; ++x) assert(!game.board.occupied(x, y));
  assert(game.funds == 0 && game.lines == 0);

  BrowserMatch match;
  match.start(81, 1, 2);
  match.opponent.queueWeapon(fallout);
  match.opponent.input(4);
  const auto generation = match.opponent.generation;
  for (int tick = 0; tick < 100; ++tick) match.tick(10);
  assert(match.opponent.generation > generation && !match.opponent.over);
}
} // namespace

void plannerRules() {
  allPieces();
  allPieces(true);
  tetrisFixture();
  obstructedSpawn();
  falloutEscape();
}
