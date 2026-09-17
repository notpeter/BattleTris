"use strict";

(() => {
  const $ = id => document.getElementById(id);
  const text = (id, value) => {
    const element = typeof id === 'string' ? $(id) : id;
    if (element.textContent !== String(value)) element.textContent = value;
  };
  const gimp = new Image();
  gimp.src = 'assets/gimp.png';
  let socket, generation = 0, reconnectTimer, attempts = 0, seq = 0;
  let room = null, token = null, snapshot = null, invitation = '', previousPhase = null;
  let terminalError = false, seatRetries = 0, catalog = [];
  const controls = [...document.querySelectorAll('[data-command]')];
  const weapons = new Map();
  let store;
  const slots = Array.from({ length: 10 }, (_, slot) => {
    const row = document.createElement('div'); row.className = 'slot';
    const label = document.createElement('button');
    const launch = document.createElement('button'); launch.textContent = 'Launch ' + ((slot + 1) % 10);
    launch.addEventListener('click', () => { command('launch', { slot }); focusBoard(); });
    const refund = document.createElement('button'); refund.textContent = 'Undo purchase';
    refund.addEventListener('click', () => command('refund', { slot }));
    row.append(label, launch, refund); $('arsenal').append(row);
    return { label, launch, refund };
  });
  function focusBoard() { $('board').focus({ preventScroll: true }); }
  function storageGet(key) { try { return sessionStorage.getItem(key); } catch { return null; } }
  function storageSet(key, value) { try { sessionStorage.setItem(key, value); } catch { /* The current connection still works without storage. */ } }
  function command(type, fields = {}) {
    if (!socket || socket.readyState !== WebSocket.OPEN || !snapshot) return;
    if (document.hidden && ['input', 'launch'].includes(type)) return;
    socket.send(JSON.stringify({ v: 1, type, seq: ++seq, ...fields }));
  }
  function disableControls() {
    controls.forEach(button => { button.disabled = true; });
    for (const { launch, refund } of slots) { launch.disabled = refund.disabled = true; }
    if (store) { for (const row of store.rows) row.disabled = true; store.refresh(); }
    for (const id of ['pause', 'ready', 'done-shopping', 'surrender']) $(id).disabled = true;
  }
  function connect(request) {
    clearTimeout(reconnectTimer);
    const current = ++generation;
    if (socket) socket.close();
    terminalError = false;
    let joined = false;
    disableControls();
    text('connection-error', '');
    text('status', request.type === 'reconnect' ? 'Reconnecting to your match...' : 'Connecting...');
    $('create-room').disabled = $('join-room').disabled = true;
    $('retry').hidden = true;
    socket = new WebSocket((location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws');
    socket.addEventListener('open', () => { if (current === generation) socket.send(JSON.stringify(request)); });
    socket.addEventListener('message', event => {
      if (current !== generation) return;
      let data;
      try { data = JSON.parse(event.data); } catch { return; }
      if (data.v !== 1) return;
      if (data.type === 'joined') {
        joined = true;
        // Every authenticated connection starts with a full catalog.
        catalog = [];
        seatRetries = 0;
        seq = Math.max(seq, data.ack || 0);
        room = data.room; token = data.token;
        storageSet('battletris-seat:' + room, token);
        // Seat credentials stay in this tab's storage, never in invitation URLs.
        history.replaceState(null, '', location.pathname + '?room=' + encodeURIComponent(room));
        if (data.invite) {
          invitation = location.origin + location.pathname + '?room=' + encodeURIComponent(room) + '&invite=' + encodeURIComponent(data.invite);
          storageSet('battletris-invite:' + room, invitation);
        } else invitation = storageGet('battletris-invite:' + room) || '';
        text('invite-link', invitation);
        $('invite-link').hidden = $('copy-invite').hidden = !invitation;
        $('lobby').hidden = true; $('room-controls').hidden = false;
        attempts = 0;
      } else if (data.type === 'state') {
        // Reconnection resumes the server's accepted sequence; commands are never replayed.
        seq = Math.max(seq, data.ack || 0);
        if (data.catalog) catalog = data.catalog;
        snapshot = { ...data, catalog };
        render();
      } else if (data.type === 'error') {
        // Reload can connect before the old socket's close reaches the server.
        if (!joined && request.type === 'reconnect' && data.code === 'seat-connected' && seatRetries++ < 3) {
          text('status', 'Waiting for your previous connection to close...');
          clearTimeout(reconnectTimer);
          reconnectTimer = setTimeout(() => { if (current === generation) connect(request); }, 700);
          return;
        }
        text('connection-error', data.message || 'The server rejected this request.');
        if (!joined) {
          terminalError = true;
          $('create-room').disabled = $('join-room').disabled = false;
          $('lobby').hidden = false;
          text('status', 'Could not join this match. Create a new match or use another invitation.');
        }
      }
    });
    socket.addEventListener('close', event => {
      if (current !== generation) return;
      disableControls();
      if (terminalError) return;
      if (event.code === 1008) {
        text('status', 'This connection was closed by the server.');
        $('create-room').disabled = $('join-room').disabled = false;
        $('lobby').hidden = false;
        return;
      }
      if (room && token && attempts < 8 && snapshot?.phase !== 'ended') {
        text('status', 'Connection lost. Reconnecting...');
        reconnectTimer = setTimeout(() => connect({ v: 1, type: 'reconnect', room, token }), Math.min(1000 * 2 ** attempts++, 8000));
      } else {
        text('status', snapshot?.phase === 'ended' ? 'Match ended.' : 'Could not connect. Check your connection and try again.');
        $('create-room').disabled = $('join-room').disabled = false;
        $('retry').hidden = !room;
        $('new-room').hidden = !room;
      }
    });
    socket.addEventListener('error', () => { /* Close handles retry and displays the connection state. */ });
  }
  function join(value) {
    try {
      const url = new URL(value, location.href), params = url.searchParams.has('room') ? url.searchParams : new URLSearchParams(url.hash.slice(1));
      const inviteRoom = params.get('room'), secret = params.get('invite');
      if (!inviteRoom || !secret) throw new Error('Paste a complete invitation link with its room and invite code.');
      snapshot = null; room = token = null; seq = 0;
      connect({ v: 1, type: 'join', room: inviteRoom, invite: secret });
    } catch (error) { text('connection-error', error.message); }
  }
  function render() {
    const state = snapshot, own = state.own, phase = state.phase;
    const playing = phase === 'playing', shopping = phase === 'bazaar';
    $('ready').hidden = phase !== 'waiting';
    $('ready').disabled = phase !== 'waiting' || !!state.ready?.[state.side];
    text('ready', state.ready?.[state.side] ? 'Ready - waiting for opponent' : 'Ready to play');
    text('room-status', phase === 'waiting' ? 'Room ' + state.room + '. ' + (state.connected?.every(Boolean) ? 'Both players connected.' : 'Waiting for opponent.') : 'Room ' + state.room);
    $('surrender').hidden = phase === 'waiting' || phase === 'ended';
    $('surrender').disabled = phase === 'ended' || phase === 'reconnecting';
    $('new-room').hidden = phase !== 'ended';
    $('invite-note').hidden = phase !== 'waiting';
    $('invite-link').hidden = $('copy-invite').hidden = phase !== 'waiting' || !invitation;
    const descriptions = { waiting: 'Waiting for both players to be ready.', playing: 'Match in progress.', paused: 'Match paused. Either player can resume.', bazaar: 'Weapons bazaar - waiting for both players to finish.', reconnecting: 'Match stopped while a player reconnects.', ended: state.result === 'win' ? 'You win!' : state.result === 'loss' ? 'You suck!' : 'Match ended in a draw.' };
    text('status', descriptions[phase] || phase);
    text('combat-message', state.message || '');
    $('game-layout').hidden = !own;
    if (own && $('room-controls').parentElement !== $('online-sidebar'))
      $('online-sidebar').prepend($('room-controls'));
    else if (!own && $('room-controls').parentElement === $('online-sidebar'))
      $('bazaar').before($('room-controls'));
    $('bazaar').hidden = !shopping;
    if (!own) { previousPhase = phase; return; }
    for (const weapon of state.catalog || []) {
      weapons.set(weapon.token, weapon);
    }
    if (!store && state.catalog?.length) store = BattleTrisUI.shop(state.catalog, token => command('buy', { token }));
    const name = token => weapons.get(token)?.name || 'Weapon ' + token;
    text('score', own.score); text('lines', own.lines); text('funds', own.funds); text('shop-funds', own.funds);
    text('op-score', state.opponent?.score ?? 0); text('op-lines', state.opponent?.lines ?? 0);
    drawBoard($('board'), own.cells);
    const recon = state.recon;
    $('opponent-board').hidden = !recon?.known;
    if (recon?.known && recon.cells) drawBoard($('opponent-board'), recon.cells);
    text('op-funds', recon?.known && recon.funds !== null ? '$' + recon.funds : 'Unknown');
    text('recon-status', recon?.remaining > 0 ? name(recon.token) + ': ' + (recon.known ? 'settled-board report' : 'waiting for a report') + '. ' + recon.remaining + ' opponent lines remaining.' : 'No reconnaissance. Buy a spy to reveal the opponent\'s board and funds.');
    text('player-effects', own.pending || own.effects?.length ? 'Incoming: ' + own.pending + '. Active: ' + ((own.effects || []).map(effect => name(effect.token) + ': ' + effect.remaining + ' lines').join('; ') || 'none') : '');
    text('bazaar-countdown', state.linesUntilBazaar);
    controls.forEach(button => { button.disabled = !playing; });
    $('pause').disabled = !playing && !shopping && phase !== 'paused';
    text('pause', phase === 'paused' ? 'Resume' : 'Pause');
    $('done-shopping').disabled = !shopping || own.bazaarReady;
    text('done-shopping', own.bazaarReady ? 'Waiting...' : 'DONE');
    const held = new Map(); let empty = false;
    for (let slot = 0; slot < 10; ++slot) {
      const item = own.inventory[slot], owned = item && item.token >= 0 && item.quantity > 0;
      if (owned) held.set(item.token, item.quantity); else empty = true;
      const row = slots[slot];
      text(row.label, (shopping ? '' : ((slot + 1) % 10) + '. ') + (owned ? name(item.token) + (item.quantity > 1 ? ' x' + item.quantity : '') : '< Empty >'));
      row.label.hidden = !shopping;
      row.label.disabled = !owned;
      text(row.launch, row.label.textContent);
      row.launch.setAttribute('aria-label', 'Launch slot ' + ((slot + 1) % 10) + ': ' + row.label.textContent);
      row.launch.hidden = shopping; row.launch.disabled = !playing || !owned;
      row.refund.hidden = !shopping || !owned; row.refund.disabled = !shopping || own.bazaarReady || !(item?.refundable > 0);
      text(row.refund, 'Undo purchase (' + (item?.refundable || 0) + ')');
      row.refund.setAttribute('aria-label', 'Undo purchase from arsenal slot ' + ((slot + 1) % 10));
    }
    if (store) {
      for (const row of store.rows) {
        row.weapon = weapons.get(row.weapon.token);
        const weapon = row.weapon;
        row.disabled = !shopping || own.bazaarReady || weapon.price > own.funds ||
          (held.get(weapon.token) || 0) >= 32767 || (!empty && !held.has(weapon.token));
      }
      store.refresh();
    }
    if (phase !== previousPhase) {
      $(shopping ? 'shop-inventory' : 'combat').append($('inventory-panel'));
      if (shopping) $('bazaar').focus();
      else if (playing) focusBoard();
    }
    previousPhase = phase;
  }
  function drawBoard(target, cells) { BattleTrisUI.drawBoard(target, cells, gimp); }
  $('create-room').addEventListener('click', () => {
    snapshot = null; room = token = null; seq = 0;
    connect({ v: 1, type: 'create' });
  });
  $('join-room').addEventListener('click', () => join($('join-link').value));
  $('join-link').addEventListener('keydown', event => { if (event.key === 'Enter') join(event.target.value); });
  $('ready').addEventListener('click', () => command('ready'));
  $('pause').addEventListener('click', () => { command(snapshot?.phase === 'paused' ? 'resume' : 'pause'); focusBoard(); });
  $('done-shopping').addEventListener('click', () => command('bazaar-ready'));
  $('surrender').addEventListener('click', () => { if (confirm('Surrender this match?')) command('surrender'); });
  $('new-room').addEventListener('click', () => { location.href = location.pathname; });
  $('retry').addEventListener('click', () => { attempts = 0; seatRetries = 0; connect({ v: 1, type: 'reconnect', room, token }); });
  $('copy-invite').addEventListener('click', async () => {
    try { await navigator.clipboard.writeText(invitation); text('connection-error', ''); text('room-status', 'Invitation copied. Share it privately.'); }
    catch {
      const selection = getSelection(), range = document.createRange();
      range.selectNodeContents($('invite-link')); selection.removeAllRanges(); selection.addRange(range);
      text('room-status', 'Invitation selected. Copy it and share it privately.');
    }
  });
  for (const button of controls) button.addEventListener('click', () => {
    if (snapshot?.phase === 'playing') command('input', { command: Number(button.dataset.command) });
    focusBoard();
  });
  document.addEventListener('keydown', event => {
    if (document.hidden || event.target.closest('input, select, textarea, button, summary, a') || event.ctrlKey || event.altKey || event.metaKey) return;
    const key = event.key.toLowerCase();
    if (key === 'p' && ['playing', 'paused', 'bazaar'].includes(snapshot?.phase) && !event.repeat) {
      event.preventDefault(); command(snapshot.phase === 'paused' ? 'resume' : 'pause'); return;
    }
    if (snapshot?.phase !== 'playing') return;
    const commands = { arrowleft: 0, arrowright: 1, arrowup: 2, arrowdown: 3, ' ': 4 };
    if (key in commands) { event.preventDefault(); command('input', { command: commands[key] }); }
    else if (/^[0-9]$/.test(key) && !event.repeat) { event.preventDefault(); command('launch', { slot: (Number(key) + 9) % 10 }); }
  });
  document.addEventListener('visibilitychange', () => {
    if (document.hidden && ['playing', 'bazaar'].includes(snapshot?.phase)) command('pause');
  });
  const params = new URLSearchParams(location.search || location.hash.slice(1));
  if (params.get('room')) {
    room = params.get('room'); token = storageGet('battletris-seat:' + room);
    if (token) connect({ v: 1, type: 'reconnect', room, token });
    else if (params.get('invite')) join(location.href);
    else text('connection-error', 'This tab has no seat for that room. Open the original invitation to join.');
  }
})();

document.addEventListener('battletris:about', () => {
  const pause = document.getElementById('pause');
  if (!pause.disabled && pause.textContent === 'Pause') pause.click();
});
