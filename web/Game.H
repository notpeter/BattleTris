#ifndef BT_WEB_GAME_H
#define BT_WEB_GAME_H

#include "BTBoardManager.H"
#include "BTPieceManager.H"
#include "BTPiece.H"
#include "BTLine.H"
#include <vector>

// Browser-facing lifecycle adapter. Rules remain in the original game sources.
class BrowserGame : public BTRingNode {
public:
  struct Events {
    virtual ~Events() {}
    virtual bool applyWeapon(BrowserGame &, BTWeapon &) { return false; }
    virtual int taxFunds(BrowserGame &, int gross) { return gross; }
    virtual void clearedLines(BrowserGame &, int) {}
    virtual void placed(BrowserGame &, int) {}
    virtual void queued(BrowserGame &, const BTWeapon &) {}
  };
  Events *events = nullptr; // Non-owning match observer; retained across reset.
  BTWeaponManager weapons;
  BTBoardManager board;
  BTPieceManager pieces;
  BTPiece *active = nullptr;
  int lines = 0, funds = 0, score = 0;
  unsigned generation = 0;
  bool paused = false, over = false;
  double elapsed = 0;

  explicit BrowserGame(bool computer = false);
  ~BrowserGame();
  void receive(BTRingPacket *packet) override;
  void reset(unsigned seed);
  void setComputer(bool computer) { computer_ = computer; }
  void input(int command);
  void setPaused(bool paused);
  void stop();
  void tick(double milliseconds, bool gravity = true);
  bool sliding() const { return sliding_; }
  double slideElapsed() const { return slideElapsed_; }
  unsigned motionRevision() const { return motionRevision_; }
  double hatterElapsed() const { return hatterElapsed_; }
  double slickElapsed() const { return slickElapsed_; }
  int slickDirection() const { return slickDirection_; }
  bool slickSuppressed() const { return slickSuppressed_; }
  int cell(int x, int y);
  void queueWeapon(const BTWeapon &weapon);
  void queueWeapon(const BTWeapon *weapon) { if (weapon) queueWeapon(*weapon); }
  std::size_t pendingWeapons() const { return pendingWeapons_.size(); }
  double gravityInterval() const;
  int gravityDirection() const { return weapons.BTActive[BT_UPBYSIDE] ? -1 : 1; }
  void swapSettledBoard(BrowserGame &other);
private:
  struct PendingWeapon {
    BTWeaponToken token;
    unsigned short duration;
  };
  std::vector<PendingWeapon> pendingWeapons_;
  void applyPendingWeapons();
  bool computer_, dropAwarded_ = false;
  bool sliding_ = false;
  double slideElapsed_ = 0;
  double hatterElapsed_ = 0, slickElapsed_ = 0;
  int slickDirection_ = -1;
  bool slickSuppressed_ = false;
  unsigned motionRevision_ = 0;
  bool moveActive(int dx, int dy);
  bool rotateActive();
  BrowserGame(const BrowserGame &) = delete;
  BrowserGame &operator=(const BrowserGame &) = delete;
  void spawn();
  void down();
  void lock();
};
#endif
