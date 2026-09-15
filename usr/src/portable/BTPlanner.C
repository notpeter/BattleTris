#include "BTConfig.H"
#ifndef BT_PORTABLE
#error BTPlanner requires the portable box backend
#endif

#include "BTPlanner.H"
#include "BTCBoard.H"
#include "BTBox.H"
#include "BTWeaponManager.H"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <tuple>

namespace {
// The original piece destructors do not free their map; reset/landed explicitly
// manage ownership. These shallow polymorphic copies borrow the live boxes and
// only rearrange pointers. They must never call reset, landed or construct.
// Portable BTBox move/redraw methods are no-ops, so even custom rotations cannot
// move or redraw the original boxes. Copying the concrete type retains private
// state used by wall, star and weird-long rotations.
std::unique_ptr<BTPiece> snapshot(BTPiece &piece) {
#define COPY_PIECE(Type) \
  if (Type *p = dynamic_cast<Type *>(&piece)) \
    return std::unique_ptr<BTPiece>(new Type(*p));
  COPY_PIECE(BTElPiece)
  COPY_PIECE(BTRevElPiece)
  COPY_PIECE(BTSldLftPiece)
  COPY_PIECE(BTSldRtPiece)
  COPY_PIECE(BTLongPiece)
  COPY_PIECE(BTPlugPiece)
  COPY_PIECE(BTBoxPiece)
  COPY_PIECE(BTDiePiece)
  COPY_PIECE(BTHappyPiece)
  COPY_PIECE(BTDogPiece)
  COPY_PIECE(BTRevDogPiece)
  COPY_PIECE(BTCapPiece)
  COPY_PIECE(BTWallPiece)
  COPY_PIECE(BTTowerPiece)
  COPY_PIECE(BTStarPiece)
  COPY_PIECE(BTWeirdLongPiece)
  COPY_PIECE(BTFourByFourPiece)
  COPY_PIECE(BTLongDongPiece)
#undef COPY_PIECE
  return std::unique_ptr<BTPiece>();
}

typedef std::tuple<int, int, std::uint64_t> Position;
Position position(BTPiece &piece) {
  static_assert(BT_PIECE_WIDTH * BT_PIECE_HEIGHT <= 64,
                "Update planner shape key for larger pieces");
  std::uint64_t shape = 0;
  for (int y = 0; y < BT_PIECE_HEIGHT; ++y)
    for (int x = 0; x < BT_PIECE_WIDTH; ++x)
      if (piece.isMapped(x, y))
        shape |= std::uint64_t(1) << (y * BT_PIECE_WIDTH + x);
  return Position(piece.x(), piece.y(), shape);
}

struct Node {
  std::unique_ptr<BTPiece> piece;
  int parent, command;
  Node(std::unique_ptr<BTPiece> p, int previous, int action)
    : piece(std::move(p)), parent(previous), command(action) {}
};

// BTCBoard evaluates downward gravity. Reflect occupancy into its coordinate
// system without copying BTCBox's self-referential owner pointers.
void rescan(BTCBoard &evaluation, BTBoardManager &board, bool upside) {
  if (!upside) { evaluation.rescan(&board); return; }
  evaluation.top_ = BT_BOARD_HGT;
  evaluation.closed_holes_ = evaluation.open_holes_ = 0;
  for (int x = 0; x < BT_BOARD_WTH; ++x) {
    evaluation.tops_[x] = BT_BOARD_HGT;
    evaluation.h_holes_[x] = 0;
  }
  for (int y = BT_BOARD_HGT - 1; y >= 0; --y) {
    evaluation.v_holes_[y] = 0;
    for (int x = 0; x < BT_BOARD_WTH; ++x) {
      evaluation.box_[x][y].reset();
      if (board.occupied(x, BT_BOARD_HGT - 1 - y)) {
        evaluation.box_[x][y].hole_ = BTC_OCCUPIED;
        evaluation.top_ = evaluation.tops_[x] = y;
      }
    }
  }
}

class ReflectedPiece : public BTPiece {
  BTBox marker_;
  bool happy_;
public:
  explicit ReflectedPiece(BTPiece &piece)
    : BTPiece(0, nullptr), happy_(piece.isHappy()) {
    x_ = piece.x();
    y_ = BT_BOARD_HGT - BT_PIECE_HEIGHT - piece.y();
    for (int y = 0; y < BT_PIECE_HEIGHT; ++y)
      for (int x = 0; x < BT_PIECE_WIDTH; ++x) {
        const int reflected = BT_PIECE_HEIGHT - 1 - y;
        // Fallout can leave mapped cells beyond the visible board. Cells below
        // the reflected floor are skipped by eval itself; negative indices must
        // be omitted explicitly, because the legacy evaluator doesn't clamp.
        if (piece.isMapped(x, y) && y_ + reflected >= 0)
          map_[x][reflected] = &marker_;
      }
  }
  int isHappy() override { return happy_; }
  // Like other planner snapshots, this proxy must never call reset/landed.
};

// Defaults from BTComputer::reset. BTCBoard::eval preserves the original hole,
// height, variance, line and happy-box evaluation, including tetris preference.
BTCPenalties penalties() {
  BTCPenalties p;
  p.chp_ = 10000;
  p.ohp_ = 7000;
  p.cvhp_ = 3000;
  p.hp_ = 30000;
  p.vp_ = 50;
  p.lb_ = 5000;
  p.hap_ = 20000;
  p.midline_ = 14;
  return p;
}
} // namespace

std::vector<int> BTPlanner::plan(BTPiece &active, BTBoardManager &board,
                                  BTWeaponManager &weapons) {
  visited_ = evaluated_ = 0;
  const bool upside = weapons.BTActive[BT_UPBYSIDE];
  const int direction = upside ? -1 : 1;
  if (!active.canMoveTo(active.x(), active.y())) return {};
  const int firstOrigin = weapons.BTActive[BT_FALL_OUT]
    ? -2 * BT_PIECE_HEIGHT : 1 - BT_PIECE_HEIGHT;
  // Fallout opens the middle of the floor. The original collision rules stop
  // a departed piece beyond the visible board, where disposal discards it.
  const int lastOrigin = weapons.BTActive[BT_FALL_OUT]
    ? BT_BOARD_HGT + BT_PIECE_HEIGHT : BT_BOARD_HGT - 1;
  std::unique_ptr<BTPiece> initial = snapshot(active);
  if (!initial) return {};

  BTCPenalties weights = penalties();
  BTCBoard baseline, candidate;
  rescan(baseline, board, upside);
  baseline.eval(0, 0, nullptr, &weapons, 0, 0, weights);
  const bool noTetris = weapons.BTActive[BT_FEARED_WEIRD] ||
    weapons.BTActive[BT_FALL_OUT] || weapons.BTActive[BT_NO_DICE] ||
    weapons.BTActive[BT_FOUR_BY_FOUR] || weapons.BTActive[BT_FORCE] ||
    weapons.BTActive[BT_BROKEN] || weapons.BTActive[BT_BOTTLE];

  std::vector<Node> nodes;
  nodes.reserve(MaxStates);
  std::set<Position> seen;
  seen.insert(position(*initial));
  nodes.emplace_back(std::move(initial), -1, -1);
  float bestValue = std::numeric_limits<float>::infinity();
  int best = -1;

  for (std::size_t index = 0; index < nodes.size(); ++index) {
    BTPiece &piece = *nodes[index].piece;
    const bool canDescend = piece.canMoveTo(piece.x(), piece.y() + direction);
    if (!canDescend) {
      // Rescan rather than memcpy: BTCBox contains pointers to its owning board.
      rescan(candidate, board, upside);
      float value;
      if (upside) {
        ReflectedPiece reflected(piece);
        value = candidate.eval(reflected.x(), reflected.y(), &reflected,
          &weapons, noTetris, 0, weights, &baseline);
      } else {
        value = candidate.eval(piece.x(), piece.y(), &piece,
          &weapons, noTetris, 0, weights, &baseline);
      }
      ++evaluated_;
      if (value < bestValue) {
        bestValue = value;
        best = static_cast<int>(index);
      }
    }
    // Unlike the old recursive traversal, BFS retains a shortest legal path.
    // Down-first tie ordering follows the original checkMove traversal.
    const int actions[] = {3, 0, 1, 2};
    for (int action : actions) {
      if (nodes.size() == MaxStates) break;
      std::unique_ptr<BTPiece> next = snapshot(piece);
      bool moved = false;
      switch (action) {
      case 0: moved = next->moveTo(piece.x() - direction, piece.y()); break;
      case 1: moved = next->moveTo(piece.x() + direction, piece.y()); break;
      case 2: moved = next->rotate(0) != 0; break;
      case 3:
        if (canDescend) moved = next->moveTo(piece.x(), piece.y() + direction);
        break;
      }
      if (!moved || next->y() < firstOrigin || next->y() > lastOrigin) continue;
      if (seen.insert(position(*next)).second)
        nodes.emplace_back(std::move(next), static_cast<int>(index), action);
    }
  }
  visited_ = nodes.size();
  if (best < 0) return {};
  std::vector<int> commands;
  for (int at = best; nodes[at].parent >= 0; at = nodes[at].parent)
    commands.push_back(nodes[at].command);
  std::reverse(commands.begin(), commands.end());
  commands.push_back(3);
  return commands;
}
