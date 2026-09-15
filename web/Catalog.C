#include "Catalog.H"
#include <array>
#include <memory>

namespace {
struct WeaponData {
  const char *name, *description;
  unsigned short price, duration;
};
#include "WeaponData.H"
static_assert(sizeof(weaponData) / sizeof(weaponData[0]) == BT_MAX_WEAPONS,
              "Weapon database and protocol token counts differ");

struct Catalog {
  std::array<std::unique_ptr<BTWeapon>, BT_MAX_WEAPONS> weapons;
  Catalog() {
    for (int i = 0; i < BT_MAX_WEAPONS; ++i) {
      const auto &data = weaponData[i];
      weapons[i].reset(new BTWeapon(i, data.name, data.description, data.price, data.duration));
    }
  }
};
}

BTWeapon *catalogWeapon(int token) {
  if (token < 0 || token >= BT_MAX_WEAPONS) return nullptr;
  static Catalog catalog;
  return catalog.weapons[token].get();
}

bool supportedWeapon(int token) {
  switch (token) {
  case BT_AMES: case BT_ACE: case BT_CONDOR:
  case BT_UPBYSIDE:
  case BT_HATTER: case BT_SLICK:
  case BT_NO_SLIDE:
  case BT_FALL_OUT: case BT_BOTTLE:
  case BT_SWAP: case BT_LAWYERS: case BT_MONDALE: case BT_KEATING: case BT_SUSAN:
  case BT_FEARED_WEIRD: case BT_FOUR_BY_FOUR: case BT_RISE_UP:
  case BT_FLIP_OUT: case BT_SPEEDY: case BT_MISSING: case BT_PIECE_IT:
  case BT_BLIND: case BT_CARTER: case BT_REAGAN: case BT_NICE_DAY:
  case BT_SO_LONG: case BT_NO_DICE: case BT_BUG: case BT_MEADOW:
  case BT_MIRROR: case BT_TWILIGHT: case BT_BROKEN: case BT_FORCE: case BT_GIMP:
    return true;
  default: return false;
  }
}
