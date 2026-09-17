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
  return token >= 0 && token < BT_MAX_WEAPONS;
}
