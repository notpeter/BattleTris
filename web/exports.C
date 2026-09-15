#include "Match.H"
#include <emscripten/emscripten.h>

static BrowserMatch match;
static int cells[2][BT_BOARD_WTH * BT_BOARD_HGT];
static int reports[2][BT_BOARD_WTH * BT_BOARD_HGT];
static int *snapshot(BrowserGame &game, int side) {
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      cells[side][y * BT_BOARD_WTH + x] = game.cell(x, y);
  return cells[side];
}
extern "C" {
EMSCRIPTEN_KEEPALIVE void bt_start(unsigned seed, int mode, int level) { match.start(seed, mode, level); }
EMSCRIPTEN_KEEPALIVE void bt_reset(unsigned seed) { match.reset(seed); }
EMSCRIPTEN_KEEPALIVE void bt_input(int command) { match.input(command); }
EMSCRIPTEN_KEEPALIVE void bt_tick(double ms) { match.tick(ms); }
EMSCRIPTEN_KEEPALIVE int bt_width() { return BT_BOARD_WTH; }
EMSCRIPTEN_KEEPALIVE int bt_height() { return BT_BOARD_HGT; }
EMSCRIPTEN_KEEPALIVE int bt_lines() { return match.player.lines; }
EMSCRIPTEN_KEEPALIVE int bt_funds() { return match.player.funds; }
EMSCRIPTEN_KEEPALIVE int bt_score() { return match.player.score; }
EMSCRIPTEN_KEEPALIVE int bt_op_lines() { return match.opponent.lines; }
EMSCRIPTEN_KEEPALIVE int bt_op_funds() { return match.opponent.funds; }
EMSCRIPTEN_KEEPALIVE int bt_op_score() { return match.opponent.score; }
EMSCRIPTEN_KEEPALIVE int bt_status() { return match.status(); }
EMSCRIPTEN_KEEPALIVE int bt_mode() { return match.mode(); }
EMSCRIPTEN_KEEPALIVE int bt_lines_until_bazaar() { return match.linesUntilBazaar(); }
EMSCRIPTEN_KEEPALIVE int bt_weapon_count() { return BT_MAX_WEAPONS; }
EMSCRIPTEN_KEEPALIVE int bt_weapon_supported(int token) { return supportedWeapon(token); }
EMSCRIPTEN_KEEPALIVE const char *bt_weapon_name(int token) {
  BTWeapon *weapon = catalogWeapon(token);
  return weapon ? weapon->name_ : "";
}
EMSCRIPTEN_KEEPALIVE const char *bt_weapon_description(int token) {
  BTWeapon *weapon = catalogWeapon(token);
  return weapon ? weapon->description_ : "";
}
EMSCRIPTEN_KEEPALIVE int bt_weapon_price(int token) { return match.price(token); }
EMSCRIPTEN_KEEPALIVE int bt_weapon_duration(int token) {
  BTWeapon *weapon = catalogWeapon(token);
  return weapon ? weapon->duration() : 0;
}
EMSCRIPTEN_KEEPALIVE int bt_buy(int token) { return match.buy(token); }
EMSCRIPTEN_KEEPALIVE int bt_refund(int slot) { return match.refund(slot); }
EMSCRIPTEN_KEEPALIVE int bt_refundable(int slot) { return match.refundable(slot); }
EMSCRIPTEN_KEEPALIVE int bt_arsenal_token(int side, int slot) { return match.arsenalToken(side, slot); }
EMSCRIPTEN_KEEPALIVE int bt_arsenal_quantity(int side, int slot) { return match.arsenalQuantity(side, slot); }
EMSCRIPTEN_KEEPALIVE int bt_launch(int slot) { return match.launch(slot); }
EMSCRIPTEN_KEEPALIVE int bt_remaining(int side, int token) {
  if (side < 0 || side > 1) return 0;
  return (side ? match.opponent : match.player).weapons.remaining(token);
}
EMSCRIPTEN_KEEPALIVE int bt_pending(int side) {
  if (side < 0 || side > 1) return 0;
  return (side ? match.opponent : match.player).pendingWeapons();
}
EMSCRIPTEN_KEEPALIVE int bt_leave_bazaar() { return match.leaveBazaar(); }
EMSCRIPTEN_KEEPALIVE const char *bt_message() { return match.message(); }
EMSCRIPTEN_KEEPALIVE int bt_toggle_recon() { return match.toggleRecon(); }
EMSCRIPTEN_KEEPALIVE int bt_recon_enabled() { return match.reconEnabled(); }
EMSCRIPTEN_KEEPALIVE int bt_recon_known() { return match.recon().known(); }
EMSCRIPTEN_KEEPALIVE int bt_recon_token() { return match.recon().token(); }
EMSCRIPTEN_KEEPALIVE int bt_recon_remaining() { return match.recon().remaining(); }
EMSCRIPTEN_KEEPALIVE int bt_recon_funds() { return match.recon().funds(); }
EMSCRIPTEN_KEEPALIVE int *bt_recon_cells() {
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      reports[0][y * BT_BOARD_WTH + x] = match.recon().cell(x, y);
  return reports[0];
}
EMSCRIPTEN_KEEPALIVE int *bt_cells() { return snapshot(match.player, 0); }
EMSCRIPTEN_KEEPALIVE int *bt_op_cells() { return snapshot(match.opponent, 1); }

// The server selects a viewer and publishes only that side's private fields.
// Invalid sides never alias player zero, including pointer-returning getters.
EMSCRIPTEN_KEEPALIVE void bt_online_start(unsigned seed) { match.start(seed, 2, 0); }
EMSCRIPTEN_KEEPALIVE int bt_side_input(int side, int command) { return match.sideInput(side, command); }
EMSCRIPTEN_KEEPALIVE int bt_side_buy(int side, int token) { return match.sideBuy(side, token); }
EMSCRIPTEN_KEEPALIVE int bt_side_refund(int side, int slot) { return match.sideRefund(side, slot); }
EMSCRIPTEN_KEEPALIVE int bt_side_launch(int side, int slot) { return match.sideLaunch(side, slot); }
EMSCRIPTEN_KEEPALIVE int bt_side_ready(int side) { return match.sideReady(side); }
EMSCRIPTEN_KEEPALIVE int bt_side_surrender(int side) { return match.sideSurrender(side); }
EMSCRIPTEN_KEEPALIVE int bt_set_paused(int paused) { return match.setPaused(paused != 0); }
EMSCRIPTEN_KEEPALIVE int *bt_side_cells(int side) {
  return side < 0 || side > 1 ? nullptr : snapshot(side ? match.opponent : match.player, side);
}
EMSCRIPTEN_KEEPALIVE int bt_side_score(int side) {
  return side < 0 || side > 1 ? 0 : (side ? match.opponent : match.player).score;
}
EMSCRIPTEN_KEEPALIVE int bt_side_lines(int side) {
  return side < 0 || side > 1 ? 0 : (side ? match.opponent : match.player).lines;
}
EMSCRIPTEN_KEEPALIVE int bt_side_funds(int side) {
  return side < 0 || side > 1 ? 0 : (side ? match.opponent : match.player).funds;
}
EMSCRIPTEN_KEEPALIVE int bt_side_pending(int side) { return bt_pending(side); }
EMSCRIPTEN_KEEPALIVE int bt_side_ready_state(int side) { return match.ready(side); }
EMSCRIPTEN_KEEPALIVE int bt_side_refundable(int side, int slot) { return match.refundable(slot, side); }
EMSCRIPTEN_KEEPALIVE int bt_side_price(int side, int token) { return match.price(token, side); }
EMSCRIPTEN_KEEPALIVE int bt_side_recon_known(int side) {
  return side >= 0 && side < 2 && match.recon(side).known();
}
EMSCRIPTEN_KEEPALIVE int bt_side_recon_token(int side) {
  return side < 0 || side > 1 ? -1 : match.recon(side).token();
}
EMSCRIPTEN_KEEPALIVE int bt_side_recon_remaining(int side) {
  return side < 0 || side > 1 ? 0 : match.recon(side).remaining();
}
EMSCRIPTEN_KEEPALIVE int bt_side_recon_funds(int side) {
  return side < 0 || side > 1 ? 0 : match.recon(side).funds();
}
EMSCRIPTEN_KEEPALIVE int *bt_side_recon_cells(int side) {
  if (side < 0 || side > 1) return nullptr;
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x)
      reports[side][y * BT_BOARD_WTH + x] = match.recon(side).cell(x, y);
  return reports[side];
}
}
