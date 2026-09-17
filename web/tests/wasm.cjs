const assert = require("node:assert/strict");
const createBattleTris = require("../build/battletris.js");

(async () => {
  const game = await createBattleTris();
  const drop = () => {
    game._bt_input(4);
    for (let i = 0; i < 45 && game._bt_status() === 0; ++i) game._bt_tick(10);
  };
  const snapshot = (opponent = false) => {
    const ptr = (opponent ? game._bt_op_cells() : game._bt_cells()) >>> 2;
    return Array.from(game.HEAP32.subarray(ptr, ptr + game._bt_width() * game._bt_height()));
  };
  game._bt_reset(42);
  assert.equal(game._bt_width(), 10);
  assert.equal(game._bt_height(), 28);
  const initial = snapshot();
  assert(initial.some(id => id > 0));
  game._bt_input(5);
  for (let i = 0; i < 100; ++i) game._bt_tick(100);
  assert.deepEqual(snapshot(), initial);
  game._bt_input(5);
  for (let i = 0; i < 6; ++i) game._bt_tick(100);
  assert.notDeepEqual(snapshot(), initial);
  for (let i = 0; i < 1000 && game._bt_status() !== 2; ++i) drop();
  assert.equal(game._bt_status(), 2);
  game._bt_reset(42);
  assert.deepEqual(snapshot(), initial);
  assert.equal(game._bt_lines(), 0);
  assert.equal(game._bt_funds(), 0);
  assert.equal(game._bt_score(), 0);
  game._bt_start(42, 1, 10);
  assert.equal(game._bt_mode(), 1);
  const opponentInitial = snapshot(true);
  assert.equal(game._bt_recon_enabled(), 1);
  assert.equal(game._bt_recon_remaining(), 65535);
  assert.equal(game._bt_recon_known(), 0);
  const nextOpponentPlacement = () => {
    const score = game._bt_op_score();
    for (let i = 0; i < 100 && game._bt_op_score() === score; ++i) game._bt_tick(10);
    assert.equal(game._bt_op_score(), score + 14);
  };
  nextOpponentPlacement();
  assert.equal(game._bt_recon_known(), 0); // First lock activates the request.
  nextOpponentPlacement();
  assert.equal(game._bt_recon_known(), 1);
  assert.equal(game._bt_recon_token(), 19); // Free native Ernie Condor.
  const report = () => Array.from(game.HEAP32.subarray(game._bt_recon_cells() >>> 2,
    (game._bt_recon_cells() >>> 2) + 280));
  const cached = report();
  assert.deepEqual(report(), cached);
  assert.equal(game._bt_toggle_recon(), 1);
  assert.equal(game._bt_recon_known(), 0);
  assert(report().every(cell => cell === 0));
  assert.equal(game._bt_toggle_recon(), 1);
  assert.equal(game._bt_recon_known(), 0);
  nextOpponentPlacement();
  assert.equal(game._bt_recon_known(), 0);
  nextOpponentPlacement();
  assert.equal(game._bt_recon_known(), 1);
  assert.notDeepEqual(snapshot(true), opponentInitial);
  assert(game._bt_op_score() >= 14);
  assert(Number.isInteger(game._bt_op_lines()));
  assert(Number.isInteger(game._bt_op_funds()));
  game._bt_input(5);
  const pausedPlayer = snapshot(), pausedOpponent = snapshot(true);
  for (let i = 0; i < 20; ++i) game._bt_tick(100);
  drop();
  assert.deepEqual(snapshot(), pausedPlayer);
  assert.deepEqual(snapshot(true), pausedOpponent);
  game._bt_input(5);
  for (let i = 0; i < 1000 && game._bt_status() < 2; ++i) drop();
  assert.equal(game._bt_status(), 2);
  const endedOpponent = snapshot(true);
  assert.equal(game._bt_recon_known(), 0);
  assert.equal(game._bt_recon_remaining(), 0);
  assert.equal(game._bt_recon_enabled(), 0);
  assert.equal(game._bt_toggle_recon(), 0);
  assert(report().every(cell => cell === 0));
  for (let i = 0; i < 20; ++i) game._bt_tick(100);
  assert.deepEqual(snapshot(true), endedOpponent);
  game._bt_start(42, 0, 0);
  assert.equal(game._bt_mode(), 0);
  assert.equal(game._bt_toggle_recon(), 0);
  assert.equal(game._bt_recon_remaining(), 0);
  assert.equal(game._bt_status(), 0);
  assert.equal(game._bt_score(), 0);
  assert.equal(game._bt_op_score(), 0);
  assert.equal(game._bt_weapon_count(), 34);
  let supported = 0;
  for (let token = 0; token < game._bt_weapon_count(); ++token) {
    assert(game.UTF8ToString(game._bt_weapon_name(token)).length > 0);
    assert(game.UTF8ToString(game._bt_weapon_description(token)).length > 0);
    if (game._bt_weapon_supported(token)) {
      supported++;
      assert(game._bt_weapon_price(token) > 0);
    }
  }
  assert.equal(supported, 34);
  assert.equal(game._bt_weapon_price(8), 15); // Original Flip Out price.
  assert.equal(game._bt_weapon_duration(0), 3); // Feared Weird duration.
  assert.equal(game._bt_buy(8), 0);
  assert.equal(game._bt_leave_bazaar(), 0);
  game._bt_start(42, 1, 10);
  for (let i = 0; i < 5000 && game._bt_status() === 0; ++i) game._bt_tick(100);
  assert.equal(game._bt_status(), 5); // Ernie reaches 20 lines naturally.
  assert.equal(game._bt_lines_until_bazaar(), 0);
  const shopPlayer = snapshot(), shopOpponent = snapshot(true);
  for (let i = 0; i < 20; ++i) game._bt_tick(100);
  drop(); game._bt_input(5);
  assert.equal(game._bt_status(), 5);
  assert.deepEqual(snapshot(), shopPlayer);
  assert.deepEqual(snapshot(true), shopOpponent);
  assert.equal(game._bt_buy(8), 0); // Idle player has earned no funds.
  assert.equal(game._bt_refund(0), 0);
  assert.equal(game._bt_refundable(0), 0);
  assert.equal(game._bt_launch(0), 0);
  assert.equal(game._bt_arsenal_token(0, 0), -1);
  assert(game._bt_arsenal_quantity(1, 0) > 0); // Ernie has shopped.
  assert.equal(game._bt_leave_bazaar(), 1);
  assert.equal(game._bt_status(), 0);
  for (let i = 0; i < 20; ++i) game._bt_tick(100);
  assert(game._bt_pending(0) > 0);
  assert(game.UTF8ToString(game._bt_message()).includes("Ernie launched"));
  drop();
  assert.equal(game._bt_pending(0), 0);
  assert(game._bt_remaining(0, 32) > 0); // Seeded Ernie bought The Force.
  game._bt_reset(42);
  for (let token = 0; token < 34; ++token) assert.equal(game._bt_remaining(0, token), 0);
  assert.equal(game._bt_arsenal_quantity(1, 0), 0);
  game._bt_online_start(42);
  assert.equal(game._bt_mode(), 2);
  assert.equal(game._bt_toggle_recon(), 0);
  for (const side of [0, 1]) {
    assert.equal(game._bt_side_recon_known(side), 0);
    assert.equal(game._bt_side_recon_remaining(side), 0);
    const rp = game._bt_side_recon_cells(side) >>> 2;
    assert(Array.from(game.HEAP32.subarray(rp, rp + 280)).every(id => id === 0));
    assert.equal(game._bt_side_score(side), 0);
    assert.equal(game._bt_side_lines(side), 0);
    assert.equal(game._bt_side_funds(side), 0);
    assert.equal(game._bt_side_pending(side), 0);
    assert.equal(game._bt_side_ready_state(side), 0);
    assert.equal(game._bt_side_refundable(side, 0), 0);
    assert(game._bt_side_price(side, 0) > 0);
    assert.equal(game._bt_side_buy(side, 0), 0);
    assert.equal(game._bt_side_refund(side, 0), 0);
    assert.equal(game._bt_side_launch(side, 0), 0);
    assert.equal(game._bt_side_ready(side), 0);
    assert.equal(game._bt_side_input(side, 5), 0);
    assert.equal(game._bt_side_input(side, 4), 1);
    assert(game._bt_side_score(side) > 14); // Human drop award, not AI's flat 14.
  }
  for (const side of [-1, 2]) {
    assert.equal(game._bt_side_cells(side), 0);
    assert.equal(game._bt_side_recon_cells(side), 0);
    assert.equal(game._bt_side_recon_token(side), -1);
    assert.equal(game._bt_side_price(side, 0), -1);
    assert.equal(game._bt_side_input(side, 4), 0);
    assert.equal(game._bt_side_buy(side, 0), 0);
    assert.equal(game._bt_side_refund(side, 0), 0);
    assert.equal(game._bt_side_launch(side, 0), 0);
    assert.equal(game._bt_side_ready(side), 0);
    assert.equal(game._bt_side_surrender(side), 0);
  }
  assert.equal(game._bt_set_paused(1), 1);
  assert.equal(game._bt_status(), 1);
  assert.equal(game._bt_side_input(1, 4), 0);
  assert.equal(game._bt_set_paused(0), 1);
  assert.equal(game._bt_side_surrender(1), 1);
  assert.equal(game._bt_status(), 3);
  assert.equal(game._bt_side_input(0, 4), 0);
  assert.equal(game._bt_set_paused(0), 0);
  console.log("WASM smoke test passed: lifecycle, catalogue, natural bazaar, AI shopping, deferred attack, reset.");
})().catch(error => { console.error(error); process.exitCode = 1; });
