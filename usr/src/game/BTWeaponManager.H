/****************************************************************/
/*    NAME: Bryan Cantrill                                      */
/*    ACCT: bmc                                                 */
/*    FILE: BTWeaponManager.H                                   */
/*    ASSN:                                                     */
/*    DATE: Wed Apr 13 18:40:09 1994                            */
/****************************************************************/

#ifndef BTWEAPONMANAGER_H
#define BTWEAPONMANAGER_H

#include "BTConfig.H"
#include "BTRingNode.H"
#include "BTWeapon.H"
#include "Block.H"
#include "BTLine.H"
#include "BTArsenal.H"

class BTCommManager;
class BTPushButtonWidget;
class BTTextWidget;
class BTFormWidget;

#ifdef BT_PORTABLE
// Line-counted effects preserve native duration accumulation. Purchasing and
// target selection remain in the match, outside this synchronous ring.
class BTWeaponManager : public BTRingNode {
  int remaining_[BT_MAX_WEAPONS], applications_[BT_MAX_WEAPONS];
public:
  short BTActive[BT_MAX_WEAPONS];
  BTWeaponManager() { clear(); }
  void clear() {
    for (int i = 0; i < BT_MAX_WEAPONS; ++i)
      BTActive[i] = remaining_[i] = applications_[i] = 0;
  }
  int remaining(int token) const {
    return token >= 0 && token < BT_MAX_WEAPONS ? remaining_[token] : 0;
  }
  int applications(int token) const {
    return token >= 0 && token < BT_MAX_WEAPONS ? applications_[token] : 0;
  }
  void receive(BTRingPacket *packet) {
    if (packet->token == BT_START) clear();
    int transient = -1;
    if (packet->token == BT_WPN_ON || packet->token == BT_WPN_OFF) {
      BTWeapon *weapon = static_cast<BTWeapon *>(packet->data);
      int token = weapon->token();
      if (token >= 0 && token < BT_MAX_WEAPONS) {
        if (packet->token == BT_WPN_OFF) {
          BTActive[token] = remaining_[token] = applications_[token] = 0;
        } else {
          BTActive[token] = 1;
          if (weapon->duration()) {
            // Saturate rather than overflowing during an exceptionally long game.
            const int limit = 1000000000;
            remaining_[token] = remaining_[token] > limit - weapon->duration()
              ? limit : remaining_[token] + weapon->duration();
            if (applications_[token] < limit) ++applications_[token];
          } else if (!remaining_[token]) {
            transient = token;
          }
        }
      }
    }
    if (packet->token == BT_LINE) {
      const int lines = static_cast<BTLine *>(packet->data)->inc();
      for (int token = 0; token < BT_MAX_WEAPONS; ++token) {
        if (!remaining_[token]) continue;
        remaining_[token] = remaining_[token] > lines ? remaining_[token] - lines : 0;
        if (!remaining_[token]) {
          BTActive[token] = applications_[token] = 0;
          BTWeapon expired(static_cast<BTWeaponToken>(token));
          send(BT_WPN_OFF, &expired);
        }
      }
    }
    pass(packet);
    // One-shot board changes are permanent mutations, not timed active effects.
    // No OFF event: no temporary rule needs reversal; existing cells stay mutated.
    if (transient >= 0) BTActive[transient] = 0;
  }
};
#else
class BTWeaponManager : public BTRingNode {
friend class BTGame;
friend class BTComputer;
private:
  short remaining_[BT_MAX_WEAPONS];
  BTArsenal *arsenal_, *old_arsenal_;
  BTTextWidget *rep_;
  BTPushButtonWidget **label_; 
  BTCommManager *comm_manager_;
  int need_ars_, computer_;

public:
  short BTActive[BT_MAX_WEAPONS];
  BTWeaponManager(BTFormWidget *parent, int x, int y, BTCommManager *comm_manager, int computer = 0);
  void receive (BTRingPacket *packet);
  void update( int num = BT_ARSENAL_SIZE );
  void launchWeapon (int number);
  ~BTWeaponManager();
};

#endif /* BT_PORTABLE */

#endif
