#ifndef BT_BROWSER_MATCH_H
#define BT_BROWSER_MATCH_H

#include "Game.H"
#include "BTPlanner.H"
#include "Catalog.H"
#include "Recon.H"
#include <array>
#include <cstddef>
#include <string>
#include <vector>

// Coordinates local sessions. Neither board sends borrowed ring packets across
// sessions; match outcomes and scheduling live outside each synchronous ring.
class BrowserMatch : private BrowserGame::Events {
public:
  BrowserMatch();
  BrowserGame player;
  BrowserGame opponent{true};
  void start(unsigned seed, int mode, int level);
  void reset(unsigned seed) { start(seed, mode(), level_); }
  void input(int command);
  void tick(double milliseconds);
  int status() const { return result_ ? result_ : bazaar_ ? 5 : paused_ ? 1 : 0; }
  int mode() const { return humans_ ? 2 : versus_ ? 1 : 0; }
  int linesUntilBazaar() const;
  int price(int token, int side = 0) const;
  int arsenalToken(int side, int slot) const;
  int arsenalQuantity(int side, int slot) const;
  int refundable(int slot, int side = 0) const;
  bool sideInput(int side, int command);
  bool sideBuy(int side, int token) { return humans_ && buyFor(side, token); }
  bool sideRefund(int side, int slot);
  bool sideLaunch(int side, int slot) { return humans_ && launchFor(side, slot); }
  bool sideReady(int side);
  bool sideSurrender(int side);
  bool setPaused(bool paused);
  bool ready(int side) const { return side >= 0 && side < 2 && ready_[side]; }
  bool buy(int token) { return buyFor(0, token); }
  bool refund(int slot);
  bool launch(int slot) { return launchFor(0, slot); }
  bool leaveBazaar();
  const char *message() const { return message_.c_str(); }
  const BrowserRecon &recon(int viewer = 0) const { return recon_[viewer == 1 ? 1 : 0]; }
  bool toggleRecon();
  bool reconEnabled() const { return freeRecon_; }
private:
  bool freeRecon_ = false;
  bool freeReconPending_ = false;
  bool reporting_[2] = {false, false};
  BrowserRecon recon_[2];
  struct Slot {
    int token = -1, quantity = 0, purchased = 0, paid = 0;
  };
  std::array<std::array<Slot, BT_ARSENAL_SIZE>, 2> arsenals_;
  BTPlanner planner_;
  bool humans_ = false, ready_[2] = {false, false};
  bool versus_ = false, paused_ = false, bazaar_ = false;
  int level_ = 0, result_ = 0;
  int nextBazaar_ = 20;
  double aiAttackElapsed_ = 0;
  std::string message_;
  double pending_ = 0, aiElapsed_ = 0, aiInterval_ = 0;
  std::vector<int> path_;
  std::size_t step_ = 0;
  unsigned plannedGeneration_ = 0;
  unsigned plannedMotion_ = 0;
  void stepComputer(double milliseconds);
  void finish();
  void enterBazaar();
  void shopComputer();
  bool buyFor(int side, int token);
  bool launchFor(int side, int slot);
  bool refundFor(int side, int slot);
  bool resumeBazaar();
  bool applyWeapon(BrowserGame &game, BTWeapon &weapon) override;
  int taxFunds(BrowserGame &game, int gross) override;
  void clearedLines(BrowserGame &game, int count) override;
  void placed(BrowserGame &game, int count) override;
  void queued(BrowserGame &game, const BTWeapon &weapon) override;
};
#endif
