#include "BTConfig.H"
#include "BTBox.H"

BTBox *BTBoxManager::create(int, int, int color) {
  return new BTBox(color == BT_INVISIBLE ? 0 : color);
}
BTBox *BTBoxManager::dieCreate(int, int, int value) {
  return new BTDieBox(value);
}
BTBox *BTBoxManager::happyCreate(int, int, int landed) {
  return new BTHappyBox(landed);
}
BTBox *BTBoxManager::structureCreate(int, int) {
  return new BTInvisiStructureBox();
}
BTBox *BTBoxManager::createGimp(int, int, int value) {
  return new BTGimpBox(value);
}
BTBox *BTBoxManager::createByID(int x, int y, int id) {
  if (id == BT_STRUCT) return structureCreate(x, y);
  if (id >= BT_DIE_1 && id <= BT_DIE_6)
    return dieCreate(x, y, id - BT_DIE_1 + 1);
  if (id == BT_HAPPY || id == BT_UNHAPPY)
    return happyCreate(x, y, id == BT_UNHAPPY);
  if (id == BT_GIMP_ID) return createGimp(x, y);
  return create(x, y, id);
}
