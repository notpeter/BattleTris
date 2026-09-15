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
    const colors = ["#080b0f", "#f3ecdb", "#f4d458", "#e65c59", "#628aee",
      "#e89949", "#65c17a", "#65d2d9", "#ba82de", "#aaa"];
    let previous = 0, previousState = -1;
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
      duration: game._bt_weapon_duration(token),
      supported: !!game._bt_weapon_supported(token)
    }));
    const inventory = Array.from({ length: 10 }, (_, slot) => {
      const row = document.createElement("div");
      row.className = "slot";
      const label = document.createElement("div");
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
    const store = weapons.filter(weapon => weapon.supported)
      .sort((a, b) => game._bt_weapon_price(a.token) - game._bt_weapon_price(b.token))
      .map(weapon => {
        const card = document.createElement("div");
        card.className = "weapon";
        const details = document.createElement("details");
        const title = document.createElement("summary");
        title.textContent = weapon.name;
        const description = document.createElement("p");
        description.className = "note";
        description.textContent = weapon.description;
        details.append(title, description);
        const price = document.createElement("p");
        const buy = document.createElement("button");
        buy.textContent = "Buy " + weapon.name;
        buy.addEventListener("click", () => { game._bt_buy(weapon.token); render(); });
        card.append(details, price, buy);
        document.getElementById("shop").append(card);
        return { weapon, price, buy };
      });

    function renderCombat(state, versus) {
      const shopping = state === 5;
      bazaar.hidden = !shopping;
      if (shopping !== (previousState === 5))
        document.getElementById(shopping ? "shop-inventory" : "combat")
          .append(document.getElementById("inventory-panel"));
      document.getElementById("combat").hidden = !versus;
      done.disabled = !shopping;
      text("shop-funds", game._bt_funds());
      text("bazaar-countdown", "Lines until bazaar: " + game._bt_lines_until_bazaar());
      text("combat-message", game.UTF8ToString(game._bt_message()));
      const held = new Map();
      let empty = false;
      for (let slot = 0; slot < 10; ++slot) {
        const token = game._bt_arsenal_token(0, slot);
        const quantity = game._bt_arsenal_quantity(0, slot);
        const owned = token >= 0 && quantity > 0;
        if (owned) held.set(token, quantity); else empty = true;
        const row = inventory[slot];
        text(row.label, ((slot + 1) % 10) + ". " + (owned ? weapons[token].name + " x" + quantity : "Empty"));
        row.launch.disabled = state !== 0 || !owned;
        row.launch.hidden = shopping;
        row.refund.hidden = !shopping;
        const refundable = game._bt_refundable(slot);
        row.refund.disabled = !shopping || refundable <= 0;
        text(row.refund, "Undo purchase (" + refundable + ")");
        row.refund.setAttribute("aria-label", "Undo purchase from arsenal slot " + ((slot + 1) % 10));
      }
      for (const { weapon, price, buy } of store) {
        const cost = game._bt_weapon_price(weapon.token);
        text(price, "$" + cost + " / " + (weapon.duration ? weapon.duration + " affected-player lines" : "Instant effect"));
        buy.disabled = !shopping || cost > game._bt_funds() ||
          (held.get(weapon.token) || 0) >= 32767 || (!empty && !held.has(weapon.token));
      }
      for (let side = 0; side < 2; ++side) {
        const active = weapons.map(weapon => ({ weapon, remaining: game._bt_remaining(side, weapon.token) }))
          .filter(effect => effect.remaining > 0)
          .map(({ weapon, remaining }) => weapon.name + ": " + remaining + " lines");
        text(side ? "opponent-effects" : "player-effects", versus
          ? "Incoming: " + game._bt_pending(side) + ". Active: " + (active.join("; ") || "none") : "");
      }
      if (shopping && previousState !== 5) bazaar.focus();
      previousState = state;
    }

    function drawBoard(target, address) {
      const ctx = target.getContext("2d");
      const pointer = address >>> 2;
      const cells = game.HEAP32.subarray(pointer, pointer + width * height);
      ctx.fillStyle = colors[0];
      ctx.fillRect(0, 0, target.width, target.height);
      for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
        const id = cells[y * width + x];
        if (id <= 0) continue;
        ctx.fillStyle = colors[id] || colors[1];
        ctx.fillRect(x * size + 1, y * size + 1, size - 2, size - 2);
        ctx.strokeStyle = "#ffffff66";
        ctx.strokeRect(x * size + 3, y * size + 3, size - 6, size - 6);
        ctx.fillStyle = "#191d22";
        if (id >= 24 && id <= 29) {
          const pips = id - 23;
          const positions = [];
          if (pips % 2) positions.push([.5, .5]);
          if (pips >= 2) positions.push([.25, .25], [.75, .75]);
          if (pips >= 4) positions.push([.75, .25], [.25, .75]);
          if (pips === 6) positions.push([.25, .5], [.75, .5]);
          for (const [px, py] of positions) {
            ctx.beginPath(); ctx.arc((x + px) * size, (y + py) * size, 2, 0, Math.PI * 2); ctx.fill();
          }
        } else if (id === 20) {
          // Fixed Bottleneck walls are distinct from ordinary removable cells.
          ctx.fillStyle = "#536778";
          ctx.fillRect(x * size + 2, y * size + 2, size - 4, size - 4);
          ctx.fillStyle = "#bac9d2";
          ctx.fillRect(x * size + 4, y * size + 4, 2, 2);
          ctx.fillRect((x + 1) * size - 6, (y + 1) * size - 6, 2, 2);
        } else if (id === 23) {
          ctx.imageSmoothingEnabled = false;
          ctx.drawImage(gimp, x * size + 1, y * size + 1, size - 2, size - 2);
        } else if (id === 21 || id === 22) {
          ctx.fillRect((x + .3) * size, (y + .3) * size, 3, 3);
          ctx.fillRect((x + .65) * size, (y + .3) * size, 3, 3);
          ctx.strokeStyle = "#191d22";
          ctx.beginPath();
          ctx.arc((x + .5) * size, (y + (id === 21 ? .5 : .8)) * size,
            size * .23, 0, Math.PI, id === 22);
          ctx.stroke();
        }
      }
    }

    function render() {
      drawBoard(canvas, game._bt_cells());
      const versus = game._bt_mode() === 1;
      document.getElementById("opponent-panel").hidden = !versus;
      document.getElementById("game-layout").classList.toggle("solo", !versus);
      const known = !!game._bt_recon_known();
      opponentCanvas.hidden = !known;
      if (versus && known) drawBoard(opponentCanvas, game._bt_recon_cells());
      const spy = game._bt_recon_token();
      const remaining = game._bt_recon_remaining();
      text("recon-status", remaining > 0
        ? (spy >= 0 ? weapons[spy].name : "Reconnaissance") + ": " +
          (known ? "settled-board report" : "waiting for Ernie's report") +
          ". " + remaining + " opponent lines remaining."
        : "No reconnaissance. Press C to enable free Condor against Ernie.");
      text("toggle-recon", game._bt_recon_enabled() ? "Disable Condor (C)" : "Enable Condor (C)");
      const state = game._bt_status();
      document.getElementById("toggle-recon").disabled = state >= 2;
      const match = versus ? "You vs Ernie" : "Solo practice";
      text(status, [match + " - playing", match + " - paused",
        versus ? "Ernie wins - restart to play again" : "Game over - restart to play again",
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
    function reset(initial = false) {
      game._bt_start(crypto.getRandomValues(new Uint32Array(1))[0], Number(mode.value), Number(level.value));
      if (initial) game._bt_input(5);
      previous = 0;
      render();
      if (!initial) focusBoard();
    }
    mode.disabled = false;
    function updateSettings() { level.disabled = mode.value === "0"; }
    mode.addEventListener("change", updateSettings);
    updateSettings();
    document.getElementById("restart").disabled = false;
    reset(true);
    document.dispatchEvent(new Event("battletris:ready"));
    done.addEventListener("click", () => {
      if (game._bt_leave_bazaar()) { previous = 0; render(); focusBoard(); }
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
