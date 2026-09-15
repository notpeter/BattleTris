#ifndef BT_BROWSER_RECON_H
#define BT_BROWSER_RECON_H

#include "BTConstants.H"
#include "BTProtocol.H"
#include "BTRandom.H"
#include <array>
#include <cstdint>

class BTBoardManager;

// Cached reconnaissance, with a private viewer RNG independent of either game.
class BrowserRecon {
public:
  BrowserRecon() { reset(1); }
  void reset(unsigned seed);
  void deactivate();
  void activate(int token, int duration, bool preserveReport = false);
  void clearedLines(int count);
  void report(BTBoardManager &board, int funds, int cleared);
  int token() const { return token_; }
  int remaining() const { return remaining_; }
  int funds() const { return funds_; }
  bool known() const { return known_; }
  int cell(int x, int y) const;
  std::uint32_t randomState() const { return random_.state(); }
private:
  BTRandom random_;
  std::array<int, BT_BOARD_WTH * BT_BOARD_HGT> cells_;
  int token_ = -1, remaining_ = 0, funds_ = 0;
  bool known_ = false;
  void clearReport();
  std::uint64_t uniform(std::uint64_t bound);
};

#endif
