"use strict";

(async () => {
  const status = document.getElementById("status");
  try {
    const game = await createBattleTris();
    const gimp = new Image();
    gimp.src = "assets/gimp.png";
    await gimp.decode();
    const canvas = document.getElementById("board");
    const opponentCanvas = document.getElementById("opponent-board");
    const mode = document.getElementById("mode");
    const level = document.getElementById("level");
    const controls = [...document.querySelectorAll("[data-command]")];
    const pause = document.getElementById("pause");
    const width = game._bt_width(), height = game._bt_height();
    const size = canvas.width / width;
    canvas.height = opponentCanvas.height = height * size;
    let previous = 0, previousState = -1, ernieName = "Ernie";
    const bazaar = document.getElementById("bazaar");
    const done = document.getElementById("done-shopping");
    const text = (id, value) => {
      const element = typeof id === "string" ? document.getElementById(id) : id;
      if (element.textContent !== String(value)) element.textContent = value;
    };
    const weapons = Array.from({ length: game._bt_weapon_count() }, (_, token) => ({
      token,
      name: game.UTF8ToString(game._bt_weapon_name(token)),
      description: game.UTF8ToString(game._bt_weapon_description(token)),
      duration: game._bt_weapon_duration(token)
    }));
    const inventory = Array.from({ length: 10 }, (_, slot) => {
      const row = document.createElement("div");
      row.className = "slot";
      const label = document.createElement("button");
      const launch = document.createElement("button");
      launch.textContent = "Launch " + ((slot + 1) % 10);
      launch.addEventListener("click", () => { game._bt_launch(slot); render(); focusBoard(); });
      const refund = document.createElement("button");
      refund.textContent = "Undo purchase";
      refund.addEventListener("click", () => { game._bt_refund(slot); render(); });
      row.append(label, launch, refund);
      document.getElementById("arsenal").append(row);
      return { label, launch, refund };
    });
    const store = BattleTrisUI.shop(weapons.map(weapon => ({ ...weapon,
      price: game._bt_weapon_price(weapon.token) })), token => { game._bt_buy(token); render(); });

    function renderCombat(state, versus) {
      const shopping = state === 5;
      bazaar.hidden = !shopping;
      if (shopping !== (previousState === 5))
        document.getElementById(shopping ? "shop-inventory" : "combat")
          .append(document.getElementById("inventory-panel"));
      document.getElementById("combat").hidden = !versus;
      done.disabled = !shopping;
      text("shop-funds", game._bt_funds());
      text("bazaar-countdown", game._bt_lines_until_bazaar());
      text("combat-message", game.UTF8ToString(game._bt_message()));
      const held = new Map();
      let empty = false;
      for (let slot = 0; slot < 10; ++slot) {
        const token = game._bt_arsenal_token(0, slot);
        const quantity = game._bt_arsenal_quantity(0, slot);
        const owned = token >= 0 && quantity > 0;
        if (owned) held.set(token, quantity); else empty = true;
        const row = inventory[slot];
        text(row.label, (shopping ? "" : ((slot + 1) % 10) + ". ") + (owned ? weapons[token].name + (quantity > 1 ? " x" + quantity : "") : "< Empty >"));
        row.label.hidden = !shopping;
        row.label.disabled = !owned;
        text(row.launch, row.label.textContent);
        row.launch.setAttribute('aria-label', 'Launch slot ' + ((slot + 1) % 10) + ': ' + row.label.textContent);
        row.launch.disabled = state !== 0 || !owned;
        row.launch.hidden = shopping;
        row.refund.hidden = !shopping || !owned;
        const refundable = game._bt_refundable(slot);
        row.refund.disabled = !shopping || refundable <= 0;
        text(row.refund, "Undo purchase (" + refundable + ")");
        row.refund.setAttribute("aria-label", "Undo purchase from arsenal slot " + ((slot + 1) % 10));
      }
      for (const row of store.rows) {
        const cost = row.weapon.price = game._bt_weapon_price(row.weapon.token);
        row.disabled = !shopping || cost > game._bt_funds() ||
          (held.get(row.weapon.token) || 0) >= 32767 || (!empty && !held.has(row.weapon.token));
      }
      store.refresh();
      for (let side = 0; side < 2; ++side) {
        const active = weapons.map(weapon => ({ weapon, remaining: game._bt_remaining(side, weapon.token) }))
          .filter(effect => effect.remaining > 0)
          .map(({ weapon, remaining }) => weapon.name + ": " + remaining + " lines");
        const pending = game._bt_pending(side);
        text(side ? "opponent-effects" : "player-effects", versus && (pending || active.length)
          ? "Incoming: " + pending + ". Active: " + (active.join("; ") || "none") : "");
      }
      if (shopping && previousState !== 5) bazaar.focus();
      previousState = state;
    }

    function drawBoard(target, address) {
      const pointer = address >>> 2;
      BattleTrisUI.drawBoard(target, game.HEAP32.subarray(pointer, pointer + width * height), gimp);
    }

    function render() {
      drawBoard(canvas, game._bt_cells());
      const versus = game._bt_mode() === 1;
      const showRecon = versus && (!!game._bt_recon_enabled() || game._bt_recon_remaining() > 0);
      document.getElementById("opponent-panel").hidden = !showRecon;
      document.getElementById("game-layout").classList.toggle("no-recon", versus && !showRecon);
      document.getElementById("toggle-recon").hidden = !versus;
      document.getElementById("game-layout").classList.toggle("solo", !versus);
      const known = !!game._bt_recon_known();
      opponentCanvas.hidden = !known;
      if (versus && known) drawBoard(opponentCanvas, game._bt_recon_cells());
      document.getElementById("toggle-recon").setAttribute("aria-pressed", !!game._bt_recon_enabled());
      const state = game._bt_status();
      document.getElementById("toggle-recon").disabled = state >= 2;
      const match = versus ? "You vs " + ernieName : "Solo practice";
      text(status, [match + " - playing", match + " - paused",
        "You suck!",
        "You win! Restart to play again", "Draw - restart to play again",
        "Bazaar - both boards stopped"][state]);
      pause.textContent = state === 1 ? "Play" : "Pause";
      pause.disabled = state >= 2;
      for (const button of controls) button.disabled = state !== 0;
      document.getElementById("score").textContent = game._bt_score();
      document.getElementById("lines").textContent = game._bt_lines();
      document.getElementById("funds").textContent = game._bt_funds();
      document.getElementById("op-score").textContent = game._bt_op_score();
      document.getElementById("op-lines").textContent = game._bt_op_lines();
      document.getElementById("op-funds").textContent = known ? game._bt_recon_funds() : "?";
      renderCombat(state, versus);
    }

    function command(value) { game._bt_input(value); render(); }
    function focusBoard() {
      if (game._bt_status() !== 5) canvas.focus({ preventScroll: true });
    }
    function reset() {
      ernieName = level.selectedOptions[0].textContent + " Ernie";
      text("opponent-heading", ernieName);
      game._bt_start(crypto.getRandomValues(new Uint32Array(1))[0], Number(mode.value), Number(level.value));
      previous = 0;
      render();
      focusBoard();
    }
    mode.disabled = false;
    function updateSettings() { level.disabled = mode.value === "0"; }
    mode.addEventListener("change", updateSettings);
    updateSettings();
    document.getElementById("restart").disabled = false;
    reset();
    document.dispatchEvent(new Event("battletris:ready"));
    done.addEventListener("click", () => {
      if (game._bt_leave_bazaar()) { previous = 0; render(); focusBoard(); }
    });
    document.addEventListener('battletris:about', () => {
      if (game._bt_status() === 0) command(5);
    });
    pause.addEventListener("click", () => { command(5); focusBoard(); });
    document.getElementById("toggle-recon").addEventListener("click", () => {
      game._bt_toggle_recon(); render(); focusBoard();
    });
    document.getElementById("restart").addEventListener("click", () => reset());
    for (const button of controls)
      button.addEventListener("click", () => { command(Number(button.dataset.command)); focusBoard(); });

    const keys = { ArrowLeft: 0, ArrowRight: 1, ArrowUp: 2, ArrowDown: 3, Space: 4, KeyP: 5 };
    document.addEventListener("keydown", event => {
      if (event.altKey || event.ctrlKey || event.metaKey) return;
      // Preserve native selection and activation behavior in form controls.
      if (event.target.closest("select, input, textarea, button, summary, a, [contenteditable]")) return;
      if (game._bt_status() === 5) return;
      if (event.code === "KeyC") {
        event.preventDefault();
        if (!event.repeat) { game._bt_toggle_recon(); render(); }
        return;
      }
      if (/^Digit[0-9]$/.test(event.code)) {
        event.preventDefault();
        if (!event.repeat) { game._bt_launch((Number(event.code.slice(-1)) + 9) % 10); render(); }
        return;
      }
      const value = keys[event.code];
      if (value === undefined) return;
      event.preventDefault();
      if (event.repeat && value >= 4) return;
      command(value);
    });
    function suspend() {
      if (game._bt_status() === 0) command(5);
      previous = 0;
    }
    window.addEventListener("blur", suspend);
    document.addEventListener("visibilitychange", () => { if (document.hidden) suspend(); });
    function frame(now) {
      if (previous) game._bt_tick(now - previous);
      previous = now;
      render();
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
  } catch (error) {
    document.dispatchEvent(new Event("battletris:load-error"));
    console.error(error);
  }
})();
