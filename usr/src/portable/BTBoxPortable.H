#ifndef BTBOXPORTABLE_H
#define BTBOXPORTABLE_H

#include "BTConstants.H"

class BTWidget;
class BTPixmap;

// Boxes retain the original identity, value and landing rules. Rendering reads
// snapshots from the board and active piece instead of drawing during mutation.
class BTBox {
protected:
  int id_, hidden_;
public:
  explicit BTBox(int id = 0) : id_(id), hidden_(0) {}
  virtual ~BTBox() {}
  virtual void moveTo(int, int, int = 0, int = 0) {}
  virtual void redraw() {}
  virtual void erase() {}
  virtual int isRemoveable() { return 1; }
  virtual int value() { return 0; }
  virtual void value(int) {}
  virtual int id() { return hidden_ ? -1 : id_; }
  virtual void landed() {}
  virtual void hide() { hidden_ = 1; }
};

class BTDieBox : public BTBox {
  int value_;
public:
  explicit BTDieBox(int value) : BTBox(value + BT_DIE_1 - 1), value_(value) {}
  int value() { return value_; }
  void value(int value) { value_ = value; }
};

class BTHappyBox : public BTBox {
  int landed_;
public:
  explicit BTHappyBox(int landed) : BTBox(BT_HAPPY + landed), landed_(landed) {}
  int value() { return landed_ ? 0 : BT_HAPPY_VAL; }
  void landed() { landed_ = 1; id_ = BT_UNHAPPY; }
};

class BTInvisiStructureBox : public BTBox {
public:
  BTInvisiStructureBox() : BTBox(BT_STRUCT) {}
  int isRemoveable() { return 0; }
};

class BTGimpBox : public BTBox {
  int value_;
public:
  explicit BTGimpBox(int value) : BTBox(BT_GIMP_ID), value_(value) {}
  int value() { return value_; }
  void value(int value) { value_ = value; }
};

class BTBoxManager {
public:
  BTBoxManager(BTWidget *, BTPixmap * = 0) {}
  BTBox *create(int x, int y, int color);
  BTBox *dieCreate(int x, int y, int value);
  BTBox *happyCreate(int x, int y, int landed = 0);
  BTBox *createByID(int x, int y, int id);
  BTBox *structureCreate(int x, int y);
  BTBox *createGimp(int x, int y, int value = 0);
  void dispose(BTBox *box) { delete box; }
};

#endif
