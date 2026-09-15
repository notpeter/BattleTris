#ifndef BT_WEB_CATALOG_H
#define BT_WEB_CATALOG_H

#include "BTWeapon.H"

// Stable records shared by both arsenals. Callers do not own these weapons.
BTWeapon *catalogWeapon(int token);
bool supportedWeapon(int token);

#endif
