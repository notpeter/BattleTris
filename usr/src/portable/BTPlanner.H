#ifndef BTPLANNER_H
#define BTPLANNER_H

#include <cstddef>
#include <vector>

class BTPiece;
class BTBoardManager;
class BTWeaponManager;

// A placement planner, not the original Ernie shopping/network state machine.
// Replay commands against the same board without concurrent gravity or changes.
class BTPlanner {
public:
  // 0 logical left, 1 right, 2 rotation, 3 gravity step. Final step locks.
  // Horizontal controls and gravity reverse with Upbyside-down.
  // Empty means unsupported/no legal plan. Planning leaves the live game intact.
  std::vector<int> plan(BTPiece &, BTBoardManager &, BTWeaponManager &);
  std::size_t visited() const { return visited_; }
  std::size_t evaluated() const { return evaluated_; }
  static const std::size_t MaxStates = 4096;
private:
  std::size_t visited_ = 0, evaluated_ = 0;
};

#endif
